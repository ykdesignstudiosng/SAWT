/*
  SAWT - WaveformView.h - waveform with slice markers + playhead
*/
#pragma once

#include "../Params.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace sawt
{

class WaveformView : public juce::Component
{
public:
    WaveformView() { setInterceptsMouseClicks (false, false); }

    // call on the message thread when a sample changes
    void setSample (const SamplePtr& s);

    // slice markers in normalised [0..1] start/end pairs
    void setSliceMarkers (const std::vector<std::pair<float, float>>& normSlices);

    void setPlayheadNorm (float p) noexcept { playhead = juce::jlimit (0.0f, 1.0f, p); }

    void paint (juce::Graphics&) override;

private:
    struct Peak { float min, max; };
    void rebuildPeaks();

    SamplePtr sample;
    std::vector<Peak> peaks;             // one per sample-per-pixel at cache width
    std::vector<std::pair<float, float>> slices;
    float playhead = 0.0f;
    int lastWidth = 0;
};

} // namespace sawt
