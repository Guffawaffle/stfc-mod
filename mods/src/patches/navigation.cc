/**
 * @file navigation.cc
 * @brief Section navigation helpers for hotkey-driven screen transitions.
 *
 * Wraps Hub::get_SectionManager() and NavigationSectionManager to provide
 * simple section-change functions used by the dispatch table handlers.
 */
#include "errormsg.h"
#include "config.h"

#include "patches/navigation.h"

#include "prime/Hub.h"
#include "prime/NavigationSectionManager.h"
#include "prime/ScreenManager.h"

void GotoSection(SectionID sectionID, void* section_data)
{
  Hub::get_SectionManager()->TriggerSectionChange(sectionID, section_data, false, false, true);
}

void ChangeNavigationSection(SectionID sectionID)
{
  const auto section_data = Hub::get_SectionManager()->_sectionStorage->GetState(sectionID);

  if (section_data) {
    GotoSection(sectionID, section_data);
  } else {
    NavigationSectionManager::ChangeNavigationSection(sectionID);
  }
}

bool MoveOfficerCanvas(bool goLeft)
{
  // ScreenManager/CanvasRoot/MainFrame/ShipManagement_Canvas/Content/Pagination/
  // ScreenManager/CanvasRoot/MainFrame/OfficerShowcase_Canvas/
  // ScreenManager/CanvasRoot/MainFrame/LeftArrow and RightArrow

  auto const canvas = ScreenManager::GetTopCanvas(true);
  if (strcmp(((Il2CppObject*)(canvas))->klass->name, "OfficerShowcase_Canvas") == 0) {}

  return false;
}
