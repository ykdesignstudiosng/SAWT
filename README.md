# SAWT — صوت — The Voice-Only Synthesizer

**Every sound you hear was once a voice.**

SAWT is a cross-platform instrument plugin (VST3 / AU / CLAP) built around a
single design law: **no instrumental oscillator is ever used as a sound
source.** You supply vocal recordings — a hummed note, a chanted vowel, a
beatboxed phrase, even a breath — and SAWT derives *everything* from them:

* **Melodic engines** — a vocal chop becomes a playable keyboard instrument
  (Sampler), a cloud of evolving vocal grains (Granular), a pitch-tracked
  voice that keeps its human timbre at any note (Formant), or a morph between
  them (XY).
* **Drums from the mouth** — a beatbox phrase is auto-sliced into an 8-pad kit;
  or SAWT shapes *808-style* kicks, snares, hats and claps out of the singer's
  own waveform cycle and breath segments.
* **Live input** — route a microphone into the plugin, sing/beatbox in real
  time, capture takes straight into the sampler, and "snap" your pitch onto
  the notes you hold on a MIDI keyboard.
* **Full FX rack** — EQ, compressor, drive, chorus, delay, reverb, lo-fi
  (with a *breath* noise layer taken from your own recording) and stereo
  width. Every effect *processes* the voice; none of them replace it.

This design exists to serve musicians who, following their faith, avoid
musical instruments — the a cappella / nasheed tradition — while still wanting
a modern, playable studio instrument. It is equally a creative tool for
anyone who loves the sound of the human voice.

---

## Status

| Format    | Windows | macOS | Linux |
|-----------|---------|-------|-------|
| VST3      | ✅      | ✅    | ✅ (built & verified in CI) |
| AU        | —       | ✅    | —     |
| CLAP      | ✅      | ✅    | ✅ (built & verified) |
| Standalone| ✅      | ✅    | ✅ (requires ALSA) |

The codebase compiles cleanly on Linux (GCC 14 / CMake / Ninja, JUCE 8.0.15);
Windows and macOS binaries are produced by the included GitHub Actions
workflow (`.github/workflows/build.yml`) — push the repo to GitHub and
download the artefacts, or build locally per [BUILD.md](BUILD.md).

## Quick start (producer workflow)

1. **Load a voice** — drop any WAV/AIFF/FLAC/OGG/MP3 of a vocal into the
   *Voice Engine* drop zone. A sustained vowel ("aaah") works best to begin.
2. **Choose an engine** — *Sampler* for MPC-style chops, *Granular* for pads
   and choir textures, *Formant* for a lead voice that stays human at any
   pitch, *XY* to morph.
3. **Play** — C4 (MIDI 60) plays the sample at its original pitch.
4. **Drums** — drop a beatbox phrase (or any rhythmic vocal) into the *Vocal
   Drums* slot, press **AUTO SLICE**, and play pads from MIDI channel 10
   (notes C1–B1) or by clicking. Switch the source to **Vocal 808s** to
   synthesize drums purely from the voice's waveform and breath.
5. **Live mic** — enable *Monitor*, route your DAW's audio input into the
   plugin, and optionally arm **CAPTURE MIC** to record a take that
   immediately becomes your melodic sample. **SNAP** pitch-corrects the live
   input onto the held MIDI note.

## A note on the design constraint

Nothing in the signal path generates audio that did not come from your
recordings. The drum "oscillators" are single pitch-periods extracted from
your voice; the lo-fi noise layer is your breath; the reverb, delay, chorus,
compression and saturation only transform what you gave them. Presets store
parameter state *and the path to the samples* so a session can be fully
reconstructed.

## Repository layout

```
Source/
  Params.h/.cpp          Parameter registry + per-block snapshots
  PluginProcessor.*      Audio thread: MIDI, engine routing, FX, live input
  PluginEditor.*         UI layout (1160×740 design, resizable)
  Core/                  Sample loading, transient slicer, voice analyser, presets
  DSP/                   Sampler, Granular, PSOLA formant engine, drum synth,
                         effects rack, pitch detector
  UI/                    Theme, look-and-feel, widgets, spectrum, drum pads
```

## License

SAWT source is released under the MIT License (see [LICENSE](LICENSE)).
It builds against [JUCE 8](https://juce.com) (AGPLv3/commercial dual licence)
and [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions) (MIT).
