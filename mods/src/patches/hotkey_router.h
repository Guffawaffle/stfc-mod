#pragma once

struct ScreenManager;
class RewardsButtonWidget;
struct PreScanTargetWidget;

// Layer 2 — routes hook entry points to concern handlers.
// Returns true when the hook should call original(), false to suppress.

void  hotkey_router_init();
bool  hotkey_router_screen_update(ScreenManager* _this);
bool  hotkey_router_init_actions();
void  hotkey_router_bind_context(RewardsButtonWidget* _this);
void  hotkey_router_show_fleet(PreScanTargetWidget* _this);
