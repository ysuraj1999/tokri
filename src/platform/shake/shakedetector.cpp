#include <cstdlib>

#include "platform/shake/shakedetector.h"

ShakeDetector::ShakeDetector(int travelThreshold, size_t completionsRequired,
                             std::chrono::milliseconds budget)
    : travelThreshold(travelThreshold),
      completionsRequired(completionsRequired),
      budget(budget) {}

bool ShakeDetector::feed(int dx) {
    const auto now = std::chrono::steady_clock::now();

    if (windowStarted && now - windowStart > budget) {
        reset();
        return false;
    }

    const int sign = dx > 0 ? 1 : -1;
    if (dx == 0)
        return false;

    if (direction == 0) {
        direction = sign;
        travel = std::abs(dx);
        windowStarted = true;
        windowStart = now;
    } else if (sign == direction) {
        if (!windowCompleted) {
            travel += std::abs(dx);
            if (travel >= travelThreshold) {
                windowCompleted = true;
                travel = 0;
                ++completions;
            }
        }
    } else {
        direction = sign;
        travel = std::abs(dx);
        windowCompleted = false;
    }

    if (completions >= completionsRequired) {
        reset();
        return true;
    }

    return false;
}

void ShakeDetector::reset() {
    direction = 0;
    travel = 0;
    completions = 0;
    windowStarted = false;
    windowCompleted = false;
}