/*
  SAWT - PresetManager.cpp
*/
#include "PresetManager.h"

namespace sawt
{

juce::File PresetManager::presetDir()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("SAWT").getChildFile ("Presets");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

juce::StringArray PresetManager::factoryNames()
{
    return { "Init Voice", "Choir Pad", "Formant Lead", "Human Beatbox 808" };
}

void PresetManager::loadFactory (int index)
{
    loadFactory (index, state);
}

void PresetManager::loadFactory (int index, juce::AudioProcessorValueTreeState& s)
{
    auto set = [&s] (const char* id, float v)
    {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (v));
    };

    switch (index)
    {
        case 1: // Choir Pad
            set (pid::engine, 1.0f);          // granular
            set (pid::att, 0.8f); set (pid::rel, 2.2f); set (pid::sus, 1.0f);
            set (pid::gSize, 0.18f); set (pid::gRate, 14.0f);
            set (pid::gDrift, 0.6f); set (pid::gJitter, 0.35f); set (pid::gMode, 1.0f);
            set (pid::unison, 3.0f); set (pid::detune, 18.0f); set (pid::spread, 0.9f);
            set (pid::cutoff, 6500.0f); set (pid::eHigh, -3.0f);
            set (pid::rvMix, 0.45f); set (pid::rvSize, 0.8f); set (pid::chMix, 0.35f);
            break;
        case 2: // Formant Lead
            set (pid::engine, 2.0f);          // formant (PSOLA)
            set (pid::att, 0.02f); set (pid::rel, 0.15f); set (pid::sus, 0.8f);
            set (pid::glide, 0.08f);
            set (pid::cutoff, 14000.0f); set (pid::formant, 2.0f);
            set (pid::rvMix, 0.15f); set (pid::dlMix, 0.2f); set (pid::dlTime, 0.32f);
            break;
        case 3: // Human Beatbox 808
            set (pid::engine, 0.0f);
            set (pid::dSource, 1.0f);         // vocal 808s
            set (pid::dTone, 0.45f); set (pid::dDrive, 0.35f); set (pid::dPunch, 0.5f);
            set (pid::rvMix, 0.1f); set (pid::sDrive, 0.2f);
            break;
        case 0: default: // Init Voice
            set (pid::engine, 0.0f);
            set (pid::att, 0.01f); set (pid::dec, 0.3f); set (pid::sus, 0.7f); set (pid::rel, 0.4f);
            set (pid::cutoff, 18000.0f); set (pid::formant, 0.0f);
            set (pid::dSource, 0.0f);
            set (pid::rvMix, 0.2f); set (pid::dlMix, 0.15f);
            break;
    }

    juce::ignoreUnused (s);
}

juce::StringArray PresetManager::listUserPresets() const
{
    juce::StringArray names;
    for (const auto& f : presetDir().findChildFiles (juce::File::findFiles, false, "*.sawt"))
        names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}

bool PresetManager::saveUserPreset (const juce::String& name) const
{
    if (name.trim().isEmpty()) return false;
    auto tree = state.copyState();
    std::unique_ptr<juce::XmlElement> xml (tree.createXml());
    if (xml == nullptr) return false;
    auto file = presetDir().getChildFile (name.trim() + ".sawt");
    return xml->writeTo (file);
}

bool PresetManager::loadUserPreset (const juce::String& name)
{
    auto file = presetDir().getChildFile (name + ".sawt");
    if (! file.existsAsFile()) return false;
    auto xml = juce::parseXML (file);
    if (xml == nullptr) return false;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid()) return false;
    state.replaceState (tree);
    return true;
}

} // namespace sawt
