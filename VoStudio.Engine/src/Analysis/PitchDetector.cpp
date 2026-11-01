#include "VoStudio/Engine/Analysis/PitchDetector.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace VoStudio::Engine {
namespace {
[[nodiscard]] double mono_sample(const AudioBuffer& audio, const std::size_t frame) noexcept {
    double sum = 0.0;
    for (int channel = 0; channel < audio.channel_count(); ++channel) {
        sum += static_cast<double>(audio.samples()[frame * static_cast<std::size_t>(audio.channel_count())
                                                   + static_cast<std::size_t>(channel)]);
    }
    return sum / static_cast<double>(audio.channel_count());
}
}

std::vector<PitchFrame> PitchDetector::analyze(
    const AudioBuffer& audio, const PitchDetectionOptions& options) {
    if (!(options.minimum_frequency_hz > 0.0)
        || !(options.maximum_frequency_hz > options.minimum_frequency_hz)
        || !(options.frame_length_ms > 0.0)
        || !(options.hop_length_ms > 0.0)
        || !(options.yin_threshold > 0.0 && options.yin_threshold < 1.0)) {
        throw std::invalid_argument("Invalid YIN pitch detector options");
    }
    if (audio.frame_count() == 0) return {};

    const auto sample_rate = static_cast<double>(audio.sample_rate());
    const auto frame_size = std::max<std::size_t>(
        64, static_cast<std::size_t>(std::llround(sample_rate * options.frame_length_ms / 1000.0)));
    const auto hop_size = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::llround(sample_rate * options.hop_length_ms / 1000.0)));
    const auto min_lag = std::max<std::size_t>(2, static_cast<std::size_t>(sample_rate / options.maximum_frequency_hz));
    const auto max_lag = std::min(frame_size / 2,
        static_cast<std::size_t>(sample_rate / options.minimum_frequency_hz));
    if (max_lag <= min_lag + 1) throw std::invalid_argument("YIN frame is too short for the requested frequency range");

    std::vector<double> mono(audio.frame_count());
    for (std::size_t i = 0; i < audio.frame_count(); ++i) mono[i] = mono_sample(audio, i);

    std::vector<PitchFrame> result;
    std::vector<double> difference(max_lag + 1, 0.0);
    std::vector<double> normalized(max_lag + 1, 1.0);

    for (std::size_t start = 0; start < audio.frame_count(); start += hop_size) {
        const std::size_t available = std::min(frame_size, audio.frame_count() - start);
        if (available <= max_lag + 2) break;

        double energy = 0.0;
        for (std::size_t i = 0; i < available; ++i) energy += mono[start + i] * mono[start + i];
        if (energy / static_cast<double>(available) < 1.0e-9) {
            result.push_back({(static_cast<double>(start) + static_cast<double>(available) * 0.5) / sample_rate,
                              0.0, 0.0, false});
            continue;
        }

        std::fill(difference.begin(), difference.end(), 0.0);
        for (std::size_t tau = 1; tau <= max_lag; ++tau) {
            double sum = 0.0;
            const std::size_t count = available - tau;
            for (std::size_t i = 0; i < count; ++i) {
                const double delta = mono[start + i] - mono[start + i + tau];
                sum += delta * delta;
            }
            difference[tau] = sum;
        }

        normalized[0] = 1.0;
        double running_sum = 0.0;
        for (std::size_t tau = 1; tau <= max_lag; ++tau) {
            running_sum += difference[tau];
            normalized[tau] = running_sum > 1.0e-18
                ? difference[tau] * static_cast<double>(tau) / running_sum : 1.0;
        }

        std::size_t best_lag = 0;
        for (std::size_t tau = min_lag; tau < max_lag; ++tau) {
            if (normalized[tau] < options.yin_threshold) {
                while (tau + 1 <= max_lag && normalized[tau + 1] < normalized[tau]) ++tau;
                best_lag = tau;
                break;
            }
        }
        if (best_lag == 0) {
            const auto first = normalized.begin() + static_cast<std::ptrdiff_t>(min_lag);
            const auto last = normalized.begin() + static_cast<std::ptrdiff_t>(max_lag + 1);
            const auto minimum = std::min_element(first, last);
            if (minimum != last && *minimum < 0.35) {
                best_lag = static_cast<std::size_t>(std::distance(normalized.begin(), minimum));
            }
        }

        const double confidence = best_lag == 0 ? 0.0 : std::clamp(1.0 - normalized[best_lag], 0.0, 1.0);
        const double frequency = best_lag == 0 ? 0.0 : sample_rate / static_cast<double>(best_lag);
        const bool voiced = best_lag != 0 && frequency >= options.minimum_frequency_hz
                         && frequency <= options.maximum_frequency_hz && confidence >= 0.45;
        result.push_back({(static_cast<double>(start) + static_cast<double>(available) * 0.5) / sample_rate,
                          voiced ? frequency : 0.0, confidence, voiced});
        if (start + hop_size >= audio.frame_count()) break;
    }
    return result;
}

double PitchDetector::frequency_at(const std::vector<PitchFrame>& contour, const double time_seconds) noexcept {
    if (contour.empty() || !std::isfinite(time_seconds)) return 0.0;
    const auto upper = std::lower_bound(contour.begin(), contour.end(), time_seconds,
        [](const PitchFrame& frame, const double time) { return frame.time_seconds < time; });
    if (upper == contour.begin()) return upper->voiced ? upper->frequency_hz : 0.0;
    if (upper == contour.end()) return contour.back().voiced ? contour.back().frequency_hz : 0.0;
    const auto lower = upper - 1;
    if (!lower->voiced || !upper->voiced) {
        return std::abs(time_seconds - lower->time_seconds) <= std::abs(upper->time_seconds - time_seconds)
            ? (lower->voiced ? lower->frequency_hz : 0.0)
            : (upper->voiced ? upper->frequency_hz : 0.0);
    }
    const double span = upper->time_seconds - lower->time_seconds;
    if (span <= 0.0) return lower->frequency_hz;
    const double amount = (time_seconds - lower->time_seconds) / span;
    return lower->frequency_hz + (upper->frequency_hz - lower->frequency_hz) * amount;
}

} // namespace VoStudio::Engine
