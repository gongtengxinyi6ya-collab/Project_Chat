#pragma once

#include <chrono>
#include <functional>

using TimePoint = std::chrono::steady_clock::time_point;
using Duration = std::chrono::milliseconds;
using TimerCallback = std::function<void()>;