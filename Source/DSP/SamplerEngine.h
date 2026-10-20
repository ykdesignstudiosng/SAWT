/*
  SAWT - SamplerEngine.h
  Classic sampler engine: the vocal chop is pitched across the keyboard by
  resampling (MPC-style). Loop is always on, so any short vocal becomes a
  sustaining instrument. Per-voice SVF filter + ADSR + unison.
*/
#pragma once

#include "../Params.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

namespace sawt
{

class SamplerEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void setSample (SamplePtr s) noexcept { sample = std::move (s); }
    void reset();

    void render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi,
                 const MelodicParams& p, float bendSemi);

    std::atomic<float> playPosNorm { 0.0f };
    int activeVoices() const noexcept;

private:
    static constexpr int maxVoices = 16;

    struct Voice
    {
        bool active = false;
        int note = 60;
        float vel = 1.0f;
        double pos = 0.0;
        float speed = 1.0f, speedTarget = 1.0f;
        int envStage = 0;               // 0 idle 1 atk 2 dec 3 sus 4 rel
        float env = 0.0f;
        double stageTime = 0.0;
        float panL = 1.0f, panR = 1.0f;
        float detuneOffset = 0.0f;      // unison detune in semis
        // per-voice SVF (Cytomic), stereo
        float ic1L = 0, ic2L = 0, ic1R = 0, ic2R = 0;
        float g = 0.0f, k = 0.5f;
        float gTarget = 0.0f;
        long long seed = 0;             // allocation order for voice stealing
    };

    void noteOn (int note, float vel, const MelodicParams& p);
    void noteOff (int note);

    Voice* oldestVoice();

    std::array<Voice, maxVoices> voices;
    SamplePtr sample;
    double sr = 48000.0f;
    float lastRatio = 1.0f;
    long long voiceCounter = 0;      // for steal-oldest ordering
};

} // namespace sawt
