#include "VoStudio/Engine/HybridSynthesisEngine.hpp"

#include "VoStudio/Engine/DSP/PSOLAPitchShifter.hpp"
#include "VoStudio/Engine/DSP/FormantProcessor.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <limits>
#include <stdexcept>

namespace VoStudio::Engine {
namespace {
constexpr int kSincRadius = 16;

[[nodiscard]] double sinc(const double value) noexcept {
    if (std::abs(value) < 1.0e-12) return 1.0;
    const double x = std::numbers::pi * value;
    return std::sin(x) / x;
}

[[nodiscard]] double blackman_window(const double normalized_distance) noexcept {
    const double x = std::abs(normalized_distance);
    if (x >= 1.0) return 0.0;
    return 0.42 + 0.5 * std::cos(std::numbers::pi * x)
                + 0.08 * std::cos(2.0 * std::numbers::pi * x);
}

[[nodiscard]] float sample_sinc(const AudioBuffer& source, const double position, const int channel,
                                const double step) {
    const auto center = static_cast<long long>(std::floor(position));
    const double cutoff = std::min(1.0, 1.0 / std::max(step, 1.0e-12));
    double weighted_sum = 0.0;
    double weight_sum = 0.0;
    for (long long frame = center - kSincRadius + 1; frame <= center + kSincRadius; ++frame) {
        if (frame < 0 || frame >= static_cast<long long>(source.frame_count())) continue;
        const double distance = position - static_cast<double>(frame);
        const double weight = cutoff * sinc(cutoff * distance)
                            * blackman_window(distance / static_cast<double>(kSincRadius));
        weighted_sum += static_cast<double>(source.sample(static_cast<std::size_t>(frame), channel)) * weight;
        weight_sum += weight;
    }
    return std::abs(weight_sum) < 1.0e-12 ? 0.0F : static_cast<float>(weighted_sum / weight_sum);
}
}

AudioBuffer HybridSynthesisEngine::render(const AudioBuffer& source, const RenderParameters& parameters) {
    if (source.frame_count() == 0) throw std::invalid_argument("Cannot render an empty source sample");
    if (!std::isfinite(parameters.source_start_seconds) || parameters.source_start_seconds < 0.0)
        throw std::invalid_argument("Source start time must be finite and non-negative");
    if (!std::isfinite(parameters.output_duration_seconds) || parameters.output_duration_seconds < 0.0)
        throw std::invalid_argument("Output duration must be finite and non-negative");
    if (!std::isfinite(parameters.gain)) throw std::invalid_argument("Render gain must be finite");

    const double source_start_value = parameters.source_start_seconds * static_cast<double>(source.sample_rate());
    if (source_start_value >= static_cast<double>(source.frame_count()))
        throw std::invalid_argument("Source start is beyond the audio sample");
    if (source_start_value > static_cast<double>(std::numeric_limits<long long>::max()))
        throw std::length_error("Source start time is too large");
    const auto source_start = static_cast<std::size_t>(std::llround(source_start_value));
    const std::size_t remaining = source.frame_count() - source_start;
    AudioBuffer region(source.sample_rate(), source.channel_count(), remaining);
    for (std::size_t frame = 0; frame < remaining; ++frame) {
        for (int channel = 0; channel < source.channel_count(); ++channel)
            region.set_sample(frame, channel, source.sample(source_start + frame, channel));
    }

    std::size_t target_frames = remaining;
    if (parameters.output_duration_seconds > 0.0) {
        const double count = parameters.output_duration_seconds * static_cast<double>(source.sample_rate());
        if (count > static_cast<double>(std::numeric_limits<long long>::max())
            || count > static_cast<double>(std::numeric_limits<std::size_t>::max()))
            throw std::length_error("Requested render duration is too large");
        target_frames = std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(count)));
    }

    AudioBuffer output = TimeStretcher::process(region, target_frames, parameters.time_stretch);
    const AudioBuffer pitch_reference = output;
    const double semitone_shift = static_cast<double>(parameters.target_midi_note)
                               - static_cast<double>(parameters.source_root_midi_note);
    PSOLAPitchShifter::process(output, semitone_shift, parameters.pitch_curve);
    if (std::abs(semitone_shift) > 1.0e-9
        || (parameters.pitch_curve != nullptr && !parameters.pitch_curve->points().empty())) {
        output = FormantProcessor::preserve(
    pitch_reference,
    output,
    parameters.formant_preservation);
    }

    // Keep floating-point headroom intact. The mix/output stage can inspect
    // AudioQualityMetrics and choose a limiter or export gain deliberately.
    for (float& sample : output.samples())
        sample *= parameters.gain;
    return output;
}

void HybridSynthesisEngine::render_sample(
    const AudioBuffer& source, AudioBuffer& destination, const RenderParameters& parameters) {
    if (source.frame_count() == 0) throw std::invalid_argument("Cannot render an empty source sample");
    if (!std::isfinite(parameters.source_start_seconds) || parameters.source_start_seconds < 0.0)
        throw std::invalid_argument("Source start time must be finite and non-negative");
    if (!std::isfinite(parameters.gain)) throw std::invalid_argument("Render gain must be finite");
    if (parameters.source_start_seconds * static_cast<double>(source.sample_rate())
        >= static_cast<double>(source.frame_count()))
        throw std::invalid_argument("Source start is beyond the audio sample");

    // Preserve the original scaffold API for callers with a preallocated output.
    // This compatibility path performs high-quality pitch resampling; new code
    // should use render() to get duration-independent WSOLA/PSOLA processing.
    destination.clear();
    double source_position = parameters.source_start_seconds * static_cast<double>(source.sample_rate());
    const double sample_rate_ratio = static_cast<double>(source.sample_rate())
                                   / static_cast<double>(destination.sample_rate());
    for (std::size_t frame = 0; frame < destination.frame_count(); ++frame) {
        const double time = static_cast<double>(frame) / static_cast<double>(destination.sample_rate());
        const double cents = parameters.pitch_curve ? parameters.pitch_curve->cents_at(time) : 0.0;
        const double semitones = static_cast<double>(parameters.target_midi_note - parameters.source_root_midi_note)
                               + cents / 100.0;
        const double step = sample_rate_ratio * std::exp2(semitones / 12.0);
        for (int channel = 0; channel < destination.channel_count(); ++channel) {
            float value = 0.0F;
            if (destination.channel_count() == 1 && source.channel_count() > 1) {
                double sum = 0.0;
                for (int source_channel = 0; source_channel < source.channel_count(); ++source_channel)
                    sum += sample_sinc(source, source_position, source_channel, step);
                value = static_cast<float>(sum / static_cast<double>(source.channel_count()));
            } else {
                const int source_channel = source.channel_count() == 1
                    ? 0 : std::min(channel, source.channel_count() - 1);
                value = sample_sinc(source, source_position, source_channel, step);
            }
            destination.set_sample(frame, channel, value * parameters.gain);
        }
        source_position += step;
    }
}

} // namespace VoStudio::Engine
