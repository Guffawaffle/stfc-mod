#pragma once

#include "prime/FleetBarViewController.h"
#include "prime/HullSpec.h"

#include <cstdint>

struct BattleTargetData;

extern bool force_space_action_next_frame;

void     ClearDeferredSpaceAction();

uint64_t DeferredSpaceActionGeneration();
bool     HandleShipSelection(int ship_select_request);
void     ExecuteSpaceAction(FleetBarViewController* fleet_bar);
bool     DidExecuteRecall(FleetBarViewController* fleet_bar);
bool     DidExecuteRepair(FleetBarViewController* fleet_bar);
HullType GetHullTypeFromBattleTarget(BattleTargetData* context);
