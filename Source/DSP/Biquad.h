#pragma once
#include <cmath>
#include <algorithm>

// C++17 constant — replaces the fragile #ifndef M_PI / #define M_PI pattern.
constexpr double kPi = 3.14159265358979323846;

struct Biquad
{
    // Transposed Direct Form II
    double b0=1, b1=0, b2=0, a1=0, a2=0;
    double z1L=0, z2L=0, z1R=0, z2R=0;

    void reset()
    {
        z1L = z2L = z1R = z2R = 0.0;
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

    enum class Type { Bell, LowShelf, HighShelf, HighPass, LowPass, Bandpass };

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

            case Type::LowPass:
            {
                b0_ = (1.0 - cosw0) * 0.5;
                b1_ = 1.0 - cosw0;
                b2_ = (1.0 - cosw0) * 0.5;
                a0_ = 1.0 + alpha;
                a1_ = -2.0 * cosw0;
                a2_ = 1.0 - alpha;
            } break;

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
                //     existing presets in that range are unchanged.
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
        }

        // normalize
        b0 = b0_ / a0_;
        b1 = b1_ / a0_;
        b2 = b2_ / a0_;
        a1 = a1_ / a0_;
        a2 = a2_ / a0_;
    }
};
