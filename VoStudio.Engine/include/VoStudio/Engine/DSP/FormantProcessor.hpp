#pragma once

#include "VoStudio/Engine/AudioBuffer.hpp"

namespace VoStudio::Engine {

class FormantProcessor final {
public:
    // Compensates broad spectral-envelope changes by comparing time-aligned
    // reference and pitch-shifted audio. Amount is clamped to [0, 1]. The
    // operation is deliberately conservative; phoneme-local envelopes will
    // be preferable once the voicebank renderer supplies phoneme boundaries.
    [[nodiscard]] static AudioBuffer preserve(
        const AudioBuffer& reference,
        const AudioBuffer& pitch_shifted,
        double amount = 0.65,
        std::size_t fft_size = 2048);
};

} // namespace VoStudio::Engine
