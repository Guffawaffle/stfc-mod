#pragma once
#include "config.h"
#include <string_view>

// Canonical source-list adapters. Ship startup and editor parsing share logic;
// strict faction/banner editors use the existing catalogs while startup retains
// its tolerant warning/default behavior.
bool        ParseShipFilter(std::string_view text, ShipFilter& filter);
std::string FormatShipFilter(const ShipFilter& filter);
bool        ParseFactionFilter(std::string_view text, std::vector<std::string>& factions);
bool        ParseBannerFilter(std::string_view text, std::vector<int>& types);
std::string FormatBannerFilter(const std::vector<int>& types);
