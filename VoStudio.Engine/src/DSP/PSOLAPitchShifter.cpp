#include "VoStudio/Engine/DSP/PSOLAPitchShifter.hpp"

#include "VoStudio/Engine/Analysis/PitchDetector.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace VoStudio::Engine {
namespace {
struct PitchMark final {
    std::size_t frame{};
    double period_samples{};
    double frequency_hz{};
};

[[nodiscard]] double mono_at(const AudioBuffer& audio, const std::size_t frame) noexcept {
    double value = 0.0;
    for (int channel = 0; channel < audio.channel_count(); ++channel)
        value += audio.samples()[frame * static_cast<std::size_t>(audio.channel_count())
                                 + static_cast<std::size_t>(channel)];
    return value / static_cast<double>(audio.channel_count());
}


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

[[nodiscard]] float interpolate_source(
    const AudioBuffer& audio, const double position, const int channel, const double source_step) {
    constexpr int radius = 12;
    const auto center = static_cast<long long>(std::floor(position));
    const double cutoff = std::min(1.0, 1.0 / std::max(1.0, source_step));
    double sum = 0.0;
    double weight_sum = 0.0;
    for (long long frame = center - radius + 1; frame <= center + radius; ++frame) {
        if (frame < 0 || frame >= static_cast<long long>(audio.frame_count())) continue;
        const double distance = position - static_cast<double>(frame);
        const double weight = cutoff * sinc(cutoff * distance)
                            * blackman_window(distance / static_cast<double>(radius));
        sum += static_cast<double>(audio.sample(static_cast<std::size_t>(frame), channel)) * weight;
        weight_sum += weight;
    }
    return std::abs(weight_sum) < 1.0e-12 ? 0.0F : static_cast<float>(sum / weight_sum);
}

[[nodiscard]] double local_pitch_ratio(
    const double semitones, const PitchCurve* curve, const double time_seconds) noexcept {
    const double cents = curve ? curve->cents_at(time_seconds) : 0.0;
    return std::exp2((semitones + cents / 100.0) / 12.0);
}

[[nodiscard]] std::vector<PitchMark> find_pitch_marks(
    const AudioBuffer& audio, const std::vector<PitchFrame>& contour) {
    std::vector<PitchMark> marks;
    if (audio.frame_count() < 3) return marks;
    double dc = 0.0;
    for (std::size_t frame = 0; frame < audio.frame_count(); ++frame) dc += mono_at(audio, frame);
    dc /= static_cast<double>(audio.frame_count());
    for (std::size_t frame = 1; frame < audio.frame_count(); ++frame) {
        const double previous = mono_at(audio, frame - 1) - dc;
        const double current = mono_at(audio, frame) - dc;
        if (!(previous <= 0.0 && current > 0.0)) continue;
        const double time = static_cast<double>(frame) / static_cast<double>(audio.sample_rate());
        const double frequency = PitchDetector::frequency_at(contour, time);
        if (!(frequency > 0.0)) continue;
        const double period = static_cast<double>(audio.sample_rate()) / frequency;
        if (!marks.empty()) {
            const double distance = static_cast<double>(frame - marks.back().frame);
            // Remove extra crossings caused by harmonic-rich waveforms while
            // retaining reasonable natural variation in the fundamental period.
            if (distance < period * 0.55) continue;
            if (distance > period * 2.4 && distance < period * 4.8) {
                // A missed positive crossing is tolerated; the upcoming mark is
                // still useful for the subsequent nearest-mark lookup.
            }
        }
        marks.push_back({frame, period, frequency});
    }
    return marks;
}
}

void PSOLAPitchShifter::process(
    AudioBuffer& audio, const double semitone_shift, const PitchCurve* pitch_curve,
    const PSOLAOptions& options) {
    if (!std::isfinite(semitone_shift)) throw std::invalid_argument("PSOLA pitch shift must be finite");
    if (audio.frame_count() == 0 || (std::abs(semitone_shift) < 1.0e-9
        && (pitch_curve == nullptr || pitch_curve->points().empty()))) return;

    PitchDetectionOptions detection;
    detection.minimum_frequency_hz = options.minimum_frequency_hz;
    detection.maximum_frequency_hz = options.maximum_frequency_hz;
    detection.frame_length_ms = options.frame_length_ms;
    detection.hop_length_ms = options.hop_length_ms;
    detection.yin_threshold = options.yin_threshold;
    const auto contour = PitchDetector::analyze(audio, detection);
    const auto marks = find_pitch_marks(audio, contour);
    if (marks.size() < 3) return; // Unvoiced or unreliable material is left untouched.

    const auto channels = static_cast<std::size_t>(audio.channel_count());
    std::vector<double> accumulation(audio.frame_count() * channels, 0.0);
    std::vector<double> weight_sum(audio.frame_count(), 0.0);

    // Split pitch marks into voiced islands. This prevents pitch shifting from
    // bridging silence or unvoiced consonants between vowel regions.
    std::size_t island_start = 0;
    while (island_start < marks.size()) {
        std::size_t island_end = island_start;
        while (island_end + 1 < marks.size()) {
            const double distance = static_cast<double>(marks[island_end + 1].frame - marks[island_end].frame);
            const double expected = 0.5 * (marks[island_end].period_samples + marks[island_end + 1].period_samples);
            if (distance > expected * 2.25) break;
            ++island_end;
        }

        if (island_end > island_start) {
            const double first_time = static_cast<double>(marks[island_start].frame)
                                    / static_cast<double>(audio.sample_rate());
            const double last_time = static_cast<double>(marks[island_end].frame)
                                   / static_cast<double>(audio.sample_rate());
            double synthesis_time = first_time;
            std::size_t source_mark_index = island_start;
            const std::size_t max_iterations = audio.frame_count() * 2;
            std::size_t iterations = 0;

            while (synthesis_time <= last_time && iterations++ < max_iterations) {
                const double source_frequency = PitchDetector::frequency_at(contour, synthesis_time);
                const double ratio = local_pitch_ratio(semitone_shift, pitch_curve, synthesis_time);
                if (!(source_frequency > 0.0) || !(ratio > 0.0) || !std::isfinite(ratio)) {
                    synthesis_time += options.hop_length_ms / 1000.0;
                    continue;
                }
                const double target_frequency = source_frequency * ratio;
                if (target_frequency < 20.0 || target_frequency > 3000.0) break;

                const auto upper = std::lower_bound(marks.begin() + static_cast<std::ptrdiff_t>(island_start),
                    marks.begin() + static_cast<std::ptrdiff_t>(island_end + 1), synthesis_time,
                    [&](const PitchMark& mark, const double time) {
                        return static_cast<double>(mark.frame) / static_cast<double>(audio.sample_rate()) < time;
                    });
                if (upper == marks.begin() + static_cast<std::ptrdiff_t>(island_start)) {
                    source_mark_index = island_start;
                } else if (upper == marks.begin() + static_cast<std::ptrdiff_t>(island_end + 1)) {
                    source_mark_index = island_end;
                } else {
                    const auto upper_index = static_cast<std::size_t>(std::distance(marks.begin(), upper));
                    const auto lower_index = upper_index - 1;
                    const double upper_time = static_cast<double>(marks[upper_index].frame) / audio.sample_rate();
                    const double lower_time = static_cast<double>(marks[lower_index].frame) / audio.sample_rate();
                    source_mark_index = std::abs(upper_time - synthesis_time) < std::abs(synthesis_time - lower_time)
                        ? upper_index : lower_index;
                }

                const auto& source_mark = marks[source_mark_index];
                const auto target_center = static_cast<long long>(std::llround(
                    synthesis_time * static_cast<double>(audio.sample_rate())));
                const double target_period = static_cast<double>(audio.sample_rate()) / target_frequency;
                const auto radius = static_cast<long long>(std::clamp(
                    std::llround(target_period), 8LL, 4096LL));
                for (long long offset = -radius; offset <= radius; ++offset) {
                    const auto destination_frame = target_center + offset;
                    // Warp one source-period neighbourhood into the requested
                    // target-period neighbourhood. The grain remains anchored
                    // to a detected source pitch mark, avoiding discontinuous
                    // free-running resampling between successive cycles.
                    const double source_position = static_cast<double>(source_mark.frame)
                                                 + static_cast<double>(offset) * ratio;
                    if (destination_frame < 0 || source_position < 0.0
                        || destination_frame >= static_cast<long long>(audio.frame_count())
                        || source_position >= static_cast<double>(audio.frame_count() - 1)) continue;
                    const double normalized = static_cast<double>(offset) / static_cast<double>(radius + 1);
                    const double window = 0.5 + 0.5 * std::cos(std::numbers::pi * normalized);
                    const auto out_frame = static_cast<std::size_t>(destination_frame);
                    weight_sum[out_frame] += window;
                    for (int channel = 0; channel < audio.channel_count(); ++channel) {
                        const auto index = out_frame * channels + static_cast<std::size_t>(channel);
                        const float source_value = interpolate_source(audio, source_position, channel, ratio);
                        accumulation[index] += static_cast<double>(source_value) * window;
                    }
                }

                synthesis_time += 1.0 / target_frequency;
            }
        }
        island_start = island_end + 1;
    }

    for (std::size_t frame = 0; frame < audio.frame_count(); ++frame) {
        if (weight_sum[frame] <= 1.0e-9) continue;
        const double blend = std::clamp(weight_sum[frame], 0.0, 1.0);
        for (int channel = 0; channel < audio.channel_count(); ++channel) {
            const auto index = frame * channels + static_cast<std::size_t>(channel);
            const double shifted = accumulation[index] / weight_sum[frame];
            const double original = audio.sample(frame, channel);
            audio.set_sample(frame, channel, static_cast<float>(original * (1.0 - blend) + shifted * blend));
        }
    }
}

} // namespace VoStudio::Engine
