// Per-action shortcut editing uses the existing parser, dispatcher, capture owner and registered-key writer.
// Recording or exact entry stages a draft; explicit confirmation owns the binding mutation.
#include "shortcut_settings.h"
#include "config.h"
#include "features/features.h"
#include "keyboard_layout.h"
#include "native_controls.h"
#include "page_catalog.h"
#include "runtime_config.h"
#include "shortcut_capture.h"
#include "shortcut_catalog.h"
#include "shortcut_undo.h"
#include "text_entry.h"

#include <algorithm>
#include <memory>
#include <optional>

namespace mod_settings
{
namespace
{
  struct ShortcutEditor;
  ShortcutEditor* editing        = nullptr;
  void (*presentation_changed)() = nullptr;

  void Notify()
  {
    native::RequestPageRefresh();
    if (presentation_changed)
      presentation_changed();
  }
  const Shortcut::ActionInfo* Info(Action action)
  {
    const auto catalog = Shortcut::Catalog();
    const auto found   = std::ranges::find(catalog, action, &Shortcut::ActionInfo::action);
    return found == catalog.end() ? nullptr : &*found;
  }
  std::string Join(const ShortcutList& bindings)
  {
    std::string value;
    for (const auto& binding : bindings) {
      if (!value.empty())
        value += " | ";
      value += binding;
    }
    return value.empty() ? "NONE" : value;
  }
  ShortcutList Defaults(const Shortcut::ActionInfo& info)
  {
    ShortcutList     result;
    std::string_view text = info.defaults;
    if (text == "NONE" || text.empty())
      return result;
    while (!text.empty()) {
      const auto split = text.find('|');
      const auto chord = KeyChord::ParseExact(text.substr(0, split));
      if (!chord)
        throw std::invalid_argument("invalid registered shortcut default");
      result.push_back(chord->Text());
      if (split == text.npos)
        break;
      text.remove_prefix(split + 1);
    }
    return result;
  }
  bool SameBinding(const std::string& first, const std::string& second)
  {
    const auto a = KeyChord::ParseExact(first), b = KeyChord::ParseExact(second);
    if (!a || !b || a->key != b->key || a->modifiers.size() != b->modifiers.size())
      return false;
    return std::ranges::all_of(a->modifiers, [&](const auto* modifier) {
      return std::ranges::find(b->modifiers, modifier) != b->modifiers.end();
    });
  }
  bool IsDeletionOf(const ShortcutList& before, const ShortcutList& desired)
  {
    if (desired.size() >= before.size())
      return false;
    auto next = desired.begin();
    for (const auto& binding : before)
      if (next != desired.end() && binding == *next)
        ++next;
    return next == desired.end();
  }

  struct ShortcutEditor {
    const Shortcut::ActionInfo&                 info;
    uint64_t                                    observed_global = 0, model_generation = 1, edit_serial = 0;
    native::PageVisitToken                      visit = 0;
    ValueSetting<ShortcutList>                  state;
    ShortcutDraft                               draft;
    ShortcutUndo                                undo;
    std::optional<Shortcut::BindingSnapshot>    undo_original;
    std::vector<std::unique_ptr<ActionSetting>> rows;
    std::string                                 status;
    std::vector<std::string>                    overlaps;

    ShortcutEditor(const Shortcut::ActionInfo& info, std::string label)
        : info(info)
        , state(MakeDefinition(std::move(label)))
        , draft(state, SameBinding)
        , undo(state)
    {
    }
    ValueDefinition<ShortcutList> MakeDefinition(std::string label)
    {
      const auto                    key = std::string(info.key);
      ValueDefinition<ShortcutList> definition{
          "community_mod.shortcuts." + key, std::move(label),
          [this] {
            if (!Available())
              return ValueReadResult<ShortcutList>{};
            auto current = Shortcut::Read(info.action);
            if (!current.generation)
              return ValueReadResult<ShortcutList>{};
            if (observed_global && observed_global != current.generation)
              ++model_generation;
            observed_global = current.generation;
            return ValueReadResult<ShortcutList>::Known(std::move(current.alternatives), model_generation);
          },
          [this](ShortcutList desired, uint64_t generation) {
            if (!Available() || generation != model_generation)
              return ApplyResult::Rejected;
            const auto current   = Shortcut::Read(info.action);
            const bool restoring = undo.Restoring(desired) && undo_original && undo_original->alternatives == desired;
            if (current.generation != observed_global
                || (!restoring && !ShortcutCountFitsEdit(current.alternatives.size(), desired.size())))
              return ApplyResult::Rejected;
            const bool deleting = IsDeletionOf(current.alternatives, desired);
            auto       retained = current;
            if (deleting)
              retained.alternatives = desired;
            // A deletion retains an exact ordered subset of the observed source. Like exact Undo, it may
            // preserve duplicates already authored in TOML; no new alternative uses the restoration path.
            if (!(restoring  ? Shortcut::Restore(info.action, *undo_original, current.generation)
                  : deleting ? Shortcut::Restore(info.action, retained, current.generation)
                             : Shortcut::Replace(info.action, desired, current.generation)))
              return ApplyResult::Rejected;
            const auto after = Shortcut::Read(info.action);
            if (after.alternatives != desired || !after.generation)
              return ApplyResult::Unverified;
            // The model keeps its generation across its own verified mutation. Every other global replacement
            // advances it on observation, invalidating drafts and undo even when this action's list stayed equal.
            observed_global = after.generation;
            return ApplyResult::Applied;
          }};
      definition.metadata = {"shortcuts", key, std::string(ShortcutExplanation(info.action)), "", ApplyTiming::Live};
      definition.default_value = Defaults(info);
      runtime_config::RegisterSetting("shortcuts", key.c_str());
      definition.persist = [key](const ShortcutList& bindings) {
        runtime_config::RegisterSetting("shortcuts", key.c_str());
        runtime_config::SaveSetting("shortcuts", key.c_str(), Join(bindings));
      };
      definition.save_status = [key](const ShortcutList& bindings) {
        return runtime_config::GetSaveStatus("shortcuts", key.c_str(), Join(bindings));
      };
      return definition;
    }
    bool Available() const
    { return ShortcutUnavailableReason(info.action).empty(); }
    ShortcutList Current()
    { return state.Observe().state.value.value_or(ShortcutList{}); }
  };
  std::vector<std::unique_ptr<ShortcutEditor>> editors;

  void Cancel(ShortcutEditor& editor, bool close, bool clear_undo = false)
  {
    const bool owned = editing == &editor;
    if (owned)
      editing = nullptr;
    ++editor.edit_serial;
    editor.draft.Cancel();
    editor.overlaps.clear();
    if (clear_undo) {
      editor.undo.Clear();
      editor.undo_original.reset();
    }
    if (owned && close)
      CloseTextEntry();
  }
  std::vector<std::string> Overlaps(ShortcutEditor& editor, const std::string& candidate)
  {
    std::vector<std::string> result;
    const auto               parsed = KeyChord::ParseExact(candidate);
    if (!parsed)
      return result;
    bool unknown = false;
    for (const auto& info : Shortcut::Catalog()) {
      if (info.action == editor.info.action)
        continue;
      const auto current = Shortcut::Read(info.action);
      if (std::ranges::any_of(current.alternatives, [&](const auto& binding) {
            const auto other = KeyChord::ParseExact(binding);
            if (!other)
              return false;
            const auto overlaps = Shortcut::MayOverlap(*parsed, *other);
            unknown |= !overlaps.has_value();
            return overlaps.value_or(false);
          }))
        result.push_back("Also assigned to " + DescribeShortcut(info.action, info.key).label);
    }
    if (unknown)
      result.push_back("Some overlaps cannot be checked until the configured keyboard layout is available.");
    if (parsed && (parsed->key == unity::KeyCode::Return || parsed->key == unity::KeyCode::KeypadEnter))
      result.push_back("Enter may also confirm game dialogs or assign a ship.");
    return result;
  }
  bool IsCaptureModifier(unity::KeyCode key)
  {
    using enum unity::KeyCode;
    switch (key) {
      case LeftControl:
      case RightControl:
      case LeftAlt:
      case RightAlt:
      case LeftShift:
      case RightShift:
      case LeftCommand:
      case RightCommand:
      case LeftWindows:
      case RightWindows:
      case AltGr:
        return true;
      default:
        return false;
    }
  }
  struct Recording {
    ShortcutCapture            capture;
    std::optional<std::string> candidate;
    std::string                error;
    Recording()
    { capture.Begin(); }
    std::optional<std::string> Sample()
    {
      if (candidate)
        return {};
      ShortcutCapture::Keys held{}, down{};
      // KeyToken is the parser's catalog. Unsupported enum holes are never queried or assigned a second spelling.
      static const auto sampled_keys = [] {
        std::vector<unity::KeyCode> result;
        for (int index = 1; index < static_cast<int>(unity::KeyCode::Max); ++index) {
          const auto key = static_cast<unity::KeyCode>(index);
          if (IsCaptureModifier(key) || key == unity::KeyCode::Escape || !Shortcut::KeyToken(key).empty())
            result.push_back(key);
        }
        return result;
      }();
      for (const auto key : sampled_keys) {
        const auto index = static_cast<int>(key);
        held[index]      = unity::Key::Pressed(key);
        down[index]      = unity::Key::Down(key);
      }
      const auto primary = capture.Tick(held, down, true, IsCaptureModifier);
      if (!primary) {
        if (!capture.active())
          capture.Begin();
        return {};
      }
      const auto is_held  = [&](unity::KeyCode key) { return held[static_cast<int>(key)]; };
      const auto identity = keyboard_layout::CaptureIdentity(*primary, is_held(unity::KeyCode::LeftShift)
                                                                           || is_held(unity::KeyCode::RightShift));
      const auto token    = Shortcut::KeyToken(identity);
      if (identity == unity::KeyCode::None || token.empty() || is_held(unity::KeyCode::AltGr)) {
        error = "This key is unavailable; release it and record another.";
        return std::string("Unavailable key");
      }
      candidate = CaptureModifierPrefix(is_held) + token;
      error.clear();
      return candidate;
    }
  };
  void OpenEditor(ShortcutEditor& editor, size_t index, bool record)
  {
    if (!editor.Available() || editor.draft.pending())
      return;
    if (editing)
      Cancel(*editing, true);
    const auto current = editor.Current();
    if (index > current.size()
        || (index == current.size() && !ShortcutCountFitsEdit(current.size(), current.size() + 1)))
      return;
    editor.undo.Clear();
    editor.undo_original.reset();
    editor.status.clear();
    editor.draft.Begin();
    editor.visit = native::CurrentPageVisit();
    if (!native::IsPageVisitCurrent(editor.visit)) {
      editor.draft.Cancel();
      return;
    }
    const auto serial    = ++editor.edit_serial;
    auto       recording = record ? std::make_shared<Recording>() : nullptr;
    editing              = &editor;
    const bool opened    = OpenTextEntry(
        {.title = editor.state.label(),
         .value = record                   ? ""
                  : index < current.size() ? current[index]
                                           : "",
         .help =
             record ? "Release the opening keys, then press a shortcut. Escape cancels. Current: "
                          + (index < current.size() ? current[index] : std::string("Unbound"))
                          + ". Stage the recorded chord, then confirm the proposal on this shortcut page."
                    : "Enter one exact chord, for example CTRL-SHIFT-F8 or MOUSE4. Stage it, then confirm the proposal "
                      "on this shortcut page.",
         .validate =
             [recording](std::string_view text) {
               if (recording && !recording->error.empty())
                 return recording->error;
               return KeyChord::ParseExact(text)
                          ? std::string{}
                          : std::string(recording ? "Press a shortcut to record it."
                                                  : "Enter one complete key chord with supported key names.");
             },
         .commit =
             [&editor, serial, index](std::string_view text) {
               const auto parsed = KeyChord::ParseExact(text);
               if (!parsed || editing != &editor || editor.edit_serial != serial || !editor.Available()
                   || !native::IsPageVisitCurrent(editor.visit))
                 return false;
               const auto staged = editor.draft.Stage(index, parsed->Text());
               if (staged == ShortcutStage::AlreadyBound) {
                 editor.status = "This action already has that binding.";
                 return false;
               }
               if (staged != ShortcutStage::Staged)
                 return false;
               editor.overlaps = Overlaps(editor, parsed->Text());
               editor.status   = "Proposed: " + parsed->Text();
               editing         = nullptr;
               Notify();
               return true;
             },
         .cancel =
             [&editor, serial] {
               if (editing == &editor && editor.edit_serial == serial) {
                 Cancel(editor, false);
                 Notify();
               }
             },
         .read_only    = record,
         .apply_label  = "Stage",
         .record_frame = recording ? std::function<std::optional<std::string>()>([&editor, serial, recording] {
           if (editing != &editor || editor.edit_serial != serial || !native::IsPageVisitCurrent(editor.visit))
             return std::optional<std::string>{};
           return recording->Sample();
         })
                                   : std::function<std::optional<std::string>()>{},
         .record_again = recording ? std::function<void()>([recording] {
           recording->candidate.reset();
           recording->error.clear();
           recording->capture.Begin();
         })
                                   : std::function<void()>{}});
    if (!opened) {
      Cancel(editor, false);
      editor.status = "Native text entry unavailable; no binding changed.";
    }
  }
  void AddRows(PageCatalog& catalog, ShortcutEditor& editor)
  {
    using P         = ActionSetting::Presentation;
    const auto page = editor.state.id();
    catalog.OnLeave(page, [&editor] {
      Cancel(editor, true, true);
      editor.state.InvalidateSession();
      editor.status.clear();
    });
    catalog.SetSummary(page,
                       [&editor] { return editor.Available() ? Join(editor.Current()) : "Unavailable this session"; });
    auto add = [&](const char* id, const char* label, auto read, auto invoke, std::function<size_t()> count = {}) {
      auto action      = std::make_unique<ActionSetting>();
      action->identity = page + "." + id;
      action->label    = label;
      action->read     = [&editor, read](size_t index) {
        auto result = read(index);
        result.enabled &= editor.Available();
        return result;
      };
      action->invoke = [&editor, invoke](size_t index) {
        if (editor.Available()) {
          invoke(index);
          Notify();
        }
      };
      action->count = std::move(count);
      catalog.AddAction(page, *action);
      editor.rows.push_back(std::move(action));
    };
    add(
        "status", "Shortcut status",
        [&editor](size_t) {
          auto text = ShortcutUnavailableReason(editor.info.action);
          if (text.empty()) {
            text = ShortcutExplanation(editor.info.action);
            if (!text.empty() && !editor.status.empty())
              text += '\n';
            text += editor.status;
          }
          for (const auto& warning : editor.overlaps) {
            if (!text.empty())
              text += '\n';
            text += warning;
          }
          const auto saving = editor.state.status_text();
          if (!saving.empty()) {
            if (!text.empty())
              text += '\n';
            text += saving;
          }
          return P{text, editor.undo.available() ? "Undo" : "", "", editor.undo.available() && !editor.draft.pending(),
                   !text.empty() || editor.undo.available()};
        },
        [&editor](size_t) {
          if (!editor.undo.available() || editor.draft.pending())
            return;
          const auto result = editor.undo.Undo();
          editor.undo_original.reset();
          editor.status = result == Outcome::AppliedVerified ? "Undone" : "Bindings changed; undo unavailable.";
        });
    const auto count = [&editor] { return VisibleShortcutBindingCount(editor.Current().size()); };
    add(
        "binding", "Shortcut",
        [&editor](size_t index) {
          const auto list = editor.Current();
          return P{index < list.size() ? list[index] : "", "Change", "", !editor.draft.pending() && !editing,
                   index < list.size()};
        },
        [&editor](size_t index) { OpenEditor(editor, index, true); }, count);
    add(
        "remove", "Remove shortcut",
        [&editor](size_t index) {
          const auto list = editor.Current();
          return P{index < list.size() ? list[index] : "", "Remove", "", !editor.draft.pending() && !editing,
                   index < list.size()};
        },
        [&editor](size_t index) {
          if (editor.draft.pending() || editing)
            return;
          editor.undo_original = Shortcut::Read(editor.info.action);
          const auto result    = editor.undo.Remove(index);
          if (result != Outcome::AppliedVerified)
            editor.undo_original.reset();
          editor.status = result == Outcome::AppliedVerified ? editor.undo.message() : "Could not verify removal.";
        },
        count);
    add(
        "add", "Add shortcut",
        [&editor](size_t) {
          const auto size = editor.Current().size();
          return P{size >= ShortcutBindingDisplayLimit ? "Remove a binding or edit a larger list in TOML"
                                                       : "Add shortcut",
                   "Record", "", size < ShortcutBindingDisplayLimit && !editor.draft.pending() && !editing};
        },
        [&editor](size_t) { OpenEditor(editor, editor.Current().size(), true); });
    add(
        "type", "Enter exact shortcut",
        [&editor](size_t) {
          return P{"Add an alternative using its exact TOML chord", "Enter chord", "",
                   editor.Current().size() < ShortcutBindingDisplayLimit && !editor.draft.pending() && !editing};
        },
        [&editor](size_t) { OpenEditor(editor, editor.Current().size(), false); });
    add(
        "confirm", "Confirm proposed shortcut",
        [&editor](size_t) {
          return P{editor.status, editor.overlaps.empty() ? "Confirm" : "Use anyway", "",
                   editor.draft.pending() && !editing, editor.draft.pending()};
        },
        [&editor](size_t) {
          if (!editor.draft.pending() || editing || !native::IsPageVisitCurrent(editor.visit))
            return;
          const auto result = editor.draft.Apply();
          editor.overlaps.clear();
          editor.status = result == Outcome::AppliedVerified || result == Outcome::Unchanged
                              ? "Binding applied"
                              : "Bindings changed; reopen the editor to try again.";
        });
    add(
        "cancel", "Cancel proposed shortcut",
        [&editor](size_t) {
          return P{"Discard the proposal", "Cancel", "", editor.draft.pending(), editor.draft.pending()};
        },
        [&editor](size_t) {
          Cancel(editor, true);
          editor.status = "Proposal cancelled";
        });
    add(
        "default", "Restore default",
        [&editor](size_t) {
          return P{"Default: " + std::string(editor.info.defaults), "Restore", "", !editor.draft.pending() && !editing,
                   editor.Current() != *editor.state.default_value()};
        },
        [&editor](size_t) {
          if (editor.draft.pending() || editing)
            return;
          editor.undo_original = Shortcut::Read(editor.info.action);
          const auto result    = editor.undo.Restore(*editor.state.default_value());
          if (result != Outcome::AppliedVerified)
            editor.undo_original.reset();
          editor.status =
              result == Outcome::AppliedVerified ? "Restored defaults" : "Could not verify default restoration.";
        });
    add(
        "unbind", "Unbind all alternatives",
        [&editor](size_t) {
          return P{"Remove every shortcut for this action", "Unbind", "", !editor.draft.pending() && !editing,
                   !editor.Current().empty()};
        },
        [&editor](size_t) {
          if (editor.draft.pending() || editing)
            return;
          editor.undo_original = Shortcut::Read(editor.info.action);
          const auto result    = editor.undo.Restore({});
          if (result != Outcome::AppliedVerified)
            editor.undo_original.reset();
          editor.status = result == Outcome::AppliedVerified ? "Action unbound" : "Could not verify unbinding.";
        });
  }
} // namespace

std::string ShortcutUnavailableReason(Action action)
{
  const auto* info = Info(action);
  if (!info)
    return "This action is not registered.";
  const bool installed = std::ranges::any_of(FeatureInstallations(), [](const auto& feature) {
    return feature.name && std::string_view(feature.name) == "Hotkeys" && feature.state == InstallState::kInstalled;
  });
  if (!installed)
    return "Mod shortcut hooks are unavailable this session.";
  const auto& control = Config::Get().control;
  if (control.use_scopely_hotkeys)
    return "Native shortcut mode is active.";
  if (info->extended && !control.hotkeys_extended)
    return "Extended shortcuts are disabled this session.";
  if (info->experimental && !control.enable_experimental)
    return "Experimental shortcuts are disabled this session.";
  if (action == Action::kNavigationOrbitDrag || action == Action::kNavigationOrbitReset)
    return "System orbit is parked in this product.";
  return {};
}
void RegisterShortcutPages(PageCatalog& catalog)
{
  if (!editors.empty())
    return;
  catalog.AddPage("community_mod.shortcuts", "Shortcuts", "community_mod.settings");
  for (const auto& group : ShortcutGroups)
    catalog.AddPage(std::string("community_mod.shortcuts.") + std::string(group.id), std::string(group.label),
                    "community_mod.shortcuts");
  const auto ordered = DiscoverShortcuts([](Action action) {
    const auto* info = Info(action);
    return info ? info->key : std::string_view{};
  });
  for (const auto& description : ordered) {
    auto       editor = std::make_unique<ShortcutEditor>(*Info(description.action), description.label);
    const auto group  = std::ranges::find(ShortcutGroups, description.group, &ShortcutGroupInfo::group);
    catalog.AddPage(editor->state.id(), editor->state.label(),
                    std::string("community_mod.shortcuts.") + std::string(group->id));
    catalog.SetMetadata(editor->state.id(), editor->state.metadata());
    AddRows(catalog, *editor);
    editors.push_back(std::move(editor));
  }
}
ValueSetting<ShortcutList>* ShortcutEditorSetting(Action action)
{
  const auto found =
      std::ranges::find_if(editors, [action](const auto& editor) { return editor->info.action == action; });
  return found == editors.end() ? nullptr : &(*found)->state;
}
void CancelShortcutEditing()
{
  if (editing)
    Cancel(*editing, true);
}
void SetShortcutPresentationObserver(void (*observer)())
{ presentation_changed = observer; }
} // namespace mod_settings
