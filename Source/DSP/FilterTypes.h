#pragma once

#include "Biquad.h"

#include <algorithm>

namespace deq::filter_types
{
enum Index
{
    resLowCut = 0,
    resHighCut,
    notch,
    tilt,
    bandPass,
    bell,
    lowShelf,
    highShelf,
    lowCut,
    highCut,
    count
};

inline Biquad::Type fromParameterIndex(int index) noexcept
{
    switch (std::clamp(index, 0, count - 1))
    {
        case resLowCut:  return Biquad::Type::ResLowPass;
        case resHighCut: return Biquad::Type::ResHighPass;
        case notch:      return Biquad::Type::Notch;
        case tilt:       return Biquad::Type::Tilt;
        case bandPass:   return Biquad::Type::Bandpass;
        case bell:       return Biquad::Type::Bell;
        case lowShelf:   return Biquad::Type::LowShelf;
        case highShelf:  return Biquad::Type::HighShelf;
        case lowCut:     return Biquad::Type::LowPass;
        case highCut:    return Biquad::Type::HighPass;
        default:         return Biquad::Type::Bell;
    }
}

inline bool isGainBearingIndex(int index) noexcept
{
    return index == tilt || index == bell || index == lowShelf || index == highShelf;
}

inline bool isResonantCutIndex(int index) noexcept
{
    return index == resLowCut || index == resHighCut;
}

inline constexpr float resonantCutDefaultQ = 0.75f;
}
