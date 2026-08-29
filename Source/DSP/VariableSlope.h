#pragma once

#include "Biquad.h"
#include "ZLFilter.h"
#include <algorithm>
#include <cmath>
#include <complex>

// Shared response helpers. The audible DSP lives in EQBand/ZLFilter; this
// namespace deliberately contains only the matching graph/FIR response model
// plus the classic v0.1.0 continuously-variable cut response.
namespace variable_slope
{
inline bool distributesGain(Biquad::Type type) noexcept
{
    return type == Biquad::Type::Bell || type == Biquad::Type::LowShelf
        || type == Biquad::Type::HighShelf || type == Biquad::Type::Tilt;
}

inline std::complex<double> firstOrderCutResponse(bool highPass, double sampleRate,
                                                  double centerFrequency,
                                                  double probeFrequency) noexcept
{
    const double k = std::tan(kPi * std::clamp(centerFrequency, 5.0, sampleRate * 0.49)
                              / sampleRate);
    const double norm = 1.0 / (1.0 + k);
    const double b0 = highPass ? norm : k * norm;
    const double b1 = highPass ? -norm : b0;
    const double a1 = (k - 1.0) * norm;
    const double omega = 2.0 * kPi * probeFrequency / sampleRate;
    const std::complex<double> z1(std::cos(omega), -std::sin(omega));
    return (b0 + b1 * z1) / (1.0 + a1 * z1);
}

inline std::complex<double> response(Biquad::Type type, double sampleRate,
                                     double centerFrequency, double q,
                                     double gainDb, double slopeDbPerOct,
                                     bool /*decramp*/, double probeFrequency)
{
    if (type == Biquad::Type::HighPass || type == Biquad::Type::LowPass)
    {
        const auto one = firstOrderCutResponse(type == Biquad::Type::HighPass,
                                               sampleRate, centerFrequency,
                                               probeFrequency);
        const double amount = std::clamp(slopeDbPerOct / 6.0, 0.0, 16.0);
        const int full = (int)std::floor(amount);
        const double fraction = amount - full;
        return std::pow(one, full)
            * (std::complex<double>{1.0, 0.0}
               + fraction * (one - std::complex<double>{1.0, 0.0}));
    }

    zl_filter::Cascade cascade;
    cascade.configure(type, sampleRate, centerFrequency, q, gainDb,
                      (float)slopeDbPerOct);
    return cascade.response(probeFrequency, sampleRate);
}
}
