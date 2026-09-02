#pragma once
#include <cmath>
#include <algorithm>
#include <array>
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

    // Matched/de-cramped second-order designs adapted from Dario Sanfilippo's
    // MIT-licensed Faust vaeffects.lib implementation of Martin Vicanek's
    // "Matched Second Order Digital Filters". Source revision:
    // ccc6030e60806011ae73c9502d9bca85ff2b79fa (2026-08-12 checkout).
    // Changes: C++ coefficient output, finite-domain guards, RBJ fallback for
    // unsupported notch and pathological parameter combinations.
    void setMatched(Type type, double sampleRate, double freqHz, double Q, double gainDb)
    {
        const auto fallback = [&] { set(type, sampleRate, freqHz, Q, gainDb); };
        freqHz = std::clamp(freqHz, 10.0, sampleRate * 0.49);
        Q = std::clamp(Q, 0.1, 24.0);
        const double normalized = freqHz / sampleRate;
        const double blendX = std::clamp((normalized - 0.12) / 0.12, 0.0, 1.0);
        const double matchedBlend = blendX * blendX * (3.0 - 2.0 * blendX);
        if (matchedBlend <= 0.0) { fallback(); return; }
        Biquad rbjReference;
        rbjReference.set(type, sampleRate, freqHz, Q, gainDb);
        const auto blendWithRbj = [&]
        {
            b0 = rbjReference.b0 + matchedBlend * (b0 - rbjReference.b0);
            b1 = rbjReference.b1 + matchedBlend * (b1 - rbjReference.b1);
            b2 = rbjReference.b2 + matchedBlend * (b2 - rbjReference.b2);
            a1 = rbjReference.a1 + matchedBlend * (a1 - rbjReference.a1);
            a2 = rbjReference.a2 + matchedBlend * (a2 - rbjReference.a2);
        };
        if (type == Type::Notch || type == Type::Tilt) { fallback(); return; }
        if ((type == Type::Bell || type == Type::LowShelf || type == Type::HighShelf)
            && std::abs(gainDb) < 1.0e-7)
        {
            b0 = 1.0; b1 = b2 = a1 = a2 = 0.0; return;
        }

        const double w = 2.0 * kPi * freqHz / sampleRate;
        const double q = 1.0 / (2.0 * Q);
        const double root = q <= 1.0 ? std::sqrt(std::max(0.0, 1.0 - q * q))
                                     : std::sqrt(std::max(0.0, q * q - 1.0));
        const double ma1 = q <= 1.0 ? -2.0 * std::exp(-q * w) * std::cos(root * w)
                                    : -2.0 * std::exp(-q * w) * std::cosh(root * w);
        const double ma2 = std::exp(-2.0 * q * w);
        const double phi1 = std::pow(std::sin(0.5 * w), 2.0);
        const double phi0 = 1.0 - phi1;
        const double phi2 = 4.0 * phi0 * phi1;
        const double A0 = std::pow(1.0 + ma1 + ma2, 2.0);
        const double A1 = std::pow(1.0 - ma1 + ma2, 2.0);
        const double A2 = -4.0 * ma2;
        double mb0 = 1.0, mb1 = 0.0, mb2 = 0.0;

        if (type == Type::LowPass)
        {
            if (phi1 < 1.0e-12) { fallback(); return; }
            const double R1 = (A0 * phi0 + A1 * phi1 + A2 * phi2) * Q * Q;
            const double B0 = A0;
            const double B1 = (R1 - B0 * phi0) / phi1;
            if (B0 < 0.0 || B1 < 0.0) { fallback(); return; }
            mb0 = 0.5 * (std::sqrt(B0) + std::sqrt(B1));
            mb1 = std::sqrt(B0) - mb0;
            mb2 = 0.0;
        }
        else if (type == Type::HighPass)
        {
            if (phi1 < 1.0e-12) { fallback(); return; }
            mb0 = Q * std::sqrt(std::max(0.0, A0 * phi0 + A1 * phi1 + A2 * phi2)) / (4.0 * phi1);
            mb1 = -2.0 * mb0; mb2 = mb0;
        }
        else if (type == Type::Bandpass)
        {
            if (phi1 < 1.0e-12) { fallback(); return; }
            const double R1 = A0 * phi0 + A1 * phi1 + A2 * phi2;
            const double R2 = -A0 + A1 + 4.0 * (phi0 - phi1) * A2;
            const double B2 = (R1 - R2 * phi1) / (4.0 * phi1 * phi1);
            const double B1 = R2 + 4.0 * (phi1 - phi0) * B2;
            if (B1 < 0.0 || B2 < 0.0) { fallback(); return; }
            mb1 = -0.5 * std::sqrt(B1);
            mb0 = 0.5 * (std::sqrt(B2 + mb1 * mb1) - mb1);
            mb2 = -(mb0 + mb1);
        }
        else if (type == Type::Bell)
        {
            if (phi1 < 1.0e-12) { fallback(); return; }
            const double G = std::pow(10.0, gainDb / 20.0);
            const double B0 = A0;
            const double R1 = (A0 * phi0 + A1 * phi1 + A2 * phi2) * G * G;
            const double R2 = (-A0 + A1 + 4.0 * (phi0 - phi1) * A2) * G * G;
            const double B2 = (R1 - R2 * phi1 - B0) / (4.0 * phi1 * phi1);
            const double B1 = R2 + B0 + 4.0 * (phi1 - phi0) * B2;
            if (B0 < 0.0 || B1 < 0.0) { fallback(); return; }
            const double W = 0.5 * (std::sqrt(B0) + std::sqrt(B1));
            const double disc = W * W + B2;
            if (disc < 0.0) { fallback(); return; }
            mb0 = 0.5 * (W + std::sqrt(disc));
            mb1 = 0.5 * (std::sqrt(B0) - std::sqrt(B1));
            if (std::abs(mb0) < 1.0e-12) { fallback(); return; }
            mb2 = -B2 / (4.0 * mb0);
        }
        else
        {
            // Vicanek 2024/2025 two-pole matched shelf fit, as implemented
            // in the same MIT Faust source. It is intentionally independent
            // from oversampling and remains a zero-latency biquad.
            const double G = std::pow(10.0, gainDb / 20.0);
            const bool low = type == Type::LowShelf;
            const double safeG = G < 1.0 ? std::min(1.0 - 1.0e-9, G) : std::max(1.0 + 1.0e-9, G);
            const double g = low ? 1.0 / safeG : safeG;
            const double gInv = 1.0 / g;
            const double f = std::clamp(freqHz / (sampleRate * 0.5), 1.0e-6, 0.98);
            const double f4 = std::pow(f, 4.0);
            const double hny = (f4 + g) / (f4 + gInv);
            const auto point = [&](double c0, double c1)
            {
                const double fm = f / std::sqrt(c0 + c1 * f * f);
                const double fm4 = std::pow(fm, 4.0);
                const double hm = (f4 + fm4 * g) / (f4 + fm4 * gInv);
                const double phi = std::pow(std::sin(0.5 * kPi * fm), 2.0);
                return std::array<double, 2>{hm, phi};
            };
            const auto p1 = point(0.160, 1.543), p2 = point(0.947, 3.806);
            const double d1 = (p1[0] - 1.0) * (1.0 - p1[1]);
            const double d2 = (p2[0] - 1.0) * (1.0 - p2[1]);
            const double c11 = -p1[1] * d1, c12 = p1[1] * p1[1] * (hny - p1[0]);
            const double c21 = -p2[1] * d2, c22 = p2[1] * p2[1] * (hny - p2[0]);
            const double denom = c11 * c22 - c12 * c21;
            if (std::abs(denom) < 1.0e-14 || std::abs(c12) < 1.0e-14) { fallback(); return; }
            const double alpha1 = (c22 * d1 - c12 * d2) / denom;
            const double aa1 = (d1 - c11 * alpha1) / c12;
            const double bb1 = hny * aa1;
            const double aa2 = 0.25 * (alpha1 - aa1), bb2 = 0.25 * (alpha1 - bb1);
            if (aa1 < 0.0 || bb1 < 0.0) { fallback(); return; }
            const double v = 0.5 * (1.0 + std::sqrt(aa1));
            const double ww = 0.5 * (1.0 + std::sqrt(bb1));
            const double adisc = v * v + aa2, bdisc = ww * ww + bb2;
            if (adisc < 0.0 || bdisc < 0.0) { fallback(); return; }
            const double a0 = 0.5 * (v + std::sqrt(adisc));
            const double invA0 = 1.0 / a0;
            a1 = (1.0 - v) * invA0;
            a2 = -0.25 * aa2 * invA0 * invA0;
            const double b0temp = 0.5 * (ww + std::sqrt(bdisc)) * invA0;
            if (std::abs(b0temp) < 1.0e-14) { fallback(); return; }
            if (low)
            {
                mb0 = safeG * b0temp;
                mb1 = safeG * (1.0 - ww) * invA0;
                mb2 = safeG * (-0.25 * bb2 / b0temp) * invA0 * invA0;
            }
            else
            {
                mb0 = b0temp;
                mb1 = (1.0 - ww) * invA0;
                mb2 = (-0.25 * bb2 / mb0) * invA0 * invA0;
            }
            b0 = mb0; b1 = mb1; b2 = mb2;
            if (!(std::isfinite(b0) && std::isfinite(b1) && std::isfinite(b2)
                  && std::isfinite(a1) && std::isfinite(a2))) fallback();
            else blendWithRbj();
            return;
        }

        b0 = mb0; b1 = mb1; b2 = mb2; a1 = ma1; a2 = ma2;
        if (!(std::isfinite(b0) && std::isfinite(b1) && std::isfinite(b2)
              && std::isfinite(a1) && std::isfinite(a2))) fallback();
        else blendWithRbj();
    }

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
