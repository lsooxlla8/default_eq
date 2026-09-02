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

// Saturation / waveshaper processing evolved from the FreeEQ8 base and the
// author-owned default_distortion project; see THIRD_PARTY_NOTICES.md for the
// reused code boundaries.
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
        void processStereoBlock(float* left, float* right, int numSamples)
        {
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
            const auto vectorB0 = vdupq_n_f32(b0);
            const auto vectorB1 = vdupq_n_f32(b1);
            const auto vectorA1 = vdupq_n_f32(a1);
            float32x4_t vectorX1 { x1L, x1R, 0.0f, 0.0f };
            float32x4_t vectorY1 { y1L, y1R, 0.0f, 0.0f };
            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float32x4_t input { left[sample], right[sample], 0.0f, 0.0f };
                const auto output = vsubq_f32(vaddq_f32(vmulq_f32(vectorB0, input),
                                                       vmulq_f32(vectorB1, vectorX1)),
                                              vmulq_f32(vectorA1, vectorY1));
                vectorX1 = input;
                vectorY1 = output;
                left[sample] = vgetq_lane_f32(output, 0);
                right[sample] = vgetq_lane_f32(output, 1);
            }
            x1L = vgetq_lane_f32(vectorX1, 0); x1R = vgetq_lane_f32(vectorX1, 1);
            y1L = vgetq_lane_f32(vectorY1, 0); y1R = vgetq_lane_f32(vectorY1, 1);
#elif defined(__SSE2__) || defined(_M_X64)
            const auto vectorB0 = _mm_set1_ps(b0);
            const auto vectorB1 = _mm_set1_ps(b1);
            const auto vectorA1 = _mm_set1_ps(a1);
            auto vectorX1 = _mm_set_ps(0.0f, 0.0f, x1R, x1L);
            auto vectorY1 = _mm_set_ps(0.0f, 0.0f, y1R, y1L);
            alignas(16) float outputValues[4];
            for (int sample = 0; sample < numSamples; ++sample)
            {
                const auto input = _mm_set_ps(0.0f, 0.0f, right[sample], left[sample]);
                const auto output = _mm_sub_ps(_mm_add_ps(_mm_mul_ps(vectorB0, input),
                                                          _mm_mul_ps(vectorB1, vectorX1)),
                                               _mm_mul_ps(vectorA1, vectorY1));
                vectorX1 = input;
                vectorY1 = output;
                _mm_store_ps(outputValues, output);
                left[sample] = outputValues[0];
                right[sample] = outputValues[1];
            }
            _mm_store_ps(outputValues, vectorX1);
            x1L = outputValues[0]; x1R = outputValues[1];
            _mm_store_ps(outputValues, vectorY1);
            y1L = outputValues[0]; y1R = outputValues[1];
#else
            float localX1L = x1L, localY1L = y1L;
            float localX1R = x1R, localY1R = y1R;
            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float inputL = left[sample];
                const float outputL = b0 * inputL + b1 * localX1L - a1 * localY1L;
                localX1L = inputL; localY1L = outputL; left[sample] = outputL;
                const float inputR = right[sample];
                const float outputR = b0 * inputR + b1 * localX1R - a1 * localY1R;
                localX1R = inputR; localY1R = outputR; right[sample] = outputR;
            }
            x1L = localX1L; y1L = localY1L;
            x1R = localX1R; y1R = localY1R;
#endif
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
    float firstPlacementWeight = 1.0f;
    float secondPlacementWeight = 1.0f;
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

#include "EQBandProcessing.inl"

#include "EQBandDrive.inl"

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

#include "EQBandInternals.inl"
    double processSampleRate = 44100.0;
};
