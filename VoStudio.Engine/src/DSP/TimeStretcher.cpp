#include "VoStudio/Engine/DSP/TimeStretcher.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace VoStudio::Engine {
namespace {
[[nodiscard]] std::size_t next_power_of_two(std::size_t value) {
    std::size_t result = 1;
    while (result < value && result <= std::numeric_limits<std::size_t>::max() / 2) result <<= 1U;
    return result;
}

void fft(std::vector<std::complex<double>>& values, const bool inverse) {
    const std::size_t n = values.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1U;
        for (; (j & bit) != 0U; bit >>= 1U) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(values[i], values[j]);
    }
    for (std::size_t length = 2; length <= n; length <<= 1U) {
        const double angle = (inverse ? 2.0 : -2.0) * std::numbers::pi / static_cast<double>(length);
        const std::complex<double> root(std::cos(angle), std::sin(angle));
        for (std::size_t offset = 0; offset < n; offset += length) {
            std::complex<double> weight(1.0, 0.0);
            for (std::size_t j = 0; j < length / 2; ++j) {
                const auto even = values[offset + j];
                const auto odd = values[offset + j + length / 2] * weight;
                values[offset + j] = even + odd;
                values[offset + j + length / 2] = even - odd;
                weight *= root;
            }
        }
        if (length > n / 2) break; // Prevent unsigned overflow on the final shift.
    }
    if (inverse) {
        const double scale = 1.0 / static_cast<double>(n);
        for (auto& value : values) value *= scale;
    }
}

[[nodiscard]] double mono_at(const AudioBuffer& audio, const std::size_t frame) noexcept {
    double sum = 0.0;
    for (int channel = 0; channel < audio.channel_count(); ++channel) {
        sum += static_cast<double>(audio.samples()[frame * static_cast<std::size_t>(audio.channel_count())
                                                   + static_cast<std::size_t>(channel)]);
    }
    return sum / static_cast<double>(audio.channel_count());
}

[[nodiscard]] AudioBuffer linear_stretch(const AudioBuffer& source, const std::size_t target_frames) {
    AudioBuffer output(source.sample_rate(), source.channel_count(), target_frames);
    if (source.frame_count() == 0 || target_frames == 0) return output;
    const double ratio = target_frames > 1 && source.frame_count() > 1
        ? static_cast<double>(source.frame_count() - 1) / static_cast<double>(target_frames - 1) : 0.0;
    for (std::size_t out = 0; out < target_frames; ++out) {
        const double position = static_cast<double>(out) * ratio;
        const auto left = static_cast<std::size_t>(position);
        const auto right = std::min(left + 1, source.frame_count() - 1);
        const double fraction = position - static_cast<double>(left);
        for (int channel = 0; channel < source.channel_count(); ++channel) {
            const double a = source.sample(left, channel);
            const double b = source.sample(right, channel);
            output.set_sample(out, channel, static_cast<float>(a + (b - a) * fraction));
        }
    }
    return output;
}
}

AudioBuffer TimeStretcher::process(
    const AudioBuffer& source, const std::size_t target_frames, const TimeStretchOptions& options) {
    if (options.algorithm == TimeStretchAlgorithm::PhaseVocoder)
        return phase_vocoder(source, target_frames, options);
    return wsola(source, target_frames, options);
}

AudioBuffer TimeStretcher::wsola(
    const AudioBuffer& source, const std::size_t target_frames, const TimeStretchOptions& options) {
    AudioBuffer output(source.sample_rate(), source.channel_count(), target_frames);
    if (target_frames == 0 || source.frame_count() == 0) return output;
    if (source.frame_count() == target_frames) {
        std::copy(source.samples().begin(), source.samples().end(), output.samples().begin());
        return output;
    }
    if (!(options.window_length_ms > 0.0) || !(options.search_length_ms >= 0.0))
        throw std::invalid_argument("Invalid WSOLA window/search length");

    std::size_t window = static_cast<std::size_t>(std::llround(
        static_cast<double>(source.sample_rate()) * options.window_length_ms / 1000.0));
    window = std::clamp<std::size_t>(window, 64, 4096);
    window = std::min(window, source.frame_count());
    if (window < 32) return linear_stretch(source, target_frames);
    if ((window & 1U) != 0U) --window;
    const std::size_t hop = std::max<std::size_t>(16, window / 2);
    const std::size_t overlap = window - hop;
    const std::size_t search = static_cast<std::size_t>(std::llround(
        static_cast<double>(source.sample_rate()) * options.search_length_ms / 1000.0));
    const double stretch = static_cast<double>(target_frames) / static_cast<double>(source.frame_count());
    const double source_hop = static_cast<double>(hop) / stretch;

    std::vector<double> accumulation(target_frames * static_cast<std::size_t>(source.channel_count()), 0.0);
    std::vector<double> weight_sum(target_frames, 0.0);
    const auto channels = static_cast<std::size_t>(source.channel_count());
    std::size_t grain_index = 0;

    for (std::size_t output_start = 0; output_start < target_frames; output_start += hop, ++grain_index) {
        const auto expected_signed = static_cast<long long>(std::llround(static_cast<double>(grain_index) * source_hop));
        const std::size_t expected = static_cast<std::size_t>(std::max<long long>(0, expected_signed));
        const std::size_t max_candidate = source.frame_count() > window ? source.frame_count() - window : 0;
        const std::size_t low = expected > search ? expected - search : 0;
        const std::size_t high = std::min(max_candidate, expected + search);
        std::size_t best = std::min(expected, max_candidate);

        if (grain_index > 0 && output_start < target_frames) {
            double best_score = -std::numeric_limits<double>::infinity();
            const std::size_t compare_count = std::min({overlap, target_frames - output_start, window});
            for (std::size_t candidate = low; candidate <= high; ++candidate) {
                double dot = 0.0, source_energy = 1.0e-12, output_energy = 1.0e-12;
                for (std::size_t i = 0; i < compare_count; ++i) {
                    const auto out_index = output_start + i;
                    const double accumulated = weight_sum[out_index] > 1.0e-12
                        ? accumulation[out_index * channels] / weight_sum[out_index] : 0.0;
                    const double candidate_value = mono_at(source, candidate + i);
                    dot += accumulated * candidate_value;
                    source_energy += candidate_value * candidate_value;
                    output_energy += accumulated * accumulated;
                }
                const double score = dot / std::sqrt(source_energy * output_energy);
                if (score > best_score) { best_score = score; best = candidate; }
                if (candidate == high) break; // Avoid size_t overflow.
            }
        }

        const std::size_t count = std::min(window, target_frames - output_start);
        for (std::size_t i = 0; i < count; ++i) {
            const double phase = (static_cast<double>(i) + 0.5) / static_cast<double>(window);
            const double window_value = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * phase);
            const std::size_t out_frame = output_start + i;
            weight_sum[out_frame] += window_value;
            for (int channel = 0; channel < source.channel_count(); ++channel) {
                const auto out_index = out_frame * channels + static_cast<std::size_t>(channel);
                accumulation[out_index] += static_cast<double>(source.sample(best + i, channel)) * window_value;
            }
        }
        if (output_start + hop >= target_frames) break;
    }

    for (std::size_t frame = 0; frame < target_frames; ++frame) {
        for (int channel = 0; channel < source.channel_count(); ++channel) {
            const auto index = frame * channels + static_cast<std::size_t>(channel);
            const double value = weight_sum[frame] > 1.0e-12 ? accumulation[index] / weight_sum[frame] : 0.0;
            output.set_sample(frame, channel, static_cast<float>(value));
        }
    }
    return output;
}

AudioBuffer TimeStretcher::phase_vocoder(
    const AudioBuffer& source, const std::size_t target_frames, const TimeStretchOptions& options) {
    AudioBuffer output(source.sample_rate(), source.channel_count(), target_frames);
    if (target_frames == 0 || source.frame_count() == 0) return output;
    if (source.frame_count() == target_frames) {
        std::copy(source.samples().begin(), source.samples().end(), output.samples().begin());
        return output;
    }
    if (!(options.window_length_ms > 0.0)) throw std::invalid_argument("Invalid phase-vocoder window length");

    const auto requested = static_cast<std::size_t>(std::llround(
        static_cast<double>(source.sample_rate()) * options.window_length_ms / 1000.0));
    const std::size_t fft_size = std::clamp<std::size_t>(next_power_of_two(std::max<std::size_t>(256, requested)), 256, 4096);
    const std::size_t hop = fft_size / 4;
    const std::size_t bins = fft_size / 2 + 1;
    const double stretch = static_cast<double>(target_frames) / static_cast<double>(source.frame_count());
    const auto channels = static_cast<std::size_t>(source.channel_count());
    std::vector<double> accumulation(target_frames * channels, 0.0);
    std::vector<double> weight_sum(target_frames, 0.0);
    std::vector<double> window(fft_size);
    for (std::size_t i = 0; i < fft_size; ++i)
        window[i] = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * (static_cast<double>(i) + 0.5) / static_cast<double>(fft_size));

    for (int channel = 0; channel < source.channel_count(); ++channel) {
        std::vector<double> previous_phase(bins, 0.0);
        std::vector<double> phase_accumulator(bins, 0.0);
        bool first = true;
        const std::size_t max_frames = (source.frame_count() + fft_size + hop - 1) / hop;

        for (std::size_t frame_index = 0; frame_index < max_frames; ++frame_index) {
            const auto analysis_center = static_cast<long long>(frame_index * hop);
            const auto synthesis_center = static_cast<long long>(std::llround(static_cast<double>(analysis_center) * stretch));
            if (synthesis_center - static_cast<long long>(fft_size / 2) >= static_cast<long long>(target_frames)) break;
            const auto analysis_start = analysis_center - static_cast<long long>(fft_size / 2);
            std::vector<std::complex<double>> spectrum(fft_size, {0.0, 0.0});
            for (std::size_t i = 0; i < fft_size; ++i) {
                const auto source_index = analysis_start + static_cast<long long>(i);
                const double sample = source_index >= 0 && source_index < static_cast<long long>(source.frame_count())
                    ? static_cast<double>(source.sample(static_cast<std::size_t>(source_index), channel)) : 0.0;
                spectrum[i] = {sample * window[i], 0.0};
            }
            fft(spectrum, false);

            for (std::size_t bin = 0; bin < bins; ++bin) {
                const double phase = std::arg(spectrum[bin]);
                const double expected = 2.0 * std::numbers::pi * static_cast<double>(bin * hop) / static_cast<double>(fft_size);
                if (first) {
                    phase_accumulator[bin] = phase;
                } else {
                    double delta = phase - previous_phase[bin] - expected;
                    while (delta > std::numbers::pi) delta -= 2.0 * std::numbers::pi;
                    while (delta < -std::numbers::pi) delta += 2.0 * std::numbers::pi;
                    phase_accumulator[bin] += expected * stretch + delta * stretch;
                }
                previous_phase[bin] = phase;
                const double magnitude = std::abs(spectrum[bin]);
                spectrum[bin] = std::polar(magnitude, phase_accumulator[bin]);
            }
            first = false;
            for (std::size_t bin = 1; bin < fft_size / 2; ++bin)
                spectrum[fft_size - bin] = std::conj(spectrum[bin]);
            fft(spectrum, true);

            const auto synthesis_start = synthesis_center - static_cast<long long>(fft_size / 2);
            for (std::size_t i = 0; i < fft_size; ++i) {
                const auto out_signed = synthesis_start + static_cast<long long>(i);
                if (out_signed < 0 || out_signed >= static_cast<long long>(target_frames)) continue;
                const auto out_frame = static_cast<std::size_t>(out_signed);
                const double w = window[i];
                accumulation[out_frame * channels + static_cast<std::size_t>(channel)] += spectrum[i].real() * w;
                // Same window is used at analysis and synthesis; normalize by w^2.
                if (channel == 0) weight_sum[out_frame] += w * w;
            }
        }
    }

    for (std::size_t frame = 0; frame < target_frames; ++frame) {
        for (int channel = 0; channel < source.channel_count(); ++channel) {
            const auto index = frame * channels + static_cast<std::size_t>(channel);
            const double value = weight_sum[frame] > 1.0e-10 ? accumulation[index] / weight_sum[frame] : 0.0;
            output.set_sample(frame, channel, static_cast<float>(value));
        }
    }
    return output;
}

} // namespace VoStudio::Engine
