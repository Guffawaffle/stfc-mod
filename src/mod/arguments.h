#pragma once

#include <string>
#include <vector>

// Immutable owned copy of this process's UTF-8 launch arguments, acquired once. It contains launch requests,
// not authenticated account identity. No platform argv pointers escape the acquisition boundary.
const std::vector<std::string>& ProcessArguments();
