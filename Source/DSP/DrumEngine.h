/*
  SAWT - DrumEngine.h
  8-pad drum module. Two sources, both strictly vocal:
    * Slices     : percussive segments found by the transient slicer
    * Vocal 808s : DrumSynth voices shaped from the singer's cycle + breath
  Pads receive MIDI channel 10, notes C1..B1 (36..43), plus UI clicks.
  Closed/open hat choke each other.
*/
#pragma once

#include "../Params.h"
#include "../Core/Slicer.h"
#include "../Core/Analyzers.h"
#include "DrumSynth.h"
#include <array>
#include <vector>

namespace sawt
{

class DrumEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // message-thread APIs
    void setSlices (const SampleData& sample, const std::vector<DrumSlice>& cuts);
    void setMaterial (const VoiceMaterial& m) noexcept { synth.setMaterial (m); }
    void triggerPad (int pad, float velocity) noexcept;

    // slice ranges normalised to the drum sample length (for UI markers)
    std::vector<std::pair<float, float>> sliceNorms() const;

    void render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi,
                 const DrumParams& p);

    std::atomic<int> sliceCount { 0 };
    std::atomic<int> lastTriggered { -1 };
    std::array<std::atomic<float>, 8> padFlash;

private:
    static constexpr int maxPadVoices = 8;
    std::vector<std::pair<int, float>> pendingPads;

    struct PadVoice
    {
        bool active = false;
        int pad = 0;
        float vel = 1.0f;
        double pos = 0.0;
        double t = 0.0;
        float decay = 0.5f;
    };

    void startSlice (int pad, float vel, const DrumParams& p);
    void chokeHats (int pad);

    std::array<PadVoice, maxPadVoices> padVoices;
    std::array<juce::AudioBuffer<float>, 8> slices;
    std::array<std::pair<int, int>, 8> cutRanges {};
    int drumSampleLen = 1;
    DrumSynth synth;
    double sr = 48000.0;
    juce::AudioBuffer<float> drumBus;
};

} // namespace sawt
