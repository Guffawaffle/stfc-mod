#pragma once

// Checked access for the preset save boundary. Resolve metadata before a view
// is projected; the save scope uses only rooted objects and pre-resolved fields.
#include "ui.h"

struct OfficerPresetItemContext : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.OfficerPresets", "OfficerPresetItemContext">;
};
struct OfficerPresetsViewContext : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.OfficerPresets", "OfficerPresetsViewContext">;
};
struct OfficerPresetItemWidget : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.OfficerPresets", "OfficerPresetItemWidget">;
};
struct OfficerPresetsViewController : il2cpp::Object {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Prime.OfficerPresets", "OfficerPresetsViewController">;
};
struct PresetSmartScroller : BaseListContainer {
  using Class = il2cpp::Class<"Assembly-CSharp", "Digit.Client.UI", "SmartScrollerBase">;
};

namespace preset_game
{
// Field setters have no managed callback/virtual-dispatch boundary. Resolution,
// which can allocate/report, is deliberately separate. The runtime setter owns
// reference write barriers; independent readback establishes each assignment.
template <typename T> struct Field {
  FieldInfo* field = nullptr;
  bool       Resolve(Il2CppClass* type, std::string_view name)
  {
    field = il2cpp::detail::ResolveField(type, name, il2cpp::detail::SlotOf<T>(), false);
    return field;
  }
  T Read(const Il2CppObject* object) const noexcept
  { return il2cpp::detail::ReadField<T>(field, object); }
  bool Write(Il2CppObject* object, T value) const noexcept
  {
    if (!object || !field)
      return false;
    il2cpp::detail::WriteField(field, object, value);
    return Read(object) == value;
  }
};

struct Fields {
  Field<OfficerPresetsViewContext*> controller_context, item_owner;
  Field<PresetSmartScroller*>       scroller;
  Field<il2cpp::Object*>            presets, source;
  Field<OfficerPresetItemContext*>  widget_context, cached_context;
  Field<std::int32_t>               presentation, order;
  Field<std::int64_t>               slot;
  Field<Il2CppString*>              name;
  Field<il2cpp::ArrayObject*>       officers;
  Field<bool>                       regenerating;
  Field<il2cpp::Object*>            initializing;

  bool Resolve()
  {
    // Evaluate every field so an unavailable feature has complete diagnostics.
    bool       ok         = true;
    const auto controller = OfficerPresetsViewController::Class::Raw();
    const auto item       = OfficerPresetItemContext::Class::Raw();
    const auto widget     = OfficerPresetItemWidget::Class::Raw();
    const auto scroll     = PresetSmartScroller::Class::Raw();
    ok &= controller_context.Resolve(controller, "m_context");
    ok &= scroller.Resolve(controller, "_smartScroller");
    ok &= presets.Resolve(OfficerPresetsViewContext::Class::Raw(), "PresetsItemsContext");
    ok &= cached_context.Resolve(OfficerPresetsViewContext::Class::Raw(), "CurrentCachedItemContext");
    ok &= widget_context.Resolve(widget, "m_context");
    ok &= item_owner.Resolve(item, "_officerPresetsViewContext");
    ok &= presentation.Resolve(item, "Presentation");
    ok &= order.Resolve(item, "OrderId");
    ok &= slot.Resolve(item, "SlotId");
    ok &= name.Resolve(item, "PresetName");
    ok &= officers.Resolve(item, "Officers");
    ok &= source.Resolve(scroll, "_data");
    ok &= regenerating.Resolve(scroll, "_regeneratingContent");
    ok &= initializing.Resolve(scroll, "_dataHeldWhilstInitializing");
    return ok;
  }
};

// Canonical List<T> storage, not an IList convenience call returning default
// values on failure. Preparing the fields validates this particular runtime
// list. Every read rechecks its current reference-array shape and size.
struct ListAccess {
  Field<il2cpp::ArrayObject*> items;
  Field<std::int32_t>         size;
  bool                        Resolve(il2cpp::Object* list)
  { return list && items.Resolve(list->klass, "_items") && size.Resolve(list->klass, "_size"); }
  il2cpp::Array<OfficerPresetItemContext*>* Read(il2cpp::Object* list, std::size_t& count) const noexcept
  {
    count = 0;
    if (!list)
      return nullptr;
    const auto length = size.Read(list);
    const auto array  = il2cpp::TryReferenceArray<OfficerPresetItemContext*>(items.Read(list));
    if (length < 0 || !array.succeeded || !array.value || static_cast<std::size_t>(length) > array.value->size())
      return nullptr;
    count = static_cast<std::size_t>(length);
    return array.value;
  }
};
} // namespace preset_game
