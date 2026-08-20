#include "PluginProcessor.h"
#include "DSP/DriveAutoGainTable.h"
#include "PluginEditor.h"
#include <complex>

static juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}

namespace
{
juce::ValueTree findStateParameter(juce::ValueTree state, const juce::String& id)
{
    for (auto child : state)
        if (child.getProperty("id").toString() == id)
            return child;
    return {};
}

float readStateParameter(juce::ValueTree state, const juce::String& id, float fallback)
{
    if (auto child = findStateParameter(state, id); child.isValid())
        return (float)child.getProperty("value", fallback);
    return (float)state.getProperty(id, fallback);
}

void writeStateParameter(juce::ValueTree state, const juce::String& id, float value)
{
    if (auto child = findStateParameter(state, id); child.isValid())
        child.setProperty("value", value, nullptr);
    else
    {
        juce::ValueTree parameter("PARAM");
        parameter.setProperty("id", id, nullptr);
        parameter.setProperty("value", value, nullptr);
        state.appendChild(parameter, nullptr);
    }
    state.removeProperty(id, nullptr);
}

void removeStateParameter(juce::ValueTree state, const juce::String& id)
{
    state.removeProperty(id, nullptr);
    if (auto child = findStateParameter(state, id); child.isValid())
        state.removeChild(child, nullptr);
}
}

// ── A5: background rebuild worker for the linear-phase FIR ──────────────
// Owned by the processor; started in prepareToPlay, stopped in the dtor.
// When linPhaseDirty is set (by a parameter listener or by
// setStateInformation), the thread wakes via notify(), clears the flag,
// and rebuilds the FIR into the engine's inactive kernel slot. The
// audio thread never calls buildLinearPhaseMagnitude().
class DefaultEqualizerAudioProcessor::LinPhaseRebuildThread : public juce::Thread
{
public:
    explicit LinPhaseRebuildThread(DefaultEqualizerAudioProcessor& p)
        : juce::Thread("default_eq8_LinPhaseRebuild"), proc(p) {}

    void run() override
    {
        while (!threadShouldExit())
        {
            // Park until notified by requestLinearPhaseRebuild(). A
            // negative timeout means "wait forever until notify()".
            wait(-1);
            if (threadShouldExit()) break;

            // Drain the dirty flag; if spuriously woken, loop back to wait.
            while (proc.linPhaseDirty.exchange(false, std::memory_order_acquire))
            {
                if (threadShouldExit()) return;
                proc.buildLinearPhaseMagnitude();
            }
        }
    }

private:
    DefaultEqualizerAudioProcessor& proc;
};

DefaultEqualizerAudioProcessor::DefaultEqualizerAudioProcessor()
: AudioProcessor(BusesProperties().withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                                  .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true))
, apvts(*this, &undoManager, "STATE", createParams())
{
    cacheParameterPointers();

    // Register for latency updates and background linear-phase rebuilds.
    for (int i = 1; i <= kNumBands; ++i)
    {
        apvts.addParameterListener(bandId(i, "freq"),  this);
        apvts.addParameterListener(bandId(i, "gain"),  this);
        apvts.addParameterListener(bandId(i, "q"),     this);
        apvts.addParameterListener(bandId(i, "present"), this);
        apvts.addParameterListener(bandId(i, "on"),    this);
        apvts.addParameterListener(bandId(i, "type"),  this);
        apvts.addParameterListener(bandId(i, "slope"), this);
        apvts.addParameterListener(bandId(i, "placement"), this);
        apvts.addParameterListener(bandId(i, "placement_mode"), this);
        apvts.addParameterListener(bandId(i, "drive"), this);
        apvts.addParameterListener(bandId(i, "dyn_lookahead"), this);
        apvts.addParameterListener(bandId(i, "dyn_thresh"), this);
    }
    apvts.addParameterListener("linear_phase", this);
    apvts.addParameterListener("linear_quality", this);
    apvts.addParameterListener("scale", this);
    apvts.addParameterListener("adaptive_q", this);
    apvts.addParameterListener("oversampling", this);

    linPhaseRebuildThread = std::make_unique<LinPhaseRebuildThread>(*this);

}

void DefaultEqualizerAudioProcessor::cacheParameterPointers()
{
    const auto raw = [this](const juce::String& id) { return apvts.getRawParameterValue(id); };
    outputGainParam = raw("output_gain"); amountParam = raw("scale");
    adaptiveQParam = raw("adaptive_q");
    linearPhaseParam = raw("linear_phase"); linearQualityParam = raw("linear_quality");
    oversamplingParam = raw("oversampling"); autoGainModeParam = raw("auto_gain_mode");
    pluginEnabledParam = raw("plugin_enabled");
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
        p.dynAttack = raw(bandId(b,"dyn_attack")); p.dynRelease = raw(bandId(b,"dyn_release"));
    }
}

DefaultEqualizerAudioProcessor::~DefaultEqualizerAudioProcessor()
{
    // Tear down the rebuild thread first so it can't touch apvts mid-destruction.
    if (linPhaseRebuildThread)
    {
        linPhaseRebuildThread->signalThreadShouldExit();
        linPhaseRebuildThread->notify();
        linPhaseRebuildThread->stopThread(2000);
        linPhaseRebuildThread.reset();
    }

    apvts.removeParameterListener("adaptive_q", this);
    apvts.removeParameterListener("oversampling", this);
    apvts.removeParameterListener("scale", this);
    apvts.removeParameterListener("linear_phase", this);
    apvts.removeParameterListener("linear_quality", this);
    for (int i = 1; i <= kNumBands; ++i)
    {
        apvts.removeParameterListener(bandId(i, "slope"), this);
        apvts.removeParameterListener(bandId(i, "placement"), this);
        apvts.removeParameterListener(bandId(i, "placement_mode"), this);
        apvts.removeParameterListener(bandId(i, "drive"), this);
        apvts.removeParameterListener(bandId(i, "dyn_lookahead"), this);
        apvts.removeParameterListener(bandId(i, "dyn_thresh"), this);
        apvts.removeParameterListener(bandId(i, "type"),  this);
        apvts.removeParameterListener(bandId(i, "on"),    this);
        apvts.removeParameterListener(bandId(i, "present"), this);
        apvts.removeParameterListener(bandId(i, "q"),     this);
        apvts.removeParameterListener(bandId(i, "gain"),  this);
        apvts.removeParameterListener(bandId(i, "freq"),  this);
    }
}

void DefaultEqualizerAudioProcessor::requestLinearPhaseRebuild()
{
    linPhaseDirty.store(true, std::memory_order_release);
    if (linPhaseRebuildThread && linPhaseRebuildThread->isThreadRunning())
        linPhaseRebuildThread->notify();
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
    if (sidechainAuditionBand.load(std::memory_order_acquire) == zeroBasedBand)
        sidechainAuditionBand.store(-1, std::memory_order_release);
}

void DefaultEqualizerAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused(newValue);
    const bool affectsLinearMagnitude = parameterID == "scale" || parameterID == "adaptive_q"
        || parameterID.endsWith("_freq")
        || parameterID.endsWith("_gain") || parameterID.endsWith("_q")
        || parameterID.endsWith("_present")
        || parameterID.endsWith("_on")
        || parameterID.endsWith("_type")
        || parameterID.endsWith("_slope") || parameterID.endsWith("_placement")
        || parameterID.endsWith("_placement_mode");
    if (affectsLinearMagnitude || parameterID == "linear_phase" || parameterID == "linear_quality")
        requestLinearPhaseRebuild();

    // Handle linear phase latency update (safe: parameterChanged is called on message thread)
    if (parameterID == "linear_phase" || parameterID == "linear_quality")
    {
        updateReportedLatency();
        return;
    }

    if (parameterID == "oversampling" || parameterID.endsWith("_drive")
        || parameterID.endsWith("_present")
        || parameterID.endsWith("_on")
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
                return juce::String(percent, 0) + "%";
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

    auto typeChoices    = juce::StringArray { "Bell", "LowShelf", "HighShelf", "HighPass", "LowPass", "Bandpass", "Notch", "Tilt" };
    for (int i = 1; i <= kNumBands; ++i)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            bandId(i,"present"), "Band " + juce::String(i) + " Present", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(bandId(i,"on"), "Band " + juce::String(i) + " On", false));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(bandId(i,"type"), "Band " + juce::String(i) + " Type", typeChoices, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"slope"), "Band " + juce::String(i) + " Slope",
            juce::NormalisableRange<float>(3.0f, 48.0f, 0.1f, 1.0f), 12.0f,
            juce::AudioParameterFloatAttributes{}.withLabel("dB/oct")));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            bandId(i,"placement_mode"), "Band " + juce::String(i) + " Placement Mode", false));
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

#if DEFAULT_EQUALIZER_FULL
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            bandId(i,"sat_mode"), "Band " + juce::String(i) + " Sat Mode",
            juce::StringArray { "Soft Clip", "Hard Clip", "Diode Clipper", "Triode Stage", "Transistor / FET",
                                "Tape Hysteresis", "Harmonic Morph", "Phase Distortion", "Spectral Clip", "Sine Erosion" }, 0));
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
            bandId(i,"dyn_attack"), "Band " + juce::String(i) + " Attack",
            juce::NormalisableRange<float>(0.1f, 100.0f, 0.1f, 0.5f),
            10.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandId(i,"dyn_release"), "Band " + juce::String(i) + " Release",
            juce::NormalisableRange<float>(1.0f, 1000.0f, 1.0f, 0.5f),
            100.0f));
    }

    return { params.begin(), params.end() };
}

bool DefaultEqualizerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto main = layouts.getMainOutputChannelSet();
    if (main != juce::AudioChannelSet::stereo() && main != juce::AudioChannelSet::mono())
        return false;
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    return true;
}

void DefaultEqualizerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlockSize = samplesPerBlock;

    for (auto& b : bands)
        b.reset(sr);

    // A1: pre-build every oversampler order up-front. No heap allocation on
    //     the audio thread when the user changes the oversampling factor.
    buildAllOversamplers(sampleRate, samplesPerBlock);
    currentOversamplingOrder = (int)oversamplingParam->load(std::memory_order_relaxed);

    // Linear phase engine
    linearPhaseEngine.prepare(sampleRate, samplesPerBlock);

    // Pre-allocate linear phase magnitude buffer
    linPhaseMagBuf.resize((size_t)(LinearPhaseEngine::firLength / 2 + 1), 0.0f);

    // Build and activate the first partition set before processing begins.
    // Subsequent parameter edits continue to rebuild on the worker.
    linPhaseDirty.store(false, std::memory_order_release);
    buildLinearPhaseMagnitude();
    linearPhaseEngine.activatePendingBeforeProcessing(samplesPerBlock);
    if (linPhaseRebuildThread && ! linPhaseRebuildThread->isThreadRunning())
        linPhaseRebuildThread->startThread(juce::Thread::Priority::low);

    // Update latency based on current linear-phase setting
    const bool linPhase = linearPhaseParam->load(std::memory_order_relaxed) > 0.5f;
    setLatencySamples(linPhase ? currentLinearPhaseLatency() : 0);
    globalBypass.prepare(sampleRate, samplesPerBlock, getMainBusNumOutputChannels(),
                         LinearPhaseEngine::latency + juce::roundToInt(sampleRate * lookaheadSeconds) + 256, true);
    const int maxLookahead = juce::roundToInt(sampleRate * lookaheadSeconds);
    lookaheadDelayBuffer.setSize(2, maxLookahead + samplesPerBlock + 8, false, true, false);
    lookaheadDelayBuffer.clear();
    detectorInputBuffer.setSize(2, samplesPerBlock, false, true, false);
    detectorInputBuffer.clear();
    lookaheadWritePosition = 0;
    lookaheadMix.reset(sampleRate, 0.02);
    lookaheadMix.setCurrentAndTargetValue(requestedLookaheadSamples() > 0 ? 1.0f : 0.0f);
    latencyTransitionGain.reset(sampleRate, 0.005);
    latencyTransitionGain.setCurrentAndTargetValue(1.0f);
    processedLatency = getLatencySamples();
    latencyFadingOut = false;
    updateReportedLatency();

    // Reset spectrum FIFOs (fixes blank analyzer after DAW offline/online cycle)
    spectrumFifo.reset();
    preSpectrumFifo.reset();

    // Prime coefficients from current params
    syncBandsFromParams();

}

void DefaultEqualizerAudioProcessor::buildAllOversamplers(double /*sampleRate*/, int samplesPerBlock)
{
    // oversamplers[i] corresponds to DSP order (i + 1):
    //   i=0 -> 2x, i=1 -> 4x, i=2 -> 8x.
    // The "1x" path is represented by nullptr (currentOversamplerPtr() returns null).
    for (int i = 0; i < kNumOversamplingOrders; ++i)
    {
        const int order = i + 1;
        oversamplers[(size_t)i] = std::make_unique<juce::dsp::Oversampling<float>>(
            (size_t)juce::jmax(1, getMainBusNumOutputChannels()), order,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
            true);
        oversamplers[(size_t)i]->initProcessing((size_t) samplesPerBlock);
    }
}

juce::dsp::Oversampling<float>* DefaultEqualizerAudioProcessor::currentOversamplerPtr() const noexcept
{
    const int order = currentOversamplingOrder;
    if (order <= 0 || order > kNumOversamplingOrders) return nullptr;
    return oversamplers[(size_t)(order - 1)].get();
}

void DefaultEqualizerAudioProcessor::syncBandsFromParams()
{
    for (int index = 0; index < kNumBands; ++index)
    {
        auto& b = bands[(size_t)index];
        const auto& p = bandParams[(size_t)index];
        const bool on = p.present->load(std::memory_order_relaxed) > 0.5f
            && p.on->load(std::memory_order_relaxed) > 0.5f;
        const int t = (int)p.type->load(std::memory_order_relaxed);
        Biquad::Type tp = Biquad::Type::Bell;
        switch (t)
        {
            case 0: tp = Biquad::Type::Bell; break;
            case 1: tp = Biquad::Type::LowShelf; break;
            case 2: tp = Biquad::Type::HighShelf; break;
            case 3: tp = Biquad::Type::HighPass; break;
            case 4: tp = Biquad::Type::LowPass; break;
            case 5: tp = Biquad::Type::Bandpass; break;
            case 6: tp = Biquad::Type::Notch; break;
            case 7: tp = Biquad::Type::Tilt; break;
            default: tp = Biquad::Type::Bell; break;
        }

        const float freq = p.freq->load(std::memory_order_relaxed);
        const float q = p.q->load(std::memory_order_relaxed);
        const float gain = p.gain->load(std::memory_order_relaxed);

        b.enabled = on;
        b.parameterType = tp;
        b.targetFreqHz = freq;
        b.targetQ = q;
        b.targetGainDb = gain;
    }
}

void DefaultEqualizerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    bool transportPlaying = false;
    if (auto* playHead = getPlayHead())
        if (const auto position = playHead->getPosition())
            transportPlaying = position->getIsPlaying();
    if (transportPlaying && !transportWasPlaying)
        transportStartGeneration.fetch_add(1, std::memory_order_release);
    transportWasPlaying = transportPlaying;

    auto mainBuffer = getBusBuffer(buffer, false, 0);
    const int mainChannels = mainBuffer.getNumChannels();
    if (mainChannels < 1) return;

    for (int channel = 0; channel < mainChannels; ++channel)
    {
        auto* samples = mainBuffer.getWritePointer(channel);
        for (int i = 0; i < mainBuffer.getNumSamples(); ++i)
            samples[i] = std::isfinite(samples[i]) ? std::clamp(samples[i], -64.0f, 64.0f) : 0.0f;
    }

    globalBypass.captureInput(mainBuffer);

    const int n = mainBuffer.getNumSamples();
    if (processedLatency != getLatencySamples())
    {
        processedLatency = getLatencySamples();
        latencyTransitionGain.setTargetValue(0.0f);
        latencyFadingOut = true;
    }
    const int currentAudition = sidechainAuditionBand.load(std::memory_order_relaxed);
    bool needsDetectorCopy = currentAudition >= 0;
    for (const auto& p : bandParams)
        needsDetectorCopy = needsDetectorCopy || (p.present->load(std::memory_order_relaxed) > 0.5f
            && p.on->load(std::memory_order_relaxed) > 0.5f
            && p.dynThresh->load(std::memory_order_relaxed) < -0.05f
            && p.scSource->load(std::memory_order_relaxed) < 0.5f);
    if (needsDetectorCopy)
        for (int channel = 0; channel < 2; ++channel)
            detectorInputBuffer.copyFrom(channel, 0, mainBuffer, juce::jmin(channel, mainChannels - 1), 0, n);
    const int lookahead = requestedLookaheadSamples();
    lookaheadMix.setTargetValue(lookahead > 0 ? 1.0f : 0.0f);
    applyLookaheadDelay(mainBuffer, lookahead);

    const auto sidechain = getBusCount(true) > 1 ? getBusBuffer(buffer, true, 1)
                                                 : juce::AudioBuffer<float>();
    const float* sidechainL = sidechain.getNumChannels() > 0 ? sidechain.getReadPointer(0) : nullptr;
    const float* sidechainR = sidechain.getNumChannels() > 1 ? sidechain.getReadPointer(1) : sidechainL;

    // Pull params each block
    syncBandsFromParams();
    
    // Get global parameters
    const float amount = amountParam->load(std::memory_order_relaxed);
    const float outputGainDb = outputGainParam->load(std::memory_order_relaxed);
    const float outputGain = std::pow(10.0f, outputGainDb / 20.0f);
    const bool adaptiveQ = adaptiveQParam->load(std::memory_order_relaxed) > 0.5f;
    constexpr bool decramp = true;
    const bool linearPhase = linearPhaseParam->load(std::memory_order_relaxed) > 0.5f;

    // A1: look up oversampler from the pre-built pool. Zero heap allocation
    //     on the audio thread, even when the user changes the oversampling
    //     factor live. A factor change may produce a one-block latency blip
    //     because IIR half-band filter state differs between orders; this
    //     mirrors JUCE's own behavior and is acceptable.
    // Oversampling is a global quality selection, but only the nonlinear
    // drive section enters the oversampled domain. Clean/dynamic EQ remains
    // native-rate and de-cramping owns its near-Nyquist correction.
    const int osOrder = (int)oversamplingParam->load(std::memory_order_relaxed);
    if (osOrder != currentOversamplingOrder)
    {
        // Reset the new filter chain so we don't carry over stale delay state
        // from the last time this order was engaged. Oversampling::reset()
        // is non-allocating; it only zeros internal filter state.
        if (osOrder >= 1 && osOrder <= kNumOversamplingOrders)
        {
            if (auto* os = oversamplers[(size_t)(osOrder - 1)].get())
                os->reset();
        }
        currentOversamplingOrder = osOrder;
    }
    juce::dsp::Oversampling<float>* const configuredOS = currentOversamplerPtr();

    // Check if any band is soloed
    const int soloedBand = soloBand.load(std::memory_order_relaxed);
    const int auditionBand = sidechainAuditionBand.load(std::memory_order_relaxed);

    // Read per-band slope and continuous stereo placement, then set up beginBlock.
    const double effectiveSR = (configuredOS != nullptr)
        ? sr * std::pow(2.0, currentOversamplingOrder)
        : sr;
    std::array<int, kNumBands> activeBands {};
    std::array<int, kNumBands> drivenBands {};
    int activeBandCount = 0;
    int drivenBandCount = 0;

    for (int i = 0; i < kNumBands; ++i)
    {
        auto& b = bands[(size_t)i];
        const auto& p = bandParams[(size_t)i];

        const bool isCut = b.parameterType == Biquad::Type::HighPass
            || b.parameterType == Biquad::Type::LowPass;
        const bool effectiveEnabled = b.enabled && (!isCut || amount > 0.0001f);

        const float bandGain = b.targetGainDb;

        float effectiveQ = b.targetQ;
        if (adaptiveQ)
            effectiveQ = calculateAdaptiveQ(b.targetQ, bandGain);

        const float slope = p.slope->load(std::memory_order_relaxed);
        const bool gainBearing = variable_slope::distributesGain(b.parameterType);

        const bool placementMS = mainChannels > 1
            && p.placementMode->load(std::memory_order_relaxed) > 0.5f;
        const float placement = mainChannels > 1
            ? p.placement->load(std::memory_order_relaxed) * 0.01f : 0.0f;

        // Drive
        const float driveDb = p.drive->load(std::memory_order_relaxed);
        const float drive = driveDb / 36.0f;
        const float driveCharacterRaw = p.driveCharacter->load(std::memory_order_relaxed);
        const float driveSecondary = p.driveSecondary->load(std::memory_order_relaxed);
        const int satIdx = (int)p.satMode->load(std::memory_order_relaxed);
#if DEFAULT_EQUALIZER_FULL
        b.satType = static_cast<SaturationType>(std::clamp(satIdx, 0, 9));
#endif
        const bool bipolarCharacter = satIdx == 3 || satIdx == 4 || satIdx == 6;
        const float driveCharacter = bipolarCharacter
            ? 0.5f * (std::clamp(driveCharacterRaw, -1.0f, 1.0f) + 1.0f)
            : std::clamp(driveCharacterRaw, 0.0f, 1.0f);

        // Dynamic EQ params
        const bool dynUpward = p.dynMode->load(std::memory_order_relaxed) > 0.5f;
        const bool externalSC = p.scSource->load(std::memory_order_relaxed) > 0.5f;
        const float dynThr = p.dynThresh->load(std::memory_order_relaxed);
        const float dynRange = p.dynRange->load(std::memory_order_relaxed);
        const float dynRat = p.dynRatio->load(std::memory_order_relaxed);
        const float dynAtk = p.dynAttack->load(std::memory_order_relaxed);
        const float dynRel = p.dynRelease->load(std::memory_order_relaxed);

        // Zero drive is a hard off state: the band is not added to the driven
        // list, so neither the nonlinear code nor oversampling runs at all.
        b.driveAmount   = driveDb > 0.0001f ? drive : 0.0f;
        b.driveCharacter = driveCharacter;
        b.driveSecondary = driveSecondary;
        b.driveAutoGainLinear = deq::drive_auto_gain_table::lookup(satIdx, driveDb);
        // Threshold doubles as the dynamic enable control. Its stepped default
        // of 0 dB is truly inactive; the first active value is -0.1 dB.
        const bool wasDynamic = b.dynEnabled;
        b.dynEnabled    = dynThr < -0.05f;
        if (!b.dynEnabled)
        {
            b.dynGainMod = 0.0f;
            if (wasDynamic) b.coefficientsValid = false;
        }
        b.dynUpward     = dynUpward;
        b.useExternalSidechain = externalSC && sidechainL != nullptr;
        b.dynRangeDb    = dynRange;
        b.dynThreshDb   = dynThr;
        b.dynRatio      = dynRat;
        b.dynAttackMs   = dynAtk;
        b.dynReleaseMs  = dynRel;
        b.detectorListen = auditionBand == i;
        // Gain-bearing filters scale their actual dB parameters for the whole
        // -200..200% range. This is monotonic and cannot create the phase-
        // cancellation reversal caused by dry + amount * (wet - dry).
        b.gainScale = gainBearing ? amount : 1.0f;
        b.globalAmount = (gainBearing || isCut) ? 1.0f : std::clamp(amount, 0.0f, 1.0f);
        b.driveGlobalAmount = std::max(0.0f, amount);

        float amountAdjustedFrequency = b.targetFreqHz;
        if (isCut && amount > 0.0f)
        {
            const float neutralEdge = b.parameterType == Biquad::Type::LowPass
                ? (float)sr * 0.45f : 10.0f;
            amountAdjustedFrequency = neutralEdge * std::pow(
                std::max(1.0e-6f, b.targetFreqHz / neutralEdge), amount);
            amountAdjustedFrequency = std::clamp(amountAdjustedFrequency, 10.0f,
                                                  (float)sr * 0.45f);
        }

        // Stereo static magnitude is handled by the FIR in linear mode. The
        // post stage remains a neutral Bell so dynamic modulation and per-band
        // drive still work. Asymmetrically placed bands retain their minimum-phase
        // filter because a single stereo FIR cannot represent that routing.
        const bool linearStereo = linearPhase && !placementMS && std::abs(placement) < 0.0001f;
        b.beginBlock(sr, effectiveEnabled,
                     linearStereo ? Biquad::Type::Bell : b.parameterType,
                     amountAdjustedFrequency, effectiveQ, linearStereo ? 0.0f : bandGain,
                     linearStereo ? 12.0f : slope, placementMS, placement, decramp);
        if (effectiveEnabled)
        {
            activeBands[(size_t)activeBandCount++] = i;
            if (b.driveActive())
            {
                drivenBands[(size_t)drivenBandCount++] = i;
                b.prepareDriveRate(configuredOS != nullptr ? effectiveSR : sr);
            }
        }
    }
    auto* const activeOS = drivenBandCount > 0 ? configuredOS : nullptr;

    auto* L = mainBuffer.getWritePointer(0);
    auto* R = mainChannels > 1 ? mainBuffer.getWritePointer(1) : L;
    // Push pre-EQ samples to spectrum FIFO
    const bool shouldAnalyze = analyzerEnabled.load(std::memory_order_acquire);
    if (shouldAnalyze) preSpectrumFifo.pushBlock(L, R, n);

    // Measure input RMS for auto-gain compensation
    const int autoGainMode = (int)autoGainModeParam->load(std::memory_order_relaxed);
    const bool autoGain = autoGainMode > 0;
    std::uint64_t smartSignature = smartParameterSignature;
    if (autoGainMode == 2)
    {
        smartSignature = (std::uint64_t)autoGainMode + 1469598103934665603ULL;
        const auto hash = [&smartSignature](float value)
        { smartSignature = (smartSignature ^ (std::uint64_t)std::llround(value * 1000.0f)) * 1099511628211ULL; };
        for (const auto& p : bandParams)
            for (auto* value : { p.present, p.on, p.type, p.freq, p.q, p.gain, p.dynThresh, p.dynRange,
                                 p.drive, p.satMode, p.driveCharacter, p.driveSecondary })
                hash(value->load(std::memory_order_relaxed));
    }
    if (autoGainMode == 2 && smartSignature != smartParameterSignature)
    {
        smartParameterSignature = smartSignature;
        smartInputEnergy = smartOutputEnergy = 0.0;
        smartEnergySamples = 0;
        smartAutoGainLocked.store(false, std::memory_order_release);
        smartAutoGainProgress.store(0.0f, std::memory_order_release);
    }
    float inputRms = 0.0f;
    if (autoGain)
    {
        float sumSq = 0.0f;
        for (int i = 0; i < n; ++i)
            sumSq += L[i] * L[i] + R[i] * R[i];
        inputRms = std::sqrt(sumSq / (float)(n * 2));
    }

    // Static and dynamic filtering stay at base rate. Oversampling is reserved
    // for the nonlinear per-band drive section; de-cramping handles the linear
    // response near Nyquist without multiplying the complete EQ workload.
    auto processEQ = [&](float* left, float* right, int numSamples, double processSR)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float l = left[i];
            float r = right[i];

            for (int active = 0; active < activeBandCount; ++active)
            {
                auto& b = bands[(size_t)activeBands[(size_t)active]];
                if (b.dynEnabled || b.detectorListen)
                {
                    const float detectorL = b.useExternalSidechain ? sidechainL[i]
                        : detectorInputBuffer.getSample(0, i);
                    const float detectorR = b.useExternalSidechain ? sidechainR[i]
                        : detectorInputBuffer.getSample(1, i);
                    b.updateDynamicEnvelope(detectorL, detectorR, processSR);
                }
                b.maybeUpdateCoeffs(processSR);
                if (soloedBand < 0)
                    b.processEqualizer(l, r);
            }

            if (soloedBand >= 0 && soloedBand < kNumBands)
                bands[(size_t) soloedBand].processAudition(l, r);

            if (auditionBand >= 0 && auditionBand < kNumBands)
                l = r = bands[(size_t)auditionBand].lastSidechainSample;

            left[i]  = l;
            right[i] = r;
        }

    };

    if (linearPhase)
    {
        // A5: the background rebuild thread owns the FIR. The audio thread
        //     only reads the currently-published kernel via an acquire load.
        //     If the thread hasn't finished its first rebuild yet, the
        //     engine's processBlock is pass-through (documented in
        //     LinearPhaseEngine::processBlock).
        linearPhaseEngine.processBlock(L, R, n);
        processEQ(L, R, n, sr);
    }
    else
        processEQ(L, R, n, sr);

    if (soloedBand < 0 && drivenBandCount > 0)
    {
        if (activeOS != nullptr)
        {
            juce::dsp::AudioBlock<float> block(mainBuffer);
            auto osBlock = activeOS->processSamplesUp(block);
            auto* osL = osBlock.getChannelPointer(0);
            auto* osR = mainChannels > 1 ? osBlock.getChannelPointer(1) : osL;
            const int osN = (int)osBlock.getNumSamples();
            for (int sample = 0; sample < osN; ++sample)
            {
                float l = osL[sample], r = osR[sample];
                for (int driven = 0; driven < drivenBandCount; ++driven)
                    bands[(size_t)drivenBands[(size_t)driven]].processDrive(l, r);
                osL[sample] = l; osR[sample] = r;
            }
            activeOS->processSamplesDown(block);
            L = mainBuffer.getWritePointer(0); R = mainChannels > 1 ? mainBuffer.getWritePointer(1) : L;
        }
        else
            for (int sample = 0; sample < n; ++sample)
                for (int driven = 0; driven < drivenBandCount; ++driven)
                    bands[(size_t)drivenBands[(size_t)driven]].processDrive(L[sample], R[sample]);
    }

    for (int i = 0; i < kNumBands; ++i)
        bandDynamicGainDb[(size_t)i].store(bands[(size_t)i].dynGainMod, std::memory_order_relaxed);

    // Two deliberately different semantic modes. Regular is parameter-derived
    // and reacts immediately; Smart measures a stable 500 ms pre/post window.
    // Compensation, output gain, transition ramp and output guarding are fused
    // into one pass below.
    float compensation = 1.0f;
    if (autoGainMode == 1)
    {
        float estimatedDelta = 0.0f;
        for (int index = 0; index < kNumBands; ++index)
        {
            const auto& p = bandParams[(size_t)index];
            if (p.present->load(std::memory_order_relaxed) < 0.5f
                || p.on->load(std::memory_order_relaxed) < 0.5f) continue;
            const int type = (int)p.type->load(std::memory_order_relaxed);
            const bool gainBearing = type == 0 || type == 1 || type == 2 || type == 7;
            const float effectiveAmount = amount < 0.0f && !gainBearing ? 0.0f : amount;
            const float gain = p.gain->load(std::memory_order_relaxed) * effectiveAmount;
            const float q = p.q->load(std::memory_order_relaxed);
            const float coverage = (type == 1 || type == 2) ? 0.42f : std::clamp(0.32f / std::sqrt(q), 0.04f, 0.32f);
            estimatedDelta += gain * coverage;
            if (p.drive->load(std::memory_order_relaxed) > 0.0001f)
                estimatedDelta += p.drive->load(std::memory_order_relaxed) * 0.018f
                    * std::max(0.0f, amount);
        }
        const float targetDb = std::clamp(-estimatedDelta, -24.0f, 24.0f);
        const float previous = autoGainCompDb.load(std::memory_order_relaxed);
        const float alpha = 1.0f - std::exp(-(float)n / (float)(sr * 0.05));
        const float current = previous + alpha * (targetDb - previous);
        autoGainCompDb.store(current, std::memory_order_relaxed);
        smartAutoGainLocked.store(false, std::memory_order_relaxed);
        smartAutoGainProgress.store(0.0f, std::memory_order_relaxed);
        compensation = std::pow(10.0f, current / 20.0f);
    }
    else if (autoGainMode == 2)
        compensation = std::pow(10.0f, autoGainCompDb.load(std::memory_order_relaxed) / 20.0f);
    else
    {
        autoGainCompDb.store(0.0f, std::memory_order_relaxed);
        smartInputEnergy = smartOutputEnergy = 0.0; smartEnergySamples = 0;
        smartAutoGainLocked.store(false, std::memory_order_relaxed);
        smartAutoGainProgress.store(0.0f, std::memory_order_relaxed);
    }

    double smartBlockOutputEnergy = 0.0;
    const float combinedGain = outputGain * compensation;
    for (int i = 0; i < n; ++i)
    {
        const float sourceL = L[i];
        const float sourceR = R[i];
        const float postOutputL = sourceL * outputGain;
        const float postOutputR = sourceR * outputGain;
        if (autoGainMode == 2)
            smartBlockOutputEnergy += (double)postOutputL * postOutputL
                                    + (double)postOutputR * postOutputR;

        const float guard = latencyTransitionGain.getNextValue();
        const float finalL = (std::isfinite(sourceL) ? std::clamp(sourceL * combinedGain, -8.0f, 8.0f) : 0.0f) * guard;
        const float finalR = (std::isfinite(sourceR) ? std::clamp(sourceR * combinedGain, -8.0f, 8.0f) : 0.0f) * guard;
        L[i] = finalL;
        if (mainChannels > 1) R[i] = finalR;
        if (latencyFadingOut && !latencyTransitionGain.isSmoothing())
        {
            latencyFadingOut = false;
            latencyTransitionGain.setTargetValue(1.0f);
        }
    }

    if (autoGainMode == 2 && inputRms > 1.0e-7f)
    {
        smartInputEnergy += (double)inputRms * (double)inputRms * (double)(n * 2);
        smartOutputEnergy += smartBlockOutputEnergy;
        smartEnergySamples += n * 2;
        smartAutoGainProgress.store(std::min(1.0f, (float)smartEnergySamples / (float)(sr * 0.5 * 2.0)),
                                    std::memory_order_relaxed);
        if (smartEnergySamples >= (int64_t)(sr * 0.5 * 2.0) && smartOutputEnergy > 1.0e-12)
        {
            const float measuredIn = std::sqrt((float)(smartInputEnergy / (double)smartEnergySamples));
            const float measuredOut = std::sqrt((float)(smartOutputEnergy / (double)smartEnergySamples));
            float targetDb = 20.0f * std::log10(std::max(measuredIn, 1.0e-7f) / std::max(measuredOut, 1.0e-7f));
            targetDb = std::clamp(targetDb, -24.0f, 24.0f);
            const float previous = autoGainCompDb.load(std::memory_order_relaxed);
            autoGainCompDb.store(previous + 0.35f * (targetDb - previous), std::memory_order_relaxed);
            smartInputEnergy = smartOutputEnergy = 0.0; smartEnergySamples = 0;
            smartAutoGainLocked.store(true, std::memory_order_release);
            smartAutoGainProgress.store(1.0f, std::memory_order_release);
        }
    }

    // Match EQ capture/analysis only enqueues samples here. Its FFT runs on a
    // low-priority worker and the result is converted into normal EQ bands by
    // the editor, so no analysis FFT executes on the audio thread.
    const bool useMatchSidechain = matchUseSidechain.load(std::memory_order_relaxed);
    matchEQ.pushSamples(useMatchSidechain && sidechainL != nullptr ? sidechainL : L,
                        useMatchSidechain && sidechainR != nullptr ? sidechainR : R, n);

    // Push post-EQ samples to spectrum FIFO
    if (shouldAnalyze) spectrumFifo.pushBlock(L, R, n);

    const bool pluginEnabled = pluginEnabledParam->load(std::memory_order_relaxed) > 0.5f;
    globalBypass.processOutput(mainBuffer, getLatencySamples(), pluginEnabled);

    // --- Output metering ---
    {
        float peakL = 0.0f, peakR = 0.0f;
        float sumSqL = 0.0f, sumSqR = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float al = std::abs(L[i]);
            const float ar = std::abs(R[i]);
            if (al > peakL) peakL = al;
            if (ar > peakR) peakR = ar;
            sumSqL += L[i] * L[i];
            sumSqR += R[i] * R[i];
        }
        meterPeakL.store(peakL, std::memory_order_relaxed);
        meterPeakR.store(peakR, std::memory_order_relaxed);
        meterRmsL.store(std::sqrt(sumSqL / (float) n), std::memory_order_relaxed);
        meterRmsR.store(std::sqrt(sumSqR / (float) n), std::memory_order_relaxed);
    }
}

void DefaultEqualizerAudioProcessor::updateReportedLatency() noexcept
{
    const bool linear = linearPhaseParam->load(std::memory_order_relaxed) > 0.5f;
    const int order = (int)oversamplingParam->load(std::memory_order_relaxed);
    bool driven = false;
    for (const auto& p : bandParams)
        driven = driven || (p.present->load(std::memory_order_relaxed) > 0.5f
            && p.on->load(std::memory_order_relaxed) > 0.5f
            && p.drive->load(std::memory_order_relaxed) > 0.0001f);
    const auto* os = driven && order > 0 && order <= kNumOversamplingOrders
        ? oversamplers[(size_t)(order - 1)].get() : nullptr;
    const int osLatency = os != nullptr ? juce::roundToInt(os->getLatencyInSamples()) : 0;
    setLatencySamples((linear ? currentLinearPhaseLatency() : 0) + osLatency
                      + requestedLookaheadSamples());
}

int DefaultEqualizerAudioProcessor::requestedLookaheadSamples() const noexcept
{
    float maximumMs = 0.0f;
    for (const auto& p : bandParams)
        if (p.present->load(std::memory_order_relaxed) > 0.5f
            && p.on->load(std::memory_order_relaxed) > 0.5f
            && p.dynThresh->load(std::memory_order_relaxed) < -0.05f
            && p.dynLookahead->load(std::memory_order_relaxed) > 0.001f)
            maximumMs = std::max(maximumMs, p.dynLookahead->load(std::memory_order_relaxed));
    return juce::roundToInt(sr * maximumMs * 0.001);
}

void DefaultEqualizerAudioProcessor::applyLookaheadDelay(juce::AudioBuffer<float>& buffer,
                                                          int delaySamples) noexcept
{
    const int ringSize = lookaheadDelayBuffer.getNumSamples();
    if (ringSize <= 0 || ringSize <= delaySamples) return;
    const int samples = buffer.getNumSamples();
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;
    auto* delayL = lookaheadDelayBuffer.getWritePointer(0);
    auto* delayR = buffer.getNumChannels() > 1 ? lookaheadDelayBuffer.getWritePointer(1) : delayL;
    if (delaySamples <= 0)
    {
        lookaheadMix.skip(samples);
        return;
    }
    for (int i = 0; i < samples; ++i)
    {
        int read = lookaheadWritePosition - delaySamples;
        if (read < 0) read += ringSize;
        const float dryL = left[i], dryR = right[i];
        const float wetL = delayL[read], wetR = delayR[read];
        delayL[lookaheadWritePosition] = dryL;
        delayR[lookaheadWritePosition] = dryR;
        const float mix = lookaheadMix.getNextValue();
        left[i] = dryL + mix * (wetL - dryL);
        right[i] = dryR + mix * (wetR - dryR);
        if (++lookaheadWritePosition >= ringSize) lookaheadWritePosition = 0;
    }
}


void DefaultEqualizerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree root("DEFAULT_EQUALIZER_STATE");
    root.setProperty("schemaVersion", 9, nullptr);
    auto current = apvts.copyState();
    current.setProperty("stateRole", "current", nullptr);
    root.appendChild(current, nullptr);
    std::unique_ptr<juce::XmlElement> xml (root.createXml());
    copyXmlToBinary(*xml, destData);
}

void DefaultEqualizerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary(data, sizeInBytes));
    if (!xmlState)
        return;

    const auto restored = juce::ValueTree::fromXml(*xmlState);
    int restoredSchema = 0;
    bool recognizedState = false;
    if (restored.hasType("DEFAULT_EQUALIZER_STATE"))
    {
        recognizedState = true;
        restoredSchema = (int) restored.getProperty("schemaVersion", 0);
        juce::ValueTree current;
        if (restoredSchema >= 4)
        {
            for (auto child : restored)
                if (child.getProperty("stateRole").toString() == "current")
                    current = child.createCopy();
        }
        else
        {
            // v1-v3 stored two A/B snapshots. Preserve the state that was
            // audible when the project was saved, then discard the obsolete
            // comparison slot.
            const auto active = restored.getProperty("activeSlot", "A").toString();
            for (auto child : restored)
                if (child.getProperty("snapshotSlot").toString() == active)
                    current = child.createCopy();
        }
        if (!current.isValid()) return;
        current.removeProperty("stateRole", nullptr);
        current.removeProperty("snapshotSlot", nullptr);
        for (int i = 1; i <= kNumBands; ++i)
        {
            removeStateParameter(current, bandId(i, "link"));
            removeStateParameter(current, bandId(i, "dyn_on"));
        }
        if (restoredSchema < 6)
            for (int i = 1; i <= kNumBands; ++i)
            {
                const int route = (int)readStateParameter(current, bandId(i, "ch"), 0.0f);
                writeStateParameter(current, bandId(i, "placement_mode"), route >= 3 ? 1.0f : 0.0f);
                writeStateParameter(current, bandId(i, "placement"),
                    route == 1 || route == 3 ? -100.0f : route == 2 || route == 4 ? 100.0f : 0.0f);
                removeStateParameter(current, bandId(i, "ch"));
                const int mode = (int)readStateParameter(current, bandId(i, "sat_mode"), 0.0f);
                const float legacyDrive = readStateParameter(current, bandId(i, "drive"), 0.0f);
                const float legacyCharacter = readStateParameter(current, bandId(i, "drive_character"), 0.0f) * 0.01f;
                writeStateParameter(current, bandId(i, "drive"), legacyDrive * 0.36f);
                writeStateParameter(current, bandId(i, "drive_character"),
                    mode == 3 || mode == 4 || mode == 6 ? 2.0f * legacyCharacter - 1.0f : legacyCharacter);
                writeStateParameter(current, bandId(i, "drive_secondary"),
                    readStateParameter(current, bandId(i, "drive_secondary"), 0.0f) * 0.01f);
                removeStateParameter(current, bandId(i, "drive_tone"));
            }
        if (restoredSchema < 8)
            for (int i = 1; i <= kNumBands; ++i)
                writeStateParameter(current, bandId(i, "present"),
                    readStateParameter(current, bandId(i, "on"), 0.0f));
        apvts.replaceState(current);
    }
    else if (restored.hasType(apvts.state.getType()))
    {
        recognizedState = true;
        // Migration from upstream FreeEQ8-compatible state: preserve matching IDs,
        // and write the current single-state schema on the next save.
        auto current = restored.createCopy();
        for (int i = 1; i <= kNumBands; ++i)
        {
            removeStateParameter(current, bandId(i, "link"));
            removeStateParameter(current, bandId(i, "dyn_on"));
            const int route = (int)readStateParameter(current, bandId(i, "ch"), 0.0f);
            writeStateParameter(current, bandId(i, "placement_mode"), route >= 3 ? 1.0f : 0.0f);
            writeStateParameter(current, bandId(i, "placement"),
                route == 1 || route == 3 ? -100.0f : route == 2 || route == 4 ? 100.0f : 0.0f);
            removeStateParameter(current, bandId(i, "ch"));
            const int mode = (int)readStateParameter(current, bandId(i, "sat_mode"), 0.0f);
            const float legacyDrive = readStateParameter(current, bandId(i, "drive"), 0.0f);
            const float legacyCharacter = readStateParameter(current, bandId(i, "drive_character"), 0.0f) * 0.01f;
            writeStateParameter(current, bandId(i, "drive"), legacyDrive * 0.36f);
            writeStateParameter(current, bandId(i, "drive_character"),
                mode == 3 || mode == 4 || mode == 6 ? 2.0f * legacyCharacter - 1.0f : legacyCharacter);
            writeStateParameter(current, bandId(i, "drive_secondary"),
                readStateParameter(current, bandId(i, "drive_secondary"), 0.0f) * 0.01f);
            removeStateParameter(current, bandId(i, "drive_tone"));
            writeStateParameter(current, bandId(i, "present"),
                readStateParameter(current, bandId(i, "on"), 0.0f));
        }
        apvts.replaceState(current);
        restoredSchema = 0;
    }
    if (!recognizedState) return;

    if (restoredSchema < 2)
    {
        // v1 inherited FreeEQ8's choice parameter: 0/1/2 meant 12/24/48.
        // v2 exposes a continuous 0..48 dB/oct value for every filter type.
        const auto migrateCurrent = [this]
        {
            for (int i = 1; i <= kNumBands; ++i)
            {
                const auto id = bandId(i, "slope");
                const float legacy = apvts.getRawParameterValue(id)->load();
                const float migrated = legacy < 0.5f ? 12.0f : (legacy < 1.5f ? 24.0f : 48.0f);
                if (auto* parameter = apvts.getParameter(id))
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(migrated));
            }
        };

        migrateCurrent();
    }
    if (restoredSchema < 3)
    {
        const auto migrateAutoGain = [this]
        {
            const bool oldEnabled = (bool) apvts.state.getProperty("auto_gain", false);
            if (auto* mode = apvts.getParameter("auto_gain_mode"))
                mode->setValueNotifyingHost(mode->convertTo0to1(oldEnabled ? 2.0f : 0.0f));
            apvts.state.removeProperty("auto_gain", nullptr);
        };
        migrateAutoGain();
    }
    if (restoredSchema < 4)
    {
        // The v3 lookahead control was a boolean selecting a fixed 5 ms.
        // Preserve the audible timing when loading those projects into the
        // continuous 0..5 ms parameter.
        for (int i = 1; i <= kNumBands; ++i)
        {
            const auto id = bandId(i, "dyn_lookahead");
            if (apvts.getRawParameterValue(id)->load() > 0.5f)
                if (auto* parameter = apvts.getParameter(id))
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(5.0f));
        }
    }

    // Ask the background worker to rebuild the FIR from the restored
    // state; audio thread will keep using the previous kernel until then.
    requestLinearPhaseRebuild();

    updateReportedLatency();
}

double DefaultEqualizerAudioProcessor::getTailLengthSeconds() const
{
    // Match EQ analysis is converted to normal bands and has no convolution
    // tail. Only the optional linear-phase FIR can outlive the input boundary.
    if (sr <= 0.0)
        return 0.0;

    const bool linPhase    = apvts.getRawParameterValue("linear_phase")->load() > 0.5f;
    return linPhase ? (double)(currentLinearPhaseLatency() * 2) / sr : 0.0;
}

juce::AudioProcessorEditor* DefaultEqualizerAudioProcessor::createEditor()
{
    return new DefaultEqualizerAudioProcessorEditor(*this);
}

void DefaultEqualizerAudioProcessor::buildLinearPhaseMagnitude()
{
    // Build composite magnitude response for the linear phase FIR.
    // Use the same logic as ResponseCurveComponent but at FFT resolution.
    // Centered L/R static bands are synthesized into this FIR. Asymmetrically
    // placed L/R or M/S bands,
    // dynamic modulation, and per-band drive remain in the post-FIR stage.
    const int numBins = LinearPhaseEngine::firLength / 2 + 1;

    // Use pre-allocated member buffer (avoid heap allocation on audio thread)
    linPhaseMagBuf.resize((size_t)numBins);
    std::fill(linPhaseMagBuf.begin(), linPhaseMagBuf.end(), 0.0f);
    float* magDb = linPhaseMagBuf.data();
    const float amount = apvts.getRawParameterValue("scale")->load();

    for (int b = 0; b < kNumBands; ++b)
    {
        const int idx = b + 1;
        const bool on = apvts.getRawParameterValue(bandId(idx, "present"))->load() > 0.5f
            && apvts.getRawParameterValue(bandId(idx, "on"))->load() > 0.5f;
        if (!on) continue;
        const bool routed = apvts.getRawParameterValue(bandId(idx, "placement_mode"))->load() > 0.5f
            || std::abs(apvts.getRawParameterValue(bandId(idx, "placement"))->load()) > 0.001f;
        if (routed) continue; // routed bands use their minimum-phase post stage

        const int t = (int)apvts.getRawParameterValue(bandId(idx, "type"))->load();
        Biquad::Type tp = Biquad::Type::Bell;
        switch (t)
        {
            case 0: tp = Biquad::Type::Bell; break;
            case 1: tp = Biquad::Type::LowShelf; break;
            case 2: tp = Biquad::Type::HighShelf; break;
            case 3: tp = Biquad::Type::HighPass; break;
            case 4: tp = Biquad::Type::LowPass; break;
            case 5: tp = Biquad::Type::Bandpass; break;
            case 6: tp = Biquad::Type::Notch; break;
            case 7: tp = Biquad::Type::Tilt; break;
        }

        const float freq = apvts.getRawParameterValue(bandId(idx, "freq"))->load();
        float q    = apvts.getRawParameterValue(bandId(idx, "q"))->load();
        const float rawGain = apvts.getRawParameterValue(bandId(idx, "gain"))->load();
        const bool gainBearing = variable_slope::distributesGain(tp);
        const float gain = gainBearing ? rawGain * amount : rawGain;

        // Apply adaptive Q in linear phase magnitude build
        const bool adaptiveQ = apvts.getRawParameterValue("adaptive_q")->load() > 0.5f;
        if (adaptiveQ)
            q = calculateAdaptiveQ(q, rawGain);

        const float slope = apvts.getRawParameterValue(bandId(idx, "slope"))->load();
        constexpr bool decramp = true;
        const bool cut = tp == Biquad::Type::HighPass || tp == Biquad::Type::LowPass;
        double responseFreq = freq;
        if (cut && amount > 0.0f)
        {
            const double neutralEdge = tp == Biquad::Type::LowPass ? sr * 0.45 : 10.0;
            responseFreq = neutralEdge * std::pow(std::max(1.0e-9, freq / neutralEdge), amount);
            responseFreq = std::clamp(responseFreq, 10.0, sr * 0.45);
        }

        for (int i = 0; i < numBins; ++i)
        {
            const double f = (double)i / (double)(numBins - 1) * sr * 0.5;
            if (f < 1.0) continue;

            const auto rawResponse = variable_slope::response(tp, sr, responseFreq, q, gain,
                                                              slope, decramp, f);
            const auto response = gainBearing || (cut && amount > 0.0f)
                ? rawResponse
                : cut ? std::complex<double>(1.0, 0.0)
                      : std::complex<double>(1.0, 0.0)
                          + (double)std::clamp(amount, 0.0f, 1.0f)
                              * (rawResponse - std::complex<double>(1.0, 0.0));
            magDb[(size_t)i] += (float)(20.0 * std::log10(
                std::max(std::abs(response), 1.0e-15)));
        }
    }

    linearPhaseEngine.rebuildFromMagnitude(magDb, numBins, currentLinearPhaseLatency() * 2);
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DefaultEqualizerAudioProcessor();
}
