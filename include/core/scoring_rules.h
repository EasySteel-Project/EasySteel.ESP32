#pragma once

#include "config.h"

/// Return whether a sensor belongs to the active target set for the selected mode.
bool scoringRulesShouldRecordTarget(int targetIndex, int targetCount, int roundMode);

/// Count missing configured intermediate targets. Index 0 is always the stop plate.
/// Reaction drill (roundMode == 2) requires only one hit and has no missed intermediate targets.
int scoringRulesCalculateMissedTargets(int targetCount, const bool hitMask[MAX_TARGETS], int roundMode = 0);

