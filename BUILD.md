# Building SAWT

## Requirements

* **CMake 3.22+** (4.x works)
* **C++17 toolchain**: MSVC 2022 (Windows), Xcode 15+ (macOS), GCC 11+/Clang 14+ (Linux)
* **Ninja** or Make
* JUCE 8.0.15 — auto-downloaded by CMake (FetchContent) or vendored at `lib/JUCE`
* clap-juce-extensions — auto-downloaded (set `-DSAWT_BUILD_CLAP=OFF` to skip)

## Linux packages

```bash
sudo apt install build-essential cmake ninja-build pkg-config \
  libasound2-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev libfreetype-dev \
  libfontconfig1-dev libglu1-mesa-dev
```

## Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSAWT_BUILD_CLAP=ON
cmake --build build --config Release --parallel
```

### Without internet (vendored JUCE)

Clone JUCE once and place it in the tree — CMake picks it up automatically:

```bash
git clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE.git lib/JUCE
git clone --depth 1 https://github.com/free-audio/clap-juce-extensions.git lib/clap-juce-extensions
```

## Artefact locations

```
build/SAWT_artefacts/Release/VST3/SAWT.vst3
build/SAWT_artefacts/Release/CLAP/SAWT.clap
build/SAWT_artefacts/Release/AU/SAWT.component        (macOS only)
build/SAWT_artefacts/Release/Standalone/SAWT          (when audio devices available)
```

## Install

* **Windows** — copy `SAWT.vst3` (the whole folder) to
  `C:\Program Files\Common Files\VST3\`, or the `.clap` to
  `C:\Program Files\Common Files\CLAP\`.
* **macOS** — copy `SAWT.vst3` to `/Library/Audio/Plug-Ins/VST3/`,
  `SAWT.component` to `/Library/Audio/Plug-Ins/Components/`,
  `SAWT.clap` to `/Library/Audio/Plug-Ins/CLAP/`.
  Unsigned builds: run `xattr -cr <plugin>` after copying, or sign with your
  Developer ID and notarise.
* **Linux** — copy `SAWT.vst3/` to `~/.vst3/`, `SAWT.clap` to `~/.clap/`.

## FL Studio notes

* Add the VST3 folder to FL's plugin search paths, then rescan.
* SAWT receives MIDI on **all channels** for the melodic engine and on
  **channel 10** (notes C1–B1) for drums. In the piano roll you can set the
  note/channel colour per pattern.
* To use **live mic**: place SAWT on an *audio track's* slot (or route a
  mic mixer track into it), enable **MONITOR**, and enable input on the
  mixer track. **CAPTURE MIC** records up to 30 s directly into the sampler.
* Latency: when the Formant engine or SNAP is active SAWT reports ~37 ms
  compensation to the host automatically.

## Headless Linux hosts without ALSA

The standalone target is skipped automatically when ALSA headers are absent
(plugins do not need them — the host provides audio). CI machines that install
`libasound2-dev` build the standalone too.
