# Engine Roadmap

## Alpha 0.1 — shared DSP foundation

- [x] C++20 shared engine library
- [x] Floating-point interleaved audio buffer
- [x] WAV PCM/float reader and PCM writer
- [x] YIN pitch and voiced/unvoiced analysis
- [x] WSOLA time stretching
- [x] Phase-vocoder time stretching
- [x] Pitch-mark-anchored voiced pitch shifting
- [x] Conservative spectral-envelope compensation
- [x] Linear/equal-power crossfades
- [x] Basic audio-quality metrics
- [x] Offline render API and CLI test tool
- [x] Synthetic-signal tests

## Alpha 0.2 — recording-driven quality work

- [ ] Test against real UTAU voicebanks and multiple reclist styles
- [ ] Improve pitch-mark selection for breathy, creaky, and noisy vowels
- [ ] Add transient detection and adaptive consonant protection
- [ ] Improve phoneme-local spectral-envelope estimation
- [ ] Add anti-aliasing and sample-rate conversion tests
- [ ] Add render diagnostics report and golden audio comparisons

## Alpha 0.3 — UTAU data integration

- [ ] Voicebank folder importer
- [ ] Robust `oto.ini` parser, including aliases and flags
- [ ] Prefix-map and character configuration support
- [ ] Structured phoneme/alias event model
- [ ] External phonemizer C ABI and platform-specific plugin packaging
- [ ] Render one alias from a selected UTAU bank

## Alpha 0.4 — song editing

- [ ] Basic piano roll
- [ ] Pencil tool for note entry
- [ ] Note timing, MIDI pitch, and lyric editing
- [ ] Render selected note and export WAV
- [ ] Realtime preview and audio-device backends
