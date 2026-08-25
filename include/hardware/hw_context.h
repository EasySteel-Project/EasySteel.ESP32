#pragma once

#include "button.h"

/// Hardware button references passed to subsystems during initialization.
/// Eliminates global extern coupling between modules.
struct HwButtons {
  Button& start;
  Button& stop;
  Button& reset;
};
