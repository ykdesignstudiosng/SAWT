/*
  SAWT - GranularEngine.h
  Scatter-granular texture engine. Tiny grains of the vocal are re-arranged
  into evolving pads / choir-like tones. Position (Scan/Freeze/Random),
  grain size, rate, drift, jitter, per-grain random pan & detune.
*/
#pragma once

#include "../Params.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

namespace sawt
{

class GranularEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void setSample (SamplePtr s) noexcept { sample = std::move (s); }
    void reset();

    void render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi,
                 const MelodicParams& p, float bendSemi);

    int activeVoices() const noexcept;

private:
    static constexpr int maxVoices = 8;
    static constexpr int maxGrains = 64;

    struct Grain
    {
        bool active = false;
        double readPos = 0.0;
        float speed = 1.0f;
        double age = 0.0;
        float len = 0.08f;
        float panL = 0.7f, panR = 0.7f;
        float amp = 1.0f;
    };

    struct Voice
    {
        bool active = false;
        int note = 60;
        float vel = 1.0f;
        double basePos = 0.0;       // normalised 0..1 * sampleLen computed at use
        double scanPos = 0.0;       // absolute sample index
        double spawnAcc = 0.0;
        int envStage = 0;
        float env = 0.0f;
        double stageTime = 0.0;
        float ratio = 1.0f;
        long long seed = 0;
    };

    void noteOn (int note, float vel, const MelodicParams& p);
    void noteOff (int note);
    void spawnGrain (Voice& v, const MelodicParams& p, double sampleLen);
    Voice* oldestVoice();

    std::array<Voice, maxVoices> voices;
    std::array<Grain, maxGrains> grains;
    SamplePtr sample;
    double sr = 48000.0;
    long long voiceCounter = 0, grainCounter = 0;
    juce::Random rng;
};

} // namespace sawt
