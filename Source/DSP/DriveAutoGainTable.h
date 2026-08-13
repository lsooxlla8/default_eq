#pragma once

#include <array>
#include <algorithm>
#include <cmath>

namespace deq::drive_auto_gain_table
{
// Offline RMS references generated against default_equalizer's actual ten
// per-band transfer functions. This intentionally follows default_distortion's
// deterministic lookup-table model without reusing incompatible gain values.
inline constexpr std::array<float, 8> drivesDb { 0.0f, 1.0f, 3.0f, 6.0f, 12.0f, 18.0f, 27.0f, 36.0f };
inline constexpr std::array<std::array<float, 8>, 10> gains {{
    {{ 1.00000000f, 0.72625567f, 0.47589128f, 0.33183049f, 0.23503404f, 0.20103527f, 0.17952856f, 0.16958972f }},
    {{ 1.00000000f, 0.70588235f, 0.44444444f, 0.28658656f, 0.20109474f, 0.17854663f, 0.16572387f, 0.15995777f }},
    {{ 1.00000000f, 0.45249629f, 0.31207483f, 0.23205633f, 0.17596327f, 0.15392262f, 0.13800577f, 0.12956483f }},
    {{ 1.00000000f, 0.67641043f, 0.44807298f, 0.32073947f, 0.24300262f, 0.22310820f, 0.20309733f, 0.18746078f }},
    {{ 1.00000000f, 3.77365522f, 1.10154050f, 0.49001366f, 0.30402492f, 0.26241614f, 0.23963360f, 0.22965387f }},
    {{ 1.00000000f, 0.77276307f, 0.51142192f, 0.36267770f, 0.26414699f, 0.22987884f, 0.20829059f, 0.19835917f }},
    {{ 1.00000000f, 0.27286382f, 0.26701976f, 0.28030981f, 0.32086528f, 0.31560410f, 0.28415913f, 0.26251559f }},
    {{ 1.00000000f, 1.00421763f, 1.00714114f, 1.00359024f, 1.00655599f, 1.00550755f, 1.00780754f, 1.00741121f }},
    {{ 1.00000000f, 0.73092013f, 0.48286328f, 0.34146330f, 0.24772730f, 0.21515723f, 0.19461223f, 0.18510359f }},
    {{ 1.00000000f, 1.00450703f, 1.00242850f, 1.00726568f, 1.00755250f, 1.00607267f, 1.00715308f, 1.00718723f }}
}};

inline float lookup(int mode, float driveDb) noexcept
{
    const auto& row = gains[(size_t)std::clamp(mode, 0, 9)];
    const float drive = std::clamp(driveDb, drivesDb.front(), drivesDb.back());
    for (size_t upper = 1; upper < drivesDb.size(); ++upper)
        if (drive <= drivesDb[upper])
        {
            const size_t lower = upper - 1;
            const float fraction = (drive - drivesDb[lower]) / (drivesDb[upper] - drivesDb[lower]);
            // Interpolate in dB so the compensation curve stays perceptually smooth.
            const float a = std::log(std::max(row[lower], 1.0e-6f));
            const float b = std::log(std::max(row[upper], 1.0e-6f));
            return std::exp(a + fraction * (b - a));
        }
    return row.back();
}
}
