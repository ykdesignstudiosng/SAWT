/*
  SAWT - PresetManager.h
  Factory + user presets. Sample file paths travel inside the preset; the
  processor re-loads the audio when the files are present.
*/
#pragma once

#include "../Params.h"

namespace sawt
{

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& s) : state (s) {}

    static juce::File presetDir();
    static juce::StringArray factoryNames();
    void loadFactory (int index);              // parameter states only
    static void loadFactory (int index, juce::AudioProcessorValueTreeState& s);

    juce::StringArray listUserPresets() const;
    bool saveUserPreset (const juce::String& name) const;
    bool loadUserPreset (const juce::String& name);

    juce::String currentPresetName { "Init" };

private:
    juce::AudioProcessorValueTreeState& state;
};

} // namespace sawt
