# Building the VoStudio app

VoStudio's current audio editor is a shared Qt Quick application in `VoStudio.App`. It links the shared C++ DSP library in `VoStudio.Engine`, so the audio processing is not reimplemented per platform.

## Interface features

- Native WAV file picker.
- Pitch shift: -12 to +12 semitones (whole-semitone steps in this alpha).
- Duration: 50% to 200% of source duration.
- Output gain: 0% to 150%.
- Formant-preservation amount: 0% to 100%.
- Time-stretch selector: WSOLA or phase vocoder.
- Asynchronous offline render, 16-bit PCM preview playback, and 24-bit PCM WAV export.

This alpha is a sample processor, not yet a complete vocal synthesizer. It does not yet include piano roll, note/phoneme timing, UTAU voicebank import, `oto.ini`, or the future phonemizer DLL interface. The current pitch UI applies an equal semitone shift to the imported sample; it does not yet infer the desired musical note from a score.

## Local desktop build

Install Qt 6.8.3 including Qt Declarative/Quick, Quick Controls, Quick Dialogs, and Multimedia.

```sh
cmake -S . -B build \
  -DVOSTUDIO_BUILD_APP=ON \
  -DVOSTUDIO_BUILD_TESTS=OFF \
  -DVOSTUDIO_BUILD_TOOLS=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --prefix dist/VoStudio
```

The generated CMake deployment script deploys the Qt runtime and QML imports on Windows, macOS, and Linux.

## Android build

The CI workflow uses:

- Qt 6.8.3 Android `arm64-v8a`.
- JDK 17.
- Android SDK Platform 35 and Build Tools 35.0.1.
- Android NDK 26.1.10909125 (Qt 6.8-compatible NDK).
- `android-actions/setup-android@v4`.

The workflow configures with Qt's `qt-cmake` and packages the APK with:

```sh
cmake --build build-android --target apk --config Release
```

The workflow forces Qt deployment type `Debug` so Android deploy tooling creates a test APK signed with the standard debug key. The resulting APK is uploaded as the `VoStudio-Android-APK-arm64` workflow artifact and can be installed directly for testing. A public release should be signed with a privately managed release keystore. The workflow uses `android-actions/setup-android@v4`; it installs command-line tools and platform-tools, while `sdkmanager` installs the exact SDK platform, Build Tools, and NDK versions required by this Qt build.

## Desktop workflow artifacts

- Windows: self-contained x64 deployment ZIP.
- Linux: self-contained x86_64 deployment tarball (host system glibc and graphics drivers remain platform dependencies).
- macOS: `.app` bundle ZIP for the GitHub runner architecture.

## iOS

The iOS workflow builds an unsigned arm64 device `.app` bundle. iOS does not allow arbitrary unsigned apps to be installed on normal devices. Signing and provisioning are required for physical-device installation; this workflow deliberately does not store signing credentials.

## Build verification note

The portable C++ engine can be built and tested without Qt. The Qt Quick application and platform packaging workflows require Qt SDKs and platform toolchains; they must be validated by the first GitHub Actions run after the source is pushed. This checkout did not have Qt installed, so no APK, desktop installer, or iOS app bundle has been produced locally.
