#pragma once

#include "VoStudio/Engine/AudioBuffer.hpp"
#include "VoStudio/Engine/DSP/TimeStretcher.hpp"
#include "VoStudio/Engine/PitchCurve.hpp"

namespace VoStudio::Engine {

struct RenderParameters final {
    int source_root_midi_note{60};
    int target_midi_note{60};
    double source_start_seconds{0.0};
    double output_duration_seconds{0.0};
    float gain{1.0F};

    double formant_preservation{0.65};

    const PitchCurve* pitch_curve{nullptr};
    TimeStretchOptions time_stretch{};
};

// Offline, phoneme-agnostic hybrid renderer. It time-stretches recorded audio,
// then pitch-shifts voiced regions with PSOLA while leaving unvoiced regions
// largely intact. Voicebank/oto.ini/phonemizer integration is intentionally
// above this layer and will provide the samples and timing metadata later.
class HybridSynthesisEngine final {
public:
    [[nodiscard]] static AudioBuffer render(
        const AudioBuffer& source,
        const RenderParameters& parameters = {});

    // Compatibility helper retained for the initial engine scaffold. This is
    // a simple resampling operation, not the full hybrid render path.
    static void render_sample(
        const AudioBuffer& source,
        AudioBuffer& destination,
        const RenderParameters& parameters = {});
};

} // namespace VoStudio::Engine
