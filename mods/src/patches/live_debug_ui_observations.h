#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct TopCanvasObservation {
  bool                     found = false;
  std::string              pointer;
  std::string              className;
  std::string              classNamespace;
  std::string              name;
  bool                     visible = false;
  bool                     enabled = false;
  bool                     internalVisible = false;
  std::vector<std::string> activeChildNames;
};

struct StationWarningObservation {
  bool        tracked = false;
  std::string pointer;
  bool        hasContext = false;
  int         targetType = 0;
  uint64_t    targetFleetId = 0;
  std::string targetUserId;
  uint64_t    quickScanTargetFleetId = 0;
  std::string quickScanTargetId;
};

struct NavigationInteractionObservation {
  struct Entry {
    std::string pointer;
    bool        hasContext = false;
    int         contextDataState = -1;
    int         inputInteractionType = -1;
    std::string userId;
    bool        isMarauder = false;
    int         threatLevel = -2;
    bool        validNavigationInput = false;
    bool        showSetCourseArm = false;
    int64_t     locationTranslationId = 0;
    std::string poiPointer;
  };

  bool               tracked = false;
  size_t             trackedCount = 0;
  std::vector<Entry> entries;
};