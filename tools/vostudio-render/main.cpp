#include "VoStudio/Engine/HybridSynthesisEngine.hpp"
#include "VoStudio/Engine/WavFile.hpp"

#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 6 || argc > 7) {
        std::cerr << "Usage: vostudio-render <input.wav> <output.wav> <source-midi> <target-midi> <duration-seconds> [wsola|phase]\n";
        return 2;
    }
    try {
        const auto input = VoStudio::Engine::WavFile::read(argv[1]);
        VoStudio::Engine::RenderParameters parameters;
        parameters.source_root_midi_note = std::stoi(argv[3]);
        parameters.target_midi_note = std::stoi(argv[4]);
        parameters.output_duration_seconds = std::stod(argv[5]);
        if (argc == 7) {
            const std::string mode(argv[6]);
            if (mode == "phase") parameters.time_stretch.algorithm = VoStudio::Engine::TimeStretchAlgorithm::PhaseVocoder;
            else if (mode != "wsola") throw std::invalid_argument("Stretch mode must be 'wsola' or 'phase'");
        }
        const auto rendered = VoStudio::Engine::HybridSynthesisEngine::render(input, parameters);
        VoStudio::Engine::WavFile::write(argv[2], rendered, 24);
        std::cout << "Rendered " << rendered.frame_count() << " frames at "
                  << rendered.sample_rate() << " Hz to " << argv[2] << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "VoStudio render failed: " << error.what() << '\n';
        return 1;
    }
}
