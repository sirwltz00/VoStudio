#pragma once

#include "VoStudio/Engine/AudioBuffer.hpp"

namespace VoStudio::Engine {

enum class CrossfadeCurve {
    Linear,
    EqualPower
};

class CrossfadeProcessor final {
public:
    // Joins two buffers with an overlap. Both buffers must have the same
    // sample rate and channel count; overlap is clamped to the shorter buffer.
    [[nodiscard]] static AudioBuffer join(
        const AudioBuffer& first,
        const AudioBuffer& second,
        std::size_t overlap_frames,
        CrossfadeCurve curve = CrossfadeCurve::EqualPower);
};

} // namespace VoStudio::Engine
