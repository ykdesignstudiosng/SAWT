/*
  SAWT - Slicer.h
  Transient slicer: finds percussive consonants ("t","k","p","b", beatbox hits)
  inside any vocal recording and proposes drum slices. Classification is a
  suggestion - the user can always trigger any pad on any slice.
*/
#pragma once

#include "../Params.h"

namespace sawt
{

enum class PadKind : int { Kick = 0, Snare, HatC, HatO, Perc1, Perc2, Clap, Rim, Unknown };

struct DrumSlice
{
    int start = 0;
    int length = 0;
    PadKind kind = PadKind::Unknown;
    float energy = 0.0f;
};

class TransientSlicer
{
public:
    // Returns up to maxSlices slices, sorted by pad assignment, energy-ranked.
    std::vector<DrumSlice> slice (const SampleData& sample, int maxSlices = 8);

    static juce::String kindName (PadKind k);
};

} // namespace sawt
