    inline float processPhaseDistortion(float x, float character, float depth,
                                        bool rightState)
    {
        auto& delay = rightState ? driveDelayR : driveDelayL;
        auto& pos = rightState ? driveDelayPosR : driveDelayPosL;
        auto& envelope = rightState ? phaseEnvelopeR : phaseEnvelopeL;
        auto& tailGain = rightState ? phaseTailGainR : phaseTailGainL;
        auto& silenceSamples = rightState ? phaseSilenceSamplesR : phaseSilenceSamplesL;
        const int delaySize = (int)delay.size();
        if (delaySize < 2 || driveProcessSampleRate <= 0.0) return x;

        const float c01 = std::clamp(character, 0.0f, 1.0f);
        const float nyquistSafe = (float)std::max(40.0, 0.45 * driveProcessSampleRate);
        const float toneHz = 40.0f * std::pow(nyquistSafe / 40.0f, c01);
        const float toneCoefficient = c01 >= 0.999f ? 0.0f
            : (float)std::exp(-juce::MathConstants<double>::twoPi * toneHz
                             / driveProcessSampleRate);
        envelope = (1.0f - toneCoefficient) * x + toneCoefficient * envelope;

        const float boundedDepth = std::clamp(depth, 0.0f, 1.0f);
        const float modulator = std::clamp(envelope, 0.0f, 1.0f);
        const float delaySamples = std::clamp(0.001f * 50.0f * boundedDepth * modulator
                                                * (float)driveProcessSampleRate,
                                              0.0f, (float)(delaySize - 2));
        delay[(size_t)pos] = x;
        float readPosition = (float)pos - delaySamples;
        while (readPosition < 0.0f) readPosition += (float)delaySize;
        const int first = (int)std::floor(readPosition) % delaySize;
        const int second = (first + 1) % delaySize;
        const float delayed = juce::jmap(readPosition - (float)first,
                                        delay[(size_t)first], delay[(size_t)second]);
        pos = (pos + 1) % delaySize;

        constexpr float silenceThreshold = 3.16227766e-5f;
        const int silenceHold = std::max(8, juce::roundToInt(0.001 * driveProcessSampleRate));
        if (std::abs(x) <= silenceThreshold)
            ++silenceSamples;
        else
        {
            silenceSamples = 0;
            tailGain = 1.0f;
        }
        if (silenceSamples >= silenceHold)
            tailGain = std::max(0.0f, tailGain
                - (float)(1.0 / std::max(1.0, 0.006 * driveProcessSampleRate)));
        return delayed * tailGain;
    }

    inline float processModulatedDelay(float x, float character, float depth,
                                       float noiseMix, bool rightState)
    {
        auto& delay = rightState ? driveDelayR : driveDelayL;
        auto& pos = rightState ? driveDelayPosR : driveDelayPosL;
        auto& phase = rightState ? drivePhaseR : drivePhaseL;
        auto& memory = rightState ? driveMemoryR : driveMemoryL;
        const float frequency = character <= 0.5f
            ? 4000.0f * character * character
            : 1000.0f * std::pow(10.0f, 2.0f * character - 1.0f);
        if (frequency <= 0.0f) return x;
        phase += juce::MathConstants<double>::twoPi * frequency / std::max(1.0, driveProcessSampleRate);
        if (phase >= juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
        memory = 0.97f * memory + 0.03f * std::sin((float)phase * 12.9898f + 78.233f);
        const float source = juce::jmap(noiseMix, std::sin((float)phase), std::tanh(memory * 2.0f));
        const int delayCapacity = (int)delay.size();
        if (delayCapacity < 2) return x;
        const float delaySamples = std::clamp(0.001f * 50.0f * std::pow(depth, 2.5849625f)
                                              * (0.5f + 0.5f * source) * (float)driveProcessSampleRate,
                                              0.0f, (float)(delayCapacity - 2));
        delay[(size_t)pos] = x;
        float read = (float)pos - delaySamples;
        while (read < 0.0f) read += (float)delayCapacity;
        const int first = (int)read % delayCapacity;
        const int second = (first + 1) % delayCapacity;
        const float out = juce::jmap(read - std::floor(read), delay[(size_t)first], delay[(size_t)second]);
        pos = (pos + 1) % delayCapacity;
        return out;
    }

    // The v0.1.0 cut: identical first-order stages and a continuously wet/dry
    // final stage. Q is deliberately absent from this path.
    inline float processClassicCut(float x, bool right)
    {
        if (!right && classicCutCoefficientRamping)
        {
            bool remains = false;
            const int activeStages = std::min(maxClassicCutStages,
                classicCutFullStages + (classicCutFractionalStage > 0.0001f ? 1 : 0));
            for (int stage = 0; stage < activeStages; ++stage)
            {
                classicCutStages[(size_t)stage].advanceCoefficientRamp();
                remains = remains
                    || classicCutStages[(size_t)stage].coefficientRampRemaining > 0;
            }
            classicCutCoefficientRamping = remains;
        }
        for (int stage=0; stage<classicCutFullStages; ++stage)
            x=classicCutStages[(size_t)stage].process(x,right);
        if (classicCutFractionalStage>0.0001f && classicCutFullStages<maxClassicCutStages)
        {
            const float wet=classicCutStages[(size_t)classicCutFullStages].process(x,right);
            x += classicCutFractionalStage*(wet-x);
        }
        return x;
    }

    static inline float dcBlock(float x, float& previousX, float& previousY)
    {
        const float y = x - previousX + 0.995f * previousY;
        previousX = x;
        previousY = y;
        return y;
    }

    inline void applyBandDrive(float& l, float& r)
    {
        constexpr float invSqrt2 = 0.7071067811865475f;
        const auto driveOneBand = [this](float band, bool rightState)
        {
            float wet = saturateOne(band, rightState) * driveAutoGainLinear;
            wet = rightState ? dcBlock(wet, dcXR, dcYR) : dcBlock(wet, dcXL, dcYL);
            return wet - band;
        };
        const float firstWeight = firstPlacementWeight;
        const float secondWeight = secondPlacementWeight;
        if (!midSidePlacement)
        {
            float bandL = l, bandR = r;
            driveBandBiquad.processStereo(bandL, bandR);
            l += routeEffectWeight * driveGlobalAmount * firstWeight * driveOneBand(bandL, false);
            r += routeEffectWeight * driveGlobalAmount * secondWeight * driveOneBand(bandR, true);
        }
        else
        {
            float mid = (l + r) * invSqrt2;
            float side = (l - r) * invSqrt2;
            mid += routeEffectWeight * driveGlobalAmount * firstWeight * driveOneBand(driveBandBiquad.processL(mid), false);
            side += routeEffectWeight * driveGlobalAmount * secondWeight * driveOneBand(driveBandBiquad.processR(side), true);
            l = (mid + side) * invSqrt2;
            r = (mid - side) * invSqrt2;
        }
    }

    void updateDynamicTimeConstants(double sampleRate)
    {
        if (cachedDynSampleRate == sampleRate && cachedDynAttackMs == dynAttackMs
            && cachedDynReleaseMs == dynReleaseMs)
            return;
        const float safeAttack = std::max(0.01f, dynAttackMs);
        const float safeRelease = std::max(0.01f, dynReleaseMs);
        dynAttackCoeff = 1.0f - std::exp(-1.0f / (float)(sampleRate * safeAttack * 0.001f));
        dynReleaseCoeff = 1.0f - std::exp(-1.0f / (float)(sampleRate * safeRelease * 0.001f));
        cachedDynSampleRate = sampleRate;
        cachedDynAttackMs = dynAttackMs;
        cachedDynReleaseMs = dynReleaseMs;
    }

    void setAllStages(double sampleRate, bool updateAuxiliary)
    {
        processSampleRate = sampleRate;
        const bool classicCut = zl_filter::isClassicCut(type);
        const bool resonantCut = zl_filter::isResonantCut(type);
        const bool bandPass = type == Biquad::Type::Bandpass;
        const float totalGainDb = (gainDb + (classicCut || resonantCut || bandPass ? 0.0f : dynGainMod))
            * gainScale;
        const float dynamicSlope = classicCut
            ? dynamicClassicCutSlope(slopeDbPerOct, dynGainMod)
            : slopeDbPerOct;
        const float dynamicQ = resonantCut ? dynamicResonantCutQ(Q, dynGainMod)
            : bandPass ? dynamicBandPassQ(Q, dynGainMod) : Q;
        if (zl_filter::isClassicCut(type))
        {
            const float classicAmount = dynamicSlope / 6.0f;
            classicCutFullStages = std::clamp((int)std::floor(classicAmount), 0,
                                              maxClassicCutStages);
            classicCutFractionalStage = classicAmount - (float)classicCutFullStages;
            const bool highPass=type==Biquad::Type::HighPass;
            const int rampSamples = coefficientsValid ? coeffUpdateInterval + 1 : 0;
            classicCutCoefficientRamping = rampSamples > 0;
            for(auto& stage:classicCutStages) stage.set(highPass,sampleRate,freqHz,rampSamples);
        }
        else
        {
            zlCascade.configure(type,sampleRate,freqHz,dynamicQ,totalGainDb,slopeDbPerOct,
                                coefficientsValid ? coeffUpdateInterval + 1 : 0);
        }
        if(updateAuxiliary)
        {
            const float detectorQ = classicCut ? 2.0f
                : std::clamp(dynamicQ, 0.2f, 12.0f);
            scBiquad.set(Biquad::Type::Bandpass,sampleRate,freqHz,detectorQ,0.0);
            auditionBiquad.set(Biquad::Type::Bandpass,sampleRate,freqHz,std::clamp(Q,0.2f,12.0f),0.0);
        }
    }
