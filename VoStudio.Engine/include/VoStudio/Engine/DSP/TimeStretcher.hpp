#pragma once

#include "VoStudio/Engine/AudioBuffer.hpp"

namespace VoStudio::Engine {

enum class TimeStretchAlgorithm {
    Wsola,
    PhaseVocoder
};

struct TimeStretchOptions final {
    TimeStretchAlgorithm algorithm{TimeStretchAlgorithm::Wsola};
    double window_length_ms{40.0};
    double search_length_ms{6.0};
};

class TimeStretcher final {
public:
    // Returns exactly target_frames. A zero-length input produces silence.
    [[nodiscard]] static AudioBuffer process(
        const AudioBuffer& source,
        std::size_t target_frames,
        const TimeStretchOptions& options = {});

    [[nodiscard]] static AudioBuffer wsola(
        const AudioBuffer& source,
        std::size_t target_frames,
        const TimeStretchOptions& options = {});

    [[nodiscard]] static AudioBuffer phase_vocoder(
        const AudioBuffer& source,
        std::size_t target_frames,
        const TimeStretchOptions& options = {});
};

} // namespace VoStudio::Engine
