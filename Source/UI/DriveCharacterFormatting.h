#pragma once

#include "../DSP/EQBand.h"
#include <juce_core/juce_core.h>
#include <cmath>

namespace deq::ui
{
inline juce::String formatDriveCharacter(int mode, double raw)
{
    if (mode == static_cast<int>(SaturationType::SineErosion))
    {
        const double unit = juce::jlimit(0.0, 1.0, raw);
        const double hz = unit <= 0.5 ? 1000.0 * std::pow(2.0 * unit, 2.0)
                                      : 1000.0 * std::pow(10.0, 2.0 * unit - 1.0);
        return hz >= 1000.0 ? juce::String(hz / 1000.0, 2) + " kHz"
                            : juce::String(juce::roundToInt(hz)) + " Hz";
    }

    double shown = saturationModeUsesBipolarCharacter(mode)
        ? raw * 100.0 : juce::jmax(0.0, raw) * 100.0;
    if (std::abs(shown) < 0.5) shown = 0.0;
    return (shown == 0.0 ? juce::String("00")
                         : juce::String(juce::roundToInt(shown))) + "%";
}
}
