/*
  SAWT - LookAndFeel.cpp
*/
#include "LookAndFeel.h"

namespace sawt
{
using namespace theme;

void SawtLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                        float sliderPos, float startA, float endA,
                                        juce::Slider&)
{
    auto box = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float r = juce::jmin (box.getWidth(), box.getHeight()) * 0.42f;
    const auto centre = box.getCentre();
    const float angle = startA + sliderPos * (endA - startA);

    // track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, r, r, 0.0f, startA, endA, true);
    g.setColour (c (line));
    g.strokePath (track, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // value arc
    if (sliderPos > 0.0005f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, r, r, 0.0f, startA, angle, true);
        g.setColour (c (gold));
        g.strokePath (value, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // hub
    g.setColour (c (panelHi));
    g.fillEllipse (centre.x - r * 0.52f, centre.y - r * 0.52f, r * 1.04f, r * 1.04f);
    g.setColour (c (line));
    g.drawEllipse (centre.x - r * 0.52f, centre.y - r * 0.52f, r * 1.04f, r * 1.04f, 1.0f);

    // pointer dot
    g.setColour (c (ink));
    const float px = centre.x + std::cos (angle - juce::MathConstants<float>::halfPi) * r * 0.68f;
    const float py = centre.y + std::sin (angle - juce::MathConstants<float>::halfPi) * r * 0.68f;
    g.fillEllipse (px - 2.4f, py - 2.4f, 4.8f, 4.8f);
}

void SawtLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                            const juce::Colour& bg,
                                            bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();

    juce::Colour fill = on ? c (goldDim) : c (panel);
    if (down) fill = fill.brighter (0.12f);
    else if (highlighted) fill = fill.brighter (0.06f);

    g.setColour (fill);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (on ? c (gold) : c (line).withAlpha (0.9f));
    g.drawRoundedRectangle (r, 5.0f, on ? 1.4f : 1.0f);

    juce::ignoreUnused (bg);
}

void SawtLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool,
                                    int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f);
    g.setColour (c (panel));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (c (line));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    // arrow
    juce::Path arrow;
    const float cx = (float) w - 16.0f, cy = (float) h * 0.5f - 1.0f;
    arrow.addTriangle (cx - 5.0f, cy - 3.0f, cx + 5.0f, cy - 3.0f, cx, cy + 4.0f);
    g.setColour (c (gold));
    g.fillPath (arrow);
}

void SawtLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& l)
{
    float fh = l.getFont().getHeight();
    if (fh < 1.0f) fh = 12.0f;
    g.setColour (l.findColour (juce::Label::textColourId));
    g.setFont (font (fh));
    g.drawFittedText (l.getText(), l.getLocalBounds(),
                      l.getJustificationType(),
                      juce::jmax (1, (int) ((float) l.getHeight() / fh)),
                      0.8f);
}

void SawtLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                        bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    const float boxH = juce::jmin (18.0f, r.getHeight() - 4.0f);
    auto switchBox = juce::Rectangle<float> (34.0f, boxH).withY (r.getCentreY() - boxH * 0.5f);
    if (b.isTransformed() || b.getWidth() < 60)
        switchBox = switchBox.withX (r.getX()).withY (r.getCentreY() - boxH * 0.5f);

    juce::Colour fill = on ? c (goldDim) : c (panel);
    if (down) fill = fill.brighter (0.12f);
    else if (highlighted) fill = fill.brighter (0.06f);

    g.setColour (fill);
    g.fillRoundedRectangle (switchBox, boxH * 0.5f);
    g.setColour (on ? c (gold) : c (line));
    g.drawRoundedRectangle (switchBox, boxH * 0.5f, 1.0f);

    const float knobR = boxH - 6.0f;
    const float kx = on ? switchBox.getRight() - knobR - 3.0f : switchBox.getX() + 3.0f;
    g.setColour (on ? c (gold) : c (inkDim));
    g.fillEllipse (kx, switchBox.getCentreY() - knobR * 0.5f, knobR, knobR);

    g.setColour (c (ink));
    g.setFont (font (12.5f));
    auto textArea = r.withTrimmedLeft (b.getWidth() < 60 ? 0.0f : 0.0f);
    textArea.setX (switchBox.getRight() + 8.0f);
    textArea.setWidth (r.getRight() - textArea.getX());
    if (textArea.getWidth() > 20.0f)
        g.drawFittedText (b.getButtonText(), textArea.toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.9f);
}

void SawtLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                        float sliderPos, float minSliderPos, float maxSliderPos,
                                        juce::Slider::SliderStyle, juce::Slider&)
{
    const float cy = (float) y + (float) h * 0.5f - 1.5f;
    g.setColour (c (line));
    g.fillRect ((float) x, cy, (float) w, 3.0f);
    g.setColour (c (gold));
    const float filled = juce::jlimit (0.0f, (float) w, sliderPos - (float) x);
    g.fillRect ((float) x, cy, filled, 3.0f);
    juce::ignoreUnused (minSliderPos, maxSliderPos);
}

void SawtLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h,
                                                juce::TextEditor&)
{
    g.setColour (c (bg));
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 5.0f);
    g.setColour (c (line));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 5.0f, 1.0f);
}

} // namespace sawt
