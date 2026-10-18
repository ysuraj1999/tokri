#ifndef SHAKEDETECTOR_H
#define SHAKEDETECTOR_H

#include <chrono>
#include <cstddef>

class ShakeDetector {
public:
    explicit ShakeDetector(int travelThreshold = 20,
                           size_t completionsRequired = 4,
                           std::chrono::milliseconds budget = std::chrono::milliseconds(500));

    bool feed(int dx);
    void reset();

private:
    const int travelThreshold;
    const size_t completionsRequired;
    const std::chrono::milliseconds budget;

    int direction = 0;
    int travel = 0;
    size_t completions = 0;
    bool windowStarted = false;
    bool windowCompleted = false;
    std::chrono::steady_clock::time_point windowStart;
};

#endif // SHAKEDETECTOR_H