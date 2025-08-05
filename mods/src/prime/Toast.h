#pragma once

#include "ToastState.h"
#include <il2cpp/il2cpp_helper.h>

struct Toast {
public:
  __declspec(property(get = get_Mode)) ToastMode   Mode;
  __declspec(property(get = get_State)) ToastState State;
  __declspec(property(get = get_ModeName)) std::string_view ModeName;
  __declspec(property(get = get_Name)) std::string_view Name;
  __declspec(property(get = get_Description)) std::string_view Description;

private:
  static IL2CppClassHelper &get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.HUD", "Toast");
    return class_helper;
  }

public:
  ToastState get_State()
  {
    static auto prop = get_class_helper().GetProperty("State");
    return *prop.Get<ToastState>((void *)this);
  }

  std::string_view get_ModeName()
  {
    switch (this->Mode) {
      case ToastMode::Normal:
        return "Normal";
      case ToastMode::Warning:
        return "Warning";
      case ToastMode::Alert:
        return "Alert";
    }

    return "Unknown";
  }

  // Returns the toast mode based on its state
  ToastMode get_Mode()
  {
    switch (this->State) {
      case ToastState::AbandonedTerritory:
      case ToastState::Achievement:
      case ToastState::ArmadaBattleWon:
      case ToastState::ArmadaIncomingAttack:
      case ToastState::AssaultVictory:
      case ToastState::ChallengeComplete:
      case ToastState::FactionDiscovered:
      case ToastState::FactionLevelDown:
      case ToastState::FactionLevelUp:
      case ToastState::FleetBattle:
      case ToastState::IncomingAttackFaction:
      case ToastState::JoinedTakeover:
      case ToastState::Standard:
      case ToastState::StrikeDefeat:
      case ToastState::TakeoverVictory:
      case ToastState::Tournament:
      case ToastState::TreasuryFull:
      case ToastState::TreasuryProgress:
      case ToastState::Victory:
        return ToastMode::Normal;

      case ToastState::ArmadaCreated:
      case ToastState::ArmadaCanceled:
      case ToastState::CompetitorJoinedTakeover:
      case ToastState::DiplomacyUpdated:
      case ToastState::Defeat:
      case ToastState::FactionWarning:
      case ToastState::StationVictory:
        return ToastMode::Warning;

      case ToastState::ArmadaBattleLost:
      case ToastState::AssaultDefeat:
      case ToastState::ChallengeFailed:
      case ToastState::IncomingAttack:
      case ToastState::StationBattle:
      case ToastState::StationDefeat:
      case ToastState::StrikeHit:
      case ToastState::TakeoverDefeat:
        return ToastMode::Alert;
    };

    return ToastMode::Normal;
  }

  // Returns a description of the toast based on its state
  std::string_view get_Description()
  {
    switch (this->State) {
      case ToastState::Standard:
        return "Desc Standard";
      case ToastState::FactionWarning:
        return "You have entered hostile space and will be attacked!";
      case ToastState::FactionLevelUp:
        return "Desc FactionLevelUp";
      case ToastState::FactionLevelDown:
        return "Desc FactionLevelDown";
      case ToastState::FactionDiscovered:
        return "Desc FactionDiscovered";
      case ToastState::IncomingAttack:
        return "You have an attack incoming";
      case ToastState::IncomingAttackFaction:
        return "One or more of your ships is being targetted by hostile enemies";
      case ToastState::FleetBattle:
        return "Desc FleetBattle";
      case ToastState::StationBattle:
        return "Your station is under attack!";
      case ToastState::StationVictory:
        return "Your station survived the attack!";
      case ToastState::Victory:
        return "SUCCESS!";
      case ToastState::Defeat:
        return "DEFEATED!!";
      case ToastState::StationDefeat:
        return "Your station was defeated!";
      case ToastState::Tournament:
        return "Desc Tournament";
      case ToastState::ArmadaCreated:
        return "Your alliance has formed an armada";
      case ToastState::ArmadaCanceled:
        return "An armada was cancelled by its host";
      case ToastState::ArmadaIncomingAttack:
        return "Your armada is underway";
      case ToastState::ArmadaBattleWon:
        return "Your armada won its battle";
      case ToastState::ArmadaBattleLost:
        return "Your armada lost its battle";
      case ToastState::DiplomacyUpdated:
        return "Alliance dieplomacy levels were changed, go check them";
      case ToastState::JoinedTakeover:
        return "You have joined a takeover";
      case ToastState::CompetitorJoinedTakeover:
        return "A rival has joined your takeover";
      case ToastState::AbandonedTerritory:
        return "Desc AbandonedTerritory";
      case ToastState::TakeoverVictory:
        return "Your takeover was successfully won!";
      case ToastState::TakeoverDefeat:
        return "Your takeover was not good!";
      case ToastState::TreasuryProgress:
        return "Desc TreasuryProgress";
      case ToastState::TreasuryFull:
        return "Desc TreasuryFull";
      case ToastState::Achievement:
        return "Desc Achievement";
      case ToastState::AssaultVictory:
        return "Desc AssaultVictory";
      case ToastState::AssaultDefeat:
        return "You were defaulted";
      case ToastState::ChallengeComplete:
        return "The challange was completed";
      case ToastState::ChallengeFailed:
        return "The changed was failed!";
      case ToastState::StrikeHit:
        return "Desc StrikeHit";
      case ToastState::StrikeDefeat:
        return "Desc StrikeDefeat";
    };
  }

  // Returns the name of the toast based on its state
  std::string_view get_Name()
  {
    switch (this->State) {
      case ToastState::Standard:
        return "Standard";
      case ToastState::FactionWarning:
        return "FactionWarning";
      case ToastState::FactionLevelUp:
        return "FactionLevelUp";
      case ToastState::FactionLevelDown:
        return "FactionLevelDown";
      case ToastState::FactionDiscovered:
        return "FactionDiscovered";
      case ToastState::IncomingAttack:
        return "IncomingAttack";
      case ToastState::IncomingAttackFaction:
        return "IncomingAttackFaction";
      case ToastState::FleetBattle:
        return "FleetBattle";
      case ToastState::StationBattle:
        return "StationBattle";
      case ToastState::StationVictory:
        return "StationVictory";
      case ToastState::Victory:
        return "Victory";
      case ToastState::Defeat:
        return "Defeat";
      case ToastState::StationDefeat:
        return "StationDefeat";
      case ToastState::Tournament:
        return "Tournament";
      case ToastState::ArmadaCreated:
        return "ArmadaCreated";
      case ToastState::ArmadaCanceled:
        return "ArmadaCanceled";
      case ToastState::ArmadaIncomingAttack:
        return "ArmadaIncomingAttack";
      case ToastState::ArmadaBattleWon:
        return "ArmadaBattleWon";
      case ToastState::ArmadaBattleLost:
        return "ArmadaBattleLost";
      case ToastState::DiplomacyUpdated:
        return "DiplomacyUpdated";
      case ToastState::JoinedTakeover:
        return "JoinedTakeover";
      case ToastState::CompetitorJoinedTakeover:
        return "CompetitorJoinedTakeover";
      case ToastState::AbandonedTerritory:
        return "AbandonedTerritory";
      case ToastState::TakeoverVictory:
        return "TakeoverVictory";
      case ToastState::TakeoverDefeat:
        return "TakeoverDefeat";
      case ToastState::TreasuryProgress:
        return "TreasuryProgress";
      case ToastState::TreasuryFull:
        return "TreasuryFull";
      case ToastState::Achievement:
        return "Achievement";
    };

    return "Unknown";
  }
};
