#include "VoStudio/Engine/WavFile.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace VoStudio::Engine {
namespace {
[[nodiscard]] std::uint16_t u16(const std::vector<std::uint8_t>& b, std::size_t p) {
    return static_cast<std::uint16_t>(b.at(p) | (static_cast<std::uint16_t>(b.at(p + 1)) << 8U));
}
[[nodiscard]] std::uint32_t u32(const std::vector<std::uint8_t>& b, std::size_t p) {
    return static_cast<std::uint32_t>(b.at(p))
        | (static_cast<std::uint32_t>(b.at(p + 1)) << 8U)
        | (static_cast<std::uint32_t>(b.at(p + 2)) << 16U)
        | (static_cast<std::uint32_t>(b.at(p + 3)) << 24U);
}
void put_u16(std::ostream& out, const std::uint16_t value) {
    out.put(static_cast<char>(value & 0xFFU));
    out.put(static_cast<char>((value >> 8U) & 0xFFU));
}
void put_u32(std::ostream& out, const std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) out.put(static_cast<char>((value >> shift) & 0xFFU));
}
[[nodiscard]] std::string fourcc(const std::vector<std::uint8_t>& b, const std::size_t p) {
    return std::string(reinterpret_cast<const char*>(b.data() + p), 4);
}
[[nodiscard]] float decode_pcm(const std::vector<std::uint8_t>& bytes, const std::size_t offset,
                               const int bits, const bool floating_point) {
    if (floating_point) {
        const std::uint32_t raw = u32(bytes, offset);
        const float value = std::bit_cast<float>(raw);
        return std::isfinite(value) ? value : 0.0F;
    }
    if (bits == 8) {
        return (static_cast<float>(bytes.at(offset)) - 128.0F) / 128.0F;
    }
    if (bits == 16) {
        const auto raw = static_cast<std::int16_t>(u16(bytes, offset));
        return static_cast<float>(raw) / 32768.0F;
    }
    if (bits == 24) {
        std::int32_t raw = static_cast<std::int32_t>(bytes.at(offset))
            | (static_cast<std::int32_t>(bytes.at(offset + 1)) << 8)
            | (static_cast<std::int32_t>(bytes.at(offset + 2)) << 16);
        if ((raw & 0x00800000) != 0) raw |= static_cast<std::int32_t>(0xFF000000U);
        return static_cast<float>(raw) / 8388608.0F;
    }
    if (bits == 32) {
        const auto raw = static_cast<std::int32_t>(u32(bytes, offset));
        return static_cast<float>(static_cast<double>(raw) / 2147483648.0);
    }
    throw std::runtime_error("Unsupported WAV bit depth");
}
}

AudioBuffer WavFile::read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Unable to open WAV file: " + path.string());
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (bytes.size() < 12 || fourcc(bytes, 0) != "RIFF" || fourcc(bytes, 8) != "WAVE") {
        throw std::runtime_error("File is not a supported RIFF/WAVE file");
    }

    std::uint16_t format = 0, channels = 0, bits = 0, block_align = 0;
    std::uint32_t sample_rate = 0;
    std::size_t data_offset = 0, data_size = 0;
    bool have_format = false, have_data = false;

    std::size_t cursor = 12;
    while (cursor + 8 <= bytes.size()) {
        const std::string chunk = fourcc(bytes, cursor);
        const std::uint32_t chunk_size = u32(bytes, cursor + 4);
        const std::size_t payload = cursor + 8;
        if (chunk_size > bytes.size() - payload) throw std::runtime_error("WAV chunk exceeds file size");
        if (chunk == "fmt ") {
            if (chunk_size < 16) throw std::runtime_error("Invalid WAV fmt chunk");
            format = u16(bytes, payload);
            channels = u16(bytes, payload + 2);
            sample_rate = u32(bytes, payload + 4);
            block_align = u16(bytes, payload + 12);
            bits = u16(bytes, payload + 14);
            if (format == 0xFFFE && chunk_size >= 40) format = u16(bytes, payload + 24);
            have_format = true;
        } else if (chunk == "data") {
            data_offset = payload;
            data_size = chunk_size;
            have_data = true;
        }
        cursor = payload + chunk_size + (chunk_size & 1U);
    }

    if (!have_format || !have_data) throw std::runtime_error("WAV file is missing fmt or data chunk");
    const bool floating_point = format == 3;
    if (format != 1 && !floating_point) throw std::runtime_error("Only PCM and IEEE float WAV are supported");
    if (channels == 0 || sample_rate == 0 || block_align == 0) throw std::runtime_error("Invalid WAV format values");
    if (floating_point && bits != 32) throw std::runtime_error("Only 32-bit float WAV is supported");
    if (!floating_point && bits != 8 && bits != 16 && bits != 24 && bits != 32) throw std::runtime_error("Unsupported PCM bit depth");
    const std::size_t bytes_per_sample = static_cast<std::size_t>(bits / 8U);
    if (block_align != channels * bytes_per_sample || data_size % block_align != 0) throw std::runtime_error("Invalid WAV block alignment");

    const std::size_t frames = data_size / block_align;
    AudioBuffer result(static_cast<int>(sample_rate), static_cast<int>(channels), frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < static_cast<int>(channels); ++channel) {
            const std::size_t offset = data_offset + frame * block_align + static_cast<std::size_t>(channel) * bytes_per_sample;
            result.set_sample(frame, channel, decode_pcm(bytes, offset, bits, floating_point));
        }
    }
    return result;
}

void WavFile::write(const std::filesystem::path& path, const AudioBuffer& audio, const int bit_depth) {
    if (bit_depth != 16 && bit_depth != 24 && bit_depth != 32) throw std::invalid_argument("WAV bit depth must be 16, 24, or 32");
    const std::uint64_t data_size_64 = static_cast<std::uint64_t>(audio.frame_count())
        * static_cast<std::uint64_t>(audio.channel_count()) * static_cast<std::uint64_t>(bit_depth / 8);
    if (data_size_64 > std::numeric_limits<std::uint32_t>::max() - 36U) throw std::length_error("WAV file is too large for RIFF");
    const auto data_size = static_cast<std::uint32_t>(data_size_64);
    const auto block_align = static_cast<std::uint16_t>(audio.channel_count() * (bit_depth / 8));
    const auto byte_rate_64 = static_cast<std::uint64_t>(audio.sample_rate()) * block_align;
    if (byte_rate_64 > std::numeric_limits<std::uint32_t>::max()) throw std::length_error("WAV byte rate overflows format field");

    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Unable to create WAV file: " + path.string());
    out.write("RIFF", 4); put_u32(out, 36U + data_size); out.write("WAVE", 4);
    out.write("fmt ", 4); put_u32(out, 16U); put_u16(out, 1U);
    put_u16(out, static_cast<std::uint16_t>(audio.channel_count()));
    put_u32(out, static_cast<std::uint32_t>(audio.sample_rate()));
    put_u32(out, static_cast<std::uint32_t>(byte_rate_64));
    put_u16(out, block_align); put_u16(out, static_cast<std::uint16_t>(bit_depth));
    out.write("data", 4); put_u32(out, data_size);

    for (const float sample : audio.samples()) {
        const double clamped = std::clamp(static_cast<double>(sample), -1.0, 1.0);
        if (bit_depth == 16) {
            const auto value = static_cast<std::int16_t>(std::lrint(clamped * 32767.0));
            put_u16(out, static_cast<std::uint16_t>(value));
        } else if (bit_depth == 24) {
            const auto value = static_cast<std::int32_t>(std::llround(clamped * 8388607.0));
            out.put(static_cast<char>(value & 0xFF));
            out.put(static_cast<char>((value >> 8) & 0xFF));
            out.put(static_cast<char>((value >> 16) & 0xFF));
        } else {
            const auto value = static_cast<std::int32_t>(std::llround(clamped * 2147483647.0));
            put_u32(out, static_cast<std::uint32_t>(value));
        }
    }
    if (!out) throw std::runtime_error("Failed while writing WAV file: " + path.string());
}

} // namespace VoStudio::Engine
