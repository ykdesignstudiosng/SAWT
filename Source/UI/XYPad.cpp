/*
  SAWT - XYPad.cpp
*/
#include "XYPad.h"
#include "Theme.h"

namespace sawt
{
using namespace theme;

XYPad::XYPad (juce::AudioProcessorValueTreeState& s,
              const juce::String& xParam, const juce::String& yParam)
    : state (s), xID (xParam), yID (yParam)
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void XYPad::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (c (panel).withAlpha (0.75f));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (c (line).withAlpha (0.6f));
    g.drawRoundedRectangle (b, 6.0f, 1.0f);

    // grid
    g.setColour (c (line).withAlpha (0.35f));
    for (int i = 1; i < 4; ++i)
    {
        g.drawVerticalLine ((int) (b.getX() + b.getWidth() * i / 4.0f), b.getY(), b.getBottom());
        g.drawHorizontalLine ((int) (b.getY() + b.getHeight() * i / 4.0f), b.getX(), b.getRight());
    }

    // axis hints
    g.setColour (c (goldDim));
    g.setFont (font (9.0f));
    g.drawSingleLineText ("SAMPLER", (int) b.getX() + 6, (int) b.getBottom() - 6);
    g.drawSingleLineText ("GRANULAR", (int) b.getRight() - 62, (int) b.getBottom() - 6);
    g.drawSingleLineText ("DARK", (int) b.getX() + 6, (int) b.getY() + 14);
    g.drawSingleLineText ("BRIGHT", (int) b.getRight() - 46, (int) b.getY() + 14);

    // handle
    const float hx = b.getX() + handle.x * b.getWidth();
    const float hy = b.getBottom() - handle.y * b.getHeight();
    g.setColour (c (gold).withAlpha (0.25f));
    g.fillEllipse (hx - 11.0f, hy - 11.0f, 22.0f, 22.0f);
    g.setColour (c (gold));
    g.fillEllipse (hx - 5.0f, hy - 5.0f, 10.0f, 10.0f);
    g.setColour (c (ink));
    g.drawEllipse (hx - 11.0f, hy - 11.0f, 22.0f, 22.0f, 1.0f);

    g.drawLine (hx, b.getY(), hx, b.getBottom(), 0.6f);
    g.drawLine (b.getX(), hy, b.getRight(), hy, 0.6f);
}

void XYPad::setNorm (juce::Point<float> p)
{
    const auto b = getLocalBounds().toFloat();
    handle.x = juce::jlimit (0.0f, 1.0f, (p.x - b.getX()) / juce::jmax (1.0f, b.getWidth()));
    handle.y = juce::jlimit (0.0f, 1.0f, (b.getBottom() - p.y) / juce::jmax (1.0f, b.getHeight()));

    if (auto* px = state.getParameter (xID))
        px->setValueNotifyingHost (px->getNormalisableRange().convertTo0to1 (handle.x));
    if (auto* py = state.getParameter (yID))
        py->setValueNotifyingHost (py->getNormalisableRange().convertTo0to1 (handle.y));

    repaint();
}

void XYPad::mouseDown (const juce::MouseEvent& e) { setNorm (e.position); }
void XYPad::mouseDrag (const juce::MouseEvent& e) { setNorm (e.position); }

void XYPad::syncFromParams()
{
    if (auto* px = state.getRawParameterValue (xID))
        handle.x = juce::jlimit (0.0f, 1.0f, px->load (std::memory_order_relaxed));
    if (auto* py = state.getRawParameterValue (yID))
        handle.y = juce::jlimit (0.0f, 1.0f, py->load (std::memory_order_relaxed));
    repaint();
}

} // namespace sawt
