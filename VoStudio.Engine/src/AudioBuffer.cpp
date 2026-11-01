#include "VoStudio/Engine/AudioBuffer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace VoStudio::Engine {

AudioBuffer::AudioBuffer(const int sample_rate, const int channel_count, const std::size_t frame_count)
    : sample_rate_(sample_rate), channel_count_(channel_count), frame_count_(frame_count) {
    if (sample_rate_ <= 0) {
        throw std::invalid_argument("AudioBuffer sample rate must be positive");
    }
    if (channel_count_ <= 0) {
        throw std::invalid_argument("AudioBuffer channel count must be positive");
    }
    const auto channels = static_cast<std::size_t>(channel_count_);
    if (frame_count_ > std::numeric_limits<std::size_t>::max() / channels) {
        throw std::length_error("AudioBuffer size overflows size_t");
    }
    samples_.assign(frame_count_ * channels, 0.0F);
}

double AudioBuffer::duration_seconds() const noexcept {
    return static_cast<double>(frame_count_) / static_cast<double>(sample_rate_);
}

std::size_t AudioBuffer::index(const std::size_t frame, const int channel) const {
    if (frame >= frame_count_ || channel < 0 || channel >= channel_count_) {
        throw std::out_of_range("AudioBuffer sample index is out of range");
    }
    return frame * static_cast<std::size_t>(channel_count_) + static_cast<std::size_t>(channel);
}

float AudioBuffer::sample(const std::size_t frame, const int channel) const {
    return samples_.at(index(frame, channel));
}

void AudioBuffer::set_sample(const std::size_t frame, const int channel, const float value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("AudioBuffer samples must be finite");
    }
    samples_.at(index(frame, channel)) = value;
}

void AudioBuffer::clear() noexcept {
    std::fill(samples_.begin(), samples_.end(), 0.0F);
}

float AudioBuffer::peak() const noexcept {
    float result = 0.0F;
    for (const float value : samples_) {
        result = std::max(result, std::abs(value));
    }
    return result;
}

} // namespace VoStudio::Engine
