#include "VoStudio/Engine/DSP/FormantProcessor.hpp"
#include "VoStudio/Engine/Analysis/PitchDetector.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace VoStudio::Engine {
namespace {
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
        if (length > n / 2) break;
    }
    if (inverse) for (auto& value : values) value /= static_cast<double>(n);
}

[[nodiscard]] std::vector<double> smooth_log_envelope(
    const std::vector<std::complex<double>>& spectrum) {
    const std::size_t bins = spectrum.size() / 2 + 1;
    std::vector<double> log_magnitude(bins);
    for (std::size_t i = 0; i < bins; ++i)
        log_magnitude[i] = std::log(std::max(1.0e-8, std::abs(spectrum[i])));

    std::vector<double> envelope(bins);
    for (std::size_t bin = 0; bin < bins; ++bin) {
        const std::size_t radius = std::max<std::size_t>(1, bin / 10); // Approx. one-sixth-octave smoothing.
        const std::size_t begin = bin > radius ? bin - radius : 0;
        const std::size_t end = std::min(bins - 1, bin + radius);
        double sum = 0.0;
        for (std::size_t k = begin; k <= end; ++k) sum += log_magnitude[k];
        envelope[bin] = sum / static_cast<double>(end - begin + 1);
    }
    return envelope;
}
}

AudioBuffer FormantProcessor::preserve(
    const AudioBuffer& reference, const AudioBuffer& pitch_shifted,
    double amount, std::size_t fft_size) {
    if (reference.sample_rate() != pitch_shifted.sample_rate()
        || reference.channel_count() != pitch_shifted.channel_count()
        || reference.frame_count() != pitch_shifted.frame_count()) {
        throw std::invalid_argument("Formant preservation requires aligned audio buffers");
    }
    if (!std::isfinite(amount)) throw std::invalid_argument("Formant preservation amount must be finite");
    amount = std::clamp(amount, 0.0, 1.0);
    if (amount <= 0.0 || reference.frame_count() == 0) return pitch_shifted;
    const auto reference_voicing = PitchDetector::analyze(reference);
    if (fft_size < 256 || (fft_size & (fft_size - 1U)) != 0U)
        throw std::invalid_argument("Formant processor FFT size must be a power of two of at least 256");

    AudioBuffer output(pitch_shifted.sample_rate(), pitch_shifted.channel_count(), pitch_shifted.frame_count());
    const std::size_t hop = fft_size / 4;
    const std::size_t bins = fft_size / 2 + 1;
    std::vector<double> window(fft_size);
    for (std::size_t i = 0; i < fft_size; ++i)
        window[i] = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * (static_cast<double>(i) + 0.5)
                                       / static_cast<double>(fft_size));

    const auto frames = pitch_shifted.frame_count();
    std::vector<double> accumulation(frames * static_cast<std::size_t>(pitch_shifted.channel_count()), 0.0);
    std::vector<double> weight_sum(frames, 0.0);

    for (int channel = 0; channel < pitch_shifted.channel_count(); ++channel) {
        for (std::size_t center = 0; center < frames + fft_size / 2; center += hop) {
            const auto start = static_cast<long long>(center) - static_cast<long long>(fft_size / 2);
            std::vector<std::complex<double>> ref_spectrum(fft_size, {0.0, 0.0});
            std::vector<std::complex<double>> shifted_spectrum(fft_size, {0.0, 0.0});
            for (std::size_t i = 0; i < fft_size; ++i) {
                const auto index = start + static_cast<long long>(i);
                if (index < 0 || index >= static_cast<long long>(frames)) continue;
                const auto frame = static_cast<std::size_t>(index);
                ref_spectrum[i] = {static_cast<double>(reference.sample(frame, channel)) * window[i], 0.0};
                shifted_spectrum[i] = {static_cast<double>(pitch_shifted.sample(frame, channel)) * window[i], 0.0};
            }
            fft(ref_spectrum, false);
            fft(shifted_spectrum, false);
            const auto ref_envelope = smooth_log_envelope(ref_spectrum);
            const auto shifted_envelope = smooth_log_envelope(shifted_spectrum);
            const double frame_time = std::max(0.0, static_cast<double>(center)
                / static_cast<double>(reference.sample_rate()));
            const bool is_voiced = PitchDetector::frequency_at(reference_voicing, frame_time) > 0.0;
            double maximum_magnitude = 0.0;
            for (std::size_t bin = 1; bin < bins; ++bin)
                maximum_magnitude = std::max(maximum_magnitude, std::abs(ref_spectrum[bin]));
            std::size_t significant_bins = 0;
            for (std::size_t bin = 1; bin < bins; ++bin)
                if (std::abs(ref_spectrum[bin]) >= maximum_magnitude * 0.015) ++significant_bins;
            // A pure tone or very sparse harmonic frame does not contain enough
            // information to infer a vocal-tract envelope. Leave it untouched.
            const bool has_enough_spectral_detail = significant_bins >= 8;
            if (is_voiced && has_enough_spectral_detail) {
                for (std::size_t bin = 0; bin < bins; ++bin) {
                    const double correction_db = (ref_envelope[bin] - shifted_envelope[bin]) * amount;
                    const double correction = std::clamp(std::exp(correction_db), 0.5, 2.0);
                    shifted_spectrum[bin] *= correction;
                }
            }
            for (std::size_t bin = 1; bin < fft_size / 2; ++bin)
                shifted_spectrum[fft_size - bin] = std::conj(shifted_spectrum[bin]);
            fft(shifted_spectrum, true);

            for (std::size_t i = 0; i < fft_size; ++i) {
                const auto index = start + static_cast<long long>(i);
                if (index < 0 || index >= static_cast<long long>(frames)) continue;
                const auto frame = static_cast<std::size_t>(index);
                const double w = window[i];
                accumulation[frame * static_cast<std::size_t>(pitch_shifted.channel_count())
                             + static_cast<std::size_t>(channel)] += shifted_spectrum[i].real() * w;
                if (channel == 0) weight_sum[frame] += w * w;
            }
            if (center > frames + hop) break;
        }
    }

    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < pitch_shifted.channel_count(); ++channel) {
            const auto index = frame * static_cast<std::size_t>(pitch_shifted.channel_count())
                             + static_cast<std::size_t>(channel);
            const double value = weight_sum[frame] > 1.0e-10
                ? accumulation[index] / weight_sum[frame] : pitch_shifted.sample(frame, channel);
            output.set_sample(frame, channel, static_cast<float>(value));
        }
    }
    return output;
}

} // namespace VoStudio::Engine
