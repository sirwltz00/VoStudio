#pragma once

#include "VoStudio/Engine/AudioBuffer.hpp"

#include <filesystem>

namespace VoStudio::Engine {

class WavFile final {
public:
    // Reads RIFF/WAVE PCM (8/16/24/32-bit) and IEEE float32 files.
    [[nodiscard]] static AudioBuffer read(const std::filesystem::path& path);

    // Writes interleaved floating-point audio as PCM 16-, 24-, or 32-bit WAV.
    static void write(const std::filesystem::path& path, const AudioBuffer& audio, int bit_depth = 24);
};

} // namespace VoStudio::Engine
