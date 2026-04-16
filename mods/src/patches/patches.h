/**
 * @file patches.h
 * @brief Entry point for the community patch hook system.
 *
 * Declares the top-level function that bootstraps all game modifications
 * by detouring il2cpp_init and installing individual patch modules.
 */
#pragma once

/// Load GameAssembly, detour il2cpp_init, and register all patch hooks.
void ApplyPatches();
