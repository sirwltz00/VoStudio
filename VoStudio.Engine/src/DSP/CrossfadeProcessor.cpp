#include "VoStudio/Engine/DSP/CrossfadeProcessor.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace VoStudio::Engine {

AudioBuffer CrossfadeProcessor::join(
    const AudioBuffer& first, const AudioBuffer& second,
    std::size_t overlap_frames, const CrossfadeCurve curve) {
    if (first.sample_rate() != second.sample_rate() || first.channel_count() != second.channel_count())
        throw std::invalid_argument("Crossfade inputs must have matching sample rates and channel counts");
    overlap_frames = std::min({overlap_frames, first.frame_count(), second.frame_count()});
    AudioBuffer output(first.sample_rate(), first.channel_count(),
        first.frame_count() + second.frame_count() - overlap_frames);

    for (std::size_t i = 0; i < first.frame_count() - overlap_frames; ++i)
        for (int channel = 0; channel < first.channel_count(); ++channel)
            output.set_sample(i, channel, first.sample(i, channel));

    const std::size_t overlap_start = first.frame_count() - overlap_frames;
    for (std::size_t i = 0; i < overlap_frames; ++i) {
        const double phase = overlap_frames <= 1 ? 1.0
            : static_cast<double>(i) / static_cast<double>(overlap_frames - 1);
        const double fade_in = curve == CrossfadeCurve::EqualPower
            ? std::sin(phase * std::numbers::pi / 2.0) : phase;
        const double fade_out = curve == CrossfadeCurve::EqualPower
            ? std::cos(phase * std::numbers::pi / 2.0) : 1.0 - phase;
        for (int channel = 0; channel < first.channel_count(); ++channel) {
            const double mixed = static_cast<double>(first.sample(overlap_start + i, channel)) * fade_out
                               + static_cast<double>(second.sample(i, channel)) * fade_in;
            output.set_sample(overlap_start + i, channel, static_cast<float>(mixed));
        }
    }

    for (std::size_t i = overlap_frames; i < second.frame_count(); ++i) {
        const std::size_t destination = first.frame_count() + i - overlap_frames;
        for (int channel = 0; channel < first.channel_count(); ++channel)
            output.set_sample(destination, channel, second.sample(i, channel));
    }
    return output;
}

} // namespace VoStudio::Engine
