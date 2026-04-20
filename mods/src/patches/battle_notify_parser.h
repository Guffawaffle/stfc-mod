/**
 * @file battle_notify_parser.h
 * @brief Extracts player/enemy names and ship hulls from battle toast data.
 *
 * Parses the BattleResultHeader attached to combat-related Toast objects to
 * produce a "Player (Ship) vs Enemy (Ship)" summary string used in OS
 * notifications. Uses SEH guards on Windows to survive bad IL2CPP pointers.
 */
#pragma once

#include <string>

struct Toast;

/**
 * @brief Build a human-readable battle summary from a toast's attached data.
 *
 * Only processes combat-related toast states (Victory, Defeat, etc.).
 * Returns an empty string for non-combat toasts or when parsing fails.
 *
 * @param toast The game Toast object to extract battle data from.
 * @return Formatted "Name (Ship) vs Name (Ship)" string, or empty.
 */
std::string battle_notify_parse(Toast* toast);
