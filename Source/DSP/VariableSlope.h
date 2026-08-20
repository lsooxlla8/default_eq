#pragma once
#include "Biquad.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>

namespace variable_slope
{
constexpr int maxOrder = 4;

inline float shapePosition(float slopeDbPerOct) noexcept
{
    return 1.0f + 3.0f * std::clamp((slopeDbPerOct - 3.0f) / 45.0f, 0.0f, 1.0f);
}

inline float cutStageAmount(float slopeDbPerOct) noexcept
{
    return 1.0f + 7.0f * std::clamp((slopeDbPerOct - 3.0f) / 45.0f, 0.0f, 1.0f);
}

inline bool distributesGain(Biquad::Type type) noexcept
{
    return type == Biquad::Type::Bell || type == Biquad::Type::LowShelf
        || type == Biquad::Type::HighShelf || type == Biquad::Type::Tilt;
}

inline std::complex<double> response(const Biquad& stage, double frequency,
                                     double sampleRate) noexcept
{
    const double omega = 2.0 * kPi * frequency / sampleRate;
    const std::complex<double> z1(std::cos(omega), -std::sin(omega));
    const auto z2 = z1 * z1;
    return (stage.b0 + stage.b1 * z1 + stage.b2 * z2)
        / (1.0 + stage.a1 * z1 + stage.a2 * z2);
}

inline double bellStageFrequency(double centerFrequency, double q,
                                 int order, int stageIndex) noexcept
{
    if (order <= 1) return centerFrequency;
    const double maximumSpreadOctaves = std::clamp(0.40 / std::max(0.1, q), 0.04, 1.25);
    const double orderSpread = maximumSpreadOctaves * (double)(order - 1) / (double)(maxOrder - 1);
    const double position = (2.0 * stageIndex - (order - 1)) / (double)(order - 1);
    return centerFrequency * std::pow(2.0, position * orderSpread);
}

inline double bellStageQ(double q, int order) noexcept
{
    return std::clamp(q * (1.0 + 0.70 * (double)(order - 1)), 0.1, 24.0);
}

inline double bellStageGain(double sampleRate, double centerFrequency, double q,
                            double totalGainDb, int order, bool decramp)
{
    if (order <= 1 || std::abs(totalGainDb) < 1.0e-9) return totalGainDb;
    double stageGainDb = totalGainDb / (double)order;
    for (int iteration = 0; iteration < 3; ++iteration)
    {
        double achievedDb = 0.0;
        for (int stage = 0; stage < order; ++stage)
        {
            Biquad probe;
            const double frequency = bellStageFrequency(centerFrequency, q, order, stage);
            const double stageQ = bellStageQ(q, order);
            if (decramp) probe.setMatched(Biquad::Type::Bell, sampleRate, frequency, stageQ, stageGainDb);
            else         probe.set(Biquad::Type::Bell, sampleRate, frequency, stageQ, stageGainDb);
            achievedDb += 20.0 * std::log10(std::max(
                std::abs(response(probe, centerFrequency, sampleRate)), 1.0e-15));
        }
        if (std::abs(achievedDb) < 1.0e-9) break;
        stageGainDb *= totalGainDb / achievedDb;
    }
    return stageGainDb;
}

inline double stageGain(Biquad::Type type, double totalGainDb, int order) noexcept
{
    return distributesGain(type) ? totalGainDb / (double)std::max(1, order) : totalGainDb;
}

inline void configureStage(Biquad& stage, Biquad::Type type, double sampleRate,
                           double frequency, double q, double gainDb,
                           int order, int stageIndex, double slopeDbPerOct, bool decramp,
                           double precomputedBellGain = std::numeric_limits<double>::quiet_NaN())
{
    (void)slopeDbPerOct;
    const bool squareBell = type == Biquad::Type::Bell && order > 1;
    double stageFrequency = squareBell
        ? bellStageFrequency(frequency, q, order, stageIndex) : frequency;
    double stageQ = squareBell ? bellStageQ(q, order) : q;
    double gain = squareBell
        ? (std::isfinite(precomputedBellGain) ? precomputedBellGain
                                              : bellStageGain(sampleRate, frequency, q, gainDb, order, decramp))
        : stageGain(type, gainDb, order);
    if (decramp) stage.setMatched(type, sampleRate, stageFrequency, stageQ, gain);
    else         stage.set(type, sampleRate, stageFrequency, stageQ, gain);
}

inline Biquad designStage(Biquad::Type type, double sampleRate, double frequency,
                          double q, double gainDb, int order, bool decramp)
{
    Biquad stage;
    configureStage(stage, type, sampleRate, frequency, q, gainDb, order, 0, 12.0, decramp);
    return stage;
}

inline std::complex<double> nonCutResponse(Biquad::Type type, double sampleRate,
                                           double centerFrequency, double q,
                                           double gainDb, double slopeDbPerOct,
                                           bool decramp, double probeFrequency)
{
    const float position = shapePosition((float)slopeDbPerOct);
    const int lowOrder = std::clamp((int)std::floor(position), 1, maxOrder);
    const int highOrder = std::min(maxOrder, lowOrder + 1);
    const double mix = highOrder == lowOrder ? 0.0 : position - (double)lowOrder;
    const auto legacyOrderResponse = [&](int order)
    {
        std::complex<double> result { 1.0, 0.0 };
        const double bellGain = type == Biquad::Type::Bell && order > 1
            ? bellStageGain(sampleRate, centerFrequency, q, gainDb, order, decramp)
            : std::numeric_limits<double>::quiet_NaN();
        for (int stageIndex = 0; stageIndex < order; ++stageIndex)
        {
            Biquad stage;
            configureStage(stage, type, sampleRate, centerFrequency, q, gainDb,
                           order, stageIndex, slopeDbPerOct, decramp, bellGain);
            result *= response(stage, probeFrequency, sampleRate);
        }
        return result;
    };
    if (!distributesGain(type))
    {
        const auto low = legacyOrderResponse(lowOrder);
        const auto high = legacyOrderResponse(highOrder);
        return low + mix * (high - low);
    }
    if (type == Biquad::Type::Bell)
    {
        const auto low = legacyOrderResponse(lowOrder);
        const auto high = legacyOrderResponse(highOrder);
        return low + mix * (high - low);
    }
    const int crossoverOrder = std::clamp((int)std::lround(position), 1, maxOrder);

    const auto crossoverForOrder = [&](int order, double cutoff, bool highPass)
    {
        std::complex<double> result { 1.0, 0.0 };
        if ((order & 1) != 0)
        {
            const double k = std::tan(kPi * std::clamp(cutoff, 5.0, sampleRate * 0.49)
                                      / sampleRate);
            const double norm = 1.0 / (1.0 + k);
            const double omega = 2.0 * kPi * probeFrequency / sampleRate;
            const std::complex<double> z1(std::cos(omega), -std::sin(omega));
            const double b0 = highPass ? norm : k * norm;
            const double b1 = highPass ? -norm : b0;
            result *= (b0 + b1 * z1) / (1.0 + (k - 1.0) * norm * z1);
        }
        for (int pair = 0; pair < order / 2; ++pair)
        {
            const double sectionQ = 1.0 / (2.0 * std::sin(
                (2.0 * pair + 1.0) * kPi / (2.0 * order)));
            Biquad section;
            const auto sectionType = highPass ? Biquad::Type::HighPass : Biquad::Type::LowPass;
            if (decramp) section.setMatched(sectionType, sampleRate, cutoff, sectionQ, 0.0);
            else         section.set(sectionType, sampleRate, cutoff, sectionQ, 0.0);
            result *= response(section, probeFrequency, sampleRate);
        }
        return result;
    };
    const auto morphedLinkwitzRiley = [&](bool highPass)
    {
        auto result = std::pow(crossoverForOrder(crossoverOrder, centerFrequency, highPass), 2);
        if (highPass && (crossoverOrder & 1) != 0) result = -result;
        return result;
    };

    const double linearGain = std::pow(10.0, gainDb / 20.0);
    const auto low = morphedLinkwitzRiley(false);
    const auto high = morphedLinkwitzRiley(true);
    std::complex<double> result;
    if (type == Biquad::Type::LowShelf)
        result = linearGain * low + high;
    else if (type == Biquad::Type::HighShelf)
        result = low + linearGain * high;
    else
    {
        const double lowGain = std::pow(10.0, -gainDb / 20.0);
        result = lowGain * low + linearGain * high;
    }
    Biquad resonance;
    const double midpointLinear = type == Biquad::Type::Tilt
        ? 0.5 * (std::pow(10.0, -gainDb / 20.0) + linearGain)
        : 0.5 * (1.0 + linearGain);
    const double midpointTargetDb = type == Biquad::Type::Tilt ? 0.0 : 0.5 * gainDb;
    const double pivotCorrectionDb = midpointTargetDb
        - 20.0 * std::log10(std::max(midpointLinear, 1.0e-15));
    const double resonanceGain = std::clamp(
        pivotCorrectionDb + 6.0 * std::log2(std::max(0.1, q)), -12.0, 12.0);
    if (decramp) resonance.setMatched(Biquad::Type::Bell, sampleRate, centerFrequency, 0.7, resonanceGain);
    else         resonance.set(Biquad::Type::Bell, sampleRate, centerFrequency, 0.7, resonanceGain);
    result *= response(resonance, probeFrequency, sampleRate);
    return result;
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

inline std::complex<double> cutBaseResponse(Biquad::Type type, double sampleRate,
                                            double centerFrequency, double slopeDbPerOct,
                                            double probeFrequency)
{
    const double position = cutStageAmount((float)slopeDbPerOct);
    const int lowOrder = std::clamp((int)std::floor(position), 1, 8);
    const int highOrder = std::min(8, lowOrder + 1);
    const double mix = highOrder == lowOrder ? 0.0 : position - (double)lowOrder;
    const auto responseForOrder = [&](int order)
    {
        std::complex<double> result { 1.0, 0.0 };
        if ((order & 1) != 0)
            result *= firstOrderCutResponse(type == Biquad::Type::HighPass,
                                            sampleRate, centerFrequency, probeFrequency);
        for (int pair = 0; pair < order / 2; ++pair)
        {
            const double sectionQ = 1.0 / (2.0 * std::sin(
                (2.0 * pair + 1.0) * kPi / (2.0 * order)));
            Biquad section;
            section.set(type, sampleRate, centerFrequency, sectionQ, 0.0);
            result *= response(section, probeFrequency, sampleRate);
        }
        return result;
    };
    const auto low = responseForOrder(lowOrder);
    const auto high = responseForOrder(highOrder);
    return low + mix * (high - low);
}

inline double cutResonanceGainDb(double, double q) noexcept
{
    // Q=1 is deliberately neutral. Resonance is an independent bell around
    // the Butterworth cut, never a compensation boost for cascade loss.
    return std::clamp(20.0 * std::log10(std::max(q, 0.1)), -20.0, 20.0);
}

inline Biquad designCutResonance(double sampleRate, double centerFrequency,
                                 double q, double slopeDbPerOct, bool decramp)
{
    Biquad resonance;
    const double gain = cutResonanceGainDb(slopeDbPerOct, q);
    const double resonanceQ = std::clamp(0.7 + 0.35 * std::abs(std::log2(std::max(q, 0.1))), 0.7, 3.0);
    if (decramp) resonance.setMatched(Biquad::Type::Bell, sampleRate, centerFrequency,
                                     resonanceQ, gain);
    else         resonance.set(Biquad::Type::Bell, sampleRate, centerFrequency,
                                resonanceQ, gain);
    return resonance;
}

inline void configureCutResonance(Biquad& resonance, double sampleRate,
                                  double centerFrequency, double q,
                                  double slopeDbPerOct, bool decramp)
{
    const double gain = cutResonanceGainDb(slopeDbPerOct, q);
    const double resonanceQ = std::clamp(0.7 + 0.35 * std::abs(std::log2(std::max(q, 0.1))), 0.7, 3.0);
    if (decramp) resonance.setMatched(Biquad::Type::Bell, sampleRate, centerFrequency,
                                     resonanceQ, gain);
    else         resonance.set(Biquad::Type::Bell, sampleRate, centerFrequency,
                                resonanceQ, gain);
}

inline std::complex<double> cutResponse(Biquad::Type type, double sampleRate,
                                        double centerFrequency, double q,
                                        double slopeDbPerOct, bool decramp,
                                        double probeFrequency)
{
    const auto base = cutBaseResponse(type, sampleRate, centerFrequency,
                                      slopeDbPerOct, probeFrequency);
    const auto resonance = designCutResonance(sampleRate, centerFrequency, q,
                                              slopeDbPerOct, decramp);
    return base * response(resonance, probeFrequency, sampleRate);
}

inline std::complex<double> response(Biquad::Type type, double sampleRate,
                                     double centerFrequency, double q,
                                     double gainDb, double slopeDbPerOct,
                                     bool decramp, double probeFrequency)
{
    if (type == Biquad::Type::HighPass || type == Biquad::Type::LowPass)
        return cutResponse(type, sampleRate, centerFrequency, q,
                           slopeDbPerOct, decramp, probeFrequency);
    return nonCutResponse(type, sampleRate, centerFrequency, q, gainDb,
                          slopeDbPerOct, decramp, probeFrequency);
}
}
