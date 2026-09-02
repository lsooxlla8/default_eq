    void reset(double sampleRate)
    {
        scBiquad.reset();
        auditionBiquad.reset();
        driveBandBiquad.reset();
        zlCascade.reset();
        for (auto& stage : classicCutStages) stage.reset();

        freqSm.reset(sampleRate, 0.02);   // 20ms
        qSm.reset(sampleRate, 0.02);
        gainSm.reset(sampleRate, 0.02);
        slopeSm.reset(sampleRate, 0.02);
        amountSm.reset(sampleRate, 0.01);

        freqSm.setCurrentAndTargetValue(freqHz);
        qSm.setCurrentAndTargetValue(Q);
        gainSm.setCurrentAndTargetValue(gainDb);
        slopeSm.setCurrentAndTargetValue(slopeDbPerOct);
        amountSm.setCurrentAndTargetValue(globalAmount);

        targetFreqHz = freqHz;
        targetQ = Q;
        targetGainDb = gainDb;
        targetSlopeDbPerOct = slopeDbPerOct;

        envLevel = 0.0f;
        detectorPeakL = detectorPeakR = 0.0f;
        dynGainMod = 0.0f;
        intervalCounter = 0;
        dynamicControlCounter = 0;
        coefficientsValid = false;
        classicCutCoefficientRamping = false;
        appliedGainScale = std::numeric_limits<float>::quiet_NaN();
        driveProcessSampleRate = 0.0;
        drivePreparedFrequency = drivePreparedQ = -1.0f;
        driveMemoryL = driveMemoryR = 0.0f;
        drivePhaseL = drivePhaseR = 0.0;
        phaseEnvelopeL = phaseEnvelopeR = 0.0f;
        phaseTailGainL = phaseTailGainR = 1.0f;
        phaseSilenceSamplesL = phaseSilenceSamplesR = 0;
        driveDelayPosL = driveDelayPosR = 0;
        const size_t maximumDelay = (size_t)std::ceil(sampleRate * 8.0 * 0.05) + 2u;
        driveDelayL.assign(maximumDelay, 0.0f);
        driveDelayR.assign(maximumDelay, 0.0f);
    }

    void beginBlock(double sampleRate, bool isEnabled, Biquad::Type newType,
                    float newFreqHz, float newQ, float newGainDb,
                    float newSlopeDbPerOct = 12.0f, bool useMidSidePlacement = false,
                    float newPlacement = 0.0f,
                    bool useDecramping = false)
    {
        const float clampedSlope = zl_filter::isClassicCut(newType)
            ? std::clamp(newSlopeDbPerOct, 3.0f, 96.0f)
            : zl_filter::snapSlope(newType, newSlopeDbPerOct);
        const bool newTypeIsClassicCut = zl_filter::isClassicCut(newType);
        const float clampedPlacement = std::clamp(newPlacement, -1.0f, 1.0f);
        const bool topologyChanged = !coefficientsValid || processSampleRate != sampleRate
            || enabled != isEnabled || type != newType
            || (!newTypeIsClassicCut && slopeDbPerOct != clampedSlope)
            || decrampEnabled != useDecramping;
        const bool gainScaleChanged = !std::isfinite(appliedGainScale)
            || std::abs(appliedGainScale - gainScale) > 1.0e-6f;
        const bool auxiliaryChanged = topologyChanged || targetFreqHz != newFreqHz || targetQ != newQ;
        enabled = isEnabled;
        detectorPeakL = detectorPeakR = 0.0f;
        type = newType;
        targetSlopeDbPerOct = clampedSlope;
        if (!coefficientsValid || !newTypeIsClassicCut)
        {
            slopeDbPerOct = targetSlopeDbPerOct;
            slopeSm.setCurrentAndTargetValue(slopeDbPerOct);
        }
        else if (slopeSm.getTargetValue() != targetSlopeDbPerOct)
            slopeSm.setTargetValue(targetSlopeDbPerOct);
        const float classicAmount = slopeDbPerOct / 6.0f;
        classicCutFullStages = std::clamp((int)std::floor(classicAmount), 0, maxClassicCutStages);
        classicCutFractionalStage = classicAmount - (float)classicCutFullStages;
        midSidePlacement = useMidSidePlacement;
        placement = clampedPlacement;
        firstPlacementWeight = std::clamp(1.0f - placement, 0.0f, 1.0f);
        secondPlacementWeight = std::clamp(1.0f + placement, 0.0f, 1.0f);
        decrampEnabled = useDecramping;
        if (!coefficientsValid)
            amountSm.setCurrentAndTargetValue(globalAmount);
        else
            amountSm.setTargetValue(globalAmount);

        targetFreqHz = newFreqHz;
        targetQ = newQ;
        targetGainDb = newGainDb;

        // If targets changed, set smoothers
        if (freqSm.getTargetValue() != targetFreqHz) freqSm.setTargetValue(targetFreqHz);
        if (qSm.getTargetValue() != targetQ)         qSm.setTargetValue(targetQ);
        if (gainSm.getTargetValue() != targetGainDb) gainSm.setTargetValue(targetGainDb);

        if (!enabled) return;

        updateDynamicTimeConstants(sampleRate);
        if (topologyChanged || auxiliaryChanged || gainScaleChanged || !coefficientsValid)
        {
            if (!coefficientsValid)
            {
                freqHz = targetFreqHz;
                Q = targetQ;
                gainDb = targetGainDb;
            }
            setAllStages(sampleRate, true);
            coefficientsValid = true;
            appliedGainScale = gainScale;
        }

    }

    void beginMeterBlock() noexcept
    {
        detectorPeakL = detectorPeakR = 0.0f;
    }

    inline void maybeUpdateCoeffs(double sampleRate)
    {
        if (!enabled) return;

        // Force periodic coefficient updates when dynamic EQ is active (dynGainMod changes per-sample)
        if (dynEnabled || freqSm.isSmoothing() || qSm.isSmoothing()
            || gainSm.isSmoothing() || slopeSm.isSmoothing())
        {
            if (intervalCounter++ >= coeffUpdateInterval)
            {
                intervalCounter = 0;
                freqHz = freqSm.getNextValue();
                Q      = qSm.getNextValue();
                gainDb = gainSm.getNextValue();
                slopeDbPerOct = slopeSm.getNextValue();
                const bool movingFrequencyOrQ = freqSm.isSmoothing() || qSm.isSmoothing();
                const bool dynamicallyMovingQ = dynEnabled
                    && (zl_filter::isResonantCut(type) || type == Biquad::Type::Bandpass);
                setAllStages(sampleRate, movingFrequencyOrQ || dynamicallyMovingQ);
            }
            else
            {
                (void)freqSm.getNextValue();
                (void)qSm.getNextValue();
                (void)gainSm.getNextValue();
                (void)slopeSm.getNextValue();
            }
        }
    }

    // Update dynamic EQ envelope from the input signal (call per sample, before process).
    inline void updateDynamicEnvelope(float l, float r, double sampleRate)
    {
        if (!enabled)
        {
            dynGainMod = 0.0f;
            return;
        }

        // Route the detector through the same domain and placement as the
        // audio path. At the centre both components remain fully linked; the
        // endpoints listen exclusively to L/R or M/S respectively.
        const float firstWeight = firstPlacementWeight;
        const float secondWeight = secondPlacementWeight;
        float first = l;
        float second = r;
        if (midSidePlacement)
        {
            // Detector calibration uses the conventional -6 dB matrix so a
            // mono signal reads the same at centre-L/R and full-Mid (and an
            // anti-phase signal reads the same at centre-L/R and full-Side).
            // The audible M/S matrix remains energy-preserving.
            first = (l + r) * 0.5f;
            second = (l - r) * 0.5f;
        }

        // Sidechain: bandpass-filter the routed input at the band frequency.
        // Weighting before the filters also keeps the stereo meter honest:
        // an unselected component reads silence rather than showing a source
        // that cannot trigger this band.
        const float scFilteredL = scBiquad.processL(first * firstWeight);
        const float scFilteredR = scBiquad.processR(second * secondWeight);
        detectorPeakL = std::max(detectorPeakL, std::abs(scFilteredL));
        detectorPeakR = std::max(detectorPeakR, std::abs(scFilteredR));
        if (!dynEnabled)
        {
            dynGainMod = 0.0f;
            return;
        }
        const float rectified = std::max(std::abs(scFilteredL), std::abs(scFilteredR));

        if (rectified > envLevel)
            envLevel += dynAttackCoeff * (rectified - envLevel);
        else
            envLevel += dynReleaseCoeff * (rectified - envLevel);

        // The envelope itself remains sample-accurate. The transfer curve and
        // coefficient target are control-rate values, which removes almost all
        // per-sample log work without making the result block-size dependent.
        if (++dynamicControlCounter < coeffUpdateInterval)
            return;
        dynamicControlCounter = 0;

        // Compute gain reduction
        const float envDb = 20.0f * std::log10(std::max(envLevel, 1e-7f));
        if (!dynUpward && envDb > dynThreshDb)
        {
            const float overDb = envDb - dynThreshDb;
            dynGainMod = -std::min(dynRangeDb, overDb * (1.0f - 1.0f / dynRatio));
        }
        else if (dynUpward && envDb < dynThreshDb)
        {
            const float underDb = dynThreshDb - envDb;
            dynGainMod = std::min(dynRangeDb, underDb * (1.0f - 1.0f / dynRatio));
        }
        else
        {
            dynGainMod = 0.0f;
        }
    }

    inline void process(float& l, float& r)
    {
        processEqualizer(l, r);
        processDrive(l, r);
    }

    inline void processEqualizer(float& l, float& r)
    {
        if (!enabled) return;
        const float wetAmount = amountSm.getNextValue();
        const float firstWeight = firstPlacementWeight;
        const float secondWeight = secondPlacementWeight;
        if (!midSidePlacement)
        {
            const float dryL = l, dryR = r;
            l += routeEffectWeight * wetAmount * firstWeight * (processLeft(dryL) - dryL);
            r += routeEffectWeight * wetAmount * secondWeight * (processRight(dryR) - dryR);
        }
        else
        {
            constexpr float invSqrt2 = 0.7071067811865475f;
            const float dryMid = (l + r) * invSqrt2;
            const float drySide = (l - r) * invSqrt2;
            const float mid = dryMid + routeEffectWeight * wetAmount * firstWeight * (processLeft(dryMid) - dryMid);
            const float side = drySide + routeEffectWeight * wetAmount * secondWeight * (processRight(drySide) - drySide);
            l = (mid + side) * invSqrt2;
            r = (mid - side) * invSqrt2;
        }
    }

    bool canUseStaticKernel() const noexcept
    {
        return enabled && !dynEnabled && !freqSm.isSmoothing()
            && !qSm.isSmoothing() && !gainSm.isSmoothing()
            && !slopeSm.isSmoothing() && !amountSm.isSmoothing()
            && !zlCascade.isCoefficientRamping() && !classicCutCoefficientRamping;
    }

    inline void processEqualizerStatic(float& l, float& r)
    {
        const float wetAmount = amountSm.getCurrentValue();
        if (!midSidePlacement)
        {
            const float dryL = l, dryR = r;
            float filteredL = dryL, filteredR = dryR;
            if (zl_filter::isClassicCut(type))
            {
                filteredL = processClassicCut(filteredL, false);
                filteredR = processClassicCut(filteredR, true);
            }
            else
                zlCascade.processStereo(filteredL, filteredR);
            l += routeEffectWeight * wetAmount * firstPlacementWeight * (filteredL - dryL);
            r += routeEffectWeight * wetAmount * secondPlacementWeight * (filteredR - dryR);
        }
        else
        {
            constexpr float invSqrt2 = 0.7071067811865475f;
            const float dryMid = (l + r) * invSqrt2;
            const float drySide = (l - r) * invSqrt2;
            float filteredMid = dryMid, filteredSide = drySide;
            if (zl_filter::isClassicCut(type))
            {
                filteredMid = processClassicCut(filteredMid, false);
                filteredSide = processClassicCut(filteredSide, true);
            }
            else
                zlCascade.processStereo(filteredMid, filteredSide);
            const float mid = dryMid + routeEffectWeight * wetAmount
                * firstPlacementWeight * (filteredMid - dryMid);
            const float side = drySide + routeEffectWeight * wetAmount
                * secondPlacementWeight * (filteredSide - drySide);
            l = (mid + side) * invSqrt2;
            r = (mid - side) * invSqrt2;
        }
    }

    void processEqualizerStaticBlock(float* left, float* right,
                                     float* scratchFirst, float* scratchSecond,
                                     int numSamples)
    {
        const float wetAmount = amountSm.getCurrentValue();
        if (!midSidePlacement && !zl_filter::isClassicCut(type)
            && routeEffectWeight == 1.0f && wetAmount == 1.0f
            && firstPlacementWeight == 1.0f && secondPlacementWeight == 1.0f)
        {
            zlCascade.processStereoBlock(left, right, numSamples);
            return;
        }
        if (!midSidePlacement && zl_filter::isClassicCut(type)
            && routeEffectWeight == 1.0f && wetAmount == 1.0f
            && firstPlacementWeight == 1.0f && secondPlacementWeight == 1.0f
            && classicCutFractionalStage <= 0.0001f)
        {
            for (int stage = 0; stage < classicCutFullStages; ++stage)
                classicCutStages[(size_t)stage].processStereoBlock(left, right, numSamples);
            return;
        }
        if (!zl_filter::isClassicCut(type))
        {
            if (!midSidePlacement)
            {
                for (int sample = 0; sample < numSamples; ++sample)
                {
                    scratchFirst[sample] = left[sample];
                    scratchSecond[sample] = right[sample];
                }
                zlCascade.processStereoBlock(scratchFirst, scratchSecond, numSamples);
                for (int sample = 0; sample < numSamples; ++sample)
                {
                    const float dryL = left[sample], dryR = right[sample];
                    left[sample] += routeEffectWeight * wetAmount * firstPlacementWeight
                        * (scratchFirst[sample] - dryL);
                    right[sample] += routeEffectWeight * wetAmount * secondPlacementWeight
                        * (scratchSecond[sample] - dryR);
                }
            }
            else
            {
                constexpr float invSqrt2 = 0.7071067811865475f;
                for (int sample = 0; sample < numSamples; ++sample)
                {
                    scratchFirst[sample] = (left[sample] + right[sample]) * invSqrt2;
                    scratchSecond[sample] = (left[sample] - right[sample]) * invSqrt2;
                }
                zlCascade.processStereoBlock(scratchFirst, scratchSecond, numSamples);
                for (int sample = 0; sample < numSamples; ++sample)
                {
                    const float dryMid = (left[sample] + right[sample]) * invSqrt2;
                    const float drySide = (left[sample] - right[sample]) * invSqrt2;
                    const float mid = dryMid + routeEffectWeight * wetAmount
                        * firstPlacementWeight * (scratchFirst[sample] - dryMid);
                    const float side = drySide + routeEffectWeight * wetAmount
                        * secondPlacementWeight * (scratchSecond[sample] - drySide);
                    left[sample] = (mid + side) * invSqrt2;
                    right[sample] = (mid - side) * invSqrt2;
                }
            }
            return;
        }
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float l = left[sample];
            float r = right[sample];
            processEqualizerStatic(l, r);
            left[sample] = l;
            right[sample] = r;
        }
    }

    inline void processDrive(float& l, float& r)
    {
        if (enabled && driveAmount > 0.0f)
            applyBandDrive(l, r);
    }

    bool driveActive() const noexcept { return enabled && driveAmount > 0.0f; }

    static float dynamicClassicCutSlope(float baseSlope, float dynamicModDb) noexcept
    {
        return std::clamp(baseSlope - dynamicModDb * 6.0f, 3.0f, 96.0f);
    }

    static float dynamicResonantCutQ(float baseQ, float dynamicModDb) noexcept
    {
        return std::clamp(baseQ * std::pow(2.0f, dynamicModDb / 6.0f), 0.1f, 24.0f);
    }

    static float amountResonantCutQ(float baseQ, float amount) noexcept
    {
        const float strength = std::clamp(amount, 0.0f, 1.0f);
        return deq::filter_types::resonantCutDefaultQ
            + (baseQ - deq::filter_types::resonantCutDefaultQ) * strength;
    }

    static float cutAmountMix(float amount) noexcept
    {
        // The cutoff itself already moves towards a neutral edge as Amount
        // approaches zero. This short smooth easing makes the final
        // few percent converge to an exact dry signal instead of hard-bypassing
        // the filter between the 1% and 0% parameter steps.
        const float x = std::clamp(amount / 0.05f, 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    }

    static float dynamicBandPassQ(float baseQ, float dynamicModDb) noexcept
    {
        return std::clamp(baseQ * std::pow(2.0f, -dynamicModDb / 6.0f), 0.1f, 24.0f);
    }

    void prepareDriveRate(double sampleRate)
    {
        if (driveProcessSampleRate == sampleRate && drivePreparedFrequency == freqHz
            && drivePreparedQ == Q)
            return;
        driveProcessSampleRate = sampleRate;
        driveBandBiquad.set(Biquad::Type::Bandpass, sampleRate, freqHz,
                            std::clamp(Q, 0.2f, 12.0f), 0.0);
        drivePreparedFrequency = freqHz;
        drivePreparedQ = Q;
    }

    // A solo is an audition operation, not merely "disable the other EQ
    // stages": it returns only the frequency window represented by this node.
    inline void processAudition(float& l, float& r)
    {
        const float firstWeight = std::clamp(1.0f - placement, 0.0f, 1.0f);
        const float secondWeight = std::clamp(1.0f + placement, 0.0f, 1.0f);
        if (!midSidePlacement)
        {
            l = firstWeight * auditionBiquad.processL(l);
            r = secondWeight * auditionBiquad.processR(r);
        }
        else
        {
            constexpr float invSqrt2 = 0.7071067811865475f;
            const float mid = firstWeight * auditionBiquad.processL((l + r) * invSqrt2);
            const float side = secondWeight * auditionBiquad.processR((l - r) * invSqrt2);
            l = (mid + side) * invSqrt2;
            r = (mid - side) * invSqrt2;
        }
    }

    inline float processLeft(float x)
    {
        if (zl_filter::isClassicCut(type)) return processClassicCut(x, false);
        return zlCascade.processL(x);
    }

    inline float processRight(float x)
    {
        if (zl_filter::isClassicCut(type)) return processClassicCut(x, true);
        return zlCascade.processR(x);
    }
