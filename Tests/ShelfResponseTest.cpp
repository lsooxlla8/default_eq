// ShelfResponseTest — asserts shelf magnitude response is correct and bounded.
//
// Two properties are checked across the full Q and gain range:
//
//   1. Asymptotic accuracy. A low shelf must approach its set gain at DC and
//      unity at Nyquist (and the reverse for a high shelf), within tolerance.
//
//   2. Boundedness. The response must never exceed the set gain. Overshoot is a
//      resonant peak at the corner frequency and it breaks the gain control's
//      contract: a +12 dB shelf that peaks at +19 dB is both unexpected and a
//      headroom hazard. Releases up to v2.3.0 overshot by 1.4-14+ dB before
//      failing to NaN entirely.
//
// Standalone: depends only on <cmath>/<algorithm> via Biquad.h, plus <complex>.
// Build:  c++ -std=c++17 -O2 -I ../Source/DSP ShelfResponseTest.cpp
#include "../Source/DSP/Biquad.h"
#include <cstdio>
#include <complex>
#include <vector>

// |H(z)| in dB at frequency f.
static double magDb(const Biquad& bq, double f, double sr)
{
    const double w = 2.0 * kPi * f / sr;
    const std::complex<double> z = std::polar(1.0, -w);
    return 20.0 * std::log10(std::abs((bq.b0 + bq.b1 * z + bq.b2 * z * z)
                                    / (1.0    + bq.a1 * z + bq.a2 * z * z)));
}

int main()
{
    const std::vector<double> sampleRates { 44100.0, 48000.0, 96000.0 };
    const std::vector<double> corners     { 100.0, 1000.0, 5000.0 };

    // Tolerances. Overshoot is held tight because boundedness is the invariant;
    // the asymptote tolerance is looser since very gentle shelves have not fully
    // settled by the measurement frequency.
    const double kMaxOvershootDb = 0.10;
    const double kAsymptoteTolDb = 0.50;   // measured worst case is 0.06 dB

    int failures = 0;
    double worstOvershoot = 0.0;
    double worstAsymptoteErr = 0.0;

    for (double sr : sampleRates)
        for (double f0 : corners)
            for (auto type : { Biquad::Type::LowShelf, Biquad::Type::HighShelf })
                for (double q = 0.1; q <= 24.0; q += 0.1)
                    for (double g = -36.0; g <= 36.0; g += 1.0)
                    {
                        if (g == 0.0) continue;

                        Biquad bq;
                        bq.set(type, sr, f0, q, g);

                        // Boundedness: nothing may exceed the outer bound of {0, g}.
                        const double hi = (g > 0.0) ? g : 0.0;
                        const double lo = (g > 0.0) ? 0.0 : g;
                        double overshoot = 0.0;
                        for (double f = 1.0; f < sr * 0.4999; f *= 1.05)
                        {
                            const double m = magDb(bq, f, sr);
                            if (m > hi) overshoot = std::max(overshoot, m - hi);
                            if (m < lo) overshoot = std::max(overshoot, lo - m);
                        }
                        if (overshoot > worstOvershoot) worstOvershoot = overshoot;
                        if (overshoot > kMaxOvershootDb)
                        {
                            if (++failures <= 5)
                                std::printf("FAIL overshoot %.2f dB : %s sr=%.0f f0=%.0f Q=%.2f g=%+.1f\n",
                                            overshoot, type == Biquad::Type::LowShelf ? "LowShelf" : "HighShelf",
                                            sr, f0, q, g);
                            continue;
                        }

                        // Asymptotes: shelf band approaches g, pass band approaches 0.
                        const double atDc  = magDb(bq, 1.0, sr);
                        const double atNyq = magDb(bq, sr * 0.4999, sr);
                        const double shelfBand = (type == Biquad::Type::LowShelf) ? atDc  : atNyq;
                        const double passBand  = (type == Biquad::Type::LowShelf) ? atNyq : atDc;

                        const double err = std::max(std::abs(shelfBand - g), std::abs(passBand));
                        if (err > worstAsymptoteErr) worstAsymptoteErr = err;
                        if (err > kAsymptoteTolDb && ++failures <= 5)
                            std::printf("FAIL asymptote %.2f dB : %s sr=%.0f f0=%.0f Q=%.2f g=%+.1f "
                                        "(shelf %.2f, pass %.2f)\n",
                                        err, type == Biquad::Type::LowShelf ? "LowShelf" : "HighShelf",
                                        sr, f0, q, g, shelfBand, passBand);
                    }

    std::printf("worst overshoot      : %.4f dB (limit %.2f)\n", worstOvershoot, kMaxOvershootDb);
    std::printf("worst asymptote err  : %.4f dB (limit %.2f)\n", worstAsymptoteErr, kAsymptoteTolDb);
    std::printf("failures             : %d\n", failures);
    std::printf("%s\n", failures == 0 ? "PASS - shelves bounded and asymptotically correct"
                                      : "FAIL - see failures above");
    return failures == 0 ? 0 : 1;
}
