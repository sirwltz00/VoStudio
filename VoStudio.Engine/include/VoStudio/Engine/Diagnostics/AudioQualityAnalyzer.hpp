#pragma once

#include "VoStudio/Engine/AudioBuffer.hpp"

#include <cstddef>

namespace VoStudio::Engine {

struct AudioQualityMetrics final {
    double peak{};
    double rms{};
    double dc_offset{};
    double crest_factor{};
    double clipped_sample_fraction{};
    double largest_sample_discontinuity{};
    std::size_t discontinuity_count{};
};

class AudioQualityAnalyzer final {
public:
    [[nodiscard]] static AudioQualityMetrics analyze(
        const AudioBuffer& audio,
        float clipping_threshold = 0.999F,
        float discontinuity_threshold = 0.25F) noexcept;
};

} // namespace VoStudio::Engine
