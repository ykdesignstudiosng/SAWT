/*
  SAWT - Widgets.cpp
*/
#include "Widgets.h"
#include "LookAndFeel.h"

namespace sawt
{
using namespace theme;

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& paramID,
            const juce::String& title, float size, float titleSize)
    : attachment (s, paramID, slider), labelHeight (titleSize + 3.0f)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, c (gold));
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, c (line));
    slider.setColour (juce::Slider::thumbColourId, c (ink));
    slider.setTooltip (title);
    addAndMakeVisible (slider);

    label.setText (title, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (font (titleSize));
    label.setColour (juce::Label::textColourId, c (inkDim));
    addAndMakeVisible (label);

    setSize ((int) size, (int) size + (int) labelHeight);
}

void Knob::resized()
{
    auto b = getLocalBounds();
    slider.setBounds (b.removeFromTop (b.getHeight() - (int) labelHeight));
    label.setBounds (b);
}

void Knob::paint (juce::Graphics&) {}

//==============================================================================
Toggle::Toggle (juce::AudioProcessorValueTreeState& s, const juce::String& paramID,
                const juce::String& title)
    : button (title), attachment (s, paramID, button)
{
    button.setColour (juce::ToggleButton::textColourId, c (ink));
    button.setColour (juce::ToggleButton::tickColourId, c (gold));
    button.setColour (juce::ToggleButton::tickDisabledColourId, c (inkDim));
    addAndMakeVisible (button);
}

void Toggle::resized()
{
    button.setBounds (getLocalBounds());
}

//==============================================================================
DropZone::DropZone (juce::String p) : prompt (std::move (p))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void DropZone::setFileName (const juce::String& name)
{
    fileName = name;
    repaint();
}

void DropZone::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto accent = dragging ? c (gold) : c (line);

    g.setColour (c (panel).withAlpha (dragging ? 0.9f : 0.55f));
    g.fillRoundedRectangle (r, 6.0f);

    // dashed border
    juce::Path border;
    border.addRoundedRectangle (r, 6.0f);
    std::vector<float> dashes { 5.0f, 4.0f };
    g.setColour (accent);
    juce::PathStrokeType (1.2f).createDashedStroke (border, border, dashes.data(), 2);
    g.strokePath (border, juce::PathStrokeType (1.2f));

    g.setColour (fileName.isNotEmpty() ? c (ink) : c (inkDim));
    g.setFont (font (12.0f));
    const juce::String text = fileName.isNotEmpty()
        ? fileName
        : prompt;
    g.drawFittedText (text, getLocalBounds().reduced (10).withTrimmedTop (18),
                      juce::Justification::centred, 2, 0.9f);

    g.setColour (c (goldDim));
    g.setFont (font (9.5f));
    g.drawFittedText ("DROP VOCAL SAMPLE  \u2022  CLICK TO BROWSE  \u2022  WAV AIFF FLAC OGG MP3",
                      getLocalBounds().removeFromTop (18).reduced (8, 0),
                      juce::Justification::centredLeft, 1, 0.8f);
}

void DropZone::mouseDown (const juce::MouseEvent&) { browse(); }

void DropZone::filesDropped (const juce::StringArray& files, int, int)
{
    dragging = false;
    for (const auto& f : files)
    {
        auto file = juce::File (f);
        if (file.existsAsFile())
        {
            if (onFileChosen) onFileChosen (file);
            break;
        }
    }
    repaint();
}

void DropZone::browse()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Choose a vocal sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f.existsAsFile() && onFileChosen) onFileChosen (f);
        });
}

//==============================================================================
LogoView::LogoView()
{
    setInterceptsMouseClicks (false, false);
}

void LogoView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float starSize = juce::jmin (34.0f, b.getHeight() - 8.0f);
    drawStar (g, juce::Rectangle<float> (starSize, starSize).withCentre ({ b.getX() + 22.0f, b.getCentreY() }),
              c (gold), 2.0f);

    g.setColour (c (ink));
    g.setFont (font (24.0f));
    g.drawText ("SAWT", juce::Rectangle<float> (b.getX() + 46.0f, b.getY(), 110.0f, b.getHeight()),
                juce::Justification::centredLeft);

    g.setColour (c (goldDim));
    g.setFont (font (10.0f));
    g.drawText ("THE VOICE-ONLY SYNTHESIZER",
                juce::Rectangle<float> (b.getX() + 152.0f, b.getY() + b.getHeight() * 0.5f, 220.0f, b.getHeight() * 0.42f),
                juce::Justification::centredLeft);
}

} // namespace sawt
