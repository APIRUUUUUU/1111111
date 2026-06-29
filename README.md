# Crystal Voice — VST3 Prototype

A Windows-first VST3 vocal plug-in prototype for **FL Studio live singing**.

**Concept:** Make a clear, transparent VTuber vocal chain easy to operate during a singing stream. The main panel gives beginners only the controls they need; `DETAIL` exposes the remaining mix controls.

> Status: Source-code prototype. It is a working JUCE/C++ project, but it is **not yet a release-quality replacement for commercial pitch correction or harmony products**. The custom pitch shifter is intentionally lightweight for live use. Test it carefully with your own voice before using it in a broadcast.

## Included features

- Natural-style monophonic pitch correction with adjustable correction amount and speed
- Key + Major / Minor scale selection
- Diatonic harmony modes: 3rd above, 3rd below, both thirds, and double tracking
- De-esser, air/brightness, compressor, reverb, and delay
- Four factory presets: Crystal Clear / Idol Pop / Soft Ballad / Pop Correction
- `SAVE PRESET` and `LOAD PRESET` custom files (`.cvpreset`)
- Retro pink + cyan GUI
- VST3 output for FL Studio and a standalone build target for debugging

## Important audio limitations for this first build

- Pitch detection is designed for **one singer at a time**. It will not reliably analyse chords, crowded backing tracks, or very noisy microphones.
- The harmony engine chooses scale-aware thirds from the selected key and Major/Minor mode. It does **not** read chord changes from MIDI yet; therefore, some songs with borrowed chords or complex harmony will need a later MIDI-chord mode.
- The pitch shift uses short overlapping grains. It is intentionally compact for monitoring, but extreme note correction can produce audible texture. Keep `TUNE` in the 45–75% range for the most natural result.
- The plug-in reports its internal processing delay to the host. FL Studio should compensate it on mixer tracks, but direct monitoring always includes your audio-interface buffer plus the plug-in's own short buffer.

## Build prerequisites (Windows)

1. **Visual Studio 2022 Community** or **Build Tools for Visual Studio 2022**
   - Install the `Desktop development with C++` workload.
2. **CMake 3.22 or newer**.
3. **Git for Windows**.
4. An Internet connection on the first build. CMake downloads the pinned JUCE 8.0.12 source into the build folder.

The project deliberately uses CMake's `FetchContent`, so the ZIP does not redistribute JUCE or Steinberg SDK files.

## Build the VST3

1. Extract this ZIP anywhere writable, for example `C:\VSTProjects\CrystalVoice`.
2. Double-click `BUILD_VST3.bat`.
3. On a successful build, locate:

```text
build\CrystalVoice_artefacts\Release\VST3\Crystal Voice.vst3
```

4. Copy the **entire** `Crystal Voice.vst3` folder to:

```text
C:\Program Files\Common Files\VST3\
```

5. Open FL Studio:
   - `Options` → `Manage plugins`
   - Click `Find installed plugins`
   - Add **Crystal Voice** to a Mixer insert receiving the microphone input.

## Suggested FL Studio live route

```text
Microphone / audio interface input
  → FL Studio Mixer insert
  → Crystal Voice (VST3)
  → FL Studio master or a dedicated streaming bus
  → OBS audio capture / virtual audio route
```

For low-latency monitoring:

- Start around a 128-sample ASIO buffer at 48 kHz.
- Disable CPU-heavy plug-ins on the live vocal insert.
- Put Crystal Voice early in the vocal chain; use only light mastering after it.
- Avoid monitoring both direct hardware input and FL Studio output at once, or you will hear doubling.

## Basic live workflow

1. Select the song key and `Major` or `Minor`.
2. Start with `Crystal Clear`.
3. Set `TUNE` around 55–70% and adjust `DE-ESS` until `s` / `sh` sounds are controlled but not dull.
4. Pick a harmony mode for the chorus and bring in `HARMONY` gradually.
5. Add a small amount of `SPACE`.
6. Use `SAVE PRESET` to create a singer-specific `.cvpreset` file.

## Project layout

```text
CrystalVoice_VST3_Prototype/
├─ BUILD_VST3.bat
├─ CMakeLists.txt
├─ README.md
└─ Source/
   ├─ PluginProcessor.h/.cpp   # parameters, pitch/harmony/DSP engine
   └─ PluginEditor.h/.cpp      # retro GUI, factory presets, custom preset I/O
```

## Next upgrades recommended

1. MIDI chord-follow mode for harmony correctness in songs with chord changes.
2. Formant correction to improve harmony quality, especially with large upward shifts.
3. Proper transient-aware pitch shifting and more robust YIN/MPM pitch detection.
4. Input/output device and latency diagnostic page.
5. Preset metadata: singer name, song name, key, and notes.
6. A signed installer + `pluginval` validation before public free release.

## Third-party notes

- This project fetches JUCE 8.0.12 at build time. Review JUCE's licence and select the licence path appropriate to the intended distribution model before publishing binaries.
- VST is a trademark of Steinberg Media Technologies GmbH. Follow Steinberg's current VST trademark and SDK licensing guidance before public distribution.

---

## ビルド済みVST3をGitHub Actionsで作る

Windows PCに開発環境を入れずに、GitHubのWindowsビルド環境から実際のVST3を生成できます。
手順は `GITHUB_ACTIONS_BUILD.md` を参照してください。
