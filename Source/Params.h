/*
  SAWT - The Voice-Only Synthesizer
  Params.h - Central parameter registry + per-block snapshot structs.

  Design rule: every audible sound originates from a user-supplied vocal
  sample. Parameters only shape/process that vocal material.
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

namespace sawt
{

//==============================================================================
// Parameter IDs (stable - never rename, presets depend on these)
namespace pid
{
    // Global
    constexpr const char* master        = "master";

    // Melodic part
    constexpr const char* engine        = "mEngine";      // Sampler / Granular / Formant / XY
    constexpr const char* tune          = "mTune";        // semitones
    constexpr const char* fine          = "mFine";        // cents
    constexpr const char* glide         = "mGlide";
    constexpr const char* formant       = "mFormant";     // semitones of formant shift
    constexpr const char* att           = "mAttack";
    constexpr const char* dec           = "mDecay";
    constexpr const char* sus           = "mSustain";
    constexpr const char* rel           = "mRelease";
    constexpr const char* cutoff        = "mCutoff";
    constexpr const char* reso          = "mReso";
    constexpr const char* envAmt        = "mEnvAmt";
    constexpr const char* unison        = "mUnison";
    constexpr const char* detune        = "mDetune";
    constexpr const char* spread        = "mSpread";
    constexpr const char* level         = "mLevel";
    constexpr const char* xyX           = "mXYx";         // Sampler <-> Granular morph
    constexpr const char* xyY           = "mXYy";         // timbre / drift dimension
    // Granular detail
    constexpr const char* gSize         = "gSize";
    constexpr const char* gRate         = "gRate";
    constexpr const char* gPos          = "gPos";
    constexpr const char* gDrift        = "gDrift";
    constexpr const char* gJitter       = "gJitter";
    constexpr const char* gMode         = "gMode";        // Scan / Freeze / Random

    // Drums
    constexpr const char* dSource       = "dSource";      // Slices / Vocal 808s
    constexpr const char* dPunch        = "dPunch";
    constexpr const char* dDrive        = "dDrive";
    constexpr const char* dTone         = "dTone";
    constexpr const char* dLevel        = "dLevel";
    constexpr const char* dPadTune      = "dPadTune";     // prefix with pad index at runtime
    constexpr const char* dPadDec       = "dPadDec";
    constexpr const char* dPadLvl       = "dPadLvl";

    // Live input
    constexpr const char* inGain        = "inGain";
    constexpr const char* inMonitor     = "inMonitor";
    constexpr const char* inSnap        = "inSnap";

    // FX rack
    constexpr const char* fxOn          = "fxOn";
    constexpr const char* eLow          = "eLow";
    constexpr const char* eMid          = "eMid";
    constexpr const char* eHigh         = "eHigh";
    constexpr const char* eMidFreq      = "eMidFreq";
    constexpr const char* cThresh       = "cThresh";
    constexpr const char* cRatio        = "cRatio";
    constexpr const char* sDrive        = "sDrive";
    constexpr const char* chRate        = "chRate";
    constexpr const char* chDepth       = "chDepth";
    constexpr const char* chMix         = "chMix";
    constexpr const char* dlTime        = "dlTime";
    constexpr const char* dlFb          = "dlFb";
    constexpr const char* dlMix         = "dlMix";
    constexpr const char* rvSize        = "rvSize";
    constexpr const char* rvDamp        = "rvDamp";
    constexpr const char* rvMix         = "rvMix";
    constexpr const char* lfBits        = "lfBits";
    constexpr const char* lfWow         = "lfWow";
    constexpr const char* lfBreath      = "lfBreath";
    constexpr const char* width         = "width";
}

//==============================================================================
enum class EngineType : int { Sampler = 0, Granular = 1, Formant = 2, XY = 3 };
enum class GrainMode   : int { Scan = 0, Freeze = 1, Random = 2 };
enum class DrumSource  : int { Slices = 0, VocalSynth = 1 };

//==============================================================================
struct MelodicParams
{
    EngineType engine      = EngineType::Sampler;
    float tune = 0.0f, fine = 0.0f, glide = 0.05f, formant = 0.0f;
    float att = 0.01f, dec = 0.3f, sus = 0.7f, rel = 0.4f;
    float cutoff = 18000.0f, reso = 0.7f, envAmt = 0.0f;
    int   unison = 1;
    float detune = 12.0f, spread = 0.7f, level = 0.9f;
    float xyX = 0.35f, xyY = 0.5f;
    float gSize = 0.08f, gRate = 18.0f, gPos = 0.25f, gDrift = 0.3f, gJitter = 0.2f;
    GrainMode gMode = GrainMode::Scan;
};

struct DrumPadParams { float tune = 0.0f, decay = 0.5f, level = 0.8f; };

struct DrumParams
{
    DrumSource source = DrumSource::Slices;
    float punch = 0.35f, drive = 0.2f, tone = 0.5f, level = 0.85f;
    DrumPadParams pad[8];
};

struct LiveParams
{
    float inGain = 1.0f;
    bool  monitor = false;
    bool  snap = false;
};

struct FXParams
{
    bool  on = true;
    float eLow = 0.0f, eMid = 0.0f, eHigh = 0.0f, eMidFreq = 1200.0f;
    float cThresh = -18.0f, cRatio = 3.0f;
    float sDrive = 0.15f;
    float chRate = 1.2f, chDepth = 0.35f, chMix = 0.25f;
    float dlTime = 0.28f, dlFb = 0.3f, dlMix = 0.15f;
    float rvSize = 0.5f, rvDamp = 0.5f, rvMix = 0.2f;
    float lfBits = 16.0f, lfWow = 0.1f, lfBreath = 0.05f;
    float width = 1.0f;
};

//==============================================================================
// A loaded vocal sample, shared between threads via shared_ptr swap.
struct SampleData
{
    juce::AudioBuffer<float> buffer;      // original channels kept
    double sampleRate = 48000.0;
    juce::String name;
    juce::String filePath;

    // mono view for analysis / granular (filled by the loader)
    juce::AudioBuffer<float> mono;

    bool isValid() const noexcept { return buffer.getNumSamples() > 0; }
    int  numSamples() const noexcept { return buffer.getNumSamples(); }
    int  numChannels() const noexcept { return buffer.getNumChannels(); }
};

using SamplePtr = std::shared_ptr<const SampleData>;

//==============================================================================
// Builds and owns the APVTS layout; converts atomics to per-block snapshots.
class ParamStore
{
public:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    explicit ParamStore (juce::AudioProcessorValueTreeState& s) : state (s) {}

    void cachePointers();                      // call once after state is built
    MelodicParams melodic() const;             // thread-safe snapshots
    DrumParams    drums()    const;
    LiveParams    live()     const;
    FXParams      fx()       const;
    float         master()   const;

private:
    juce::AudioProcessorValueTreeState& state;
    std::map<juce::String, std::atomic<float>*> cache;
};

//==============================================================================
// Pad names suggested by the transient classifier
constexpr const char* padNames[8] =
    { "KICK", "SNARE", "HAT C", "HAT O", "PERC 1", "PERC 2", "CLAP", "RIM" };

constexpr int drumMidiBase = 36;     // C1 .. B1 on MIDI channel 10
constexpr int drumMidiChannel = 10;

constexpr int numSpectrumBands = 48; // analyser resolution

} // namespace sawt
