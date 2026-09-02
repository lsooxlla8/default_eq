    inline float saturateOne(float x, bool rightState = false)
    {
        const float d = 1.0f + driveAmount * 15.0f;
        const float c = std::clamp(driveCharacter, 0.0f, 1.0f);
        float& memory = rightState ? driveMemoryR : driveMemoryL;
        switch (satType)
        {
            case SaturationType::SoftClip:
            {
                const float cubicInput = std::clamp(x * d, -1.0f, 1.0f);
                const float cubic = 1.5f * (cubicInput
                    - cubicInput * cubicInput * cubicInput / 3.0f);
                return juce::jmap(c, std::tanh(x * d), cubic);
            }
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
            case SaturationType::FET:
            {
                const float gateBias = -0.12f + 1.18f * (c * 2.0f - 1.0f);
                const float gate = std::max(0.0f, x * d + gateBias);
                const float base = std::max(0.0f, gateBias);
                return std::tanh(2.2f * (gate * gate - base * base) / (1.0f + 0.55f * std::abs(c * 2.0f - 1.0f)));
            }
            case SaturationType::Tape:
            {
                // Stable stateful hysteresis loop using the same Drive /
                // Hysteresis / Bias semantics as default_distortion.
                const float bias = (driveSecondary * 2.0f - 1.0f) * 0.18f;
                const float target = std::tanh((x * d + bias) + c * 0.8f * memory);
                memory += (0.02f + 0.18f * (1.0f - c)) * (target - memory);
                return 0.72f * target + 0.28f * memory;
            }
            case SaturationType::OddEven:
            {
                const float bounded = std::tanh(x * d);
                const float second = 2.0f * bounded * bounded - 1.0f;
                const float third = 4.0f * bounded * bounded * bounded - 3.0f * bounded;
                return juce::jmap(c, second, third);
            }
            case SaturationType::PhaseDistortion:
                return processPhaseDistortion(x, c, driveAmount, rightState);
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
