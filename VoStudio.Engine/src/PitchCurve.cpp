#include "VoStudio/Engine/PitchCurve.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace VoStudio::Engine {

PitchCurve::PitchCurve(std::vector<PitchPoint> points) : points_(std::move(points)) {
    for (const auto& point : points_) {
        if (!std::isfinite(point.time_seconds) || !std::isfinite(point.cents) || point.time_seconds < 0.0) {
            throw std::invalid_argument("Pitch points must contain finite values and non-negative times");
        }
    }
    std::sort(points_.begin(), points_.end(), [](const PitchPoint& a, const PitchPoint& b) {
        return a.time_seconds < b.time_seconds;
    });
    for (std::size_t i = 1; i < points_.size(); ++i) {
        if (points_[i - 1].time_seconds == points_[i].time_seconds) {
            throw std::invalid_argument("Pitch curve cannot contain duplicate timestamps");
        }
    }
}

double PitchCurve::cents_at(const double time_seconds) const noexcept {
    if (points_.empty()) return 0.0;
    if (time_seconds <= points_.front().time_seconds) return points_.front().cents;
    if (time_seconds >= points_.back().time_seconds) return points_.back().cents;

    const auto upper = std::upper_bound(points_.begin(), points_.end(), time_seconds,
        [](const double time, const PitchPoint& point) { return time < point.time_seconds; });
    const auto lower = upper - 1;
    const double span = upper->time_seconds - lower->time_seconds;
    const double amount = (time_seconds - lower->time_seconds) / span;
    return lower->cents + (upper->cents - lower->cents) * amount;
}

} // namespace VoStudio::Engine
