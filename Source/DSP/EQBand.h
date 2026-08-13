#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Biquad.h"
#include "../Config.h"
#include <array>
#include <vector>

// Saturation / waveshaper modes retained and expanded from the FreeEQ8 base.
enum class SaturationType
{
    SoftClip = 0, HardClip, DiodeClipper, TriodeStage, TransistorFET,
    TapeHysteresis, HarmonicMorph, PhaseDistortion, SpectralClip, SineErosion
};

// EQBand with lightweight parameter smoothing and cascaded biquads.
// Supports 1/2/4 cascaded stages for 12/24/48 dB/oct slopes.
struct EQBand
{
    bool enabled = true;
    Biquad::Type type = Biquad::Type::Bell;
    bool midSidePlacement = false;
    float placement = 0.0f;

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
    SaturationType satType = SaturationType::SoftClip;
    float driveCharacter = 0.0f;
    float driveSecondary = 0.0f;
    float driveAutoGainLinear = 1.0f;

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
        auditionBiquad.reset();
        driveBandBiquad.reset();
        for (auto& cut : cutStages) cut.reset();

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
        driveMemoryL = driveMemoryR = 0.0f;
        drivePhaseL = drivePhaseR = 0.0;
        driveDelayPosL = driveDelayPosR = 0;
        const size_t maximumDelay = (size_t)std::ceil(sampleRate * 8.0 * 0.05) + 2u;
        driveDelayL.assign(maximumDelay, 0.0f);
        driveDelayR.assign(maximumDelay, 0.0f);
    }

    void beginBlock(double sampleRate, bool isEnabled, Biquad::Type newType,
                    float newFreqHz, float newQ, float newGainDb,
                    float newSlopeDbPerOct = 12.0f, bool useMidSidePlacement = false,
                    float newPlacement = 0.0f,
                    bool useDecramping = false)
    {
        enabled = isEnabled;
        type = newType;
        slopeDbPerOct = std::clamp(newSlopeDbPerOct, 3.0f, 48.0f);
        const float stageAmount = slopeDbPerOct / 12.0f;
        fullStages = std::clamp((int) std::floor(stageAmount), 0, maxStages);
        fractionalStage = stageAmount - (float) fullStages;
        numStages = std::clamp(fullStages + (fractionalStage > 0.0001f ? 1 : 0), 0, maxStages);
        const float cutStageAmount = slopeDbPerOct / 6.0f;
        cutFullStages = std::clamp((int)std::floor(cutStageAmount), 0, maxCutStages);
        cutFractionalStage = cutStageAmount - (float)cutFullStages;
        midSidePlacement = useMidSidePlacement;
        placement = std::clamp(newPlacement, -1.0f, 1.0f);
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
        const float firstWeight = std::clamp(1.0f - placement, 0.0f, 1.0f);
        const float secondWeight = std::clamp(1.0f + placement, 0.0f, 1.0f);
        if (!midSidePlacement)
        {
            const float dryL = l, dryR = r;
            l += firstWeight * (processLeft(dryL) - dryL);
            r += secondWeight * (processRight(dryR) - dryR);
        }
        else
        {
            constexpr float invSqrt2 = 0.7071067811865475f;
            const float dryMid = (l + r) * invSqrt2;
            const float drySide = (l - r) * invSqrt2;
            const float mid = dryMid + firstWeight * (processLeft(dryMid) - dryMid);
            const float side = drySide + secondWeight * (processRight(drySide) - drySide);
            l = (mid + side) * invSqrt2;
            r = (mid - side) * invSqrt2;
        }

        if (driveAmount > 0.001f)
            applySpectralDrive(l, r);
    }

    // A solo is an audition operation, not merely "disable the other EQ
    // stages": it returns only the frequency window represented by this node.
    inline void processAudition(float& l, float& r)
    {
        const float firstWeight = std::clamp(1.0f - placement, 0.0f, 1.0f);
        const float secondWeight = std::clamp(1.0f + placement, 0.0f, 1.0f);
        if (!midSidePlacement)
        {
            l = firstWeight * auditionBiquad.processL(l);
            r = secondWeight * auditionBiquad.processR(r);
        }
        else
        {
            constexpr float invSqrt2 = 0.7071067811865475f;
            const float mid = firstWeight * auditionBiquad.processL((l + r) * invSqrt2);
            const float side = secondWeight * auditionBiquad.processR((l - r) * invSqrt2);
            l = (mid + side) * invSqrt2;
            r = (mid - side) * invSqrt2;
        }
    }

    inline float processLeft(float x)
    {
        if (type == Biquad::Type::HighPass || type == Biquad::Type::LowPass)
            return processCut(x, false);
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
        if (type == Biquad::Type::HighPass || type == Biquad::Type::LowPass)
            return processCut(x, true);
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

    inline float saturateOne(float x, bool rightState = false)
    {
        const float d = 1.0f + driveAmount * 15.0f;
        const float c = std::clamp(driveCharacter, 0.0f, 1.0f);
        float& memory = rightState ? driveMemoryR : driveMemoryL;
        switch (satType)
        {
            case SaturationType::SoftClip:
            {
                const float cubic = std::clamp(x * d - std::pow(x * d, 3.0f) / 3.0f,
                                                -2.0f / 3.0f, 2.0f / 3.0f) * 1.5f;
                return juce::jmap(c, std::tanh(x * d), cubic);
            }
            case SaturationType::HardClip:
                return std::clamp(juce::jmap(c, x * d, std::tanh(x * d)), -1.0f, 1.0f);
            case SaturationType::DiodeClipper:
            {
                const float driven = x * d;
                const float feedback = std::atan(1.8f * driven) / std::atan(1.8f);
                const float ground = std::copysign(1.0f - std::exp(-2.8f * std::abs(driven)), driven);
                return juce::jmap(c, feedback, ground);
            }
            case SaturationType::TriodeStage:
            {
                const float bias = (c * 2.0f - 1.0f) * 0.58f;
                const float grid = x * d + bias;
                const float curve = 1.08f * grid + 0.34f * grid * grid - 0.055f * grid * grid * grid;
                const float zero = std::tanh(1.08f * bias + 0.34f * bias * bias - 0.055f * bias * bias * bias);
                return std::clamp((std::tanh(curve) - zero) / std::max(0.1f, 1.0f - std::abs(zero)), -1.0f, 1.0f);
            }
            case SaturationType::TransistorFET:
            {
                const float gateBias = -0.12f + 1.18f * (c * 2.0f - 1.0f);
                const float gate = std::max(0.0f, x * d + gateBias);
                const float base = std::max(0.0f, gateBias);
                return std::tanh(2.2f * (gate * gate - base * base) / (1.0f + 0.55f * std::abs(c * 2.0f - 1.0f)));
            }
            case SaturationType::TapeHysteresis:
            {
                // Stable stateful hysteresis loop using the same Drive /
                // Hysteresis / Bias semantics as default_distortion.
                const float bias = (driveSecondary * 2.0f - 1.0f) * 0.18f;
                const float target = std::tanh((x * d + bias) + c * 0.8f * memory);
                memory += (0.02f + 0.18f * (1.0f - c)) * (target - memory);
                return 0.72f * target + 0.28f * memory;
            }
            case SaturationType::HarmonicMorph:
            {
                const float bounded = std::tanh(x * d);
                const float second = 2.0f * bounded * bounded - 1.0f;
                const float third = 4.0f * bounded * bounded * bounded - 3.0f * bounded;
                return juce::jmap(c, second, third);
            }
            case SaturationType::PhaseDistortion:
                return processModulatedDelay(x, c, driveAmount, 0.0f, rightState);
            case SaturationType::SpectralClip:
            {
                // Per-band magnitude shaper. The surrounding band extractor
                // supplies the spectral isolation; Character controls knee.
                const float threshold = juce::jmap(c, 0.9f, 0.08f);
                const float driven = x * d;
                const float mag = std::abs(driven);
                const float clipped = threshold * std::tanh(mag / std::max(0.001f, threshold));
                return std::copysign(clipped, driven);
            }
            case SaturationType::SineErosion:
                return processModulatedDelay(x, c, driveAmount, driveSecondary, rightState);
        }
        return x;
    }

    inline void applyRoutedSaturation(float& l, float& r, bool midRoute)
    {
        constexpr float invSqrt2 = 0.7071067811865475f;
        float mid = (l + r) * invSqrt2;
        float side = (l - r) * invSqrt2;
        if (midRoute) mid = saturateOne(mid, false); else side = saturateOne(side, true);
        l = (mid + side) * invSqrt2;
        r = (mid - side) * invSqrt2;
    }

    // Saturation waveshaper — applies gain-compensated nonlinearity
    inline void applySaturation(float& l, float& r)
    {
        const float d = 1.0f + driveAmount * 9.0f; // 1x to 10x drive

        juce::ignoreUnused(d);
        l = saturateOne(l, false);
        r = saturateOne(r, true);
    }

private:
    // Sidechain bandpass for dynamic EQ envelope (always RBJ — not audible)
    Biquad scBiquad;
    Biquad auditionBiquad;
    Biquad driveBandBiquad;
    struct FirstOrderCut
    {
        float b0 = 1.0f, b1 = 0.0f, a1 = 0.0f;
        float x1L = 0.0f, y1L = 0.0f, x1R = 0.0f, y1R = 0.0f;
        void reset() { x1L = y1L = x1R = y1R = 0.0f; }
        void set(bool highPass, double sampleRate, float frequency)
        {
            const float k = std::tan(juce::MathConstants<float>::pi
                                      * std::clamp(frequency, 5.0f, (float)sampleRate * 0.49f)
                                      / (float)sampleRate);
            const float norm = 1.0f / (1.0f + k);
            if (highPass) { b0 = norm; b1 = -norm; }
            else          { b0 = k * norm; b1 = b0; }
            a1 = (k - 1.0f) * norm;
        }
        float process(float x, bool right)
        {
            auto& x1 = right ? x1R : x1L;
            auto& y1 = right ? y1R : y1L;
            const float y = b0 * x + b1 * x1 - a1 * y1;
            x1 = x; y1 = y;
            return y;
        }
    };
    static constexpr int maxCutStages = 8;
    std::array<FirstOrderCut, maxCutStages> cutStages;
    int cutFullStages = 2;
    float cutFractionalStage = 0.0f;
    float dcXL = 0.0f, dcYL = 0.0f, dcXR = 0.0f, dcYR = 0.0f;
    float driveMemoryL = 0.0f, driveMemoryR = 0.0f;
    std::vector<float> driveDelayL, driveDelayR;
    int driveDelayPosL = 0, driveDelayPosR = 0;
    double drivePhaseL = 0.0, drivePhaseR = 0.0;

    inline float processModulatedDelay(float x, float character, float depth,
                                       float noiseMix, bool rightState)
    {
        auto& delay = rightState ? driveDelayR : driveDelayL;
        auto& pos = rightState ? driveDelayPosR : driveDelayPosL;
        auto& phase = rightState ? drivePhaseR : drivePhaseL;
        auto& memory = rightState ? driveMemoryR : driveMemoryL;
        const float frequency = character <= 0.5f
            ? 4000.0f * character * character
            : 1000.0f * std::pow(10.0f, 2.0f * character - 1.0f);
        phase += juce::MathConstants<double>::twoPi * frequency / std::max(1.0, processSampleRate);
        if (phase >= juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
        memory = 0.97f * memory + 0.03f * std::sin((float)phase * 12.9898f + 78.233f);
        const float source = juce::jmap(noiseMix, std::sin((float)phase), std::tanh(memory * 2.0f));
        const int delayCapacity = (int)delay.size();
        if (delayCapacity < 2) return x;
        const float delaySamples = std::clamp(0.001f * 50.0f * std::pow(depth, 2.5849625f)
                                              * (0.5f + 0.5f * source) * (float)processSampleRate,
                                              0.0f, (float)(delayCapacity - 2));
        delay[(size_t)pos] = x;
        float read = (float)pos - delaySamples;
        while (read < 0.0f) read += (float)delayCapacity;
        const int first = (int)read % delayCapacity;
        const int second = (first + 1) % delayCapacity;
        const float out = juce::jmap(read - std::floor(read), delay[(size_t)first], delay[(size_t)second]);
        pos = (pos + 1) % delayCapacity;
        return out;
    }

    inline float processCut(float x, bool right)
    {
        for (int stage = 0; stage < cutFullStages; ++stage)
            x = cutStages[(size_t)stage].process(x, right);
        if (cutFractionalStage > 0.0001f && cutFullStages < maxCutStages)
        {
            const float wet = cutStages[(size_t)cutFullStages].process(x, right);
            x += cutFractionalStage * (wet - x);
        }
        return x;
    }

    static inline float dcBlock(float x, float& previousX, float& previousY)
    {
        const float y = x - previousX + 0.995f * previousY;
        previousX = x;
        previousY = y;
        return y;
    }

    inline void applySpectralDrive(float& l, float& r)
    {
        constexpr float invSqrt2 = 0.7071067811865475f;
        const auto driveOneBand = [this](float band, bool rightState)
        {
            float wet = saturateOne(band, rightState) * driveAutoGainLinear * driveOutputGain;
            wet = rightState ? dcBlock(wet, dcXR, dcYR) : dcBlock(wet, dcXL, dcYL);
            return driveMix * (wet - band);
        };
        const float firstWeight = std::clamp(1.0f - placement, 0.0f, 1.0f);
        const float secondWeight = std::clamp(1.0f + placement, 0.0f, 1.0f);
        if (!midSidePlacement)
        {
            const float bandL = driveBandBiquad.processL(l);
            const float bandR = driveBandBiquad.processR(r);
            l += firstWeight * driveOneBand(bandL, false);
            r += secondWeight * driveOneBand(bandR, true);
        }
        else
        {
            float mid = (l + r) * invSqrt2;
            float side = (l - r) * invSqrt2;
            mid += firstWeight * driveOneBand(driveBandBiquad.processL(mid), false);
            side += secondWeight * driveOneBand(driveBandBiquad.processR(side), true);
            l = (mid + side) * invSqrt2;
            r = (mid - side) * invSqrt2;
        }
    }

    void setAllStages(double sampleRate)
    {
        processSampleRate = sampleRate;
        // Incorporate dynamic gain modulation into the filter coefficients
        const float effectiveStages = std::max(1.0f, slopeDbPerOct / 12.0f);
        const float effectiveGainDb = (gainDb + dynGainMod) / effectiveStages;

        for (int s = 0; s < numStages; ++s)
            if (decrampEnabled)
                biquads[(size_t)s].setMatched(type, sampleRate, freqHz, Q, effectiveGainDb);
            else
                biquads[(size_t)s].set(type, sampleRate, freqHz, Q, effectiveGainDb);

        const bool highPass = type == Biquad::Type::HighPass;
        for (auto& stage : cutStages)
            stage.set(highPass, sampleRate, freqHz);

        // Sidechain bandpass tracks band center frequency for envelope detection
        scBiquad.set(Biquad::Type::Bandpass, sampleRate, freqHz, 2.0, 0.0);
        const float auditionQ = std::clamp(Q, 0.2f, 12.0f);
        auditionBiquad.set(Biquad::Type::Bandpass, sampleRate, freqHz, auditionQ, 0.0);
        driveBandBiquad.set(Biquad::Type::Bandpass, sampleRate, freqHz, auditionQ, 0.0);
    }
    double processSampleRate = 44100.0;
};
