#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Biquad.h"
#include "FilterTypes.h"
#include "ZLFilter.h"
#include "../Config.h"
#include <array>
#include <memory>
#include <limits>
#include <vector>

// Saturation / waveshaper modes retained and expanded from the FreeEQ8 base
// and default_distortion. Soft Clip has indirect Vital lineage but uses a
// modified replacement rather than Vital's rational tanh. Other current
// implementations are author-owned; see THIRD_PARTY_NOTICES.md.
enum class SaturationType
{
    SoftClip = 0, DiodeClipper, TriodeStage, FET,
    Tape, OddEven, PhaseDistortion, SineErosion
};

inline constexpr int kSaturationModeCount = 8;

inline bool saturationModeUsesBipolarCharacter(int mode) noexcept
{
    return mode == static_cast<int>(SaturationType::TriodeStage)
        || mode == static_cast<int>(SaturationType::FET)
        || mode == static_cast<int>(SaturationType::OddEven);
}

// EQBand with lightweight parameter smoothing and continuously morphed
// variable-order responses.
struct EQBand
{
    struct FirstOrderCut
    {
        float b0 = 1.0f, b1 = 0.0f, a1 = 0.0f;
        float targetB0 = 1.0f, targetB1 = 0.0f, targetA1 = 0.0f;
        float stepB0 = 0.0f, stepB1 = 0.0f, stepA1 = 0.0f;
        int coefficientRampRemaining = 0;
        float x1L = 0.0f, y1L = 0.0f, x1R = 0.0f, y1R = 0.0f;
        void reset()
        {
            x1L = y1L = x1R = y1R = 0.0f;
            coefficientRampRemaining = 0;
            targetB0 = b0; targetB1 = b1; targetA1 = a1;
            stepB0 = stepB1 = stepA1 = 0.0f;
        }
        void set(bool highPass, double sampleRate, float frequency, int rampSamples = 0)
        {
            const float k = std::tan(juce::MathConstants<float>::pi
                                      * std::clamp(frequency, 5.0f, (float)sampleRate * 0.49f)
                                      / (float)sampleRate);
            const float norm = 1.0f / (1.0f + k);
            targetB0 = highPass ? norm : k * norm;
            targetB1 = highPass ? -norm : targetB0;
            targetA1 = (k - 1.0f) * norm;
            coefficientRampRemaining = std::max(0, rampSamples);
            if (coefficientRampRemaining == 0)
            {
                b0 = targetB0; b1 = targetB1; a1 = targetA1;
                stepB0 = stepB1 = stepA1 = 0.0f;
            }
            else
            {
                const float inverse = 1.0f / (float)coefficientRampRemaining;
                stepB0 = (targetB0 - b0) * inverse;
                stepB1 = (targetB1 - b1) * inverse;
                stepA1 = (targetA1 - a1) * inverse;
            }
        }
        float process(float x, bool right)
        {
            auto& x1 = right ? x1R : x1L;
            auto& y1 = right ? y1R : y1L;
            const float y = b0 * x + b1 * x1 - a1 * y1;
            x1 = x; y1 = y;
            return y;
        }
        void advanceCoefficientRamp()
        {
            if (coefficientRampRemaining > 0)
            {
                if (--coefficientRampRemaining == 0)
                {
                    b0 = targetB0; b1 = targetB1; a1 = targetA1;
                }
                else
                {
                    b0 += stepB0; b1 += stepB1; a1 += stepA1;
                }
            }
        }
    };

    bool enabled = true;
    Biquad::Type type = Biquad::Type::Bell;
    Biquad::Type parameterType = Biquad::Type::Bell;
    bool midSidePlacement = false;
    float placement = 0.0f;
    float globalAmount = 1.0f;
    float gainScale = 1.0f;
    float appliedGainScale = std::numeric_limits<float>::quiet_NaN();
    float driveGlobalAmount = 1.0f;
    float routeEffectWeight = 1.0f;

    float freqHz = 1000.0f;
    float Q = 1.0f;
    float gainDb = 0.0f;

    // Targets coming from parameters
    float targetFreqHz = 1000.0f;
    float targetQ = 1.0f;
    float targetGainDb = 0.0f;

    // Drive / saturation (0 = off, 1 = full)
    float driveAmount = 0.0f;
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
    float detectorPeakL = 0.0f, detectorPeakR = 0.0f;
    float dynGainMod = 0.0f;     // current dynamic gain modulation in dB
    float dynAttackCoeff = 1.0f;
    float dynReleaseCoeff = 1.0f;
    float cachedDynAttackMs = -1.0f;
    float cachedDynReleaseMs = -1.0f;
    double cachedDynSampleRate = 0.0;

    float slopeDbPerOct = 12.0f;
    float targetSlopeDbPerOct = 12.0f;
    bool decrampEnabled = false;
    zl_filter::Cascade zlCascade;
    static constexpr int maxClassicCutStages = 16;
    std::array<FirstOrderCut, maxClassicCutStages> classicCutStages;
    int classicCutFullStages = 2;
    float classicCutFractionalStage = 0.0f;
    bool classicCutCoefficientRamping = false;

    // Smoothers
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> freqSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> qSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> slopeSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amountSm;

    // Coefficient update interval while smoothing (in samples)
    int coeffUpdateInterval = 16;
    int intervalCounter = 0;
    int dynamicControlCounter = 0;
    bool coefficientsValid = false;

    void reset(double sampleRate)
    {
        scBiquad.reset();
        auditionBiquad.reset();
        driveBandBiquad.reset();
        zlCascade.reset();
        for (auto& stage : classicCutStages) stage.reset();

        freqSm.reset(sampleRate, 0.02);   // 20ms
        qSm.reset(sampleRate, 0.02);
        gainSm.reset(sampleRate, 0.02);
        slopeSm.reset(sampleRate, 0.02);
        amountSm.reset(sampleRate, 0.01);

        freqSm.setCurrentAndTargetValue(freqHz);
        qSm.setCurrentAndTargetValue(Q);
        gainSm.setCurrentAndTargetValue(gainDb);
        slopeSm.setCurrentAndTargetValue(slopeDbPerOct);
        amountSm.setCurrentAndTargetValue(globalAmount);

        targetFreqHz = freqHz;
        targetQ = Q;
        targetGainDb = gainDb;
        targetSlopeDbPerOct = slopeDbPerOct;

        envLevel = 0.0f;
        detectorPeakL = detectorPeakR = 0.0f;
        dynGainMod = 0.0f;
        intervalCounter = 0;
        dynamicControlCounter = 0;
        coefficientsValid = false;
        classicCutCoefficientRamping = false;
        appliedGainScale = std::numeric_limits<float>::quiet_NaN();
        driveProcessSampleRate = 0.0;
        drivePreparedFrequency = drivePreparedQ = -1.0f;
        driveMemoryL = driveMemoryR = 0.0f;
        drivePhaseL = drivePhaseR = 0.0;
        phaseEnvelopeL = phaseEnvelopeR = 0.0f;
        phaseTailGainL = phaseTailGainR = 1.0f;
        phaseSilenceSamplesL = phaseSilenceSamplesR = 0;
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
        const float clampedSlope = zl_filter::isClassicCut(newType)
            ? std::clamp(newSlopeDbPerOct, 3.0f, 96.0f)
            : zl_filter::snapSlope(newType, newSlopeDbPerOct);
        const bool newTypeIsClassicCut = zl_filter::isClassicCut(newType);
        const float clampedPlacement = std::clamp(newPlacement, -1.0f, 1.0f);
        const bool topologyChanged = !coefficientsValid || processSampleRate != sampleRate
            || enabled != isEnabled || type != newType
            || (!newTypeIsClassicCut && slopeDbPerOct != clampedSlope)
            || decrampEnabled != useDecramping;
        const bool gainScaleChanged = !std::isfinite(appliedGainScale)
            || std::abs(appliedGainScale - gainScale) > 1.0e-6f;
        const bool auxiliaryChanged = topologyChanged || targetFreqHz != newFreqHz || targetQ != newQ;
        enabled = isEnabled;
        detectorPeakL = detectorPeakR = 0.0f;
        type = newType;
        targetSlopeDbPerOct = clampedSlope;
        if (!coefficientsValid || !newTypeIsClassicCut)
        {
            slopeDbPerOct = targetSlopeDbPerOct;
            slopeSm.setCurrentAndTargetValue(slopeDbPerOct);
        }
        else if (slopeSm.getTargetValue() != targetSlopeDbPerOct)
            slopeSm.setTargetValue(targetSlopeDbPerOct);
        const float classicAmount = slopeDbPerOct / 6.0f;
        classicCutFullStages = std::clamp((int)std::floor(classicAmount), 0, maxClassicCutStages);
        classicCutFractionalStage = classicAmount - (float)classicCutFullStages;
        midSidePlacement = useMidSidePlacement;
        placement = clampedPlacement;
        decrampEnabled = useDecramping;
        if (!coefficientsValid)
            amountSm.setCurrentAndTargetValue(globalAmount);
        else
            amountSm.setTargetValue(globalAmount);

        targetFreqHz = newFreqHz;
        targetQ = newQ;
        targetGainDb = newGainDb;

        // If targets changed, set smoothers
        if (freqSm.getTargetValue() != targetFreqHz) freqSm.setTargetValue(targetFreqHz);
        if (qSm.getTargetValue() != targetQ)         qSm.setTargetValue(targetQ);
        if (gainSm.getTargetValue() != targetGainDb) gainSm.setTargetValue(targetGainDb);

        if (!enabled) return;

        updateDynamicTimeConstants(sampleRate);
        if (topologyChanged || auxiliaryChanged || gainScaleChanged || !coefficientsValid)
        {
            if (!coefficientsValid)
            {
                freqHz = targetFreqHz;
                Q = targetQ;
                gainDb = targetGainDb;
            }
            setAllStages(sampleRate, true);
            coefficientsValid = true;
            appliedGainScale = gainScale;
        }

    }

    inline void maybeUpdateCoeffs(double sampleRate)
    {
        if (!enabled) return;

        // Force periodic coefficient updates when dynamic EQ is active (dynGainMod changes per-sample)
        if (dynEnabled || freqSm.isSmoothing() || qSm.isSmoothing()
            || gainSm.isSmoothing() || slopeSm.isSmoothing())
        {
            if (intervalCounter++ >= coeffUpdateInterval)
            {
                intervalCounter = 0;
                freqHz = freqSm.getNextValue();
                Q      = qSm.getNextValue();
                gainDb = gainSm.getNextValue();
                slopeDbPerOct = slopeSm.getNextValue();
                const bool movingFrequencyOrQ = freqSm.isSmoothing() || qSm.isSmoothing();
                const bool dynamicallyMovingQ = dynEnabled
                    && (zl_filter::isResonantCut(type) || type == Biquad::Type::Bandpass);
                setAllStages(sampleRate, movingFrequencyOrQ || dynamicallyMovingQ);
            }
            else
            {
                (void)freqSm.getNextValue();
                (void)qSm.getNextValue();
                (void)gainSm.getNextValue();
                (void)slopeSm.getNextValue();
            }
        }
    }

    // Update dynamic EQ envelope from the input signal (call per sample, before process).
    inline void updateDynamicEnvelope(float l, float r, double sampleRate)
    {
        if (!enabled)
        {
            dynGainMod = 0.0f;
            return;
        }

        // Route the detector through the same domain and placement as the
        // audio path. At the centre both components remain fully linked; the
        // endpoints listen exclusively to L/R or M/S respectively.
        const float firstWeight = std::clamp(1.0f - placement, 0.0f, 1.0f);
        const float secondWeight = std::clamp(1.0f + placement, 0.0f, 1.0f);
        float first = l;
        float second = r;
        if (midSidePlacement)
        {
            // Detector calibration uses the conventional -6 dB matrix so a
            // mono signal reads the same at centre-L/R and full-Mid (and an
            // anti-phase signal reads the same at centre-L/R and full-Side).
            // The audible M/S matrix remains energy-preserving.
            first = (l + r) * 0.5f;
            second = (l - r) * 0.5f;
        }

        // Sidechain: bandpass-filter the routed input at the band frequency.
        // Weighting before the filters also keeps the stereo meter honest:
        // an unselected component reads silence rather than showing a source
        // that cannot trigger this band.
        const float scFilteredL = scBiquad.processL(first * firstWeight);
        const float scFilteredR = scBiquad.processR(second * secondWeight);
        detectorPeakL = std::max(detectorPeakL, std::abs(scFilteredL));
        detectorPeakR = std::max(detectorPeakR, std::abs(scFilteredR));
        if (!dynEnabled)
        {
            dynGainMod = 0.0f;
            return;
        }
        const float rectified = std::max(std::abs(scFilteredL), std::abs(scFilteredR));

        if (rectified > envLevel)
            envLevel += dynAttackCoeff * (rectified - envLevel);
        else
            envLevel += dynReleaseCoeff * (rectified - envLevel);

        // The envelope itself remains sample-accurate. The transfer curve and
        // coefficient target are control-rate values, which removes almost all
        // per-sample log work without making the result block-size dependent.
        if (++dynamicControlCounter < coeffUpdateInterval)
            return;
        dynamicControlCounter = 0;

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
        processEqualizer(l, r);
        processDrive(l, r);
    }

    inline void processEqualizer(float& l, float& r)
    {
        if (!enabled) return;
        const float wetAmount = amountSm.getNextValue();
        const float firstWeight = std::clamp(1.0f - placement, 0.0f, 1.0f);
        const float secondWeight = std::clamp(1.0f + placement, 0.0f, 1.0f);
        if (!midSidePlacement)
        {
            const float dryL = l, dryR = r;
            l += routeEffectWeight * wetAmount * firstWeight * (processLeft(dryL) - dryL);
            r += routeEffectWeight * wetAmount * secondWeight * (processRight(dryR) - dryR);
        }
        else
        {
            constexpr float invSqrt2 = 0.7071067811865475f;
            const float dryMid = (l + r) * invSqrt2;
            const float drySide = (l - r) * invSqrt2;
            const float mid = dryMid + routeEffectWeight * wetAmount * firstWeight * (processLeft(dryMid) - dryMid);
            const float side = drySide + routeEffectWeight * wetAmount * secondWeight * (processRight(drySide) - drySide);
            l = (mid + side) * invSqrt2;
            r = (mid - side) * invSqrt2;
        }
    }

    inline void processDrive(float& l, float& r)
    {
        if (enabled && driveAmount > 0.0f)
            applyBandDrive(l, r);
    }

    bool driveActive() const noexcept { return enabled && driveAmount > 0.0f; }

    static float dynamicClassicCutSlope(float baseSlope, float dynamicModDb) noexcept
    {
        return std::clamp(baseSlope - dynamicModDb * 6.0f, 3.0f, 96.0f);
    }

    static float dynamicResonantCutQ(float baseQ, float dynamicModDb) noexcept
    {
        return std::clamp(baseQ * std::pow(2.0f, dynamicModDb / 6.0f), 0.1f, 24.0f);
    }

    static float amountResonantCutQ(float baseQ, float amount) noexcept
    {
        const float strength = std::clamp(amount, 0.0f, 1.0f);
        return deq::filter_types::resonantCutDefaultQ
            + (baseQ - deq::filter_types::resonantCutDefaultQ) * strength;
    }

    static float cutAmountMix(float amount) noexcept
    {
        // The cutoff itself already moves towards a neutral edge as Amount
        // approaches zero. This short smooth easing makes the final
        // few percent converge to an exact dry signal instead of hard-bypassing
        // the filter between the 1% and 0% parameter steps.
        const float x = std::clamp(amount / 0.05f, 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    }

    static float dynamicBandPassQ(float baseQ, float dynamicModDb) noexcept
    {
        return std::clamp(baseQ * std::pow(2.0f, -dynamicModDb / 6.0f), 0.1f, 24.0f);
    }

    void prepareDriveRate(double sampleRate)
    {
        if (driveProcessSampleRate == sampleRate && drivePreparedFrequency == freqHz
            && drivePreparedQ == Q)
            return;
        driveProcessSampleRate = sampleRate;
        driveBandBiquad.set(Biquad::Type::Bandpass, sampleRate, freqHz,
                            std::clamp(Q, 0.2f, 12.0f), 0.0);
        drivePreparedFrequency = freqHz;
        drivePreparedQ = Q;
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
        if (zl_filter::isClassicCut(type)) return processClassicCut(x, false);
        return zlCascade.processL(x);
    }

    inline float processRight(float x)
    {
        if (zl_filter::isClassicCut(type)) return processClassicCut(x, true);
        return zlCascade.processR(x);
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
                const float cubicInput = std::clamp(x * d, -1.0f, 1.0f);
                const float cubic = 1.5f * (cubicInput
                    - cubicInput * cubicInput * cubicInput / 3.0f);
                return juce::jmap(c, std::tanh(x * d), cubic);
            }
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
            case SaturationType::FET:
            {
                const float gateBias = -0.12f + 1.18f * (c * 2.0f - 1.0f);
                const float gate = std::max(0.0f, x * d + gateBias);
                const float base = std::max(0.0f, gateBias);
                return std::tanh(2.2f * (gate * gate - base * base) / (1.0f + 0.55f * std::abs(c * 2.0f - 1.0f)));
            }
            case SaturationType::Tape:
            {
                // Stable stateful hysteresis loop using the same Drive /
                // Hysteresis / Bias semantics as default_distortion.
                const float bias = (driveSecondary * 2.0f - 1.0f) * 0.18f;
                const float target = std::tanh((x * d + bias) + c * 0.8f * memory);
                memory += (0.02f + 0.18f * (1.0f - c)) * (target - memory);
                return 0.72f * target + 0.28f * memory;
            }
            case SaturationType::OddEven:
            {
                const float bounded = std::tanh(x * d);
                const float second = 2.0f * bounded * bounded - 1.0f;
                const float third = 4.0f * bounded * bounded * bounded - 3.0f * bounded;
                return juce::jmap(c, second, third);
            }
            case SaturationType::PhaseDistortion:
                return processPhaseDistortion(x, c, driveAmount, rightState);
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
    float dcXL = 0.0f, dcYL = 0.0f, dcXR = 0.0f, dcYR = 0.0f;
    float driveMemoryL = 0.0f, driveMemoryR = 0.0f;
    std::vector<float> driveDelayL, driveDelayR;
    int driveDelayPosL = 0, driveDelayPosR = 0;
    double drivePhaseL = 0.0, drivePhaseR = 0.0;
    double driveProcessSampleRate = 0.0;
    float drivePreparedFrequency = -1.0f, drivePreparedQ = -1.0f;
    float phaseEnvelopeL = 0.0f, phaseEnvelopeR = 0.0f;
    float phaseTailGainL = 1.0f, phaseTailGainR = 1.0f;
    int phaseSilenceSamplesL = 0, phaseSilenceSamplesR = 0;

    inline float processPhaseDistortion(float x, float character, float depth,
                                        bool rightState)
    {
        auto& delay = rightState ? driveDelayR : driveDelayL;
        auto& pos = rightState ? driveDelayPosR : driveDelayPosL;
        auto& envelope = rightState ? phaseEnvelopeR : phaseEnvelopeL;
        auto& tailGain = rightState ? phaseTailGainR : phaseTailGainL;
        auto& silenceSamples = rightState ? phaseSilenceSamplesR : phaseSilenceSamplesL;
        const int delaySize = (int)delay.size();
        if (delaySize < 2 || driveProcessSampleRate <= 0.0) return x;

        const float c01 = std::clamp(character, 0.0f, 1.0f);
        const float nyquistSafe = (float)std::max(40.0, 0.45 * driveProcessSampleRate);
        const float toneHz = 40.0f * std::pow(nyquistSafe / 40.0f, c01);
        const float toneCoefficient = c01 >= 0.999f ? 0.0f
            : (float)std::exp(-juce::MathConstants<double>::twoPi * toneHz
                             / driveProcessSampleRate);
        envelope = (1.0f - toneCoefficient) * x + toneCoefficient * envelope;

        const float boundedDepth = std::clamp(depth, 0.0f, 1.0f);
        const float modulator = std::clamp(envelope, 0.0f, 1.0f);
        const float delaySamples = std::clamp(0.001f * 50.0f * boundedDepth * modulator
                                                * (float)driveProcessSampleRate,
                                              0.0f, (float)(delaySize - 2));
        delay[(size_t)pos] = x;
        float readPosition = (float)pos - delaySamples;
        while (readPosition < 0.0f) readPosition += (float)delaySize;
        const int first = (int)std::floor(readPosition) % delaySize;
        const int second = (first + 1) % delaySize;
        const float delayed = juce::jmap(readPosition - (float)first,
                                        delay[(size_t)first], delay[(size_t)second]);
        pos = (pos + 1) % delaySize;

        constexpr float silenceThreshold = 3.16227766e-5f;
        const int silenceHold = std::max(8, juce::roundToInt(0.001 * driveProcessSampleRate));
        if (std::abs(x) <= silenceThreshold)
            ++silenceSamples;
        else
        {
            silenceSamples = 0;
            tailGain = 1.0f;
        }
        if (silenceSamples >= silenceHold)
            tailGain = std::max(0.0f, tailGain
                - (float)(1.0 / std::max(1.0, 0.006 * driveProcessSampleRate)));
        return delayed * tailGain;
    }

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
        if (frequency <= 0.0f) return x;
        phase += juce::MathConstants<double>::twoPi * frequency / std::max(1.0, driveProcessSampleRate);
        if (phase >= juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
        memory = 0.97f * memory + 0.03f * std::sin((float)phase * 12.9898f + 78.233f);
        const float source = juce::jmap(noiseMix, std::sin((float)phase), std::tanh(memory * 2.0f));
        const int delayCapacity = (int)delay.size();
        if (delayCapacity < 2) return x;
        const float delaySamples = std::clamp(0.001f * 50.0f * std::pow(depth, 2.5849625f)
                                              * (0.5f + 0.5f * source) * (float)driveProcessSampleRate,
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

    // The v0.1.0 cut: identical first-order stages and a continuously wet/dry
    // final stage. Q is deliberately absent from this path.
    inline float processClassicCut(float x, bool right)
    {
        if (!right && classicCutCoefficientRamping)
        {
            bool remains = false;
            const int activeStages = std::min(maxClassicCutStages,
                classicCutFullStages + (classicCutFractionalStage > 0.0001f ? 1 : 0));
            for (int stage = 0; stage < activeStages; ++stage)
            {
                classicCutStages[(size_t)stage].advanceCoefficientRamp();
                remains = remains
                    || classicCutStages[(size_t)stage].coefficientRampRemaining > 0;
            }
            classicCutCoefficientRamping = remains;
        }
        for (int stage=0; stage<classicCutFullStages; ++stage)
            x=classicCutStages[(size_t)stage].process(x,right);
        if (classicCutFractionalStage>0.0001f && classicCutFullStages<maxClassicCutStages)
        {
            const float wet=classicCutStages[(size_t)classicCutFullStages].process(x,right);
            x += classicCutFractionalStage*(wet-x);
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

    inline void applyBandDrive(float& l, float& r)
    {
        constexpr float invSqrt2 = 0.7071067811865475f;
        const auto driveOneBand = [this](float band, bool rightState)
        {
            float wet = saturateOne(band, rightState) * driveAutoGainLinear;
            wet = rightState ? dcBlock(wet, dcXR, dcYR) : dcBlock(wet, dcXL, dcYL);
            return wet - band;
        };
        const float firstWeight = std::clamp(1.0f - placement, 0.0f, 1.0f);
        const float secondWeight = std::clamp(1.0f + placement, 0.0f, 1.0f);
        if (!midSidePlacement)
        {
            const float bandL = driveBandBiquad.processL(l);
            const float bandR = driveBandBiquad.processR(r);
            l += routeEffectWeight * driveGlobalAmount * firstWeight * driveOneBand(bandL, false);
            r += routeEffectWeight * driveGlobalAmount * secondWeight * driveOneBand(bandR, true);
        }
        else
        {
            float mid = (l + r) * invSqrt2;
            float side = (l - r) * invSqrt2;
            mid += routeEffectWeight * driveGlobalAmount * firstWeight * driveOneBand(driveBandBiquad.processL(mid), false);
            side += routeEffectWeight * driveGlobalAmount * secondWeight * driveOneBand(driveBandBiquad.processR(side), true);
            l = (mid + side) * invSqrt2;
            r = (mid - side) * invSqrt2;
        }
    }

    void updateDynamicTimeConstants(double sampleRate)
    {
        if (cachedDynSampleRate == sampleRate && cachedDynAttackMs == dynAttackMs
            && cachedDynReleaseMs == dynReleaseMs)
            return;
        const float safeAttack = std::max(0.01f, dynAttackMs);
        const float safeRelease = std::max(0.01f, dynReleaseMs);
        dynAttackCoeff = 1.0f - std::exp(-1.0f / (float)(sampleRate * safeAttack * 0.001f));
        dynReleaseCoeff = 1.0f - std::exp(-1.0f / (float)(sampleRate * safeRelease * 0.001f));
        cachedDynSampleRate = sampleRate;
        cachedDynAttackMs = dynAttackMs;
        cachedDynReleaseMs = dynReleaseMs;
    }

    void setAllStages(double sampleRate, bool updateAuxiliary)
    {
        processSampleRate = sampleRate;
        const bool classicCut = zl_filter::isClassicCut(type);
        const bool resonantCut = zl_filter::isResonantCut(type);
        const bool bandPass = type == Biquad::Type::Bandpass;
        const float totalGainDb = (gainDb + (classicCut || resonantCut || bandPass ? 0.0f : dynGainMod))
            * gainScale;
        const float dynamicSlope = classicCut
            ? dynamicClassicCutSlope(slopeDbPerOct, dynGainMod)
            : slopeDbPerOct;
        const float dynamicQ = resonantCut ? dynamicResonantCutQ(Q, dynGainMod)
            : bandPass ? dynamicBandPassQ(Q, dynGainMod) : Q;
        if (zl_filter::isClassicCut(type))
        {
            const float classicAmount = dynamicSlope / 6.0f;
            classicCutFullStages = std::clamp((int)std::floor(classicAmount), 0,
                                              maxClassicCutStages);
            classicCutFractionalStage = classicAmount - (float)classicCutFullStages;
            const bool highPass=type==Biquad::Type::HighPass;
            const int rampSamples = coefficientsValid ? coeffUpdateInterval + 1 : 0;
            classicCutCoefficientRamping = rampSamples > 0;
            for(auto& stage:classicCutStages) stage.set(highPass,sampleRate,freqHz,rampSamples);
        }
        else
        {
            zlCascade.configure(type,sampleRate,freqHz,dynamicQ,totalGainDb,slopeDbPerOct,
                                coefficientsValid ? coeffUpdateInterval + 1 : 0);
        }
        if(updateAuxiliary)
        {
            const float detectorQ = classicCut ? 2.0f
                : std::clamp(dynamicQ, 0.2f, 12.0f);
            scBiquad.set(Biquad::Type::Bandpass,sampleRate,freqHz,detectorQ,0.0);
            auditionBiquad.set(Biquad::Type::Bandpass,sampleRate,freqHz,std::clamp(Q,0.2f,12.0f),0.0);
        }
    }
    double processSampleRate = 44100.0;
};
