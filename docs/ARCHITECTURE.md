# Architecture

## Shared engine

`VoStudio.Engine` is platform-independent C++20. It has no UI framework or platform audio dependency. Its API exposes audio buffers, WAV I/O, pitch analysis, time stretching, pitch shifting, spectral-envelope compensation, crossfades, diagnostics, and offline rendering.

## Platform applications

The following directories are reserved for platform-specific entry points, UI, packaging, and audio-device adapters:

- `VoStudio.Windows`
- `VoStudio.Linux`
- `VoStudio.MacOS`
- `VoStudio.Android`
- `VoStudio.iOS`

They are intentionally not referenced by the root CMake build yet. The shared engine is built first; platform targets will be wired in when application shells are introduced.

## Planned full render flow

`Piano-roll notes -> phonemizer -> phoneme/alias events -> UTAU voicebank samples + oto.ini timing -> sample/phoneme preparation -> WSOLA/phase-vocoder duration processing -> voiced PSOLA-style pitch processing -> spectral-envelope compensation -> crossfade/mix -> WAV/device output`

The phonemizer is an external language-dependent component. The engine consumes structured phoneme events; it should not hardcode Japanese, English, Spanish, Mandarin, Korean, or user-created language rules.

## Module boundaries

- `Audio/`: buffers, decoders, encoders, sample-rate conversion.
- `Analysis/`: F0, voicing, transient, and noise analysis.
- `DSP/`: time stretching, pitch shifting, formant processing, and crossfades.
- `Rendering/`: render orchestration and future phoneme/note render events.
- `Diagnostics/`: objective audio checks.

## Noise and quality

The engine distinguishes source-recording noise from processing artifacts. Resampling cannot remove room noise already present in a WAV recording. Diagnostics can flag clipping, DC offset, large sample jumps, and related problems, but audio-quality assessment also requires careful listening.
