#pragma once
#include <cmath>
#include <algorithm>
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
 #include <arm_neon.h>
#elif defined(__SSE2__) || defined(_M_X64)
 #include <emmintrin.h>
#endif

// C++17 constant — replaces the fragile #ifndef M_PI / #define M_PI pattern.
constexpr double kPi = 3.14159265358979323846;

struct Biquad
{
    // Transposed Direct Form II
    double b0=1, b1=0, b2=0, a1=0, a2=0;
    double z1L=0, z2L=0, z1R=0, z2R=0;
    double targetB0=1, targetB1=0, targetB2=0, targetA1=0, targetA2=0;
    double stepB0=0, stepB1=0, stepB2=0, stepA1=0, stepA2=0;
    int coefficientRampRemaining = 0;

    void reset()
    {
        z1L = z2L = z1R = z2R = 0.0;
        coefficientRampRemaining = 0;
        targetB0 = b0; targetB1 = b1; targetB2 = b2;
        targetA1 = a1; targetA2 = a2;
        stepB0 = stepB1 = stepB2 = stepA1 = stepA2 = 0.0;
    }

    void setCoefficients(double newB0, double newB1, double newB2,
                         double newA1, double newA2, int rampSamples = 0) noexcept
    {
        targetB0 = newB0; targetB1 = newB1; targetB2 = newB2;
        targetA1 = newA1; targetA2 = newA2;
        coefficientRampRemaining = std::max(0, rampSamples);
        if (coefficientRampRemaining == 0)
        {
            b0 = targetB0; b1 = targetB1; b2 = targetB2;
            a1 = targetA1; a2 = targetA2;
            stepB0 = stepB1 = stepB2 = stepA1 = stepA2 = 0.0;
            return;
        }
        const double inverse = 1.0 / (double)coefficientRampRemaining;
        stepB0 = (targetB0 - b0) * inverse;
        stepB1 = (targetB1 - b1) * inverse;
        stepB2 = (targetB2 - b2) * inverse;
        stepA1 = (targetA1 - a1) * inverse;
        stepA2 = (targetA2 - a2) * inverse;
    }

    void advanceCoefficientRamp() noexcept
    {
        if (coefficientRampRemaining <= 0) return;
        if (--coefficientRampRemaining == 0)
        {
            b0 = targetB0; b1 = targetB1; b2 = targetB2;
            a1 = targetA1; a2 = targetA2;
            return;
        }
        b0 += stepB0; b1 += stepB1; b2 += stepB2;
        a1 += stepA1; a2 += stepA2;
    }

    inline float processL(float x)
    {
        const double y = b0 * x + z1L;
        z1L = b1 * x - a1 * y + z2L;
        z2L = b2 * x - a2 * y;
        return (float)y;
    }

    inline float processR(float x)
    {
        const double y = b0 * x + z1R;
        z1R = b1 * x - a1 * y + z2R;
        z2R = b2 * x - a2 * y;
        return (float)y;
    }

    inline void processStereo(float& left, float& right)
    {
        const double yLeft = b0 * left + z1L;
        z1L = b1 * left - a1 * yLeft + z2L;
        z2L = b2 * left - a2 * yLeft;
        left = (float)yLeft;

        const double yRight = b0 * right + z1R;
        z1R = b1 * right - a1 * yRight + z2R;
        z2R = b2 * right - a2 * yRight;
        right = (float)yRight;
    }

    inline void processStereoBlock(float* left, float* right, int numSamples)
    {
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
        const float64x2_t vectorB0 = vdupq_n_f64(b0);
        const float64x2_t vectorB1 = vdupq_n_f64(b1);
        const float64x2_t vectorB2 = vdupq_n_f64(b2);
        const float64x2_t vectorA1 = vdupq_n_f64(a1);
        const float64x2_t vectorA2 = vdupq_n_f64(a2);
        float64x2_t vectorZ1 { z1L, z1R };
        float64x2_t vectorZ2 { z2L, z2R };
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float64x2_t input { (double)left[sample], (double)right[sample] };
            const auto output = vaddq_f64(vmulq_f64(vectorB0, input), vectorZ1);
            vectorZ1 = vaddq_f64(vsubq_f64(vmulq_f64(vectorB1, input),
                                           vmulq_f64(vectorA1, output)), vectorZ2);
            vectorZ2 = vsubq_f64(vmulq_f64(vectorB2, input),
                                 vmulq_f64(vectorA2, output));
            left[sample] = (float)vgetq_lane_f64(output, 0);
            right[sample] = (float)vgetq_lane_f64(output, 1);
        }
        z1L = vgetq_lane_f64(vectorZ1, 0); z1R = vgetq_lane_f64(vectorZ1, 1);
        z2L = vgetq_lane_f64(vectorZ2, 0); z2R = vgetq_lane_f64(vectorZ2, 1);
#elif defined(__SSE2__) || defined(_M_X64)
        const __m128d vectorB0 = _mm_set1_pd(b0);
        const __m128d vectorB1 = _mm_set1_pd(b1);
        const __m128d vectorB2 = _mm_set1_pd(b2);
        const __m128d vectorA1 = _mm_set1_pd(a1);
        const __m128d vectorA2 = _mm_set1_pd(a2);
        __m128d vectorZ1 = _mm_set_pd(z1R, z1L);
        __m128d vectorZ2 = _mm_set_pd(z2R, z2L);
        alignas(16) double outputValues[2];
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const __m128d input = _mm_set_pd((double)right[sample], (double)left[sample]);
            const auto output = _mm_add_pd(_mm_mul_pd(vectorB0, input), vectorZ1);
            vectorZ1 = _mm_add_pd(_mm_sub_pd(_mm_mul_pd(vectorB1, input),
                                             _mm_mul_pd(vectorA1, output)), vectorZ2);
            vectorZ2 = _mm_sub_pd(_mm_mul_pd(vectorB2, input),
                                  _mm_mul_pd(vectorA2, output));
            _mm_store_pd(outputValues, output);
            left[sample] = (float)outputValues[0];
            right[sample] = (float)outputValues[1];
        }
        alignas(16) double stateValues[2];
        _mm_store_pd(stateValues, vectorZ1);
        z1L = stateValues[0]; z1R = stateValues[1];
        _mm_store_pd(stateValues, vectorZ2);
        z2L = stateValues[0]; z2R = stateValues[1];
#else
        const double localB0 = b0, localB1 = b1, localB2 = b2;
        const double localA1 = a1, localA2 = a2;
        double localZ1L = z1L, localZ2L = z2L;
        double localZ1R = z1R, localZ2R = z2R;
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const double inputL = left[sample];
            const double yLeft = localB0 * inputL + localZ1L;
            localZ1L = localB1 * inputL - localA1 * yLeft + localZ2L;
            localZ2L = localB2 * inputL - localA2 * yLeft;
            left[sample] = (float)yLeft;

            const double inputR = right[sample];
            const double yRight = localB0 * inputR + localZ1R;
            localZ1R = localB1 * inputR - localA1 * yRight + localZ2R;
            localZ2R = localB2 * inputR - localA2 * yRight;
            right[sample] = (float)yRight;
        }
        z1L = localZ1L; z2L = localZ2L;
        z1R = localZ1R; z2R = localZ2R;
#endif
    }

    enum class Type { Bell, LowShelf, HighShelf, HighPass, LowPass, Bandpass, Notch, Tilt,
                      ResHighPass, ResLowPass };

    // RBJ cookbook coefficients
    void set(Type type, double sampleRate, double freqHz, double Q, double gainDb)
    {
        freqHz = std::clamp(freqHz, 10.0, sampleRate * 0.45);
        Q = std::clamp(Q, 0.1, 24.0);

        const double A  = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * (freqHz / sampleRate);
        const double cosw0 = std::cos(w0);
        const double sinw0 = std::sin(w0);
        const double alpha = sinw0 / (2.0 * Q);

        double b0_, b1_, b2_, a0_, a1_, a2_;

        switch (type)
        {
            case Type::Bell:
            {
                b0_ = 1.0 + alpha * A;
                b1_ = -2.0 * cosw0;
                b2_ = 1.0 - alpha * A;
                a0_ = 1.0 + alpha / A;
                a1_ = -2.0 * cosw0;
                a2_ = 1.0 - alpha / A;
            } break;

            case Type::ResLowPass:
            case Type::LowPass:
            {
                b0_ = (1.0 - cosw0) * 0.5;
                b1_ = 1.0 - cosw0;
                b2_ = (1.0 - cosw0) * 0.5;
                a0_ = 1.0 + alpha;
                a1_ = -2.0 * cosw0;
                a2_ = 1.0 - alpha;
            } break;

            case Type::ResHighPass:
            case Type::HighPass:
            {
                b0_ = (1.0 + cosw0) * 0.5;
                b1_ = -(1.0 + cosw0);
                b2_ = (1.0 + cosw0) * 0.5;
                a0_ = 1.0 + alpha;
                a1_ = -2.0 * cosw0;
                a2_ = 1.0 - alpha;
            } break;

            case Type::LowShelf:
            {
                // RBJ cookbook shelving, using the shelf-slope (S) parameterisation.
                //
                // alpha_S = sin(w0)/2 * sqrt((A + 1/A)(1/S - 1) + 2)
                //
                // This form is only defined for 0 < S <= 1, where S = 1 is the
                // steepest shelf that remains monotonic. Previously S was clamped
                // to [0.1, 4.0]; for any S > 1 the term (1/S - 1) is negative, and
                // once (A + 1/A)(1/S - 1) < -2 the radicand goes negative and sqrt
                // returns NaN. That NaN propagated into b0/b2/a0/a2, poisoned the
                // filter state, and reached the output buffer permanently. A sweep
                // of the exposed parameter space found this on ~11% of
                // (type, freq, Q, gain, rate) combinations, from Q >= 3.8 upward.
                //
                // Clamping S to RBJ's actual domain fixes it with no loss:
                //   * 1/S - 1 >= 0, so the radicand is always >= 2 and NaN is
                //     unreachable by construction rather than guarded against.
                //   * S <= 1 guarantees a monotonic shelf, so there is no resonant
                //     overshoot at any Q (measured 0.00 dB across Q 0.1-24).
                //   * For Q <= 2 the result is identical to the previous behaviour,
                //     which is the range where the old mapping was still valid, so
                //     existing saved states in that range are unchanged.
                //
                // Note: because S saturates at Q = 2, Q values above 2 all produce
                // the same (steepest monotonic) shelf. The old code varied S up to 4
                // over that range, but only by leaving the formula's valid domain -
                // it produced 1.4-7 dB of unintended peaking before failing outright.
                const double S = std::clamp(Q / 2.0, 0.1, 1.0);
                const double alphaS = sinw0/2.0 * std::sqrt((A + 1.0/A) * (1.0/S - 1.0) + 2.0);
                const double twoSqrtAlpha = 2.0 * std::sqrt(A) * alphaS;

                b0_ =    A*((A+1) - (A-1)*cosw0 + twoSqrtAlpha);
                b1_ =  2*A*((A-1) - (A+1)*cosw0);
                b2_ =    A*((A+1) - (A-1)*cosw0 - twoSqrtAlpha);
                a0_ =        (A+1) + (A-1)*cosw0 + twoSqrtAlpha;
                a1_ =   -2*((A-1) + (A+1)*cosw0);
                a2_ =        (A+1) + (A-1)*cosw0 - twoSqrtAlpha;
            } break;

            case Type::HighShelf:
            {
                // See LowShelf above for why S is clamped to RBJ's (0, 1] domain.
                const double S = std::clamp(Q / 2.0, 0.1, 1.0);
                const double alphaS = sinw0/2.0 * std::sqrt((A + 1.0/A) * (1.0/S - 1.0) + 2.0);
                const double twoSqrtAlpha = 2.0 * std::sqrt(A) * alphaS;

                b0_ =    A*((A+1) + (A-1)*cosw0 + twoSqrtAlpha);
                b1_ = -2*A*((A-1) + (A+1)*cosw0);
                b2_ =    A*((A+1) + (A-1)*cosw0 - twoSqrtAlpha);
                a0_ =        (A+1) - (A-1)*cosw0 + twoSqrtAlpha;
                a1_ =    2*((A-1) - (A+1)*cosw0);
                a2_ =        (A+1) - (A-1)*cosw0 - twoSqrtAlpha;
            } break;

            case Type::Bandpass:
            {
                // RBJ constant 0 dB peak gain BPF (gainDb ignored)
                b0_ = alpha;
                b1_ = 0.0;
                b2_ = -alpha;
                a0_ = 1.0 + alpha;
                a1_ = -2.0 * cosw0;
                a2_ = 1.0 - alpha;
            } break;

            case Type::Notch:
            {
                b0_ = 1.0;
                b1_ = -2.0 * cosw0;
                b2_ = 1.0;
                a0_ = 1.0 + alpha;
                a1_ = -2.0 * cosw0;
                a2_ = 1.0 - alpha;
            } break;

            case Type::Tilt:
            {
                // A low shelf of -2G followed by +G make-up gives a symmetric
                // response: +6 dB means -6 dB below the pivot and +6 dB above.
                const double shelfA = std::pow(10.0, (-2.0 * gainDb) / 40.0);
                const double S = std::clamp(Q / 2.0, 0.1, 1.0);
                const double alphaS = sinw0 * 0.5
                    * std::sqrt((shelfA + 1.0 / shelfA) * (1.0 / S - 1.0) + 2.0);
                const double twoSqrtAlpha = 2.0 * std::sqrt(shelfA) * alphaS;
                b0_ = shelfA * ((shelfA + 1.0) - (shelfA - 1.0) * cosw0 + twoSqrtAlpha);
                b1_ = 2.0 * shelfA * ((shelfA - 1.0) - (shelfA + 1.0) * cosw0);
                b2_ = shelfA * ((shelfA + 1.0) - (shelfA - 1.0) * cosw0 - twoSqrtAlpha);
                a0_ = (shelfA + 1.0) + (shelfA - 1.0) * cosw0 + twoSqrtAlpha;
                a1_ = -2.0 * ((shelfA - 1.0) + (shelfA + 1.0) * cosw0);
                a2_ = (shelfA + 1.0) + (shelfA - 1.0) * cosw0 - twoSqrtAlpha;
                const double makeup = std::pow(10.0, gainDb / 20.0);
                b0_ *= makeup;
                b1_ *= makeup;
                b2_ *= makeup;
            } break;
        }

        // normalize
        b0 = b0_ / a0_;
        b1 = b1_ / a0_;
        b2 = b2_ / a0_;
        a1 = a1_ / a0_;
        a2 = a2_ / a0_;
    }
};
