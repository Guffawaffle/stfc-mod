#pragma once

#include "errormsg.h"
#include <il2cpp/il2cpp_helper.h>

#include "MonoSingleton.h"
#include "CoordinateSearchContext.h"
#include "Hub.h"

struct BookmarksManager : MonoSingleton<BookmarksManager> {
  friend struct MonoSingleton<BookmarksManager>;

public:
  void ViewBookmarks()
  {
    static auto ViewBookmarksMethod = get_class_helper().GetMethod<void(BookmarksManager*)>("ViewBookmarks");
    static auto ViewBookmarksWarn   = true;

    if (ViewBookmarksMethod) {
      ViewBookmarksMethod(this);
    } else if (ViewBookmarksWarn) {
      ViewBookmarksWarn = false;
      ErrorMsg::MissingMethod("BookmarksManager", "ViewBookmarks");
    }
  }

  bool ViewCoordinateSearch()
  {
    auto* context = CoordinateSearchContext::Create();
    if (!context || !context->InitializeDefaults()) {
      spdlog::error("[CoordSearch] Coordinate search context is unavailable; opening bookmarks instead");
      ViewBookmarks();
      return false;
    }

    auto* section_manager = Hub::get_SectionManager();
    if (!section_manager) {
      spdlog::error("[CoordSearch] SectionManager is unavailable; opening bookmarks instead");
      ViewBookmarks();
      return false;
    }

    section_manager->TriggerSectionChange(SectionID::Bookmarks_Search_Coordinates, context, false, false, true);
    return true;
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Bookmarks", "BookmarksManager");
    return class_helper;
  }
};
