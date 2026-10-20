/*
  SAWT - Widgets.h - Knob, Toggle, DropZone, LogoView
*/
#pragma once

#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace sawt
{

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& paramID,
          const juce::String& title, float size = 58.0f, float titleSize = 10.5f);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::Slider slider;
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    float labelHeight;
};

//==============================================================================
class Toggle : public juce::Component
{
public:
    Toggle (juce::AudioProcessorValueTreeState& s, const juce::String& paramID,
            const juce::String& title);

    void resized() override;

    juce::ToggleButton button;

private:
    juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
};

//==============================================================================
class DropZone : public juce::Component,
                 private juce::FileDragAndDropTarget
{
public:
    explicit DropZone (juce::String prompt);
    void paint (juce::Graphics&) override;
    void setFileName (const juce::String& name);

    std::function<void (const juce::File&)> onFileChosen;

    void mouseDown (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragging = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragging = false; repaint(); }

private:
    void browse();
    juce::String prompt, fileName;
    bool dragging = false;
};

//==============================================================================
class LogoView : public juce::Component
{
public:
    LogoView();
    void paint (juce::Graphics&) override;
};

} // namespace sawt
