#include "patches/focus_search.h"

#include "prime/AssignShipsWidget.h"
#include "prime/CanvasController.h"
#include "prime/InventoryListViewController.h"
#include "prime/OfficerAssignmentViewController.h"

#include <il2cpp/il2cpp_helper.h>

namespace
{
template <typename T> bool FocusFirstVisibleSearchField()
{
  for (auto* controller : ObjectFinder<T>::GetAllNonNull()) {
    auto* input_field = controller->_inputField;
    auto* canvas      = controller->canvasController;
    if (input_field && canvas && canvas->Visible() && controller->isActiveAndEnabled
        && input_field->isActiveAndEnabled) {
      input_field->Focus();
      return true;
    }
  }
  return false;
}
} // namespace

bool FocusSearchBox()
{
  if (FocusFirstVisibleSearchField<InventoryListViewController>()
      || FocusFirstVisibleSearchField<OfficerAssignmentViewController>()) {
    return true;
  }

  for (auto* widget : ObjectFinder<AssignShipsWidget>::GetAllNonNull()) {
    auto* input_field = widget->_inputField;
    auto* canvas      = GetCanvasControllerFromComponent(widget);
    if (input_field && canvas && canvas->Visible() && widget->isActiveAndEnabled && input_field->isActiveAndEnabled) {
      input_field->Focus();
      return true;
    }
  }

  return false;
}
