#pragma once

#include <string_view>

#include "prime/Hub.h"

struct SectionChangeContext {
  SectionManager* manager = nullptr;
  SectionID       next_section = static_cast<SectionID>(0);
  void*           args = nullptr;
  bool            forced_section_change = false;
  bool            is_go_back_step = false;
  bool            allow_same_section = false;
  bool            changed = false;
};

struct SectionChangeObserver {
  std::string_view name;
  void (*before_original)(const SectionChangeContext& context) = nullptr;
  void (*after_original)(const SectionChangeContext& context) = nullptr;
};

void RegisterSectionChangeObserver(SectionChangeObserver observer);
bool SectionChangeRouterHasSubscribers();
void InstallSectionChangeRouterHooks();
