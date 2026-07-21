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
#include "prime/ElementSelectorViewController.h"

void GotoSection(SectionID sectionID, void* section_data)
{
  auto* section_manager = Hub::get_SectionManager();
  if (!section_manager) {
    NavigationSectionManager::ChangeNavigationSection(sectionID);
    return;
  }

  section_manager->TriggerSectionChange(sectionID, section_data, false, false, true);
}

void ChangeNavigationSection(SectionID sectionID)
{
  void* section_data = nullptr;
  if (auto* section_manager = Hub::get_SectionManager();
      section_manager && section_manager->_sectionStorage) {
    section_data = section_manager->_sectionStorage->GetState(sectionID);
  }

  if (section_data) {
    GotoSection(sectionID, section_data);
  } else {
    NavigationSectionManager::ChangeNavigationSection(sectionID);
  }
}

bool MoveOfficerCanvas(bool goLeft)
{
  bool acted = false;
  for (auto* selector : ObjectFinder<ElementSelectorViewController>::GetAllNonNull()) {
    if (!selector->isActiveAndEnabled()) {
      continue;
    }

    goLeft ? selector->PressDecrement() : selector->PressIncrement();
    acted = true;
  }
  return acted;
}
