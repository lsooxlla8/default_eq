#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Biquad.h"
#include "VariableSlope.h"
#include "../Config.h"
#include <array>
#include <memory>
#include <limits>
#include <vector>

// Saturation / waveshaper modes retained and expanded from the FreeEQ8 base
// and default_distortion. The latter has nested Vital and CHOW/BYOD lineage;
// this file uses modified replacements rather than Vital's rational tanh or
// the CHOW/BYOD Jiles-Atherton model. See THIRD_PARTY_NOTICES.md.
enum class SaturationType
{
    SoftClip = 0, HardClip, DiodeClipper, TriodeStage, TransistorFET,
    TapeHysteresis, HarmonicMorph, PhaseDistortion, SpectralClip, SineErosion
};

// EQBand with lightweight parameter smoothing and continuously morphed
// variable-order responses.
struct EQBand
{
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

    struct CutOrderPath
    {
        FirstOrderCut firstOrder;
        std::array<Biquad, 4> pairs;
        int pairCount = 0;
        bool hasFirstOrder = false;

        void reset()
        {
            firstOrder.reset();
            for (auto& pair : pairs) pair.reset();
        }
    };

    bool enabled = true;
    Biquad::Type type = Biquad::Type::Bell;
    Biquad::Type parameterType = Biquad::Type::Bell;
    bool midSidePlacement = false;
    float placement = 0.0f;
    float globalAmount = 1.0f;
    float gainScale = 1.0f;
    float driveGlobalAmount = 1.0f;

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
    bool detectorListen = false;
    float dynAttackMs = 10.0f;
    float dynReleaseMs = 100.0f;
    float envLevel = 0.0f;       // current envelope level (linear)
    float dynGainMod = 0.0f;     // current dynamic gain modulation in dB
    float lastSidechainSample = 0.0f;
    float dynAttackCoeff = 1.0f;
    float dynReleaseCoeff = 1.0f;
    float cachedDynAttackMs = -1.0f;
    float cachedDynReleaseMs = -1.0f;
    double cachedDynSampleRate = 0.0;

    // Four complete order paths stay live so slope morphing never activates a
    // cold filter state or redistributes gain at an order boundary.
    float slopeDbPerOct = 12.0f;
    bool decrampEnabled = false;
    using OrderPaths = std::array<std::array<Biquad, variable_slope::maxOrder>, variable_slope::maxOrder>;
    std::unique_ptr<OrderPaths> orderPaths { std::make_unique<OrderPaths>() };
    std::array<CutOrderPath, 8> cutOrderPaths;
    std::array<CutOrderPath, variable_slope::maxOrder> shapeLowerPaths;
    std::array<CutOrderPath, variable_slope::maxOrder> shapeUpperPaths;
    std::array<CutOrderPath, variable_slope::maxOrder> shapeLowerPaths2;
    std::array<CutOrderPath, variable_slope::maxOrder> shapeUpperPaths2;
    Biquad cutResonance;
    Biquad shapeResonance;
    float shapeGainLinear = 1.0f;
    float shapeLowLinear = 1.0f;
    float shapeHighLinear = 1.0f;

    // Smoothers
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> freqSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> qSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> slopeShapeSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> cutAmountSm;
    float currentSlopeShape = 1.0f;
    float currentCutAmount = 2.0f;

    // Coefficient update interval while smoothing (in samples)
    int coeffUpdateInterval = 16;
    int intervalCounter = 0;
    int dynamicControlCounter = 0;
    bool coefficientsValid = false;

    void reset(double sampleRate)
    {
        for (auto& path : *orderPaths)
            for (auto& bq : path)
                bq.reset();
        cutResonance.reset();
        scBiquad.reset();
        auditionBiquad.reset();
        driveBandBiquad.reset();
        for (auto& cut : cutOrderPaths) cut.reset();
        for (auto& path : shapeLowerPaths) path.reset();
        for (auto& path : shapeUpperPaths) path.reset();
        for (auto& path : shapeLowerPaths2) path.reset();
        for (auto& path : shapeUpperPaths2) path.reset();
        shapeResonance.reset();

        freqSm.reset(sampleRate, 0.02);   // 20ms
        qSm.reset(sampleRate, 0.02);
        gainSm.reset(sampleRate, 0.02);
        slopeShapeSm.reset(sampleRate, 0.02);
        cutAmountSm.reset(sampleRate, 0.02);

        freqSm.setCurrentAndTargetValue(freqHz);
        qSm.setCurrentAndTargetValue(Q);
        gainSm.setCurrentAndTargetValue(gainDb);
        currentSlopeShape = variable_slope::shapePosition(slopeDbPerOct);
        currentCutAmount = variable_slope::cutStageAmount(slopeDbPerOct);
        slopeShapeSm.setCurrentAndTargetValue(currentSlopeShape);
        cutAmountSm.setCurrentAndTargetValue(currentCutAmount);

        targetFreqHz = freqHz;
        targetQ = Q;
        targetGainDb = gainDb;

        envLevel = 0.0f;
        dynGainMod = 0.0f;
        intervalCounter = 0;
        dynamicControlCounter = 0;
        coefficientsValid = false;
        driveProcessSampleRate = 0.0;
        drivePreparedFrequency = drivePreparedQ = -1.0f;
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
        const float clampedSlope = std::clamp(newSlopeDbPerOct, 3.0f, 48.0f);
        const float clampedPlacement = std::clamp(newPlacement, -1.0f, 1.0f);
        const bool topologyChanged = !coefficientsValid || processSampleRate != sampleRate
            || enabled != isEnabled || type != newType || slopeDbPerOct != clampedSlope
            || decrampEnabled != useDecramping;
        const bool auxiliaryChanged = topologyChanged || targetFreqHz != newFreqHz || targetQ != newQ;
        enabled = isEnabled;
        type = newType;
        slopeDbPerOct = clampedSlope;
        slopeShapeSm.setTargetValue(variable_slope::shapePosition(slopeDbPerOct));
        cutAmountSm.setTargetValue(variable_slope::cutStageAmount(slopeDbPerOct));
        midSidePlacement = useMidSidePlacement;
        placement = clampedPlacement;
        decrampEnabled = useDecramping;

        targetFreqHz = newFreqHz;
        targetQ = newQ;
        targetGainDb = newGainDb;

        // If targets changed, set smoothers
        if (freqSm.getTargetValue() != targetFreqHz) freqSm.setTargetValue(targetFreqHz);
        if (qSm.getTargetValue() != targetQ)         qSm.setTargetValue(targetQ);
        if (gainSm.getTargetValue() != targetGainDb) gainSm.setTargetValue(targetGainDb);

        if (!enabled) return;

        updateDynamicTimeConstants(sampleRate);
        if (topologyChanged || auxiliaryChanged || !coefficientsValid)
        {
            if (!coefficientsValid)
            {
                freqHz = targetFreqHz;
                Q = targetQ;
                gainDb = targetGainDb;
            }
            setAllStages(sampleRate, true);
            coefficientsValid = true;
        }

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
                const bool movingFrequencyOrQ = freqSm.isSmoothing() || qSm.isSmoothing();
                setAllStages(sampleRate, movingFrequencyOrQ);
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
        if (!enabled || (!dynEnabled && !detectorListen))
        {
            dynGainMod = 0.0f;
            lastSidechainSample = 0.0f;
            return;
        }

        // Sidechain: bandpass-filter the input at the band frequency
        const float scMono = (l + r) * 0.5f;
        const float scFiltered = scBiquad.processL(scMono);
        lastSidechainSample = scFiltered;
        if (!dynEnabled)
        {
            dynGainMod = 0.0f;
            return;
        }
        const float rectified = std::abs(scFiltered);

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
        currentSlopeShape = slopeShapeSm.getNextValue();
        currentCutAmount = cutAmountSm.getNextValue();
        const float firstWeight = std::clamp(1.0f - placement, 0.0f, 1.0f);
        const float secondWeight = std::clamp(1.0f + placement, 0.0f, 1.0f);
        if (!midSidePlacement)
        {
            const float dryL = l, dryR = r;
            l += globalAmount * firstWeight * (processLeft(dryL) - dryL);
            r += globalAmount * secondWeight * (processRight(dryR) - dryR);
        }
        else
        {
            constexpr float invSqrt2 = 0.7071067811865475f;
            const float dryMid = (l + r) * invSqrt2;
            const float drySide = (l - r) * invSqrt2;
            const float mid = dryMid + globalAmount * firstWeight * (processLeft(dryMid) - dryMid);
            const float side = drySide + globalAmount * secondWeight * (processRight(drySide) - drySide);
            l = (mid + side) * invSqrt2;
            r = (mid - side) * invSqrt2;
        }
    }

    inline void processDrive(float& l, float& r)
    {
        if (enabled && driveAmount > 0.0f)
            applySpectralDrive(l, r);
    }

    bool driveActive() const noexcept { return enabled && driveAmount > 0.0f; }

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
        if (type == Biquad::Type::HighPass || type == Biquad::Type::LowPass)
            return processCut(x, false);
        if (type == Biquad::Type::LowShelf || type == Biquad::Type::HighShelf
            || type == Biquad::Type::Tilt)
            return processShape(x, false);
        const int low = std::clamp((int)std::floor(currentSlopeShape), 1, variable_slope::maxOrder);
        const int high = std::min(variable_slope::maxOrder, low + 1);
        const float mix = high == low ? 0.0f : currentSlopeShape - (float)low;
        const auto processPath = [this, x](int order)
        {
            float path = x;
            for (int stage = 0; stage < order; ++stage)
                path = (*orderPaths)[(size_t)(order - 1)][(size_t)stage].processL(path);
            return path;
        };
        const float lowOutput = processPath(low);
        if (mix <= 0.0001f) return lowOutput;
        const float highOutput = processPath(high);
        return lowOutput + mix * (highOutput - lowOutput);
    }

    inline float processRight(float x)
    {
        if (type == Biquad::Type::HighPass || type == Biquad::Type::LowPass)
            return processCut(x, true);
        if (type == Biquad::Type::LowShelf || type == Biquad::Type::HighShelf
            || type == Biquad::Type::Tilt)
            return processShape(x, true);
        const int low = std::clamp((int)std::floor(currentSlopeShape), 1, variable_slope::maxOrder);
        const int high = std::min(variable_slope::maxOrder, low + 1);
        const float mix = high == low ? 0.0f : currentSlopeShape - (float)low;
        const auto processPath = [this, x](int order)
        {
            float path = x;
            for (int stage = 0; stage < order; ++stage)
                path = (*orderPaths)[(size_t)(order - 1)][(size_t)stage].processR(path);
            return path;
        };
        const float lowOutput = processPath(low);
        if (mix <= 0.0001f) return lowOutput;
        const float highOutput = processPath(high);
        return lowOutput + mix * (highOutput - lowOutput);
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
    float dcXL = 0.0f, dcYL = 0.0f, dcXR = 0.0f, dcYR = 0.0f;
    float driveMemoryL = 0.0f, driveMemoryR = 0.0f;
    std::vector<float> driveDelayL, driveDelayR;
    int driveDelayPosL = 0, driveDelayPosR = 0;
    double drivePhaseL = 0.0, drivePhaseR = 0.0;
    double driveProcessSampleRate = 0.0;
    float drivePreparedFrequency = -1.0f, drivePreparedQ = -1.0f;

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

    inline float processCut(float x, bool right)
    {
        const int lowOrder = std::clamp((int)std::floor(currentCutAmount), 1, 8);
        const int highOrder = std::min(8, lowOrder + 1);
        const float mix = highOrder == lowOrder ? 0.0f : currentCutAmount - (float)lowOrder;
        const auto processPath = [this, x, right](int order)
        {
            auto& path = cutOrderPaths[(size_t)(order - 1)];
            float y = path.hasFirstOrder ? path.firstOrder.process(x, right) : x;
            for (int pair = 0; pair < path.pairCount; ++pair)
                y = right ? path.pairs[(size_t)pair].processR(y)
                          : path.pairs[(size_t)pair].processL(y);
            return y;
        };
        const float low = processPath(lowOrder);
        const float base = mix > 0.0001f
            ? low + mix * (processPath(highOrder) - low) : low;
        return right ? cutResonance.processR(base) : cutResonance.processL(base);
    }

    inline float processShapePath(CutOrderPath& path, float x, bool right)
    {
        float y = path.hasFirstOrder ? path.firstOrder.process(x, right) : x;
        for (int pair = 0; pair < path.pairCount; ++pair)
            y = right ? path.pairs[(size_t)pair].processR(y)
                      : path.pairs[(size_t)pair].processL(y);
        return y;
    }

    inline float processShape(float x, bool right)
    {
        const int lowOrder = std::clamp((int)std::lround(currentSlopeShape), 1,
                                        variable_slope::maxOrder);
        const int highOrder = lowOrder;
        constexpr float mix = 0.0f;
        const auto lrForOrder = [this, x, right](int order)
        {
            const size_t index = (size_t)(order - 1);
            const float lowA = processShapePath(shapeLowerPaths[index], x, right);
            const float low = processShapePath(shapeLowerPaths2[index], lowA, right);
            const float highA = processShapePath(shapeUpperPaths[index], x, right);
            float high = processShapePath(shapeUpperPaths2[index], highA, right);
            if ((order & 1) != 0) high = -high;
            return std::array<float, 2> { low, high };
        };
        const auto lowPair = lrForOrder(lowOrder);
        auto low = lowPair[0], high = lowPair[1];
        if (mix > 0.0001f)
        {
            const auto highPair = lrForOrder(highOrder);
            low += mix * (highPair[0] - low);
            high += mix * (highPair[1] - high);
        }
        if ((type == Biquad::Type::Tilt
                && std::abs(shapeLowLinear - 1.0f) < 1.0e-7f
                && std::abs(shapeHighLinear - 1.0f) < 1.0e-7f)
            || (type != Biquad::Type::Tilt && std::abs(shapeGainLinear - 1.0f) < 1.0e-7f))
            return x;

        float y;
        switch (type)
        {
            case Biquad::Type::LowShelf:
                y = shapeGainLinear * low + high;
                break;
            case Biquad::Type::HighShelf:
                y = low + shapeGainLinear * high;
                break;
            case Biquad::Type::Tilt:
                y = shapeLowLinear * low + shapeHighLinear * high;
                break;
            default: return x;
        }
        y = right ? shapeResonance.processR(y) : shapeResonance.processL(y);
        return y;
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
            l += driveGlobalAmount * firstWeight * driveOneBand(bandL, false);
            r += driveGlobalAmount * secondWeight * driveOneBand(bandR, true);
        }
        else
        {
            float mid = (l + r) * invSqrt2;
            float side = (l - r) * invSqrt2;
            mid += driveGlobalAmount * firstWeight * driveOneBand(driveBandBiquad.processL(mid), false);
            side += driveGlobalAmount * secondWeight * driveOneBand(driveBandBiquad.processR(side), true);
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
        // Incorporate dynamic gain modulation into the filter coefficients
        const float totalGainDb = (gainDb + dynGainMod) * gainScale;
        for (int order = 1; order <= variable_slope::maxOrder; ++order)
        {
            const double bellGain = type == Biquad::Type::Bell && order > 1
                ? variable_slope::bellStageGain(sampleRate, freqHz, Q, totalGainDb,
                                                order, decrampEnabled)
                : std::numeric_limits<double>::quiet_NaN();
            for (int stage = 0; stage < order; ++stage)
                variable_slope::configureStage(
                    (*orderPaths)[(size_t)(order - 1)][(size_t)stage], type,
                    sampleRate, freqHz, Q, totalGainDb, order, stage,
                    slopeDbPerOct, decrampEnabled, bellGain);
        }

        const auto configureCrossoverPath = [sampleRate, this](CutOrderPath& path,
                                                                int order, float cutoff,
                                                                bool highPass)
        {
            path.hasFirstOrder = (order & 1) != 0;
            path.pairCount = order / 2;
            if (path.hasFirstOrder)
                path.firstOrder.set(highPass, sampleRate, cutoff);
            for (int pair = 0; pair < path.pairCount; ++pair)
            {
                const double sectionQ = 1.0 / (2.0 * std::sin(
                    (2.0 * pair + 1.0) * juce::MathConstants<double>::pi
                    / (2.0 * order)));
                if (decrampEnabled)
                    path.pairs[(size_t)pair].setMatched(highPass ? Biquad::Type::HighPass
                                                                 : Biquad::Type::LowPass,
                                                        sampleRate, cutoff, sectionQ, 0.0);
                else
                    path.pairs[(size_t)pair].set(highPass ? Biquad::Type::HighPass
                                                          : Biquad::Type::LowPass,
                                                 sampleRate, cutoff, sectionQ, 0.0);
            }
        };

        shapeGainLinear = juce::Decibels::decibelsToGain(totalGainDb);
        shapeLowLinear = juce::Decibels::decibelsToGain(-totalGainDb);
        shapeHighLinear = juce::Decibels::decibelsToGain(totalGainDb);
        for (int order = 1; order <= variable_slope::maxOrder; ++order)
        {
            const float cutoff = std::clamp(freqHz, 10.0f, (float)sampleRate * 0.45f);
            configureCrossoverPath(shapeLowerPaths[(size_t)(order - 1)], order,
                                   cutoff, false);
            configureCrossoverPath(shapeLowerPaths2[(size_t)(order - 1)], order,
                                   cutoff, false);
            configureCrossoverPath(shapeUpperPaths[(size_t)(order - 1)], order,
                                   cutoff, true);
            configureCrossoverPath(shapeUpperPaths2[(size_t)(order - 1)], order,
                                   cutoff, true);
        }
        const double midpointLinear = type == Biquad::Type::Tilt
            ? 0.5 * ((double)shapeLowLinear + (double)shapeHighLinear)
            : 0.5 * (1.0 + (double)shapeGainLinear);
        const double midpointTargetDb = type == Biquad::Type::Tilt ? 0.0 : 0.5 * totalGainDb;
        const double pivotCorrectionDb = midpointTargetDb
            - juce::Decibels::gainToDecibels(midpointLinear, -120.0);
        const double shapeResonanceGain = std::clamp(
            pivotCorrectionDb + 6.0 * std::log2(std::max(0.1f, Q)), -12.0, 12.0);
        if (decrampEnabled)
            shapeResonance.setMatched(Biquad::Type::Bell, sampleRate, freqHz, 0.7,
                                      shapeResonanceGain);
        else
            shapeResonance.set(Biquad::Type::Bell, sampleRate, freqHz, 0.7,
                               shapeResonanceGain);
        variable_slope::configureCutResonance(
            cutResonance, sampleRate, freqHz, Q, slopeDbPerOct, decrampEnabled);

        if (updateAuxiliary)
        {
            const bool highPass = type == Biquad::Type::HighPass;
            if (type == Biquad::Type::HighPass || type == Biquad::Type::LowPass)
            {
                for (int order = 1; order <= 8; ++order)
                {
                    auto& path = cutOrderPaths[(size_t)(order - 1)];
                    path.hasFirstOrder = (order & 1) != 0;
                    path.pairCount = order / 2;
                    if (path.hasFirstOrder)
                        path.firstOrder.set(highPass, sampleRate, freqHz);
                    for (int pair = 0; pair < path.pairCount; ++pair)
                    {
                        const double sectionQ = 1.0 / (2.0 * std::sin(
                            (2.0 * pair + 1.0) * juce::MathConstants<double>::pi
                            / (2.0 * order)));
                        if (decrampEnabled)
                            path.pairs[(size_t)pair].setMatched(type, sampleRate, freqHz,
                                                               sectionQ, 0.0);
                        else
                            path.pairs[(size_t)pair].set(type, sampleRate, freqHz,
                                                        sectionQ, 0.0);
                    }
                }
            }

            scBiquad.set(Biquad::Type::Bandpass, sampleRate, freqHz, 2.0, 0.0);
            const float auditionQ = std::clamp(Q, 0.2f, 12.0f);
            auditionBiquad.set(Biquad::Type::Bandpass, sampleRate, freqHz, auditionQ, 0.0);
        }
    }
    double processSampleRate = 44100.0;
};
