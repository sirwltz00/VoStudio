# VoStudio DSP Engine Alpha

## Goals

The engine processes recorded samples rather than generating a voice from a proprietary voicebank model. It should retain source-specific tone and articulation while changing note pitch and duration. Avoiding processing artifacts is a measured engineering target, not a guarantee that all source noise can be removed.

## Implemented processing blocks

### Audio I/O and buffers

`AudioBuffer` stores interleaved floating-point PCM. `WavFile` reads RIFF/WAVE PCM 8-, 16-, 24-, and 32-bit audio plus IEEE float32 audio, and writes PCM 16-, 24-, or 32-bit WAV files.

### YIN pitch analysis

`PitchDetector` estimates F0 using the YIN difference function and cumulative mean normalized difference. It returns timestamped voiced/unvoiced frames with confidence. The current implementation analyzes a mono downmix, then exposes one contour for all channels.

The detector is a general-purpose starting point. It can make octave errors or lose voicing on noisy, breathy, creaky, polyphonic, or strongly vibrato-heavy samples. It must be tuned with real UTAU recordings.

### WSOLA time stretching

`TimeStretcher::wsola` changes duration by selecting waveform grains around expected source positions and searching for a locally similar overlap. This is the default mode and is intended to retain transient detail better than a basic phase vocoder.

It is not yet phoneme-aware. Once UTAU `oto.ini` timing and phonemizer events are connected, consonant regions can receive different stretch limits from sustained vowels.

### Phase-vocoder time stretching

`TimeStretcher::phase_vocoder` uses an STFT, phase propagation, inverse transforms, and overlap-add normalization. It is an alternative for sustained material. The initial implementation does not yet include transient phase locking, multi-resolution processing, or perceptual masking controls; WSOLA may sound better on consonants and percussive articulation.

### Pitch-mark-anchored pitch shifting

`PSOLAPitchShifter` uses the YIN contour to find voiced regions and positive-going waveform crossings as candidate pitch marks. It creates local pitch-synchronous grains and places them at target-period intervals. Unvoiced regions are passed through instead of having their noise spectrum transposed.

The current grain implementation performs a local period warp around pitch marks to support pitch changes while keeping the note duration fixed. It is PSOLA-style rather than a full textbook TD-PSOLA implementation. It needs listening tests on natural vowels, diphthongs, breathy voices, and irregular phonation.

### Spectral-envelope / formant compensation

`FormantProcessor` compares time-aligned reference and transposed STFT frames, smooths their log-magnitude spectra, and applies a bounded correction to broad spectral-envelope differences. This is a conservative first pass, not a complete vocal-tract model or a phoneme-aware formant shifter.

The next improvement should compute envelope correction per phoneme and avoid strong correction on unvoiced consonants. Avoid over-correcting: excessive envelope matching can create a hollow or phasey sound.

### Crossfades

`CrossfadeProcessor` concatenates buffers with linear or equal-power overlap. This will be used later when connecting consonant/vowel aliases into continuous phoneme sequences.

### Diagnostics

`AudioQualityAnalyzer` reports peak, RMS, DC offset, crest factor, clipped-sample fraction, largest adjacent-sample jump, and count of large jumps. These metrics help flag issues but cannot replace listening tests or a proper perceptual quality evaluation.

## Current render API

`HybridSynthesisEngine::render` accepts a source audio buffer and `RenderParameters`:

- source-root MIDI note
- target MIDI note
- optional source start time
- optional target duration
- output gain
- optional pitch curve in cents
- WSOLA or phase-vocoder time-stretch selection

The returned buffer keeps the source sample rate and channel count. Rendering is offline and deterministic for the same input and parameters.

## Known limitations

- No UTAU folder importer or `oto.ini` parser is connected yet.
- No phonemizer plugin is loaded yet; the C ABI draft is an interface proposal only.
- No consonant/vowel segmentation from alias metadata yet.
- No phrase-level pitch tracking, vibrato synthesis, or multi-note legato engine yet.
- No realtime, low-latency device callback yet.
- No automatic denoiser. Source recording noise and processing artifacts are different problems.
- The renderer currently clamps neither internal floating-point output nor mix headroom. Fixed-point WAV export clips samples outside the representable [-1, 1] interval, so select an appropriate gain before PCM export.
- Pitch-shifting and envelope compensation are experimental; test with real recordings before treating output as production-ready.

## Validation strategy

Run `ctest` after every DSP change. The tests include synthetic sine waves to check F0, time-stretch duration and pitch behavior, PSOLA-style octave transposition, crossfade lengths, buffer operations, basic formant processor contracts, quality metrics, and WAV round trips.

Synthetic signals are necessary unit tests but insufficient proof of singing quality. A later sample corpus should include voiced vowels, plosives, fricatives, sibilants, breathy and creaky phonation, multiple F0 ranges, different sample rates, and stereo/mono files.
