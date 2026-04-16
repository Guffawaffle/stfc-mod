/**
 * @file NavigationManager.h
 * @brief Navigation interaction manager.
 *
 * Mirrors Digit.Prime.Navigation.NavigationManager — controls the
 * visibility of the in-space interaction UI (tap-on-object panels).
 */
#pragma once

/**
 * @brief Manages the navigation interaction UI overlay.
 *
 * Provides methods to hide the currently displayed interaction panel
 * (e.g. the tap-on-fleet or tap-on-station popup).
 */
struct NavigationManager {
public:
  void HideInteraction()
  {
    static auto HideInteraction = get_class_helper().GetMethod<void(NavigationManager*)>("HideInteraction");
    HideInteraction(this);
  }

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper =
        il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationManager");
    return class_helper;
  }
};