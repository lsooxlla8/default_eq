#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Biquad.h"
#include "../Config.h"
#include <array>

// Channel routing for Mid/Side and L/R independent processing.
enum class ChannelRoute { Stereo = 0, Left = 1, Right = 2, Mid = 3, Side = 4 };

// Saturation / waveshaper modes retained and expanded from the FreeEQ8 base.
enum class SaturationType
{
    Tanh = 0, Tube, Tape, Transistor,
    MorphSoftClip, HardClip, RecursiveFold, SineFold
};

// EQBand with lightweight parameter smoothing and cascaded biquads.
// Supports 1/2/4 cascaded stages for 12/24/48 dB/oct slopes.
struct EQBand
{
    bool enabled = true;
    Biquad::Type type = Biquad::Type::Bell;
    ChannelRoute channelRoute = ChannelRoute::Stereo;

    float freqHz = 1000.0f;
    float Q = 1.0f;
    float gainDb = 0.0f;

    // Targets coming from parameters
    float targetFreqHz = 1000.0f;
    float targetQ = 1.0f;
    float targetGainDb = 0.0f;

    // Drive / saturation (0 = off, 1 = full)
    float driveAmount = 0.0f;
    float driveMix = 1.0f;
    float driveOutputGain = 1.0f;
    SaturationType satType = SaturationType::Tanh;

    // Dynamic EQ state
    bool dynEnabled = false;
    float dynThreshDb = -20.0f;
    float dynRatio = 4.0f;
    float dynRangeDb = 6.0f;
    bool dynUpward = false;
    bool useExternalSidechain = false;
    float dynAttackMs = 10.0f;
    float dynReleaseMs = 100.0f;
    float envLevel = 0.0f;       // current envelope level (linear)
    float dynGainMod = 0.0f;     // current dynamic gain modulation in dB
    float lastSidechainSample = 0.0f;

    // Cascaded biquad stages: 1 = 12 dB/oct, 2 = 24 dB/oct, 4 = 48 dB/oct
    int numStages = 1;
    int fullStages = 1;
    float fractionalStage = 0.0f;
    float slopeDbPerOct = 12.0f;
    bool decrampEnabled = false;
    static constexpr int maxStages = 4;
    std::array<Biquad, maxStages> biquads;

    // Smoothers
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> freqSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> qSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSm;

    // Coefficient update interval while smoothing (in samples)
    int coeffUpdateInterval = 16;
    int intervalCounter = 0;

    void reset(double sampleRate)
    {
        for (auto& bq : biquads)
            bq.reset();
        scBiquad.reset();

        freqSm.reset(sampleRate, 0.02);   // 20ms
        qSm.reset(sampleRate, 0.02);
        gainSm.reset(sampleRate, 0.02);

        freqSm.setCurrentAndTargetValue(freqHz);
        qSm.setCurrentAndTargetValue(Q);
        gainSm.setCurrentAndTargetValue(gainDb);

        targetFreqHz = freqHz;
        targetQ = Q;
        targetGainDb = gainDb;

        envLevel = 0.0f;
        dynGainMod = 0.0f;
        intervalCounter = 0;
    }

    void beginBlock(double sampleRate, bool isEnabled, Biquad::Type newType,
                    float newFreqHz, float newQ, float newGainDb,
                    float newSlopeDbPerOct = 12.0f, ChannelRoute newRoute = ChannelRoute::Stereo,
                    bool useDecramping = false)
    {
        enabled = isEnabled;
        type = newType;
        slopeDbPerOct = std::clamp(newSlopeDbPerOct, 0.0f, 48.0f);
        const float stageAmount = slopeDbPerOct / 12.0f;
        fullStages = std::clamp((int) std::floor(stageAmount), 0, maxStages);
        fractionalStage = stageAmount - (float) fullStages;
        numStages = std::clamp(fullStages + (fractionalStage > 0.0001f ? 1 : 0), 0, maxStages);
        channelRoute = newRoute;
        decrampEnabled = useDecramping;

        targetFreqHz = newFreqHz;
        targetQ = newQ;
        targetGainDb = newGainDb;

        // If targets changed, set smoothers
        if (freqSm.getTargetValue() != targetFreqHz) freqSm.setTargetValue(targetFreqHz);
        if (qSm.getTargetValue() != targetQ)         qSm.setTargetValue(targetQ);
        if (gainSm.getTargetValue() != targetGainDb) gainSm.setTargetValue(targetGainDb);

        // If not smoothing, update coefficients once per block
        if (!enabled) return;

        if (!(freqSm.isSmoothing() || qSm.isSmoothing() || gainSm.isSmoothing()))
        {
            freqHz = targetFreqHz;
            Q = targetQ;
            gainDb = targetGainDb;
            setAllStages(sampleRate);
        }

        intervalCounter = 0;
    }

    inline void maybeUpdateCoeffs(double sampleRate)
    {
        if (!enabled) return;

        // Force periodic coefficient updates when dynamic EQ is active (dynGainMod changes per-sample)
        if (dynEnabled || freqSm.isSmoothing() || qSm.isSmoothing() || gainSm.isSmoothing())
        {
            if (intervalCounter++ >= coeffUpdateInterval)
            {
                intervalCounter = 0;
                freqHz = freqSm.getNextValue();
                Q      = qSm.getNextValue();
                gainDb = gainSm.getNextValue();
                setAllStages(sampleRate);
            }
            else
            {
                (void)freqSm.getNextValue();
                (void)qSm.getNextValue();
                (void)gainSm.getNextValue();
            }
        }
    }

    // Update dynamic EQ envelope from the input signal (call per sample, before process).
    inline void updateDynamicEnvelope(float l, float r, double sampleRate)
    {
        if (!enabled) { dynGainMod = 0.0f; lastSidechainSample = 0.0f; return; }

        // Sidechain: bandpass-filter the input at the band frequency
        const float scMono = (l + r) * 0.5f;
        const float scFiltered = scBiquad.processL(scMono);
        lastSidechainSample = scFiltered;
        if (!dynEnabled) { dynGainMod = 0.0f; return; }
        const float rectified = std::abs(scFiltered);

        // One-pole envelope follower
        const float attackCoeff  = 1.0f - std::exp(-1.0f / (float)(sampleRate * dynAttackMs * 0.001f));
        const float releaseCoeff = 1.0f - std::exp(-1.0f / (float)(sampleRate * dynReleaseMs * 0.001f));

        if (rectified > envLevel)
            envLevel += attackCoeff * (rectified - envLevel);
        else
            envLevel += releaseCoeff * (rectified - envLevel);

        // Compute gain reduction
        const float envDb = 20.0f * std::log10(std::max(envLevel, 1e-7f));
        if (!dynUpward && envDb > dynThreshDb)
        {
            const float overDb = envDb - dynThreshDb;
            dynGainMod = -std::min(dynRangeDb, overDb * (1.0f - 1.0f / dynRatio));
        }
        else if (dynUpward && envDb < dynThreshDb)
        {
            const float underDb = dynThreshDb - envDb;
            dynGainMod = std::min(dynRangeDb, underDb * (1.0f - 1.0f / dynRatio));
        }
        else
        {
            dynGainMod = 0.0f;
        }
    }

    inline void process(float& l, float& r)
    {
        if (!enabled) return;

        // Dynamic gain modulation is baked into filter coefficients via setAllStages(),
        // so no separate pre-filter volume adjustment is needed here.

        switch (channelRoute)
        {
            case ChannelRoute::Stereo:
                l = processLeft(l);
                r = processRight(r);
                break;

            case ChannelRoute::Left:
                l = processLeft(l);
                break;

            case ChannelRoute::Right:
                r = processRight(r);
                break;

            case ChannelRoute::Mid:
            {
                constexpr float invSqrt2 = 0.7071067811865475f;
                float mid = (l + r) * invSqrt2;
                const float side = (l - r) * invSqrt2;
                mid = processLeft(mid);
                l = (mid + side) * invSqrt2;
                r = (mid - side) * invSqrt2;
            } break;

            case ChannelRoute::Side:
            {
                constexpr float invSqrt2 = 0.7071067811865475f;
                const float mid = (l + r) * invSqrt2;
                float side = (l - r) * invSqrt2;
                side = processRight(side);
                l = (mid + side) * invSqrt2;
                r = (mid - side) * invSqrt2;
            } break;
        }

        if (driveAmount > 0.001f)
        {
            const float preDriveL = l;
            const float preDriveR = r;
            switch (channelRoute)
            {
                case ChannelRoute::Stereo: applySaturation(l, r); break;
                case ChannelRoute::Left:   l = saturateOne(l); break;
                case ChannelRoute::Right:  r = saturateOne(r); break;
                case ChannelRoute::Mid:
                case ChannelRoute::Side:
                    // Mid/Side drive follows the already reconstructed routed
                    // filter signal; filtering remains exactly neutral at 0 dB.
                    applyRoutedSaturation(l, r, channelRoute == ChannelRoute::Mid);
                    break;
            }

            l = (preDriveL + driveMix * (l - preDriveL)) * driveOutputGain;
            r = (preDriveR + driveMix * (r - preDriveR)) * driveOutputGain;
            l = dcBlock(l, dcXL, dcYL);
            r = dcBlock(r, dcXR, dcYR);
        }
    }

    inline float processLeft(float x)
    {
        for (int s = 0; s < fullStages; ++s)
            x = biquads[(size_t)s].processL(x);
        if (fractionalStage > 0.0001f && fullStages < maxStages)
        {
            const float dry = x;
            const float wet = biquads[(size_t)fullStages].processL(x);
            x = dry + fractionalStage * (wet - dry);
        }
        return x;
    }

    inline float processRight(float x)
    {
        for (int s = 0; s < fullStages; ++s)
            x = biquads[(size_t)s].processR(x);
        if (fractionalStage > 0.0001f && fullStages < maxStages)
        {
            const float dry = x;
            const float wet = biquads[(size_t)fullStages].processR(x);
            x = dry + fractionalStage * (wet - dry);
        }
        return x;
    }

    inline float saturateOne(float x) const
    {
        float other = 0.0f;
        applySaturation(x, other);
        return x;
    }

    inline void applyRoutedSaturation(float& l, float& r, bool midRoute) const
    {
        constexpr float invSqrt2 = 0.7071067811865475f;
        float mid = (l + r) * invSqrt2;
        float side = (l - r) * invSqrt2;
        if (midRoute) mid = saturateOne(mid); else side = saturateOne(side);
        l = (mid + side) * invSqrt2;
        r = (mid - side) * invSqrt2;
    }

    // Saturation waveshaper — applies gain-compensated nonlinearity
    inline void applySaturation(float& l, float& r) const
    {
        const float d = 1.0f + driveAmount * 9.0f; // 1x to 10x drive

        switch (satType)
        {
            case SaturationType::Tanh:
            {
                const float invTanhD = 1.0f / std::tanh(d);
                l = std::tanh(l * d) * invTanhD;
                r = std::tanh(r * d) * invTanhD;
                break;
            } break;

            case SaturationType::Tube:
            {
                // Asymmetric soft clip: even harmonics from positive bias
                auto tube = [d](float x) -> float {
                    float xd = x * d;
                    // Positive half clips softer (tube-style asymmetry)
                    if (xd >= 0.0f)
                        return xd / (1.0f + xd);
                    else
                        return xd / (1.0f - 0.5f * xd);
                };
                float normL = tube(1.0f);  // normalise so unity input → unity output
                l = tube(l) / normL;
                r = tube(r) / normL;
            } break;

            case SaturationType::Tape:
            {
                // Arctangent saturation with bias — warm, smooth
                const float invAtanD = 1.0f / std::atan(d);
                l = std::atan(l * d) * invAtanD;
                r = std::atan(r * d) * invAtanD;
            } break;

            case SaturationType::Transistor:
            {
                // Hard clip — aggressive, odd harmonics
                const float invD = 1.0f / d;
                l = std::clamp(l * d, -1.0f, 1.0f) * invD;
                r = std::clamp(r * d, -1.0f, 1.0f) * invD;
            } break;

            // Adapted from the author-owned default_distortion distortion
            // family (revision recorded in THIRD_PARTY_NOTICES.md). These are
            // deliberately reduced, stateless per-band variants: no branding,
            // tables, or engine state is copied into this EQ.
            case SaturationType::MorphSoftClip:
            {
                const auto shape = [d](float x)
                {
                    const float driven = x * d;
                    const float cubic = std::clamp(driven - driven * driven * driven / 3.0f, -2.0f / 3.0f, 2.0f / 3.0f) * 1.5f;
                    const float soft = std::tanh(driven) / std::tanh(d);
                    return 0.5f * (soft + cubic);
                };
                l = shape(l); r = shape(r);
            } break;

            case SaturationType::HardClip:
                l = std::clamp(l * d, -1.0f, 1.0f);
                r = std::clamp(r * d, -1.0f, 1.0f);
                break;

            case SaturationType::RecursiveFold:
            {
                const auto fold = [d](float x)
                {
                    float value = x * d;
                    for (int iteration = 0; iteration < 12; ++iteration)
                    {
                        if (value > 1.0f) value = 2.0f - value;
                        else if (value < -1.0f) value = -2.0f - value;
                        else break;
                    }
                    return std::clamp(value, -1.0f, 1.0f);
                };
                l = fold(l); r = fold(r);
            } break;

            case SaturationType::SineFold:
                l = std::sin(juce::MathConstants<float>::halfPi * l * d);
                r = std::sin(juce::MathConstants<float>::halfPi * r * d);
                break;
        }
    }

private:
    // Sidechain bandpass for dynamic EQ envelope (always RBJ — not audible)
    Biquad scBiquad;
    float dcXL = 0.0f, dcYL = 0.0f, dcXR = 0.0f, dcYR = 0.0f;

    static inline float dcBlock(float x, float& previousX, float& previousY)
    {
        const float y = x - previousX + 0.995f * previousY;
        previousX = x;
        previousY = y;
        return y;
    }

    void setAllStages(double sampleRate)
    {
        // Incorporate dynamic gain modulation into the filter coefficients
        const float effectiveStages = std::max(1.0f, slopeDbPerOct / 12.0f);
        const float effectiveGainDb = (gainDb + dynGainMod) / effectiveStages;

        for (int s = 0; s < numStages; ++s)
            if (decrampEnabled)
                biquads[(size_t)s].setMatched(type, sampleRate, freqHz, Q, effectiveGainDb);
            else
                biquads[(size_t)s].set(type, sampleRate, freqHz, Q, effectiveGainDb);

        // Sidechain bandpass tracks band center frequency for envelope detection
        scBiquad.set(Biquad::Type::Bandpass, sampleRate, freqHz, 2.0, 0.0);
    }
};
