#pragma once

#include "VoStudio/Engine/AudioBuffer.hpp"

#include <vector>

namespace VoStudio::Engine {

struct PitchFrame final {
    double time_seconds{};
    double frequency_hz{};
    double confidence{};
    bool voiced{};
};

struct PitchDetectionOptions final {
    double minimum_frequency_hz{55.0};
    double maximum_frequency_hz{1500.0};
    double frame_length_ms{40.0};
    double hop_length_ms{10.0};
    double yin_threshold{0.15};
};

// YIN fundamental-frequency estimator. Analysis is performed on a mono downmix;
// the resulting time-aligned contour can drive multichannel processors.
class PitchDetector final {
public:
    [[nodiscard]] static std::vector<PitchFrame> analyze(
        const AudioBuffer& audio,
        const PitchDetectionOptions& options = {});

    [[nodiscard]] static double frequency_at(
        const std::vector<PitchFrame>& contour,
        double time_seconds) noexcept;
};

} // namespace VoStudio::Engine
