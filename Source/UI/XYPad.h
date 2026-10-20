/*
  SAWT - XYPad.h - morph pad (X: sampler<->granular, Y: timbre)
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace sawt
{

class XYPad : public juce::Component
{
public:
    XYPad (juce::AudioProcessorValueTreeState& s,
           const juce::String& xParam, const juce::String& yParam);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    // re-read the two parameters (e.g. after a preset load)
    void syncFromParams();

private:
    void setNorm (juce::Point<float> p);

    juce::AudioProcessorValueTreeState& state;
    juce::String xID, yID;
    juce::Point<float> handle { 0.35f, 0.5f };   // normalised, y up
};

} // namespace sawt
