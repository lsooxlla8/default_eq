// ShelfNaNSweepTest — exhaustive guard against non-finite filter coefficients.
//
// Regression test for the shelf defect present in v0.3.0 through v2.3.0, where
// the RBJ shelf-slope parameter S was clamped to [0.1, 4.0] despite being defined
// only for 0 < S <= 1. Above S = 1 the square-root argument could go negative,
// yielding NaN coefficients that poisoned the filter state permanently.
//
// Sweeps the full exposed parameter space and asserts that neither the
// coefficients nor the processed output can become non-finite.
//
// Standalone: depends only on <cmath> and <algorithm> via Biquad.h.
// Build:  c++ -std=c++17 -O2 -I ../Source/DSP ShelfNaNSweepTest.cpp
#include "../Source/DSP/Biquad.h"
#include <cstdio>
#include <string>
#include <vector>

int main()
{
    const std::vector<double> sampleRates { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    const std::vector<Biquad::Type> types {
        Biquad::Type::Bell, Biquad::Type::LowShelf, Biquad::Type::HighShelf,
        Biquad::Type::HighPass, Biquad::Type::LowPass, Biquad::Type::Bandpass
    };
    const std::vector<std::string> names {
        "Bell", "LowShelf", "HighShelf", "HighPass", "LowPass", "Bandpass"
    };

    long long tested = 0, badCoeff = 0, badOut = 0;
    std::string firstFailure;

    for (double sr : sampleRates)
        for (size_t t = 0; t < types.size(); ++t)
            for (double f = 20.0; f <= 20000.0; f *= 1.15)          // 20 Hz - 20 kHz
                for (double q = 0.1; q <= 24.0; q += 0.1)           // full Q range
                    for (double g = -36.0; g <= 36.0; g += 0.5)     // full gain range
                    {
                        Biquad bq;
                        bq.set(types[t], sr, f, q, g);
                        ++tested;

                        if (!std::isfinite(bq.b0) || !std::isfinite(bq.b1) || !std::isfinite(bq.b2)
                         || !std::isfinite(bq.a1) || !std::isfinite(bq.a2))
                        {
                            ++badCoeff;
                            if (firstFailure.empty())
                            {
                                char buf[256];
                                std::snprintf(buf, sizeof buf,
                                    "%s sr=%.0f f=%.1f Q=%.2f gain=%.1f",
                                    names[t].c_str(), sr, f, q, g);
                                firstFailure = buf;
                            }
                            continue;
                        }

                        for (int n = 0; n < 64; ++n)
                        {
                            const float y = bq.processL((n % 2 == 0) ? 0.5f : -0.5f);
                            if (!std::isfinite(y)) { ++badOut; break; }
                        }
                    }

    std::printf("combinations tested : %lld\n", tested);
    std::printf("non-finite coeffs   : %lld\n", badCoeff);
    std::printf("non-finite output   : %lld\n", badOut);
    if (!firstFailure.empty())
        std::printf("first failure       : %s\n", firstFailure.c_str());

    const bool pass = (badCoeff == 0 && badOut == 0);
    std::printf("%s\n", pass ? "PASS - no NaN/Inf anywhere" : "FAIL - non-finite values present");
    return pass ? 0 : 1;
}
