/*
  SAWT - Analyzers.h
  Derives ALL raw synthesis material from the uploaded vocal sample:

    * Wavetable  : one pitch-period of the voice, loop-smoothed. Used as the
                   oscillator for vocal-derived drums (nothing synthetic).
    * BreathNoise: an unvoiced/breath segment, used for snare noise, hats,
                   claps and the lo-fi noise layer.

  There is no sine/saw/square generator anywhere in SAWT. Every oscillator is
  a fragment of the singer's own waveform.
*/
#pragma once

#include "../Params.h"

namespace sawt
{

struct VoiceMaterial
{
    juce::AudioBuffer<float> wavetable;    // 1 x 1024, loopable vocal cycle
    juce::AudioBuffer<float> breath;       // 1 x 2048, unvoiced segment (loopable)
    float fundamentalHz = 0.0f;            // F0 the cycle was taken from
    bool  hasCycle = false;
    bool  hasBreath = false;
};

class VoiceAnalyzer
{
public:
    // tone: 0..1  (0 = prefer the lowest/darkest voiced frame, 1 = brightest)
    VoiceMaterial analyze (const SampleData& sample, float tone);
    static constexpr int tableSize = 1024;
    static constexpr int breathSize = 2048;
};

} // namespace sawt
