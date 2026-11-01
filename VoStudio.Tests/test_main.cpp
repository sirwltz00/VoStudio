#include "VoStudio/Engine/Analysis/PitchDetector.hpp"
#include "VoStudio/Engine/AudioBuffer.hpp"
#include "VoStudio/Engine/Diagnostics/AudioQualityAnalyzer.hpp"
#include "VoStudio/Engine/DSP/TimeStretcher.hpp"
#include "VoStudio/Engine/DSP/CrossfadeProcessor.hpp"
#include "VoStudio/Engine/DSP/FormantProcessor.hpp"
#include "VoStudio/Engine/HybridSynthesisEngine.hpp"
#include "VoStudio/Engine/PitchCurve.hpp"
#include "VoStudio/Engine/WavFile.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace VoStudio::Engine;

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

AudioBuffer make_sine(const int sample_rate, const double frequency, const double duration, const float amplitude = 0.2F) {
    const auto frames = static_cast<std::size_t>(std::llround(static_cast<double>(sample_rate) * duration));
    AudioBuffer audio(sample_rate, 1, frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const double time = static_cast<double>(i) / static_cast<double>(sample_rate);
        audio.set_sample(i, 0, amplitude * static_cast<float>(std::sin(2.0 * std::acos(-1.0) * frequency * time)));
    }
    return audio;
}

void test_audio_buffer() {
    AudioBuffer audio(48000, 2, 128);
    require(audio.sample_rate() == 48000, "sample rate mismatch");
    require(audio.channel_count() == 2, "channel count mismatch");
    require(audio.frame_count() == 128, "frame count mismatch");
    audio.set_sample(0, 0, 0.5F);
    require(std::abs(audio.peak() - 0.5F) < 1.0e-6F, "peak calculation failed");
    audio.clear();
    require(audio.peak() == 0.0F, "clear failed");
}

void test_pitch_curve() {
    const PitchCurve curve({{0.0, 0.0}, {1.0, 200.0}, {2.0, 0.0}});
    require(std::abs(curve.cents_at(0.5) - 100.0) < 1.0e-9, "pitch interpolation failed");
    require(std::abs(curve.cents_at(1.5) - 100.0) < 1.0e-9, "pitch interpolation failed");
}

void test_pitch_detector() {
    const auto audio = make_sine(48000, 220.0, 0.5);
    const auto contour = PitchDetector::analyze(audio);
    const double estimated = PitchDetector::frequency_at(contour, 0.25);
    require(estimated > 210.0 && estimated < 230.0, "YIN pitch estimate is outside tolerance");
}

void test_wsola_duration() {
    const auto source = make_sine(48000, 220.0, 0.35);
    const auto stretched = TimeStretcher::wsola(source, 24000);
    require(stretched.frame_count() == 24000, "WSOLA did not produce the requested duration");
    require(stretched.peak() > 0.1F, "WSOLA output unexpectedly silent");
    const auto contour = PitchDetector::analyze(stretched);
    const double f0 = PitchDetector::frequency_at(contour, 0.2);
    require(f0 > 205.0 && f0 < 235.0, "WSOLA did not preserve pitch within tolerance");
}

void test_phase_vocoder_duration() {
    const auto source = make_sine(48000, 330.0, 0.25);
    const auto stretched = TimeStretcher::phase_vocoder(source, 18000);
    require(stretched.frame_count() == 18000, "phase vocoder did not produce requested duration");
    require(stretched.peak() > 0.05F, "phase-vocoder output unexpectedly silent");
    const auto contour = PitchDetector::analyze(stretched);
    const double f0 = PitchDetector::frequency_at(contour, 0.1);
    require(f0 > 300.0 && f0 < 360.0, "phase vocoder did not preserve pitch within tolerance");
}

void test_hybrid_pitch_render() {
    const auto source = make_sine(48000, 220.0, 0.5);
    RenderParameters parameters;
    parameters.source_root_midi_note = 57; // A3
    parameters.target_midi_note = 69;      // A4
    parameters.output_duration_seconds = 0.5;
    const auto rendered = HybridSynthesisEngine::render(source, parameters);
    require(rendered.frame_count() == source.frame_count(), "hybrid renderer duration mismatch");
    const auto contour = PitchDetector::analyze(rendered);
    const double frequency = PitchDetector::frequency_at(contour, 0.25);
    require(frequency > 410.0 && frequency < 470.0, "PSOLA octave render is outside tolerance");
    require(rendered.peak() > 0.05F, "hybrid renderer output unexpectedly silent");
}

void test_hybrid_semitone_render() {
    const auto source = make_sine(48000, 220.0, 0.6);
    RenderParameters parameters;
    parameters.source_root_midi_note = 57;
    parameters.target_midi_note = 60; // Three semitones above the root.
    parameters.output_duration_seconds = 0.6;
    const auto rendered = HybridSynthesisEngine::render(source, parameters);
    const auto contour = PitchDetector::analyze(rendered);
    const double frequency = PitchDetector::frequency_at(contour, 0.3);
    require(frequency > 250.0 && frequency < 275.0, "PSOLA three-semitone render is outside tolerance");
}

void test_crossfade() {
    AudioBuffer first(48000, 1, 1000);
    AudioBuffer second(48000, 1, 1000);
    for (std::size_t i = 0; i < first.frame_count(); ++i) {
        first.set_sample(i, 0, 0.2F);
        second.set_sample(i, 0, -0.2F);
    }
    const auto joined = CrossfadeProcessor::join(first, second, 200);
    require(joined.frame_count() == 1800, "crossfade output length mismatch");
    require(std::abs(joined.sample(0, 0) - 0.2F) < 1.0e-6F, "crossfade changed first non-overlap region");
    require(std::abs(joined.sample(1799, 0) + 0.2F) < 1.0e-6F, "crossfade changed second non-overlap region");
}

void test_formant_processor_contract() {
    const auto reference = make_sine(48000, 220.0, 0.12);
    const auto shifted = make_sine(48000, 440.0, 0.12);
    const auto untouched = FormantProcessor::preserve(reference, shifted, 0.0);
    require(untouched.frame_count() == shifted.frame_count(), "formant processor changed frame count");
    require(std::abs(untouched.sample(100, 0) - shifted.sample(100, 0)) < 1.0e-6F,
            "zero-strength formant processing should be transparent");
    const auto conservative = FormantProcessor::preserve(reference, shifted, 0.65);
    require(conservative.peak() > 0.17F, "formant processor over-attenuated a sparse tonal signal");
}

void test_quality_metrics() {
    auto audio = make_sine(48000, 440.0, 0.1, 0.4F);
    const auto metrics = AudioQualityAnalyzer::analyze(audio);
    require(metrics.peak > 0.39 && metrics.peak < 0.41, "quality analyzer peak incorrect");
    require(metrics.rms > 0.25 && metrics.rms < 0.31, "quality analyzer RMS incorrect");
    require(metrics.clipped_sample_fraction == 0.0, "quality analyzer reported clipping unexpectedly");
}

void test_wav_roundtrip() {
    const auto path = std::filesystem::temp_directory_path() / "vostudio_engine_roundtrip_test.wav";
    AudioBuffer original(44100, 2, 64);
    original.set_sample(0, 0, 0.5F);
    original.set_sample(1, 1, -0.25F);
    WavFile::write(path, original, 24);
    const auto loaded = WavFile::read(path);
    std::filesystem::remove(path);
    require(loaded.sample_rate() == original.sample_rate(), "WAV sample rate mismatch");
    require(loaded.channel_count() == original.channel_count(), "WAV channel count mismatch");
    require(loaded.frame_count() == original.frame_count(), "WAV frame count mismatch");
    require(std::abs(loaded.sample(0, 0) - 0.5F) < 1.0e-5F, "WAV left sample mismatch");
    require(std::abs(loaded.sample(1, 1) + 0.25F) < 1.0e-5F, "WAV right sample mismatch");
}
}

int main() {
    try {
        test_audio_buffer();
        test_pitch_curve();
        test_pitch_detector();
        test_wsola_duration();
        test_phase_vocoder_duration();
        test_hybrid_pitch_render();
        test_hybrid_semitone_render();
        test_crossfade();
        test_formant_processor_contract();
        test_quality_metrics();
        test_wav_roundtrip();
        std::cout << "All VoStudio DSP engine tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
