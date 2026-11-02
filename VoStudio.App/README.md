# VoStudio App

This Qt Quick application is the first cross-platform user interface for the VoStudio DSP engine.

Current alpha functionality:

- Select a WAV recording using the native platform file picker.
- Change pitch in semitones (-12 to +12).
- Change duration (50% to 200%).
- Adjust output gain (0% to 150%).
- Adjust formant-preservation strength (0% to 100%).
- Select WSOLA or phase-vocoder time stretching.
- Render asynchronously, preview the result, and export a 24-bit PCM WAV.

The app is intentionally independent of the future piano roll, UTAU voicebank importer, `oto.ini` parser, and phonemizer plugin loader. Those will connect to the same engine API later.

## Build

Install Qt 6.8.3 with the Qt Quick/QML, Quick Controls, Quick Dialogs, and Multimedia modules, then build from the repository root:

```sh
cmake -S . -B build -DVOSTUDIO_BUILD_APP=ON -DVOSTUDIO_BUILD_TESTS=OFF -DVOSTUDIO_BUILD_TOOLS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Android packaging uses `cmake --build build-android --target apk`; CI uses Qt debug deployment signing so the test APK can be installed. Desktop deployment uses `cmake --install build` after building.

Android file selection uses Qt's Android Storage Access Framework integration through the native file dialog and QFile content-URI support. iOS CI produces an unsigned arm64 device app bundle; it must be signed with an Apple development/distribution identity and provisioning profile before installation on a physical iPhone or iPad.
