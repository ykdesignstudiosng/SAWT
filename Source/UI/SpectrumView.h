/*
  SAWT - SpectrumView.h - animated spectrum analyser
  Pulls samples from the processor's fifo and FFTs on the UI timer (30 fps).
*/
#pragma once

#include "../Params.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

class SAWTAudioProcessor;   // global scope (defined in PluginProcessor.h)

namespace sawt
{

class SpectrumView : public juce::Component,
                     private juce::Timer
{
public:
    explicit SpectrumView (SAWTAudioProcessor& p);
    ~SpectrumView() override = default;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void computeBands();

    SAWTAudioProcessor& processor;
    juce::dsp::FFT fft { 11 };                       // 2048
    std::array<float, 4096> fifoData {};
    std::array<float, 2048> windowed {};
    std::array<float, 2048> magnitudes {};
    std::array<float, numSpectrumBands> bands {};
    std::array<float, numSpectrumBands> peaks {};
    int fifoStart = 0, fifoValid = 0;
};

} // namespace sawt
