#pragma once

struct Il2CppObject;

// One owner for the provider shared by Haven orbit and station-interior zoom.
bool InstallOrbitConstraintHook();
void ApplyHavenOrbitConstraints(Il2CppObject* provider);
