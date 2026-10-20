/*
  SAWT - DrumPanel.h - 8-pad grid + selected-pad macro strip
*/
#pragma once

#include "../Params.h"
#include "Widgets.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

class SAWTAudioProcessor;   // global scope (defined in PluginProcessor.h)

namespace sawt
{

class PadButton : public juce::Component
{
public:
    PadButton (int index, SAWTAudioProcessor& p);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setSelected (bool s) noexcept { selected = s; repaint(); }

    std::function<void (int)> onSelected;

private:
    int pad;
    SAWTAudioProcessor& processor;
    bool selected = false;
    float flash = 0.0f;
};

class DrumPanel : public juce::Component,
                  private juce::Timer
{
public:
    DrumPanel (SAWTAudioProcessor& proc, juce::AudioProcessorValueTreeState& s);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void selectPad (int i);

    SAWTAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;

    std::array<std::unique_ptr<PadButton>, 8> pads;
    std::array<std::unique_ptr<Knob>, 3> padKnobs;      // tune/decay/level of selection
    juce::Label padTitle;
    juce::Label hint;
    int selectedPad = 0;
    int lastSliceCount = -1;
};

} // namespace sawt
