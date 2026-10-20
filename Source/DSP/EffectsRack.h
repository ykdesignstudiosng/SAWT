/*
  SAWT - EffectsRack.h
  Master effects chain. Everything here PROCESSES the vocal signal - it never
  introduces an instrumental source. Even the lo-fi noise layer is the
  singer's own breath segment, looped.

  Order: EQ3 -> Compressor -> Drive -> Chorus -> Delay -> Reverb -> LoFi -> Width
*/
#pragma once

#include "../Params.h"
#include <juce_dsp/juce_dsp.h>
#include <array>

namespace sawt
{

class EffectsRack
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // breath layer: 1-channel loopable buffer of the singer's breath (may be null)
    void setBreathLayer (std::shared_ptr<juce::AudioBuffer<float>> breath);
    void update (const FXParams& p, double sampleRate);

    void process (juce::AudioBuffer<float>& buffer);

private:
    void updateEQ (double sr);
    void updateReverb();
    void updateDelay (double sr);

    // EQ (per channel)
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                   juce::dsp::IIR::Coefficients<float>> eqLow, eqMid, eqHigh;

    // compressor (stereo-native)
    juce::dsp::Compressor<float> compressor;

    // drive (stereo-native)
    juce::dsp::WaveShaper<float> shaper;

    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;

    // custom stereo feedback delay
    struct Delay
    {
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> lineL { 1 << 16 };
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> lineR { 1 << 16 };
        float fb = 0.3f, mix = 0.15f;
        int timeSamples = 12000;
    } delay;

    // lo-fi
    float phase = 0.0f;               // wow LFO phase
    juce::SpinLock breathLock;        // protects the shared_ptr swap below
    std::shared_ptr<juce::AudioBuffer<float>> breathPtr;
    int breathPos = 0;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> wowLine { 1 << 12 };

    FXParams params;
    bool eqDirty = true, reverbDirty = true;
};

} // namespace sawt
