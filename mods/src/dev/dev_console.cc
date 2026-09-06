#include "dev/dev_console.h"

#ifdef _MODDBG

#include "dev/diagnostics.h"
#include "dev/overlay_runtime.h"
#include "errormsg.h"
#include "mod_state.h"
#include "patches/mapkey.h"
#include "patches/screen_update_hook.h"

#include <prime/ChatManager.h>
#include <prime/KeyCode.h>
#include <prime/ScreenManager.h>
#include <prime/Transform.h>

#include <simdutf.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct BackgroundOpacity {
  int   percent;
  float alpha;
};
constexpr std::array<BackgroundOpacity, 3> kBackgroundOpacities{{{45, 0.45f}, {70, 0.70f}, {91, 0.91f}}};
constexpr float                            kPanelMargin         = 24.0f;
constexpr float                            kPanelTopMargin      = 12.0f;
constexpr float                            kEstimatedGlyphWidth = 8.5f;
constexpr size_t                           kMaximumSourceBytes  = 20;
constexpr auto                             kInitialRetryDelay   = std::chrono::milliseconds(250);
constexpr auto                             kMaximumRetryDelay   = std::chrono::milliseconds(5000);

dev::overlay::NativeElement           s_background;
dev::overlay::NativeElement           s_label;
uint64_t                              s_last_sequence    = 0;
uint64_t                              s_last_dropped     = 0;
size_t                                s_visible_lines    = 1;
size_t                                s_wrap_columns     = 80;
size_t                                s_opacity_index    = kBackgroundOpacities.size() - 1;
size_t                                s_applied_opacity  = std::numeric_limits<size_t>::max();
float                                 s_last_right       = -1.0f;
float                                 s_last_top         = -1.0f;
float                                 s_last_width       = -1.0f;
float                                 s_last_height      = -1.0f;
bool                                  s_force_render     = true;
bool                                  s_initialized      = false;
bool                                  s_chat_open        = false;
bool                                  s_chat_known       = false;
bool                                  s_creation_pending = false;
std::chrono::steady_clock::time_point s_retry_after;
std::chrono::milliseconds             s_retry_delay;
const auto                            s_console_source = dev::diagnostics::RegisterSource("dev-console");
const auto                            s_chat_source    = dev::diagnostics::RegisterSource("chat-state");

struct DisplayRow {
  uint64_t    sequence       = 0;
  size_t      message_offset = 0;
  std::string text;
};

struct RowAnchor {
  uint64_t sequence       = 0;
  size_t   message_offset = 0;
};

struct ResolvedHistory {
  size_t                   end = 0;
  std::optional<RowAnchor> anchor;
};

std::optional<RowAnchor> s_history_anchor;

void load_console_opacity()
{
  const auto state = mod_state::Read();
  if (!state) {
    return;
  }

  const auto console = state->find("dev_console");
  if (console == state->end() || !console->is_object()) {
    return;
  }
  const auto opacity = console->find("background_opacity_percent");
  if (opacity == console->end() || !opacity->is_number_integer()) {
    return;
  }

  const auto persisted = opacity->get<int>();
  for (size_t index = 0; index < kBackgroundOpacities.size(); ++index) {
    if (kBackgroundOpacities[index].percent == persisted) {
      s_opacity_index = index;
      return;
    }
  }
  spdlog::warn("[DevConsole] ignored unsupported persisted background opacity {}", persisted);
}

void save_console_opacity()
{
  const auto percent = kBackgroundOpacities[s_opacity_index].percent;
  mod_state::TryUpdate([percent](nlohmann::json& state) {
    auto& console = state["dev_console"];
    if (!console.is_object()) {
      console = nlohmann::json::object();
    }
    console.erase("background_opacity");
    console["background_opacity_percent"] = percent;
  });
}

std::string severity_label(dev::diagnostics::Severity severity)
{
  switch (severity) {
    case dev::diagnostics::Severity::Trace:
      return "TRC";
    case dev::diagnostics::Severity::Info:
      return "INF";
    case dev::diagnostics::Severity::Warning:
      return "WRN";
    case dev::diagnostics::Severity::Error:
      return "ERR";
  }
  return "???";
}

size_t utf8_prefix_bytes(std::string_view text, size_t maximum)
{
  auto length = std::min(text.size(), maximum);
  while (length && length < text.size() && (static_cast<unsigned char>(text[length]) & 0xC0U) == 0x80U) {
    --length;
  }
  return length;
}

std::string normalized_message(std::string message)
{
  std::string sanitized;
  sanitized.reserve(message.size());
  for (size_t index = 0; index < message.size();) {
    const auto byte = static_cast<unsigned char>(message[index]);
    if (byte < 0x80U) {
      if (byte < 0x20U || byte == 0x7FU) {
        sanitized.push_back(' ');
      } else if (message[index] == '<') {
        sanitized.push_back('[');
      } else if (message[index] == '>') {
        sanitized.push_back(']');
      } else {
        sanitized.push_back(message[index]);
      }
      ++index;
      continue;
    }

    size_t   length    = 0;
    uint32_t codepoint = 0;
    if (byte >= 0xC2U && byte <= 0xDFU) {
      length    = 2;
      codepoint = byte & 0x1FU;
    } else if (byte >= 0xE0U && byte <= 0xEFU) {
      length    = 3;
      codepoint = byte & 0x0FU;
    } else if (byte >= 0xF0U && byte <= 0xF4U) {
      length    = 4;
      codepoint = byte & 0x07U;
    }

    bool valid = length && index + length <= message.size();
    for (size_t continuation = 1; valid && continuation < length; ++continuation) {
      const auto next = static_cast<unsigned char>(message[index + continuation]);
      valid           = (next & 0xC0U) == 0x80U;
      codepoint       = (codepoint << 6U) | (next & 0x3FU);
    }
    if (valid) {
      valid = (length != 3 || codepoint >= 0x800U) && (length != 4 || codepoint >= 0x10000U)
              && !(codepoint >= 0xD800U && codepoint <= 0xDFFFU) && codepoint <= 0x10FFFFU;
    }
    if (!valid) {
      sanitized.push_back('?');
      ++index;
      continue;
    }

    if ((codepoint >= 0x80U && codepoint <= 0x9FU) || codepoint == 0x2028U || codepoint == 0x2029U) {
      sanitized.push_back(' ');
    } else {
      sanitized.append(message, index, length);
    }
    index += length;
  }
  return sanitized;
}

struct WrappedMessageRow {
  size_t      offset = 0;
  std::string text;
};

std::vector<WrappedMessageRow> wrap_message(std::string message, size_t first_width, size_t continuation_width)
{
  message = normalized_message(std::move(message));
  std::vector<WrappedMessageRow> rows;
  size_t                         position = 0;
  while (position < message.size()) {
    while (position < message.size() && message[position] == ' ') {
      ++position;
    }
    if (position == message.size()) {
      break;
    }

    const auto width     = std::max<size_t>(8, rows.empty() ? first_width : continuation_width);
    const auto remaining = std::string_view{message}.substr(position);
    if (remaining.size() <= width) {
      rows.push_back({position, std::string{remaining}});
      break;
    }

    auto take = utf8_prefix_bytes(remaining, width);
    if (remaining[take] != ' ') {
      if (const auto space = remaining.substr(0, take).find_last_of(' ');
          space != std::string_view::npos && space >= width / 2) {
        take = space;
      }
    }
    rows.push_back({position, std::string{remaining.substr(0, take)}});
    position += take;
  }
  if (rows.empty()) {
    rows.push_back({});
  }
  return rows;
}

std::vector<DisplayRow> format_entry(const dev::diagnostics::Entry& entry, size_t wrap_columns)
{
  std::ostringstream prefix;
  const auto         minutes = entry.elapsed_ms / 60'000;
  const auto         seconds = (entry.elapsed_ms / 1'000) % 60;
  const auto         millis  = entry.elapsed_ms % 1'000;
  auto               source  = normalized_message(entry.source);
  if (source.size() > kMaximumSourceBytes) {
    source.resize(utf8_prefix_bytes(source, kMaximumSourceBytes - 3));
    source += "...";
  }
  prefix << '[' << std::setfill('0') << std::setw(2) << minutes << ':' << std::setw(2) << seconds << '.' << std::setw(3)
         << millis << "] " << severity_label(entry.severity) << ' ' << source;

  const auto prefix_text        = prefix.str();
  const auto first_width        = wrap_columns > prefix_text.size() + 3 ? wrap_columns - prefix_text.size() - 3 : 8;
  const auto continuation_width = first_width;
  auto       messages           = wrap_message(entry.message, first_width, continuation_width);
  std::vector<DisplayRow> rows;
  rows.reserve(messages.size());
  for (size_t index = 0; index < messages.size(); ++index) {
    rows.push_back({entry.sequence, messages[index].offset,
                    (index == 0 ? prefix_text : std::string(prefix_text.size(), ' ')) + " | " + messages[index].text});
  }
  return rows;
}

bool console_format_self_test()
{
  std::string hostile{"nul"};
  hostile.push_back('\0');
  hostile += "backspace";
  hostile.push_back('\x08');
  hostile += " escape";
  hostile.push_back('\x1B');
  hostile += " del";
  hostile.push_back('\x7F');
  hostile += " <b>tag</b> ";
  hostile += "\xC2\x85\xE2\x80\xA8\xE2\x80\xA9";
  hostile += " malformed \xC3(";
  const auto sanitized         = normalized_message(hostile);
  const auto has_ascii_control = std::ranges::any_of(sanitized, [](char character) {
    const auto byte = static_cast<unsigned char>(character);
    return byte < 0x20U || byte == 0x7FU;
  });
  if (has_ascii_control || sanitized.find_first_of("<>") != std::string::npos
      || sanitized.find("\xC2\x85") != std::string::npos || sanitized.find("\xE2\x80\xA8") != std::string::npos
      || sanitized.find("\xE2\x80\xA9") != std::string::npos
      || !simdutf::validate_utf8(sanitized.data(), sanitized.size())) {
    return false;
  }

  dev::diagnostics::Entry entry{
      .sequence = 7,
      .severity = dev::diagnostics::Severity::Error,
      .source   = "<source-name-that-is-deliberately-too-long>",
      .message  = std::string(160, 'W'),
  };
  const auto rows = format_entry(entry, 48);
  return rows.size() > 3 && std::ranges::all_of(rows, [](const DisplayRow& row) {
           return row.sequence == 7 && row.text.find_first_of("<>\n\r") == std::string::npos
                  && simdutf::validate_utf8(row.text.data(), row.text.size());
         });
}

std::vector<DisplayRow> display_rows(const dev::diagnostics::Snapshot& snapshot, size_t wrap_columns)
{
  std::vector<DisplayRow> rows;
  for (const auto& entry : snapshot.entries) {
    auto entry_rows = format_entry(entry, wrap_columns);
    rows.insert(rows.end(), std::make_move_iterator(entry_rows.begin()), std::make_move_iterator(entry_rows.end()));
  }
  return rows;
}

size_t entry_line_budget(const dev::diagnostics::Snapshot& snapshot, size_t max_lines)
{ return std::max<size_t>(1, max_lines - static_cast<size_t>(snapshot.dropped != 0)); }

ResolvedHistory resolve_history(const std::vector<DisplayRow>& rows, size_t minimum_end,
                                const std::optional<RowAnchor>& requested)
{
  if (!requested || rows.empty()) {
    return {rows.size(), std::nullopt};
  }

  for (size_t index = 0; index < rows.size(); ++index) {
    if (rows[index].sequence == requested->sequence) {
      auto closest = index;
      while (closest + 1 < rows.size() && rows[closest + 1].sequence == requested->sequence
             && rows[closest + 1].message_offset <= requested->message_offset) {
        ++closest;
      }
      const auto end = std::max(closest + 1, minimum_end);
      return {end, end == rows.size()
                       ? std::nullopt
                       : std::optional<RowAnchor>{RowAnchor{rows[end - 1].sequence, rows[end - 1].message_offset}}};
    }
  }

  if (requested->sequence < rows.front().sequence) {
    const auto end = std::min(minimum_end, rows.size());
    return {end, end == rows.size()
                     ? std::nullopt
                     : std::optional<RowAnchor>{RowAnchor{rows[end - 1].sequence, rows[end - 1].message_offset}}};
  }
  return {rows.size(), std::nullopt};
}

bool history_anchor_self_test()
{
  dev::diagnostics::Entry entry{
      .sequence = 11,
      .severity = dev::diagnostics::Severity::Info,
      .source   = "anchor-test",
      .message  = std::string(160, 'W'),
  };
  const auto narrow = format_entry(entry, 48);
  const auto wide   = format_entry(entry, 112);
  if (narrow.size() < 4 || wide.size() < 2) {
    return false;
  }

  const RowAnchor requested{narrow[2].sequence, narrow[2].message_offset};
  const auto      resolved = resolve_history(wide, 1, requested);
  if (!resolved.anchor || resolved.end == 0 || resolved.end > wide.size()) {
    return false;
  }
  const auto& selected = wide[resolved.end - 1];
  const bool  starts_before_requested =
      selected.sequence == requested.sequence && selected.message_offset <= requested.message_offset;
  const bool next_starts_after_requested = resolved.end == wide.size()
                                           || wide[resolved.end].sequence != requested.sequence
                                           || wide[resolved.end].message_offset > requested.message_offset;
  if (!starts_before_requested || !next_starts_after_requested) {
    return false;
  }

  const RowAnchor collapsed_request{narrow.back().sequence, narrow.back().message_offset};
  const auto      collapsed = resolve_history(wide, 1, collapsed_request);
  if (collapsed.end != wide.size() || collapsed.anchor) {
    return false;
  }
  auto appended = wide;
  appended.push_back({12, 0, "new event"});
  const auto followed = resolve_history(appended, 1, collapsed.anchor);
  return followed.end == appended.size() && !followed.anchor;
}

std::string format_snapshot(const dev::diagnostics::Snapshot& snapshot, const std::vector<DisplayRow>& rows,
                            size_t max_lines, size_t end)
{
  std::ostringstream output;
  output << "<b>MOD DEV DIAGNOSTICS</b>  |  ";
  if (end < rows.size()) {
    output << "<b>HISTORY</b>  " << rows.size() - end << " newer";
  } else {
    output << "<b>LIVE</b>";
  }
  output << "  |  BG " << kBackgroundOpacities[s_opacity_index].percent << "%\n";
  if (snapshot.dropped) {
    output << "buffer dropped " << snapshot.dropped << " older entries\n";
  }

  const auto budget = entry_line_budget(snapshot, max_lines);
  const auto first  = end > budget ? end - budget : 0;
  for (size_t index = first; index < end; ++index) {
    output << rows[index].text << '\n';
  }
  return output.str();
}

dev::overlay::Insets persistent_hud_reserved_region(float canvas_width, float)
{
  return {
      .left  = std::min(220.0f, canvas_width * 0.13f),
      .right = std::min(560.0f, (canvas_width * 0.16f) + 36.0f),
  };
}

void observe_chat_state()
{
  auto*      chat_manager   = ChatManager::Instance();
  const bool side_chat_open = chat_manager && chat_manager->IsSideChatOpen;
  if (!s_chat_known || side_chat_open != s_chat_open) {
    s_chat_known = true;
    s_chat_open  = side_chat_open;
    dev::diagnostics::PublishLazy(s_chat_source, dev::diagnostics::Severity::Info, [side_chat_open] {
      return side_chat_open ? std::string{"side chat opened"} : std::string{"side chat closed"};
    });
  }
}

void release_panel()
{
  dev::overlay::ReleaseElement(s_label);
  dev::overlay::ReleaseElement(s_background);
  s_last_sequence    = 0;
  s_last_dropped     = 0;
  s_last_right       = -1.0f;
  s_last_top         = -1.0f;
  s_last_width       = -1.0f;
  s_last_height      = -1.0f;
  s_applied_opacity  = std::numeric_limits<size_t>::max();
  s_force_render     = true;
  s_creation_pending = false;
}

void reset_panel()
{
  release_panel();
  s_retry_after = {};
  s_retry_delay = {};
}

void defer_panel_retry()
{
  release_panel();
  const bool first_failure = s_retry_delay == std::chrono::milliseconds::zero();
  s_retry_delay            = first_failure ? kInitialRetryDelay : std::min(s_retry_delay * 2, kMaximumRetryDelay);
  s_retry_after            = std::chrono::steady_clock::now() + s_retry_delay;
  if (first_failure) {
    spdlog::warn("[DevConsole] native panel setup failed; retrying with bounded backoff");
  }
}

bool ensure_panel(Transform* host)
{
  if (dev::overlay::IsAlive(s_background) && dev::overlay::IsAlive(s_label)) {
    return true;
  }
  if (std::chrono::steady_clock::now() < s_retry_after) {
    return false;
  }

  release_panel();
  s_background =
      dev::overlay::CreateElement("CommunityDevConsolePanel", host, "UnityEngine.UI", "UnityEngine.UI", "Image");
  auto* background_transform = dev::overlay::GetTransform(s_background);
  if (!dev::overlay::IsAlive(s_background) || !background_transform) {
    defer_panel_retry();
    return false;
  }
  dev::overlay::SetActive(s_background, false);

  s_label = dev::overlay::CreateElement("CommunityDevConsoleText", background_transform, "Unity.TextMeshPro", "TMPro",
                                        "TextMeshProUGUI");
  if (!dev::overlay::IsAlive(s_label)) {
    defer_panel_retry();
    return false;
  }

  if (!dev::overlay::SetRaycastTarget(s_background, false)
      || !dev::overlay::SetGraphicColor(s_background,
                                        {0.025f, 0.055f, 0.075f, kBackgroundOpacities[s_opacity_index].alpha})
      || !dev::overlay::ConfigureText(s_label, 17.0f, 257, {0.80f, 0.92f, 0.96f, 1.0f})
      || !dev::overlay::ConfigureRect(s_label, {0.0f, 0.0f}, {1.0f, 1.0f}, {0.5f, 0.5f}, {-28.0f, -24.0f}, {})) {
    defer_panel_retry();
    return false;
  }
  s_applied_opacity  = s_opacity_index;
  s_creation_pending = true;
  return true;
}

void tick_panel(Transform* host, const dev::overlay::LayoutContext& context)
{
  if (!host || !ensure_panel(host)) {
    return;
  }

  observe_chat_state();

  const float top       = kPanelTopMargin;
  const float available = context.width - context.reserved.left - context.reserved.right - (2.0f * kPanelMargin);
  const float width     = std::min(760.0f, std::max(0.0f, available));
  const float height    = std::min(300.0f, std::max(0.0f, context.height - top - kPanelMargin));
  const bool  visible   = width >= 420.0f && height >= 180.0f;
  if (!visible) {
    dev::overlay::SetActive(s_background, false);
    return;
  }

  const float right = context.reserved.right + kPanelMargin;
  if (right != s_last_right || top != s_last_top || width != s_last_width || height != s_last_height) {
    if (!dev::overlay::ConfigureRect(s_background, {1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 1.0f}, {width, height},
                                     {-right, -top})) {
      defer_panel_retry();
      return;
    }
    s_last_right   = right;
    s_last_top     = top;
    s_last_width   = width;
    s_last_height  = height;
    s_force_render = true;
  }

  if (s_applied_opacity != s_opacity_index) {
    if (!dev::overlay::SetGraphicColor(s_background,
                                       {0.025f, 0.055f, 0.075f, kBackgroundOpacities[s_opacity_index].alpha})) {
      defer_panel_retry();
      return;
    }
    s_applied_opacity = s_opacity_index;
    s_force_render    = true;
  }

  const auto snapshot  = dev::diagnostics::ReadSnapshot();
  const auto sequence  = snapshot.entries.empty() ? 0 : snapshot.entries.back().sequence;
  const auto max_lines = std::max<size_t>(1, static_cast<size_t>((height - 48.0f) / 22.0f));
  const auto wrap_columns =
      std::clamp(static_cast<size_t>((width - 28.0f) / kEstimatedGlyphWidth), size_t{48}, size_t{112});
  if (s_force_render || sequence != s_last_sequence || snapshot.dropped != s_last_dropped) {
    const auto rows        = display_rows(snapshot, wrap_columns);
    const auto minimum_end = std::min(entry_line_budget(snapshot, max_lines), rows.size());
    const auto history     = resolve_history(rows, minimum_end, s_history_anchor);
    if (!dev::overlay::SetText(s_label, format_snapshot(snapshot, rows, max_lines, history.end))) {
      defer_panel_retry();
      return;
    }
    s_history_anchor = history.anchor;
    s_last_sequence  = sequence;
    s_last_dropped   = snapshot.dropped;
    s_force_render   = false;
  }
  s_visible_lines = max_lines;
  s_wrap_columns  = wrap_columns;
  s_retry_after   = {};
  s_retry_delay   = {};
  if (s_creation_pending) {
    s_creation_pending = false;
    spdlog::info("[DevConsole] native panel created");
  }
  dev::overlay::SetActive(s_background, true);
}

void set_console_enabled(bool enabled)
{
  if (enabled) {
    s_history_anchor.reset();
    dev::diagnostics::SetAwake(true);
    dev::overlay::SetEnabled(true);
    dev::diagnostics::Publish(s_console_source, dev::diagnostics::Severity::Info,
                              "diagnostics awake; producers and native rendering resumed");
    spdlog::info("[DevConsole] enabled");
  } else {
    dev::overlay::SetEnabled(false);
    dev::diagnostics::SetAwake(false);
    spdlog::info("[DevConsole] disabled; diagnostic producers are asleep");
  }
}
} // namespace

void InstallDevConsole()
{
  if (s_initialized) {
    return;
  }
  if (!console_format_self_test() || !history_anchor_self_test()) {
    spdlog::error("[DevConsole] display formatter self-test failed; console not installed");
    return;
  }
  load_console_opacity();
  s_initialized = true;
  dev::overlay::RegisterReservedRegionProvider("stfc-persistent-hud", persistent_hud_reserved_region);
  dev::overlay::RegisterPanel("dev-console", tick_panel, reset_panel);
  install_screen_manager_update_hook();
  spdlog::info("[DevConsole] private native diagnostics ready (toggle={}, initially asleep)",
               MapKey::GetShortcuts(GameFunction::DevConsoleToggle));
  if (const auto* auto_open = std::getenv("STFC_MOD_DEV_CONSOLE_AUTO_OPEN");
      auto_open && std::string_view{auto_open} == "1") {
    set_console_enabled(true);
    spdlog::info("[DevConsole] auto-opened by the private runtime test seam");
  }
}

bool dev_console_update(ScreenManager* screen_manager)
{
  if (!s_initialized) {
    return false;
  }

  bool handled = false;
  if (MapKey::IsDown(GameFunction::DevConsoleToggle)) {
    set_console_enabled(!dev::overlay::IsEnabled());
    handled = true;
  } else if (dev::overlay::IsEnabled() && MapKey::IsDown(GameFunction::DevConsoleClear)) {
    dev::diagnostics::Clear();
    dev::diagnostics::Publish(s_console_source, dev::diagnostics::Severity::Info, "diagnostic history cleared");
    s_history_anchor.reset();
    s_force_render = true;
    handled       = true;
  } else if (dev::overlay::IsEnabled() && MapKey::IsDown(GameFunction::DevConsoleScrollUp)) {
    const auto snapshot    = dev::diagnostics::ReadSnapshot();
    const auto rows        = display_rows(snapshot, s_wrap_columns);
    const auto minimum_end = std::min(entry_line_budget(snapshot, s_visible_lines), rows.size());
    const auto history     = resolve_history(rows, minimum_end, s_history_anchor);
    if (history.end > minimum_end) {
      const auto end   = history.end - 1;
      s_history_anchor = RowAnchor{rows[end - 1].sequence, rows[end - 1].message_offset};
      s_force_render   = true;
    }
    handled = true;
  } else if (dev::overlay::IsEnabled() && MapKey::IsDown(GameFunction::DevConsoleScrollDown)) {
    if (s_history_anchor) {
      const auto snapshot    = dev::diagnostics::ReadSnapshot();
      const auto rows        = display_rows(snapshot, s_wrap_columns);
      const auto minimum_end = std::min(entry_line_budget(snapshot, s_visible_lines), rows.size());
      const auto history     = resolve_history(rows, minimum_end, s_history_anchor);
      const auto end         = std::min(history.end + 1, rows.size());
      s_history_anchor =
          end == rows.size()
              ? std::nullopt
              : std::optional<RowAnchor>{RowAnchor{rows[end - 1].sequence, rows[end - 1].message_offset}};
      s_force_render = true;
    }
    handled = true;
  } else if (dev::overlay::IsEnabled() && MapKey::IsDown(GameFunction::DevConsoleScrollLive)) {
    s_history_anchor.reset();
    s_force_render = true;
    handled       = true;
  } else if (dev::overlay::IsEnabled() && MapKey::IsDown(GameFunction::DevConsoleCycleOpacity)) {
    s_opacity_index = (s_opacity_index + 1) % kBackgroundOpacities.size();
    save_console_opacity();
    s_force_render  = true;
    handled        = true;
  }

  dev::overlay::Tick(screen_manager);
  return handled;
}

#endif
