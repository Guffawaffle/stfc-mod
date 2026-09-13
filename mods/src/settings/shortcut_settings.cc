#include "shortcut_settings.h"
#include "config.h"
#include "page_catalog.h"
#include "patches/key.h"
#include "patches/keyboard_layout.h"
#include "patches/mapkey.h"
#include "patches/runtime_config.h"
#include "patches/screen_update_hook.h"
#include "shortcut_capture.h"
#include "shortcut_draft.h"
#include <algorithm>
#include <il2cpp/il2cpp_helper.h>
#include <memory>
#include <spdlog/spdlog.h>

namespace mod_settings
{
namespace
{
  void (*changed)() = nullptr;
  void Notify()
  {
    if (changed)
      changed();
  }
  std::string Join(const ShortcutList& list)
  {
    std::string result;
    for (const auto& text : list) {
      if (!result.empty())
        result += " | ";
      result += text;
    }
    return result.empty() ? "NONE" : result;
  }
  std::string HumanName(std::string text)
  {
    std::replace(text.begin(), text.end(), '_', ' ');
    if (!text.empty() && text[0] >= 'a' && text[0] <= 'z')
      text[0] -= 'a' - 'A';
    return text;
  }
  struct Editor {
    GameFunction                                function;
    ValueSetting<ShortcutList>                  state;
    ShortcutDraft                               draft;
    std::size_t                                 selected = 0;
    std::string                                 status, overlaps;
    std::vector<std::unique_ptr<ActionSetting>> rows;
    explicit Editor(GameFunction action)
        : function(action)
        , state({"community_mod.shortcuts." + MapKey::Definition(action).key, HumanName(MapKey::Definition(action).key),
                 [action] {
                   ShortcutList list;
                   for (const auto& binding : MapKey::Bindings(action))
                     list.push_back(binding.GetParsedValues());
                   return ValueReadResult<ShortcutList>::Known(std::move(list), 1);
                 },
                 [action](ShortcutList list, std::uint64_t generation) {
                   if (generation != 1)
                     return ApplyResult::Rejected;
                   std::vector<MapKey> bindings;
                   for (const auto& text : list) {
                     auto parsed = MapKey::Parse(text);
                     if (parsed.Key == KeyCode::None)
                       return ApplyResult::Rejected;
                     bindings.push_back(std::move(parsed));
                   }
                   const auto serialized = Join(list); // Allocate before publishing live bindings.
                   if (!MapKey::ReplaceBindings(action, std::move(bindings)))
                     return ApplyResult::Rejected;
                   runtime_config::SaveSetting("shortcuts", MapKey::Definition(action).key.c_str(), serialized);
                   return ApplyResult::Applied;
                 }})
        , draft(state)
    {
    }
    ShortcutList Current()
    {
      auto snapshot = state.Observe();
      auto list     = snapshot.state.value.value_or(ShortcutList{});
      if (selected >= list.size())
        selected = 0;
      return list;
    }
    std::string Selected()
    {
      auto list = Current();
      return list.empty() ? "Unbound" : list[selected];
    }
  };
  std::vector<std::unique_ptr<Editor>> editors;
  Editor*                              recording      = nullptr;
  std::size_t                          recordingIndex = 0;
  ShortcutCapture                      capture;
  std::vector<KeyCode>                 sampledKeys;

  // Advisory and deliberately conservative: matching physical primary keys can
  // overlap in different contexts or with additional held modifiers. The legacy
  // dispatcher allows extra modifiers, so exact chord equality would miss some.
  std::string Overlaps(Editor& editor, const std::string& token)
  {
    const auto candidate = keyboard_layout::DescribeChord(MapKey::Parse(token).Key);
    if (candidate.key == KeyCode::None)
      return "Layout unavailable; check this binding";
    std::string result;
    unsigned    count = 0;
    for (int i = 0; i < GameFunction::Max; ++i) {
      const auto action = static_cast<GameFunction>(i);
      if (action == editor.function)
        continue;
      bool overlap = false;
      for (const auto& binding : MapKey::Bindings(action))
        overlap |= keyboard_layout::DescribeChord(binding.Key).key == candidate.key;
      if (!overlap)
        continue;
      if (count++ < 2) {
        if (!result.empty())
          result += ", ";
        result += HumanName(MapKey::Definition(action).key);
      }
    }
    if (count > 2)
      result += " + " + std::to_string(count - 2) + " more";
    return count ? "May overlap: " + result + ". Apply keeps both." : "No other mod binding on this key";
  }

  void Cancel(Editor& editor)
  {
    if (recording == &editor) {
      recording = nullptr;
      capture.Cancel();
    }
    editor.draft.Cancel();
    editor.overlaps.clear();
    editor.status.clear();
  }
  void Begin(Editor& editor, bool append)
  {
    if (capture.active())
      return;
    Cancel(editor);
    editor.draft.Begin();
    recordingIndex = append ? editor.Current().size() : editor.selected;
    recording      = &editor;
    capture.Begin();
    Key::shortcutCaptureActive = true;
    editor.status              = "Release keys, then press a shortcut. Esc cancels.";
  }
  void UpdateCapture()
  {
    if (!capture.active())
      return; // No input scan, layout query or logging while idle.
    try {
      static auto           focused = il2cpp_resolve_icall_typed<bool()>("UnityEngine.Application::get_isFocused()");
      ShortcutCapture::Keys held{}, down{};
      for (auto key : sampledKeys) {
        held[static_cast<int>(key)] = Key::RawPressed(key);
        down[static_cast<int>(key)] = Key::RawDown(key);
      }
      const bool hasFocus = focused && focused();
      const bool cancel   = !hasFocus || down[static_cast<int>(KeyCode::Escape)];
      auto*      editor   = recording;
      const auto primary  = capture.Tick(held, down, hasFocus, Key::IsModifier);
      if (editor && cancel) {
        Cancel(*editor);
        editor->status = "Recording cancelled";
        Notify();
      } else if (editor && primary) {
        const auto isHeld = [&](KeyCode key) { return held[static_cast<int>(key)]; };
        const auto key =
            keyboard_layout::CaptureIdentity(*primary, isHeld(KeyCode::LeftShift) || isHeld(KeyCode::RightShift));
        // New bindings use generic modifiers, matching ordinary TOML bindings.
        // Existing sided modifiers remain untouched unless that binding is replaced.
        std::string token;
        if (isHeld(KeyCode::LeftControl) || isHeld(KeyCode::RightControl))
          token += "CTRL-";
        if (isHeld(KeyCode::LeftAlt) || isHeld(KeyCode::RightAlt))
          token += "ALT-";
        if (isHeld(KeyCode::LeftShift) || isHeld(KeyCode::RightShift))
          token += "SHIFT-";
        if (isHeld(KeyCode::LeftWindows) || isHeld(KeyCode::RightWindows))
          token += "WIN-";
        if (key == KeyCode::None || isHeld(KeyCode::AltGr)) {
          editor->status = "This key/layout is unavailable; record another";
        } else {
          token += Key::Token(key);
          editor->draft.Stage(recordingIndex, token);
          editor->overlaps = Overlaps(*editor, token);
          editor->status   = "Pending: " + token;
        }
        recording = nullptr;
        Notify();
      } else if (editor && !capture.active()) {
        recording      = nullptr;
        editor->status = "Recording cancelled; press one primary key";
        Notify();
      }
      const bool wasBlocked      = Key::shortcutCaptureActive;
      Key::shortcutCaptureActive = capture.active();
      if (wasBlocked != Key::shortcutCaptureActive)
        Notify();
    } catch (...) {
      capture.Cancel();
      if (recording) {
        recording->draft.Cancel();
        recording = nullptr;
      }
      // Continue draining via raw input on subsequent frames; never leave a draft
      // from a failed capture eligible for application.
      static bool warned = false;
      if (!warned) {
        warned = true;
        spdlog::warn("[Shortcuts] Key recording unavailable");
      }
    }
  }
  void AddRows(PageCatalog& catalog, Editor& editor)
  {
    auto add = [&](const char* id, const char* label, auto read, auto invoke, bool ownsVisit = false) {
      auto row      = std::make_unique<ActionSetting>();
      row->identity = editor.state.id() + "." + id;
      row->label    = label;
      row->read     = read;
      row->invoke   = [invoke] {
        invoke();
        Notify();
      };
      if (ownsVisit)
        row->hidden = [&editor] { Cancel(editor); };
      catalog.AddAction(editor.state.id(), *row);
      editor.rows.push_back(std::move(row));
    };
    using P = ActionSetting::Presentation;
    add(
        "current", "Current binding",
        [&editor] {
          const auto list = editor.Current();
          return P{"Current binding"
                       + (list.empty()
                              ? ""
                              : " (" + std::to_string(editor.selected + 1) + "/" + std::to_string(list.size()) + ")"),
                   "Next", editor.Selected(), list.size() > 1 && !capture.active()};
        },
        [&editor] {
          const auto list = editor.Current();
          if (!list.empty())
            ++editor.selected %= list.size();
        });
    add(
        "replace", "Replace selected binding",
        [&editor] { return P{"Replace selected binding", "Record", editor.Selected(), !capture.active()}; },
        [&editor] { Begin(editor, false); }, true);
    add(
        "add", "Add another binding", [] { return P{"Add another binding", "Record", "", !capture.active()}; },
        [&editor] { Begin(editor, true); });
    add(
        "remove", "Remove selected binding",
        [&editor] {
          return P{"Remove selected binding", "Remove", editor.Selected(),
                   !editor.Current().empty() && !capture.active()};
        },
        [&editor] {
          editor.draft.Begin();
          editor.draft.Stage(editor.selected, {});
          editor.status = "Pending removal: " + editor.Selected();
          editor.overlaps.clear();
        });
    add(
        "apply", "Pending change",
        [&editor] {
          return P{editor.status.empty() ? "No pending change" : editor.status, "Apply", "",
                   editor.draft.pending() && !capture.active()};
        },
        [&editor] {
          const auto result = editor.draft.Apply();
          editor.status     = result == Outcome::AppliedVerified || result == Outcome::Unchanged
                                  ? "Applied"
                                  : "Binding changed; reopen and try again";
          editor.overlaps.clear();
        });
    add(
        "overlaps", "Overlap information",
        [&editor] {
          return P{editor.overlaps.empty() ? "Overlaps are advisory; existing bindings are kept" : editor.overlaps, "",
                   "", false};
        },
        [] {});
    add(
        "cancel", "Discard pending change",
        [&editor] { return P{"Discard pending change", "Cancel", "", editor.draft.pending() || recording == &editor}; },
        [&editor] { Cancel(editor); });
  }
} // namespace

void SetShortcutPresentationObserver(void (*observer)())
{ changed = observer; }
void RegisterShortcutPages(PageCatalog& catalog)
{
  if (!editors.empty() || !Config::Get().installHotkeyHooks || Config::Get().use_scopely_hotkeys)
    return;
#if defined(_WIN32) && defined(_M_X64)
  if (!install_screen_manager_update_hook() || !register_screen_manager_update_callback(UpdateCapture))
    return;
  for (int i = 1; i < static_cast<int>(KeyCode::Max); ++i) {
    const auto key = static_cast<KeyCode>(i);
    if (!Key::Token(key).empty())
      sampledKeys.push_back(key);
  }
  catalog.AddPage("community_mod.shortcuts", "Shortcuts", "community_mod.settings");
  for (const auto* group : {"Navigation", "Camera", "Actions", "Other"})
    catalog.AddPage(std::string("community_mod.shortcuts.") + group, group, "community_mod.shortcuts");
  for (int i = 0; i < GameFunction::Max; ++i) {
    const auto  action = static_cast<GameFunction>(i);
    const auto& key    = MapKey::Definition(action).key;
    if (key.empty())
      continue;
    // A startup NONE binding opts out of installing the hints adapter. Do not
    // present a live editor for an action that cannot dispatch in this session.
    if (action == GameFunction::ToggleShortcutHints && !ShortcutHintControlAvailable())
      continue;
    auto        editor = std::make_unique<Editor>(action);
    const char* group =
        key.starts_with("show_") || key.starts_with("select_") ? "Navigation"
        : key.find("zoom") != std::string::npos || key.starts_with("move_") || key.find("scale") != std::string::npos
            ? "Camera"
        : key.starts_with("action_") || key.starts_with("toggle_") ? "Actions"
                                                                   : "Other";
    catalog.AddPage(editor->state.id(), editor->state.label(), std::string("community_mod.shortcuts.") + group);
    AddRows(catalog, *editor);
    editors.push_back(std::move(editor));
  }
#endif
}
} // namespace mod_settings
