#pragma once

#include "EQBand.h"
#include "FilterTypes.h"
#include "VariableSlope.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

namespace deq::eq_auto_gain
{
struct BandParameters
{
    bool enabled = false;
    int type = filter_types::bell;
    float frequencyHz = 1000.0f;
    float q = 1.0f;
    float gainDb = 0.0f;
    float slopeDbPerOct = 12.0f;
    int placementMode = 0;
    float placementPercent = 0.0f;
};

namespace detail
{
using Complex = std::complex<double>;

struct Matrix
{
    Complex ll { 1.0, 0.0 }, lr {}, rl {}, rr { 1.0, 0.0 };
};

inline Matrix multiply(const Matrix& a, const Matrix& b) noexcept
{
    return {
        a.ll * b.ll + a.lr * b.rl,
        a.ll * b.lr + a.lr * b.rr,
        a.rl * b.ll + a.rr * b.rl,
        a.rl * b.lr + a.rr * b.rr
    };
}

inline double meanStereoPowerGain(const Matrix& matrix) noexcept
{
    return 0.5 * (std::norm(matrix.ll) + std::norm(matrix.lr)
                  + std::norm(matrix.rl) + std::norm(matrix.rr));
}

inline Complex rawResponse(const BandParameters& band, double sampleRate,
                           float amount, float shiftSemitones,
                           bool adaptiveQ, double probeFrequency)
{
    const auto type = filter_types::fromParameterIndex(band.type);
    const bool gainBearing = variable_slope::distributesGain(type);
    const bool cut = zl_filter::isClassicCut(type) || zl_filter::isResonantCut(type);
    float q = band.q;
    if (adaptiveQ)
        q = std::clamp(q * (1.0f + std::abs(band.gainDb) * 0.12f), 0.1f, 24.0f);
    if (zl_filter::isResonantCut(type))
        q = EQBand::amountResonantCutQ(q, amount);

    double frequency = (double)band.frequencyHz
        * std::pow(2.0, (double)shiftSemitones / 12.0);
    if (cut)
    {
        const double neutral = type == Biquad::Type::LowPass
                || type == Biquad::Type::ResLowPass
            ? sampleRate * 0.45 : 10.0;
        frequency = neutral * std::pow(std::max(1.0e-9, frequency / neutral),
                                       (double)std::max(0.0f, amount));
    }
    frequency = std::clamp(frequency, 10.0, sampleRate * 0.45);

    const double gain = gainBearing ? (double)band.gainDb * amount : band.gainDb;
    const auto wet = variable_slope::response(type, sampleRate, frequency, q, gain,
                                               band.slopeDbPerOct, true, probeFrequency);
    if (gainBearing)
        return wet;

    const double mix = cut ? (double)EQBand::cutAmountMix(amount)
                           : (double)std::clamp(amount, 0.0f, 1.0f);
    return Complex { 1.0, 0.0 } + mix * (wet - Complex { 1.0, 0.0 });
}

inline Matrix routedResponse(const BandParameters& band, const Complex& response,
                             float routeWeight = 1.0f) noexcept
{
    const float placement = std::clamp(band.placementPercent * 0.01f, -1.0f, 1.0f);
    const double firstWeight = routeWeight * std::clamp(1.0f - placement, 0.0f, 1.0f);
    const double secondWeight = routeWeight * std::clamp(1.0f + placement, 0.0f, 1.0f);
    const Complex first = Complex { 1.0, 0.0 }
        + firstWeight * (response - Complex { 1.0, 0.0 });
    const Complex second = Complex { 1.0, 0.0 }
        + secondWeight * (response - Complex { 1.0, 0.0 });

    if (band.placementMode != 1)
        return { first, {}, {}, second };

    // Energy-preserving L/R <-> M/S matrices used by EQBand::processEqualizer.
    const Complex sum = 0.5 * (first + second);
    const Complex difference = 0.5 * (first - second);
    return { sum, difference, difference, sum };
}
}

template <size_t NumBands>
inline float combinedResponseCompensationDb(const std::array<BandParameters, NumBands>& bands,
                                            double sampleRate, float amount,
                                            float shiftSemitones, bool adaptiveQ) noexcept
{
    constexpr int probeCount = 192;
    const double lowFrequency = 20.0;
    const double highFrequency = std::max(lowFrequency,
        std::min(20000.0, sampleRate * 0.45));
    const double ratio = highFrequency / lowFrequency;
    double accumulatedPower = 0.0;

    for (int probe = 0; probe < probeCount; ++probe)
    {
        const double position = ((double)probe + 0.5) / (double)probeCount;
        const double frequency = lowFrequency * std::pow(ratio, position);
        detail::Matrix regular;
        detail::Matrix transient;
        detail::Matrix sustain;
        bool hasTransientSustainBand = false;

        // The processor applies T/S bands inside each split branch first, then
        // applies the remaining L/R and M/S bands to the recombined signal.
        for (const auto& band : bands)
        {
            if (!band.enabled || band.placementMode != 2)
                continue;
            hasTransientSustainBand = true;
            const auto response = detail::rawResponse(
                band, sampleRate, amount, shiftSemitones, adaptiveQ, frequency);
            const float placement = std::clamp(band.placementPercent * 0.01f, -1.0f, 1.0f);
            const auto transientBand = detail::routedResponse(
                BandParameters { band.enabled, band.type, band.frequencyHz, band.q,
                                 band.gainDb, band.slopeDbPerOct, 0, 0.0f },
                response, std::clamp(1.0f - placement, 0.0f, 1.0f));
            const auto sustainBand = detail::routedResponse(
                BandParameters { band.enabled, band.type, band.frequencyHz, band.q,
                                 band.gainDb, band.slopeDbPerOct, 0, 0.0f },
                response, std::clamp(1.0f + placement, 0.0f, 1.0f));
            transient = detail::multiply(transientBand, transient);
            sustain = detail::multiply(sustainBand, sustain);
        }

        for (const auto& band : bands)
        {
            if (!band.enabled || band.placementMode == 2)
                continue;
            const auto response = detail::rawResponse(
                band, sampleRate, amount, shiftSemitones, adaptiveQ, frequency);
            regular = detail::multiply(detail::routedResponse(band, response), regular);
        }

        if (hasTransientSustainBand)
        {
            const auto transientTotal = detail::multiply(regular, transient);
            const auto sustainTotal = detail::multiply(regular, sustain);
            accumulatedPower += 0.5 * (detail::meanStereoPowerGain(transientTotal)
                                     + detail::meanStereoPowerGain(sustainTotal));
        }
        else
            accumulatedPower += detail::meanStereoPowerGain(regular);
    }

    const double meanPower = accumulatedPower / (double)probeCount;
    return std::clamp((float)(-10.0 * std::log10(std::max(meanPower, 1.0e-12))),
                      -36.0f, 36.0f);
}
}
