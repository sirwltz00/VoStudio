#pragma once

#include <vector>

namespace VoStudio::Engine {

struct PitchPoint final {
    double time_seconds{};
    double cents{};
};

// Time is measured from the beginning of a rendered note; pitch is a cents offset.
class PitchCurve final {
public:
    explicit PitchCurve(std::vector<PitchPoint> points = {});
    [[nodiscard]] double cents_at(double time_seconds) const noexcept;
    [[nodiscard]] const std::vector<PitchPoint>& points() const noexcept { return points_; }

private:
    std::vector<PitchPoint> points_;
};

} // namespace VoStudio::Engine
