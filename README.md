# VoStudio

VoStudio is a cross-platform, sample-based hybrid vocal synthesizer. Its own C++ engine processes recorded WAV material with the goal of clear, natural vocal output and minimal audible processing artifacts.

The project is designed to import existing UTAU voicebanks rather than require a proprietary voicebank container. The UI, rendering engine, project system, audio backends, and phonemizer interface are VoStudio components.

## Repository layout

The five requested application directories are kept exactly as named:

- `VoStudio.Windows/`
- `VoStudio.Linux/`
- `VoStudio.MacOS/`
- `VoStudio.Android/`
- `VoStudio.iOS/`

Shared, platform-independent synthesis code belongs in `VoStudio.Engine/`. Platform applications link to this library instead of copying the engine into each app.

## Engine Alpha features

- Interleaved floating-point audio buffers.
- RIFF/WAVE reader for PCM 8/16/24/32-bit and IEEE float32 WAV.
- PCM 16/24/32-bit WAV writer.
- YIN fundamental-frequency analysis with a voiced/unvoiced contour.
- WSOLA time stretching for waveform-oriented, transient-friendlier processing.
- Phase-vocoder time stretching for sustained material.
- Pitch-mark-anchored grain pitch shifting for voiced material (PSOLA-style).
- Conservative time-aligned spectral-envelope compensation for broad formant preservation.
- Equal-power and linear crossfades.
- Basic audio-quality diagnostics: peak, RMS, DC offset, crest factor, clipping fraction, and large sample discontinuities.
- Offline note/sample rendering API.
- A small command-line WAV renderer for engine experiments.

This is an early DSP implementation, not a finished commercial vocal synthesizer. The algorithms need evaluation and tuning on real voicebank recordings across different pitches, phoneme types, and recording qualities. Exact consonant protection and vowel-specific formant handling require phoneme boundaries and oto/phonemizer metadata, which are future integration stages.

## Build requirements

- CMake 3.21 or newer
- C++20 compiler

Configure and build from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## Command-line render test

After building, the executable is `vostudio-render` (or `vostudio-render.exe` on Windows):

```sh
vostudio-render input.wav output.wav 57 60 0.75 wsola
```

Arguments are input WAV, output WAV, source-root MIDI note, target MIDI note, output duration in seconds, and optional time-stretch mode (`wsola` or `phase`). The renderer exports PCM 24-bit WAV. For best experiments, start with clean, sustained, monophonic vowel recordings.

## Rendering pipeline

```text
Recorded WAV
  -> optional source-region selection
  -> WSOLA or phase-vocoder duration adjustment
  -> YIN F0 / voiced-region analysis
  -> pitch-mark-anchored voiced pitch shifting
  -> spectral-envelope compensation
  -> gain / floating-point render buffer
  -> WAV export or future platform audio output
```

The render engine is deliberately phoneme-agnostic. A future UTAU importer will parse the voicebank and `oto.ini`; a separately supplied phonemizer will turn lyrics into aliases/phoneme events. Those layers will provide correctly trimmed samples and phoneme timings to this engine.

## Phonemizer portability

A Windows `.dll` is not a universal plugin binary. A stable C ABI can be shared, but each platform needs a compatible build (`.dll`, `.so`, or Apple-compatible binary). iOS restricts arbitrary runtime code loading, so an iOS phonemizer will need to be included and signed as part of the app or linked statically. The future phonemizer is not required to build the current engine.

## Development order

1. Tune and validate the DSP engine with real recorded WAV samples.
2. Add UTAU voicebank folder import and robust `oto.ini` parsing.
3. Integrate the external phonemizer API and phoneme event model.
4. Render one phoneme/alias through the full voicebank pipeline.
5. Add the basic piano roll with a pencil tool.
6. Add realtime playback, mixing, editing, and platform-specific packaging.

## Project status

Engine Alpha. Windows/Linux/macOS CI builds and tests the shared library. Android and iOS workflows cross-compile the shared engine library; they do not yet package complete mobile applications. No voicebank importer, native song editor, or production phonemizer is implemented yet.

## GitHub upload

The connected GitHub integration currently rejects repository content writes. See [`docs/GITHUB_UPLOAD.md`](docs/GITHUB_UPLOAD.md) for commands to push this package to `sirwltz00/VoStudio` using your normal GitHub credentials.
