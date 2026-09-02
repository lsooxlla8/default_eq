#include "PluginProcessor.h"

namespace
{
juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}
}

void DefaultEqualizerAudioProcessor::cacheParameterPointers()
{
    const auto raw = [this](const juce::String& id) { return apvts.getRawParameterValue(id); };
    outputGainParam = raw("output_gain"); amountParam = raw("scale"); shiftParam = raw("shift");
    adaptiveQParam = raw("adaptive_q");
    linearPhaseParam = raw("linear_phase"); linearQualityParam = raw("linear_quality");
    oversamplingParam = raw("oversampling"); autoGainModeParam = raw("auto_gain_mode");
    pluginEnabledParam = raw("plugin_enabled");
    transientStrengthParam=raw("transient_split_strength"); transientBalanceParam=raw("transient_split_balance");
    transientHoldParam=raw("transient_split_hold"); transientSmoothParam=raw("transient_split_smooth");
    for (int i = 0; i < kNumBands; ++i)
    {
        auto& p = bandParams[(size_t)i]; const int b = i + 1;
        p.present = raw(bandId(b,"present")); p.on = raw(bandId(b,"on"));
        p.type = raw(bandId(b,"type")); p.slope = raw(bandId(b,"slope"));
        p.placementMode = raw(bandId(b,"placement_mode")); p.placement = raw(bandId(b,"placement"));
        p.freq = raw(bandId(b,"freq")); p.q = raw(bandId(b,"q")); p.gain = raw(bandId(b,"gain"));
        p.drive = raw(bandId(b,"drive"));
        p.driveCharacter = raw(bandId(b,"drive_character")); p.driveSecondary = raw(bandId(b,"drive_secondary"));
        p.satMode = raw(bandId(b,"sat_mode"));
        p.dynMode = raw(bandId(b,"dyn_mode")); p.scSource = raw(bandId(b,"sc_source"));
        p.dynLookahead = raw(bandId(b,"dyn_lookahead")); p.dynThresh = raw(bandId(b,"dyn_thresh"));
        p.dynRange = raw(bandId(b,"dyn_range")); p.dynRatio = raw(bandId(b,"dyn_ratio"));
        p.dynSpeed = raw(bandId(b,"dyn_speed"));
    }
}

void DefaultEqualizerAudioProcessor::resetBandToDefaults(int zeroBasedBand, bool enable,
                                                          float frequency, float gainDb)
{
    if (zeroBasedBand < 0 || zeroBasedBand >= kNumBands)
        return;

    const auto prefix = "b" + juce::String(zeroBasedBand + 1) + "_";
    for (auto* base : getParameters())
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(base))
            if (parameter->paramID.startsWith(prefix))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->getDefaultValue());
                parameter->endChangeGesture();
            }

    const auto setPlain = [this, zeroBasedBand](const char* suffix, float value)
    {
        if (auto* parameter = apvts.getParameter(bandId(zeroBasedBand + 1, suffix)))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            parameter->endChangeGesture();
        }
    };
    if (frequency > 0.0f) setPlain("freq", frequency);
    setPlain("gain", gainDb);
    setPlain("present", enable ? 1.0f : 0.0f);
    setPlain("on", enable ? 1.0f : 0.0f);
    if (soloBand.load(std::memory_order_acquire) == zeroBasedBand)
        soloBand.store(-1, std::memory_order_release);
}

void DefaultEqualizerAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused(newValue);
    smartObservationDirty.store(true, std::memory_order_release);
    if (parameterID.startsWithChar('b') && parameterID.length() > 2
        && parameterID[2] == '_')
    {
        const int band = (int)parameterID[1] - (int)'1';
        if (band >= 0 && band < kNumBands)
            bandDirtyMask.fetch_or(1u << band, std::memory_order_release);
    }
    if (parameterID == "scale" || parameterID == "shift"
        || parameterID == "adaptive_q" || parameterID == "linear_phase"
        || parameterID == "oversampling")
        bandDirtyMask.store((1u << kNumBands) - 1u, std::memory_order_release);
    const bool affectsLinearMagnitude = parameterID == "scale" || parameterID == "shift"
        || parameterID == "adaptive_q"
        || parameterID.endsWith("_freq")
        || parameterID.endsWith("_gain") || parameterID.endsWith("_q")
        || parameterID.endsWith("_present")
        || parameterID.endsWith("_on")
        || parameterID.endsWith("_type")
        || parameterID.endsWith("_slope") || parameterID.endsWith("_placement")
        || parameterID.endsWith("_placement_mode");
    if (parameterID == "linear_phase" || parameterID == "linear_quality"
        || (affectsLinearMagnitude
            && linearPhaseParam->load(std::memory_order_relaxed) > 0.5f))
        requestLinearPhaseRebuild();
    if (affectsLinearMagnitude || parameterID == "auto_gain_mode")
        regularAutoGainDirty.store(true, std::memory_order_release);

    // Handle linear phase latency update (safe: parameterChanged is called on message thread)
    if (parameterID == "linear_phase" || parameterID == "linear_quality")
    {
        updateReportedLatency();
        return;
    }

    if (parameterID == "oversampling" || parameterID.endsWith("_drive")
        || parameterID.endsWith("_present")
        || parameterID.endsWith("_on")
        || parameterID.endsWith("_placement_mode")
        || parameterID.endsWith("_dyn_lookahead")
        || parameterID.endsWith("_dyn_thresh"))
    {
        updateReportedLatency();
        return;
    }

}

juce::AudioProcessorValueTreeState::ParameterLayout DefaultEqualizerAudioProcessor::createParams()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.reserve(kNumBands * 24 + 8);

    // Global parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "output_gain", "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("dB")
            .withStringFromValueFunction([](float value, int)
            {
                const auto clean = std::abs(value) < 0.005f ? 0.0f : value;
                return juce::String(clean, 2) + " dB";
            })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "scale", "Amount",
        juce::NormalisableRange<float>(-2.0f, 2.0f, 0.01f, 1.0f),
        1.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("%")
            .withStringFromValueFunction([](float value, int)
            {
                const auto percent = std::abs(value) < 0.005f ? 0.0f : value * 100.0f;
                return juce::String(juce::roundToInt(percent)) + "%";
            })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "shift", "Shift",
        juce::NormalisableRange<float>(-48.0f, 48.0f, 0.01f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes{}
            .withStringFromValueFunction([](float value, int)
            {
                const float clean = std::abs(value) < 0.005f ? 0.0f : value;
                return (clean > 0.0f ? "+" : "") + juce::String(clean, 2);
            })));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "adaptive_q", "Adaptive Q", false));

    // Oversampling: 1x / 2x / 4x / 8x
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "oversampling", "Oversampling",
        juce::StringArray { "1x", "2x", "4x", "8x" }, 0));

    // Linear phase mode
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "linear_phase", "Linear Phase", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "linear_quality", "Linear Phase Quality",
        juce::StringArray { "Eco 1024", "Medium 2048", "High 4096" }, 2));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "auto_gain_mode", "Auto Gain Mode",
        juce::StringArray { "Off", "Regular", "Smart" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "plugin_enabled", "Plugin Enabled", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "transient_split_strength", "Transient Split Strength",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "transient_split_balance", "Transient Split Balance",
        juce::NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "transient_split_hold", "Transient Split Hold",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("%")));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "transient_split_smooth", "Transient Split Smooth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes{}.withLabel("%")));

    auto typeChoices = juce::StringArray { "Res Low Cut", "Res High Cut", "Notch", "Tilt",
                                            "Band Pass", "Bell", "Low Shelf", "High Shelf",
                                            "Low Cut", "High Cut" };
    for (int i = 1; i <= kNumBands; ++i)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            bandId(i,"present"), "Band " + juce::String(i) + " Present", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(bandId(i,"on"), "Band " + juce::String(i) + " On", false));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(bandId(i,"type"), "Band " + juce::String(i) + " Type", typeChoices, 5));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"slope"), "Band " + juce::String(i) + " Slope",
            juce::NormalisableRange<float>(3.0f, 96.0f, 0.1f, 1.0f), 12.0f,
            juce::AudioParameterFloatAttributes{}.withLabel("dB/oct")));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            bandId(i,"placement_mode"), "Band " + juce::String(i) + " Placement Mode",
            juce::StringArray { "L/R", "M/S", "T/S" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"placement"), "Band " + juce::String(i) + " Placement",
            juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes{}.withLabel("%")));

        // Default frequencies spread logarithmically across the spectrum
        // First 8 use classic fixed defaults; additional Pro bands use log spacing
        static const float defaultFreqs8[] = { 0.f, 80.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 12000.f };
        float defaultFreq = (i <= 8) ? defaultFreqs8[i]
            : 20.0f * std::pow(20000.0f / 20.0f, (float)(i - 1) / (float)(kNumBands - 1));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"freq"), "Band " + juce::String(i) + " Freq",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 0.001f, 0.5f),
            defaultFreq));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"q"), "Band " + juce::String(i) + " Q",
            juce::NormalisableRange<float>(0.1f, 24.0f, 0.001f, 0.5f),
            1.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"gain"), "Band " + juce::String(i) + " Gain",
            juce::NormalisableRange<float>(-36.0f, 36.0f, 0.01f, 1.0f),
            0.0f,
            juce::AudioParameterFloatAttributes{}.withLabel("dB")
                .withStringFromValueFunction([](float value, int)
                {
                    const auto clean = std::abs(value) < 0.005f ? 0.0f : value;
                    return juce::String(clean, 2) + " dB";
                })));

        // Drive / saturation per band
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"drive"), "Band " + juce::String(i) + " Drive",
            juce::NormalisableRange<float>(0.0f, 36.0f, 0.01f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes{}.withLabel("dB")
                .withStringFromValueFunction([](float value, int)
                { return juce::String(std::abs(value) < 0.005f ? 0.0f : value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"drive_character"), "Band " + juce::String(i) + " Drive Character",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"drive_secondary"), "Band " + juce::String(i) + " Drive Secondary",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

#if DEFAULT_EQ_FULL
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            bandId(i,"sat_mode"), "Band " + juce::String(i) + " Sat Mode",
            juce::StringArray { "Soft Clip", "Diode", "Triode", "Transistor",
                                "Tape", "Odd / Even", "Phase Distortion", "Sine Erosion" }, 0));
#endif

        // Dynamic EQ per band
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            bandId(i,"dyn_mode"), "Band " + juce::String(i) + " Dynamic Mode",
            juce::StringArray { "Downward", "Upward" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            bandId(i,"sc_source"), "Band " + juce::String(i) + " Sidechain Source",
            juce::StringArray { "Internal", "External" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"dyn_lookahead"), "Band " + juce::String(i) + " Lookahead",
            juce::NormalisableRange<float>(0.0f, 5.0f, 0.01f), 0.0f,
            juce::AudioParameterFloatAttributes{}.withLabel("ms")));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"dyn_thresh"), "Band " + juce::String(i) + " Threshold",
            juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f, 1.0f),
            0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"dyn_range"), "Band " + juce::String(i) + " Range",
            juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 6.0f,
            juce::AudioParameterFloatAttributes{}.withLabel("dB")));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"dyn_ratio"), "Band " + juce::String(i) + " Ratio",
            juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f),
            4.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"dyn_speed"), "Band " + juce::String(i) + " Speed",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
            75.0f,
            juce::AudioParameterFloatAttributes{}.withStringFromValueFunction([](float value, int)
            {
                return juce::String(juce::roundToInt(value)) + "%";
            })));
    }

    return { params.begin(), params.end() };
}
