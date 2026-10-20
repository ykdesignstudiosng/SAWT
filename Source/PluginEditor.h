/*
  SAWT - PluginEditor.h
*/
#pragma once

#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/LookAndFeel.h"
#include "UI/Widgets.h"
#include "UI/WaveformView.h"
#include "UI/XYPad.h"
#include "UI/SpectrumView.h"
#include "UI/DrumPanel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

class SAWTAudioProcessorEditor : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit SAWTAudioProcessorEditor (SAWTAudioProcessor&);
    ~SAWTAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshPresetList();
    void loadSelectedPreset();
    void savePresetDialog();

    SAWTAudioProcessor& processor;
    sawt::SawtLookAndFeel lnf;

    // ---- top bar ----
    sawt::LogoView logo;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, savePreset { "SAVE" };
    juce::ComboBox presetBox;
    sawt::Knob inGainKnob, masterKnob;
    sawt::Toggle monitorToggle, snapToggle;
    juce::TextButton captureButton { "CAPTURE MIC" };

    // ---- melodic ----
    sawt::DropZone melodicDrop;
    sawt::WaveformView melodicWave;
    juce::ComboBox engineBox, gModeBox;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment engineAttach,
        gModeAttach;
    sawt::XYPad xyPad;
    std::vector<std::unique_ptr<sawt::Knob>> melodicKnobs;   // row 1 + row 2
    std::vector<std::unique_ptr<sawt::Knob>> granKnobs;      // granular row

    // ---- drums ----
    sawt::DropZone drumDrop;
    juce::ComboBox drumSourceBox;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment drumSourceAttach;
    juce::TextButton sliceButton { "AUTO SLICE" };
    std::vector<std::unique_ptr<sawt::Knob>> drumKnobs;      // punch drive tone level
    sawt::DrumPanel drumPanel;

    // section titles
    juce::Label melodicTitle, drumTitle;
    sawt::SamplePtr lastMelodic, lastDrum;

    // ---- bottom ----
    sawt::SpectrumView spectrum;
    sawt::Toggle fxOnToggle;
    std::vector<std::unique_ptr<sawt::Knob>> fxKnobs;

    void addKnob (std::vector<std::unique_ptr<sawt::Knob>>& v, const char* id,
                  const char* name, float size = 56.0f, float titleSize = 10.5f);

    int designW = 1160, designH = 740;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SAWTAudioProcessorEditor)
};
