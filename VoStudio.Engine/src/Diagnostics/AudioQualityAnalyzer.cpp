#include "VoStudio/Engine/Diagnostics/AudioQualityAnalyzer.hpp"

#include <algorithm>
#include <cmath>

namespace VoStudio::Engine {

AudioQualityMetrics AudioQualityAnalyzer::analyze(
    const AudioBuffer& audio, const float clipping_threshold, const float discontinuity_threshold) noexcept {
    AudioQualityMetrics metrics{};
    const auto values = audio.samples();
    if (values.empty()) return metrics;

    long double sum = 0.0L;
    long double square_sum = 0.0L;
    std::size_t clipped = 0;
    float previous = values.front();
    for (const float value : values) {
        const float magnitude = std::abs(value);
        metrics.peak = std::max(metrics.peak, static_cast<double>(magnitude));
        if (magnitude >= clipping_threshold) ++clipped;
        sum += static_cast<long double>(value);
        square_sum += static_cast<long double>(value) * static_cast<long double>(value);
        const float jump = std::abs(value - previous);
        metrics.largest_sample_discontinuity = std::max(metrics.largest_sample_discontinuity, static_cast<double>(jump));
        if (jump >= discontinuity_threshold) ++metrics.discontinuity_count;
        previous = value;
    }
    const auto count = static_cast<long double>(values.size());
    metrics.dc_offset = static_cast<double>(sum / count);
    metrics.rms = std::sqrt(static_cast<double>(square_sum / count));
    metrics.crest_factor = metrics.rms > 1.0e-12 ? metrics.peak / metrics.rms : 0.0;
    metrics.clipped_sample_fraction = static_cast<double>(clipped) / static_cast<double>(values.size());
    return metrics;
}

} // namespace VoStudio::Engine
