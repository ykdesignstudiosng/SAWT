/*
  SAWT - FormantEngine.h
  PSOLA-based melodic engine: the voice is pitched WITHOUT losing its human
  timbre (formants preserved). Each voice owns a PSOLA shifter fed by the
  vocal sample read at unity speed. Polyphony limited to 4 (CPU).
*/
#pragma once

#include "../Params.h"
#include "PSOLA.h"
#include <array>
#include <vector>
#include <cmath>

namespace sawt
{

class FormantEngine
{
public:
    static constexpr int maxVoices = 4;

    void prepare (double sampleRate, int maxBlockSize);
    void setSample (SamplePtr s) noexcept { sample = std::move (s); }
    void reset();

    void render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi,
                 const MelodicParams& p, float bendSemi);

    int latency() const noexcept { return prepared ? voices[0].ps.latency() : 0; }
    int activeVoices() const noexcept;

private:
    struct Voice
    {
        bool active = false;
        int note = 60;
        float vel = 1.0f;
        double readPos = 0.0;
        int envStage = 0;
        float env = 0.0f;
        double stageTime = 0.0;
        float ratio = 1.0f;
        long long seed = 0;
        PSOLA ps;
    };

    void noteOn (int note, float vel);
    void noteOff (int note);
    Voice* oldestVoice();

    std::array<Voice, maxVoices> voices;
    SamplePtr sample;
    std::vector<float> scratch;
    double sr = 48000.0;
    bool prepared = false;
    long long voiceCounter = 0;
};

} // namespace sawt
