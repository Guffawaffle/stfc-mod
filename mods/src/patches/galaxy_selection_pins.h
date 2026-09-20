#pragma once
#include <vector>

namespace galaxy_selection
{
// A successful null read means no owner. A missing binding or failed invocation
// must return false, because incomplete pins cannot authorize cache retirement.
template <typename Object, typename Reader>
bool CollectOwnerPins(Object manager, std::vector<Object>& pins, Reader& reader)
{
  auto pin = [&](Object poi) { if (poi) pins.push_back(poi); };
  auto context = [&](Object value) {
    if (!value) return true;
    Object poi{}, list{};
    if (!reader.Field(value, "Poi", poi) || !reader.Field(value, "PoiList", list)) return false;
    pin(poi);
    return reader.List(list, pins);
  };
  Object ui{}, loader{}, list{}, value{};
  if (!reader.Field(manager, "_navigationInteractionUIViewController", ui) || !ui
      || !reader.Field(ui, "_objectViewerLoadAndShow", loader) || !loader
      || !reader.Field(manager, "_selectionPoiList", list) || !reader.List(list, pins)
      || !reader.Property(ui, "get_CanvasContext", value) || !context(value)
      || !reader.Property(loader, "get_Context", value) || !context(value)
      || !reader.Field(loader, "_activePOI", value)) return false;
  pin(value);
  for (const auto* name : {"_activeViewer", "_viewerToResetOnClose"}) {
    Object viewer{}, parent{};
    if (!reader.Field(loader, name, viewer)) return false;
    if (!viewer) continue;
    if (!reader.Property(viewer, "get_CanvasContext", value) || !context(value)
        || !reader.Property(viewer, "get_Context", value) || !context(value)
        || !reader.Property(viewer, "get_Parent", parent)) return false;
    if (parent && (!reader.Property(parent, "get_CanvasContext", value) || !context(value)
                   || !reader.Field(parent, "_queryPoiList", list) || !reader.List(list, pins))) return false;
  }
  return true;
}
} // namespace galaxy_selection
