#pragma once

#include "VoStudio/Engine/AudioBuffer.hpp"
#include "VoStudio/Engine/PitchCurve.hpp"

namespace VoStudio::Engine {

struct PSOLAOptions final {
    double minimum_frequency_hz{55.0};
    double maximum_frequency_hz{1500.0};
    double frame_length_ms{40.0};
    double hop_length_ms{10.0};
    double yin_threshold{0.15};
};

class PSOLAPitchShifter final {
public:
    // Pitch-shifts voiced regions while reusing local waveform periods. This
    // naturally retains much of the source spectral/formant envelope. Unvoiced
    // regions are passed through rather than having their noise spectrum pitched.
    static void process(
        AudioBuffer& audio,
        double semitone_shift,
        const PitchCurve* pitch_curve = nullptr,
        const PSOLAOptions& options = {});
};

} // namespace VoStudio::Engine
