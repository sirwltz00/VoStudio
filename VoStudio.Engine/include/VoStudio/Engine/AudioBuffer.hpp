#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace VoStudio::Engine {

// Interleaved, normalized floating-point PCM audio.
class AudioBuffer final {
public:
    AudioBuffer(int sample_rate, int channel_count, std::size_t frame_count);

    [[nodiscard]] int sample_rate() const noexcept { return sample_rate_; }
    [[nodiscard]] int channel_count() const noexcept { return channel_count_; }
    [[nodiscard]] std::size_t frame_count() const noexcept { return frame_count_; }
    [[nodiscard]] double duration_seconds() const noexcept;

    [[nodiscard]] float sample(std::size_t frame, int channel) const;
    void set_sample(std::size_t frame, int channel, float value);

    [[nodiscard]] std::span<float> samples() noexcept { return samples_; }
    [[nodiscard]] std::span<const float> samples() const noexcept { return samples_; }

    void clear() noexcept;
    [[nodiscard]] float peak() const noexcept;

private:
    [[nodiscard]] std::size_t index(std::size_t frame, int channel) const;

    int sample_rate_;
    int channel_count_;
    std::size_t frame_count_;
    std::vector<float> samples_;
};

} // namespace VoStudio::Engine
