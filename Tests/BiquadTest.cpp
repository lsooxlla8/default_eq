/*
    BiquadTest.cpp — standalone test for Biquad coefficient generation.
    Verifies RBJ Audio EQ Cookbook coefficients across multiple filter types
    and sample rates.

    Build & run:
        cmake -S . -B build -DDEFAULT_EQ_BUILD_TESTS=ON
        cmake --build build --target DefaultEQ_Tests
        ./build/DefaultEQ_Tests
*/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <array>
#include <algorithm>
#include <complex>

// Include the struct under test (standalone, no JUCE dependency)
#include "../Source/DSP/Biquad.h"
#include "../Source/DSP/VariableSlope.h"

// ── Helpers ──────────────────────────────────────────────────────────

static constexpr double kTol = 1e-10;

static bool near(double a, double b) { return std::abs(a - b) < kTol; }

static int failures = 0;

#define CHECK(cond, msg) \
    do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } } while (0)

static double magnitudeDb(const Biquad& bq, double f, double sr)
{
    const double w = 2.0 * kPi * f / sr;
    const auto z1 = std::complex<double>(std::cos(w), -std::sin(w));
    const auto z2 = z1 * z1;
    const auto h = (bq.b0 + bq.b1 * z1 + bq.b2 * z2)
                 / (1.0 + bq.a1 * z1 + bq.a2 * z2);
    return 20.0 * std::log10(std::max(1.0e-15, std::abs(h)));
}

// ── Reference RBJ coefficient generators (must exactly mirror Biquad::set) ──
struct NormCoeffs { double b0, b1, b2, a1, a2; };

static NormCoeffs ref_bell(double sr, double freq, double Q, double gain)
{
    freq = std::clamp(freq, 10.0, sr * 0.45);
    Q    = std::clamp(Q, 0.1, 24.0);
    double A     = std::pow(10.0, gain / 40.0);
    double w0    = 2.0 * kPi * (freq / sr);
    double cosw0 = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * Q);
    double b0_ = 1.0 + alpha * A;
    double b1_ = -2.0 * cosw0;
    double b2_ = 1.0 - alpha * A;
    double a0_ = 1.0 + alpha / A;
    double a1_ = -2.0 * cosw0;
    double a2_ = 1.0 - alpha / A;
    return { b0_/a0_, b1_/a0_, b2_/a0_, a1_/a0_, a2_/a0_ };
}

static NormCoeffs ref_lowpass(double sr, double freq, double Q, double /*gain*/)
{
    freq = std::clamp(freq, 10.0, sr * 0.45);
    Q    = std::clamp(Q, 0.1, 24.0);
    double w0    = 2.0 * kPi * (freq / sr);
    double cosw0 = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * Q);
    double b0_ = (1.0 - cosw0) * 0.5;
    double b1_ = 1.0 - cosw0;
    double b2_ = (1.0 - cosw0) * 0.5;
    double a0_ = 1.0 + alpha;
    return { b0_/a0_, b1_/a0_, b2_/a0_, (-2.0*cosw0)/a0_, (1.0 - alpha)/a0_ };
}

static NormCoeffs ref_highpass(double sr, double freq, double Q, double /*gain*/)
{
    freq = std::clamp(freq, 10.0, sr * 0.45);
    Q    = std::clamp(Q, 0.1, 24.0);
    double w0    = 2.0 * kPi * (freq / sr);
    double cosw0 = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * Q);
    double b0_ = (1.0 + cosw0) * 0.5;
    double b1_ = -(1.0 + cosw0);
    double b2_ = (1.0 + cosw0) * 0.5;
    double a0_ = 1.0 + alpha;
    return { b0_/a0_, b1_/a0_, b2_/a0_, (-2.0*cosw0)/a0_, (1.0 - alpha)/a0_ };
}

static NormCoeffs ref_lowshelf(double sr, double freq, double Q, double gain)
{
    freq = std::clamp(freq, 10.0, sr * 0.45);
    Q    = std::clamp(Q, 0.1, 24.0);
    double A     = std::pow(10.0, gain / 40.0);
    double w0    = 2.0 * kPi * (freq / sr);
    double cosw0 = std::cos(w0);
    double sinw0 = std::sin(w0);
    double S     = std::clamp(Q / 2.0, 0.1, 4.0);
    double alphaS = sinw0/2.0 * std::sqrt((A + 1.0/A) * (1.0/S - 1.0) + 2.0);
    double b0_ =    A*((A+1) - (A-1)*cosw0 + 2*std::sqrt(A)*alphaS);
    double b1_ =  2*A*((A-1) - (A+1)*cosw0);
    double b2_ =    A*((A+1) - (A-1)*cosw0 - 2*std::sqrt(A)*alphaS);
    double a0_ =        (A+1) + (A-1)*cosw0 + 2*std::sqrt(A)*alphaS;
    double a1_ =   -2*((A-1) + (A+1)*cosw0);
    double a2_ =        (A+1) + (A-1)*cosw0 - 2*std::sqrt(A)*alphaS;
    return { b0_/a0_, b1_/a0_, b2_/a0_, a1_/a0_, a2_/a0_ };
}

static NormCoeffs ref_highshelf(double sr, double freq, double Q, double gain)
{
    freq = std::clamp(freq, 10.0, sr * 0.45);
    Q    = std::clamp(Q, 0.1, 24.0);
    double A     = std::pow(10.0, gain / 40.0);
    double w0    = 2.0 * kPi * (freq / sr);
    double cosw0 = std::cos(w0);
    double sinw0 = std::sin(w0);
    double S     = std::clamp(Q / 2.0, 0.1, 4.0);
    double alphaS = sinw0/2.0 * std::sqrt((A + 1.0/A) * (1.0/S - 1.0) + 2.0);
    double b0_ =    A*((A+1) + (A-1)*cosw0 + 2*std::sqrt(A)*alphaS);
    double b1_ = -2*A*((A-1) + (A+1)*cosw0);
    double b2_ =    A*((A+1) + (A-1)*cosw0 - 2*std::sqrt(A)*alphaS);
    double a0_ =        (A+1) - (A-1)*cosw0 + 2*std::sqrt(A)*alphaS;
    double a1_ =    2*((A-1) - (A+1)*cosw0);
    double a2_ =        (A+1) - (A-1)*cosw0 - 2*std::sqrt(A)*alphaS;
    return { b0_/a0_, b1_/a0_, b2_/a0_, a1_/a0_, a2_/a0_ };
}

static NormCoeffs ref_bandpass(double sr, double freq, double Q, double /*gain*/)
{
    freq = std::clamp(freq, 10.0, sr * 0.45);
    Q    = std::clamp(Q, 0.1, 24.0);
    double w0    = 2.0 * kPi * (freq / sr);
    double cosw0 = std::cos(w0);
    double alpha = std::sin(w0) / (2.0 * Q);
    double a0_ = 1.0 + alpha;
    return { alpha/a0_, 0.0, -alpha/a0_, (-2.0*cosw0)/a0_, (1.0 - alpha)/a0_ };
}

static NormCoeffs ref_notch(double sr, double freq, double Q, double /*gain*/)
{
    freq = std::clamp(freq, 10.0, sr * 0.45);
    Q = std::clamp(Q, 0.1, 24.0);
    const double w0 = 2.0 * kPi * (freq / sr);
    const double cosw0 = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * Q);
    const double a0 = 1.0 + alpha;
    return { 1.0 / a0, (-2.0 * cosw0) / a0, 1.0 / a0,
             (-2.0 * cosw0) / a0, (1.0 - alpha) / a0 };
}

// ── Unified test runner ──

using RefFn = NormCoeffs(*)(double, double, double, double);

static void testType(Biquad::Type type, const char* name, RefFn ref,
                     double sr, double freq, double Q, double gain)
{
    Biquad bq;
    bq.set(type, sr, freq, Q, gain);
    NormCoeffs exp = ref(sr, freq, Q, gain);

    char buf[256];
    auto check = [&](double actual, double expected, const char* coeff) {
        std::snprintf(buf, sizeof(buf), "%s @ %.0fHz/%.0fHz Q=%.2f gain=%.1f — %s (got %.12g, exp %.12g)",
                      name, freq, sr, Q, gain, coeff, actual, expected);
        CHECK(near(actual, expected), buf);
    };
    check(bq.b0, exp.b0, "b0");
    check(bq.b1, exp.b1, "b1");
    check(bq.b2, exp.b2, "b2");
    check(bq.a1, exp.a1, "a1");
    check(bq.a2, exp.a2, "a2");
}

// ── Main ──

int main()
{
    static const double srs[] = { 44100.0, 48000.0, 96000.0 };

    for (double sr : srs)
    {
        testType(Biquad::Type::Bell,      "Bell",      ref_bell,      sr, 1000.0,  1.0,  6.0);
        testType(Biquad::Type::Bell,      "Bell",      ref_bell,      sr,  200.0,  0.5, -3.0);
        testType(Biquad::Type::LowShelf,  "LowShelf",  ref_lowshelf,  sr,  100.0,  0.7,  4.0);
        testType(Biquad::Type::LowShelf,  "LowShelf",  ref_lowshelf,  sr,  250.0,  1.5, -6.0);
        testType(Biquad::Type::HighShelf, "HighShelf",  ref_highshelf, sr, 8000.0,  0.7,  3.0);
        testType(Biquad::Type::HighShelf, "HighShelf",  ref_highshelf, sr, 4000.0,  1.0, -5.0);
        testType(Biquad::Type::HighPass,  "HighPass",   ref_highpass,  sr,   80.0,  0.7,  0.0);
        testType(Biquad::Type::HighPass,  "HighPass",   ref_highpass,  sr,  300.0,  1.4,  0.0);
        testType(Biquad::Type::LowPass,   "LowPass",    ref_lowpass,  sr,10000.0,  0.7,  0.0);
        testType(Biquad::Type::LowPass,   "LowPass",    ref_lowpass,  sr, 5000.0,  2.0,  0.0);
        testType(Biquad::Type::Bandpass,  "Bandpass",   ref_bandpass,  sr, 1000.0,  2.0,  0.0);
        testType(Biquad::Type::Bandpass,  "Bandpass",   ref_bandpass,  sr,  500.0,  0.5,  0.0);
        testType(Biquad::Type::Notch,     "Notch",      ref_notch,     sr, 1000.0,  2.0,  0.0);
    }

    // ── Sanity: process a sample, verify no NaN ──
    {
        Biquad bq;
        bq.set(Biquad::Type::Bell, 44100.0, 1000.0, 1.0, 6.0);
        float out = bq.processL(1.0f);
        CHECK(!std::isnan(out), "processL returned NaN");
        CHECK(!std::isinf(out), "processL returned Inf");
    }

    // ── Sanity: reset clears delay state ──
    {
        Biquad bq;
        bq.set(Biquad::Type::Bell, 44100.0, 1000.0, 1.0, 6.0);
        bq.processL(1.0f);
        bq.processR(0.5f);
        bq.reset();
        CHECK(bq.z1L == 0.0 && bq.z2L == 0.0 && bq.z1R == 0.0 && bq.z2R == 0.0,
              "reset() clears all delay state");
    }

    // Tilt gain is symmetric around its pivot: +6 dB means -6 dB in the
    // low band and +6 dB in the high band.
    {
        Biquad tilt;
        tilt.set(Biquad::Type::Tilt, 48000.0, 1000.0, 1.0, 6.0);
        const double low = magnitudeDb(tilt, 40.0, 48000.0);
        const double high = magnitudeDb(tilt, 16000.0, 48000.0);
        CHECK(std::abs(low + 6.0) < 0.35, "tilt reaches -6 dB below its pivot");
        CHECK(std::abs(high - 6.0) < 0.35, "tilt reaches +6 dB above its pivot");
        CHECK(std::abs(low + high) < 0.35, "tilt endpoints remain symmetric");
    }

    // ── Matched/de-cramped sweep: finite coefficients over the exposed domain ──
    for (double sr : srs)
        for (int type = 0; type <= 7; ++type)
            for (double freq : { 20.0, 1000.0, sr * 0.2, sr * 0.45 })
                for (double q : { 0.1, 0.707, 4.0, 24.0 })
                    for (double gain : { -36.0, 0.0, 36.0 })
                    {
                        Biquad bq;
                        bq.setMatched(static_cast<Biquad::Type>(type), sr, freq, q, gain);
                        CHECK(std::isfinite(bq.b0) && std::isfinite(bq.b1) && std::isfinite(bq.b2)
                              && std::isfinite(bq.a1) && std::isfinite(bq.a2),
                              "matched coefficients remain finite");
                    }

    // At a high center frequency the matched bell must be closer to its analog
    // prototype away from the center than the bilinear/RBJ design.
    {
        constexpr double sr = 48000.0, center = 16000.0, probe = 12000.0, q = 1.0, gainDb = 12.0;
        Biquad rbj, matched;
        rbj.set(Biquad::Type::Bell, sr, center, q, gainDb);
        matched.setMatched(Biquad::Type::Bell, sr, center, q, gainDb);
        const double G = std::pow(10.0, gainDb / 20.0);
        const double x = probe / center;
        const double real = 1.0 - x * x;
        const double analog = 10.0 * std::log10((real * real + x * x * G / (q * q))
                                               / (real * real + x * x / (G * q * q)));
        const double rbjError = std::abs(magnitudeDb(rbj, probe, sr) - analog);
        const double matchedError = std::abs(magnitudeDb(matched, probe, sr) - analog);
        CHECK(matchedError < rbjError, "matched bell improves near-Nyquist analog response");
        std::printf("decramp probe: RBJ error %.4f dB, matched error %.4f dB\n", rbjError, matchedError);
    }

    // De-cramping must not gratuitously reshape ordinary low/mid-frequency EQ.
    {
        Biquad rbj, matched;
        rbj.set(Biquad::Type::Bell, 48000.0, 1000.0, 1.0, 9.0);
        matched.setMatched(Biquad::Type::Bell, 48000.0, 1000.0, 1.0, 9.0);
        double worstDifference = 0.0;
        for (double probe : { 100.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0 })
            worstDifference = std::max(worstDifference,
                std::abs(magnitudeDb(rbj, probe, 48000.0) - magnitudeDb(matched, probe, 48000.0)));
        CHECK(worstDifference < 0.12, "decramping leaves low-frequency bell shape materially unchanged");
        std::printf("decramp low-frequency worst delta %.5f dB\n", worstDifference);
    }

    // Variable slope keeps the requested center gain and makes a material,
    // externally measurable change at normal EQ gains.
    {
        constexpr double sr = 48000.0, center = 1000.0, q = 1.0, gain = 24.0;
        const auto db = [](std::complex<double> h)
        {
            return 20.0 * std::log10(std::max(std::abs(h), 1.0e-15));
        };
        for (double slope : { 12.0, 24.0, 48.0, 96.0 })
            CHECK(std::abs(db(variable_slope::response(Biquad::Type::Bell, sr, center,
                q, gain, slope, false, center)) - gain) < 2.6,
                "ZL bell cascades keep the center within their designed high-order tolerance");
        const double softShoulder = db(variable_slope::response(Biquad::Type::Bell,
            sr, center, q, gain, 12.0, false, center * 0.8));
        const double steepShoulder = db(variable_slope::response(Biquad::Type::Bell,
            sr, center, q, gain, 48.0, false, center * 0.8));
        const double softSkirt = db(variable_slope::response(Biquad::Type::Bell,
            sr, center, q, gain, 12.0, false, center * 0.25));
        const double steepSkirt = db(variable_slope::response(Biquad::Type::Bell,
            sr, center, q, gain, 48.0, false, center * 0.25));
        CHECK(steepShoulder > softShoulder + 1.0,
              "high bell shape creates a flatter top shoulder");
        CHECK(steepSkirt < softSkirt - 1.0,
              "high bell shape steepens the outer skirt");
        const double narrowShoulder = db(variable_slope::response(Biquad::Type::Bell,
            sr, center, 2.0, gain, 12.0, false, center * 0.8));
        CHECK(steepShoulder > softShoulder && narrowShoulder < softShoulder,
              "Bell slope squares the shoulder while Q independently narrows it");

        const double shelfSoftLowSide = db(variable_slope::response(Biquad::Type::LowShelf,
            sr, center, q, 6.0, 6.0, false, center * 0.7));
        const double shelfSteepLowSide = db(variable_slope::response(Biquad::Type::LowShelf,
            sr, center, q, 6.0, 48.0, false, center * 0.7));
        const double shelfSoftHighSide = db(variable_slope::response(Biquad::Type::LowShelf,
            sr, center, q, 6.0, 6.0, false, center * 1.4));
        const double shelfSteepHighSide = db(variable_slope::response(Biquad::Type::LowShelf,
            sr, center, q, 6.0, 48.0, false, center * 1.4));
        CHECK(shelfSteepLowSide > shelfSoftLowSide + 0.8
                  && shelfSteepHighSide < shelfSoftHighSide - 0.8,
              "Shelf slope steepens opposite sides of the transition instead of acting as Q");

        for (auto type : { Biquad::Type::Bell, Biquad::Type::LowShelf,
                           Biquad::Type::HighShelf })
        {
            double maximumDelta = 0.0;
            for (double probe : { 250.0, 500.0, 800.0, 1250.0, 2000.0, 4000.0 })
                maximumDelta = std::max(maximumDelta, std::abs(
                    db(variable_slope::response(type, sr, center, q, 6.0, 12.0, false, probe))
                    - db(variable_slope::response(type, sr, center, q, 6.0, 48.0, false, probe))));
            CHECK(maximumDelta > 0.5,
                  "gain-bearing filter slope is visible to an external response analyser");
        }
        const auto flatTiltSoft = variable_slope::response(Biquad::Type::Tilt,
            sr, center, q, 6.0, 12.0, false, center);
        const auto flatTiltSteep = variable_slope::response(Biquad::Type::Tilt,
            sr, center, q, 6.0, 48.0, false, center);
        CHECK(std::abs(db(flatTiltSoft)) < 0.02 && std::abs(flatTiltSoft - flatTiltSteep) < 1.0e-12,
              "Flat Tilt keeps its pivot at unity and does not expose a fictitious slope order");
    }

    // Cut Q now has audible, normalized resonance while the independent cut
    // cascade still controls the asymptotic dB/oct slope.
    {
        constexpr double sr = 48000.0, center = 1000.0;
        const auto db = [](std::complex<double> h)
        {
            return 20.0 * std::log10(std::max(std::abs(h), 1.0e-15));
        };
        double qOnePeak = -300.0;
        for (int index = 0; index < 2048; ++index)
        {
            const double probe = 20.0 * std::pow(1000.0, (double)index / 2047.0);
            qOnePeak = std::max(qOnePeak, db(variable_slope::response(Biquad::Type::ResLowPass,
                sr, center, 1.0, 0.0, 12.0, false, probe)));
        }
        CHECK(qOnePeak > 1.0 && qOnePeak < 1.3,
              "ZL cut Q 1 has the upstream mild resonance");
        const double neutral = db(variable_slope::response(Biquad::Type::ResLowPass,
            sr, center, 1.0, 0.0, 24.0, false, center));
        const double resonant = db(variable_slope::response(Biquad::Type::ResLowPass,
            sr, center, 2.0, 0.0, 24.0, false, center));
        CHECK(resonant > neutral + 5.5,
              "cut Q 2 produces a clear cutoff resonance");
        const double shallowA = db(variable_slope::response(Biquad::Type::HighPass,
            sr, center, 1.0, 0.0, 6.0, false, center / 8.0));
        const double shallowB = db(variable_slope::response(Biquad::Type::HighPass,
            sr, center, 1.0, 0.0, 6.0, false, center / 4.0));
        const double steepA = db(variable_slope::response(Biquad::Type::HighPass,
            sr, center, 1.0, 0.0, 48.0, false, center / 8.0));
        const double steepB = db(variable_slope::response(Biquad::Type::HighPass,
            sr, center, 1.0, 0.0, 48.0, false, center / 4.0));
        CHECK((steepA - steepB) < (shallowA - shallowB) - 25.0,
              "cut slope remains independently variable from resonance");
        const auto classicQ1=variable_slope::response(Biquad::Type::LowPass,sr,center,1.0,0.0,24.0,false,center);
        const auto classicQ12=variable_slope::response(Biquad::Type::LowPass,sr,center,12.0,0.0,24.0,false,center);
        CHECK(std::abs(classicQ1-classicQ12)<1.0e-12,
              "classic v0.1.0 cut ignores Q completely");
        CHECK(zl_filter::snapSlope(Biquad::Type::Bell,6.0f)==12.0f
              && zl_filter::snapSlope(Biquad::Type::ResLowPass,6.0f)==6.0f
              && zl_filter::snapSlope(Biquad::Type::Tilt,90.0f)==96.0f,
              "ZL slopes use the agreed discrete set and per-type minimum");
    }

    if (failures == 0)
        std::printf("ALL TESTS PASSED (RBJ + matched/de-cramped filters)\n");
    else
        std::printf("%d TEST(S) FAILED\n", failures);

    return failures == 0 ? 0 : 1;
}
