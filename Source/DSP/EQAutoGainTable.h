#pragma once

#include "FilterTypes.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace deq::eq_auto_gain_table
{
// Deterministic coverage anchors for a single static EQ band, calibrated by the
// integration matrix rather than presented as a material-independent loudness
// truth. Shelves intentionally use full boost compensation: a low-frequency-
// heavy programme can realise the complete shelf gain, which the previous
// equal-energy average underestimated.
inline constexpr std::array<float, 6> qAnchors { 0.1f, 0.5f, 1.0f, 2.0f, 8.0f, 24.0f };
inline constexpr std::array<float, 6> bellBoostCoverage { 0.80f, 0.58f, 0.44f, 0.30f, 0.14f, 0.08f };
inline constexpr std::array<float, 6> bellCutCoverage { 0.24f, 0.16f, 0.12f, 0.08f, 0.04f, 0.02f };

inline float interpolateBellCoverage(float q, float gainDb) noexcept
{
    const auto& values = gainDb >= 0.0f ? bellBoostCoverage : bellCutCoverage;
    const float clamped = std::clamp(q, qAnchors.front(), qAnchors.back());
    for (size_t upper = 1; upper < qAnchors.size(); ++upper)
        if (clamped <= qAnchors[upper])
        {
            const size_t lower = upper - 1;
            const float position = std::log(clamped / qAnchors[lower])
                / std::log(qAnchors[upper] / qAnchors[lower]);
            return values[lower] + position * (values[upper] - values[lower]);
        }
    return values.back();
}

inline float coverage(int type, float q, float gainDb) noexcept
{
    switch (type)
    {
        case filter_types::lowShelf:
        case filter_types::highShelf:
            return gainDb >= 0.0f ? 1.0f : 0.30f;
        case filter_types::bell:
            return interpolateBellCoverage(q, gainDb);
        case filter_types::tilt:
            return gainDb >= 0.0f ? 0.42f : 0.25f;
        default:
            return 0.0f;
    }
}

inline float compensationDb(int type, float gainDb, float q, float amount,
                            float placementPercent) noexcept
{
    const float placementCoverage = 1.0f - 0.5f * std::abs(
        std::clamp(placementPercent, -100.0f, 100.0f)) * 0.01f;
    return std::clamp(-gainDb * amount * coverage(type, q, gainDb * amount)
                          * placementCoverage,
                      -36.0f, 36.0f);
}
}
