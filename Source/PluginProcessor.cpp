#include "PluginProcessor.h"
#include "DSP/DriveAutoGainTable.h"
#include "DSP/FilterTypes.h"
#include "DSP/EQAutoGain.h"
#include "DSP/VariableSlope.h"
#include "PluginEditor.h"
#include <complex>
#include <limits>

static juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}

static constexpr std::array<const char*, 19> observedBandParameterSuffixes {
    "present", "on", "type", "slope", "placement_mode", "placement",
    "freq", "q", "gain", "drive", "drive_character", "drive_secondary",
    "sat_mode", "dyn_mode", "sc_source", "dyn_lookahead", "dyn_thresh",
    "dyn_range", "dyn_ratio"
};

static inline void sanitizeAndClamp(float* samples, int count,
                                    float minimum, float maximum) noexcept
{
    int sample = 0;
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    const auto low = vdupq_n_f32(minimum);
    const auto high = vdupq_n_f32(maximum);
    const auto zero = vdupq_n_f32(0.0f);
    const auto maximumFinite = vdupq_n_f32(std::numeric_limits<float>::max());
    for (; sample + 4 <= count; sample += 4)
    {
        const auto input = vld1q_f32(samples + sample);
        const auto finite = vcleq_f32(vabsq_f32(input), maximumFinite);
        const auto clamped = vmaxq_f32(low, vminq_f32(high, input));
        vst1q_f32(samples + sample, vbslq_f32(finite, clamped, zero));
    }
#elif defined(__SSE2__) || defined(_M_X64)
    const auto low = _mm_set1_ps(minimum);
    const auto high = _mm_set1_ps(maximum);
    const auto signBit = _mm_set1_ps(-0.0f);
    const auto maximumFinite = _mm_set1_ps(std::numeric_limits<float>::max());
    for (; sample + 4 <= count; sample += 4)
    {
        const auto input = _mm_loadu_ps(samples + sample);
        const auto finite = _mm_cmple_ps(_mm_andnot_ps(signBit, input), maximumFinite);
        const auto clamped = _mm_max_ps(low, _mm_min_ps(high, input));
        _mm_storeu_ps(samples + sample, _mm_and_ps(finite, clamped));
    }
#endif
    for (; sample < count; ++sample)
        samples[sample] = std::isfinite(samples[sample])
            ? std::clamp(samples[sample], minimum, maximum) : 0.0f;
}

static inline void scaleSanitizeAndClamp(float* samples, int count, float gain,
                                         float minimum, float maximum) noexcept
{
    int sample = 0;
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    const auto vectorGain = vdupq_n_f32(gain);
    const auto low = vdupq_n_f32(minimum);
    const auto high = vdupq_n_f32(maximum);
    const auto zero = vdupq_n_f32(0.0f);
    const auto maximumFinite = vdupq_n_f32(std::numeric_limits<float>::max());
    for (; sample + 4 <= count; sample += 4)
    {
        const auto input = vld1q_f32(samples + sample);
        const auto finite = vcleq_f32(vabsq_f32(input), maximumFinite);
        const auto scaled = vmulq_f32(input, vectorGain);
        const auto clamped = vmaxq_f32(low, vminq_f32(high, scaled));
        vst1q_f32(samples + sample, vbslq_f32(finite, clamped, zero));
    }
#elif defined(__SSE2__) || defined(_M_X64)
    const auto vectorGain = _mm_set1_ps(gain);
    const auto low = _mm_set1_ps(minimum);
    const auto high = _mm_set1_ps(maximum);
    const auto signBit = _mm_set1_ps(-0.0f);
    const auto maximumFinite = _mm_set1_ps(std::numeric_limits<float>::max());
    for (; sample + 4 <= count; sample += 4)
    {
        const auto input = _mm_loadu_ps(samples + sample);
        const auto finite = _mm_cmple_ps(_mm_andnot_ps(signBit, input), maximumFinite);
        const auto scaled = _mm_mul_ps(input, vectorGain);
        const auto clamped = _mm_max_ps(low, _mm_min_ps(high, scaled));
        _mm_storeu_ps(samples + sample, _mm_and_ps(finite, clamped));
    }
#endif
    for (; sample < count; ++sample)
        samples[sample] = std::isfinite(samples[sample])
            ? std::clamp(samples[sample] * gain, minimum, maximum) : 0.0f;
}


std::pair<float, float> DefaultEqualizerAudioProcessor::dynamicsTimingForSpeed(float speed) noexcept
{
    const float s = std::clamp(speed, 0.0f, 100.0f);
    const auto logLerp = [](float from, float to, float amount)
    { return from * std::pow(to / from, amount); };
    if (s <= 50.0f)
    {
        const float t = s / 50.0f;
        return { logLerp(100.0f, 10.0f, t), logLerp(1000.0f, 100.0f, t) };
    }
    const float t = (s - 50.0f) / 50.0f;
    return { logLerp(10.0f, 0.1f, t), logLerp(100.0f, 15.0f, t) };
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
        : juce::Thread("default_eq_LinPhaseRebuild"), proc(p) {}

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
        for (auto* suffix : observedBandParameterSuffixes)
            apvts.addParameterListener(bandId(i, suffix), this);
    for (int i = 1; i <= kNumBands; ++i)
        apvts.addParameterListener(bandId(i, "dyn_speed"), this);
    apvts.addParameterListener("linear_phase", this);
    apvts.addParameterListener("linear_quality", this);
    apvts.addParameterListener("scale", this);
    apvts.addParameterListener("shift", this);
    apvts.addParameterListener("adaptive_q", this);
    apvts.addParameterListener("oversampling", this);
    apvts.addParameterListener("auto_gain_mode", this);
    for (auto* id : { "transient_split_strength", "transient_split_balance",
                      "transient_split_hold", "transient_split_smooth" })
        apvts.addParameterListener(id, this);

    linPhaseRebuildThread = std::make_unique<LinPhaseRebuildThread>(*this);

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
    apvts.removeParameterListener("auto_gain_mode", this);
    for (auto* id : { "transient_split_strength", "transient_split_balance",
                      "transient_split_hold", "transient_split_smooth" })
        apvts.removeParameterListener(id, this);
    apvts.removeParameterListener("scale", this);
    apvts.removeParameterListener("shift", this);
    apvts.removeParameterListener("linear_phase", this);
    apvts.removeParameterListener("linear_quality", this);
    for (int i = 1; i <= kNumBands; ++i)
    {
        for (auto* suffix : observedBandParameterSuffixes)
            apvts.removeParameterListener(bandId(i, suffix), this);
        apvts.removeParameterListener(bandId(i, "dyn_speed"), this);
    }
}

void DefaultEqualizerAudioProcessor::requestLinearPhaseRebuild()
{
    linPhaseDirty.store(true, std::memory_order_release);
    if (linPhaseRebuildThread && linPhaseRebuildThread->isThreadRunning())
        linPhaseRebuildThread->notify();
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
    for(auto& b:transientBands)b.reset(sr);
    for(auto& b:sustainBands)b.reset(sr);
    transientSplitter.prepare(sampleRate,samplesPerBlock);
    internalDetectorTransientSplitter.prepare(sampleRate,samplesPerBlock);
    externalDetectorTransientSplitter.prepare(sampleRate,samplesPerBlock);
    transientBuffer.setSize(2,samplesPerBlock,false,true,false);
    sustainBuffer.setSize(2,samplesPerBlock,false,true,false);
    staticRoutingScratch.setSize(2,samplesPerBlock,false,true,false);
    internalDetectorTransientBuffer.setSize(2,samplesPerBlock,false,true,false);
    internalDetectorSustainBuffer.setSize(2,samplesPerBlock,false,true,false);
    externalDetectorTransientBuffer.setSize(2,samplesPerBlock,false,true,false);
    externalDetectorSustainBuffer.setSize(2,samplesPerBlock,false,true,false);
    externalDetectorInputBuffer.setSize(2,samplesPerBlock,false,true,false);
    for(auto& detector:internalBandDetectorBuffers)
        detector.setSize(2,samplesPerBlock,false,true,false);
    for(auto& detector:externalBandDetectorBuffers)
        detector.setSize(2,samplesPerBlock,false,true,false);

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
    const int maximumPluginLatency = LinearPhaseEngine::latency + transientSplitter.latency()
        + juce::roundToInt(sampleRate * lookaheadSeconds) + 256;
    globalBypass.prepare(sampleRate, samplesPerBlock, getMainBusNumOutputChannels(),
                         maximumPluginLatency, true);
    smartReferenceDelayBuffer.setSize(2, maximumPluginLatency + samplesPerBlock + 1,
                                      false, true, false);
    smartReferenceDelayBuffer.clear();
    smartReferenceDelayPosition = 0;
    const int maxLookahead = juce::roundToInt(sampleRate * lookaheadSeconds);
    lookaheadDelayBuffer.setSize(2, maxLookahead + samplesPerBlock + 8, false, true, false);
    lookaheadDelayBuffer.clear();
    externalLookaheadDelayBuffer.setSize(2, maxLookahead + samplesPerBlock + 8, false, true, false);
    externalLookaheadDelayBuffer.clear();
    internalTSDetectorDelayBuffer.setSize(4, maxLookahead + samplesPerBlock + 8, false, true, false);
    internalTSDetectorDelayBuffer.clear();
    externalTSDetectorDelayBuffer.setSize(4, maxLookahead + samplesPerBlock + 8, false, true, false);
    externalTSDetectorDelayBuffer.clear();
    detectorInputBuffer.setSize(2, samplesPerBlock, false, true, false);
    detectorInputBuffer.clear();
    lookaheadWritePosition = 0;
    tsDetectorLookaheadWritePosition = 0;
    transientRoutingWasActive = false;
    externalTSDetectorWasActive = false;
    externalTSDetectorWarmupSamplesRemaining = 0;
    externalTSDetectorMix.reset(sampleRate, 0.01);
    externalTSDetectorMix.setCurrentAndTargetValue(0.0f);
    lookaheadMix.reset(sampleRate, 0.02);
    lookaheadMix.setCurrentAndTargetValue(requestedLookaheadSamples() > 0 ? 1.0f : 0.0f);
    latencyTransitionGain.reset(sampleRate, 0.005);
    latencyTransitionGain.setCurrentAndTargetValue(1.0f);
    processedLatency = getLatencySamples();
    latencyFadingOut = false;
    regularAutoGainDirty.store(true, std::memory_order_release);
    regularTargetCompDb = 0.0f;
    regularCompensationLinear = 1.0f;
    cachedOutputGainDb = std::numeric_limits<float>::quiet_NaN();
    cachedOutputGainLinear = 1.0f;
    autoGainLinearSm.reset(sampleRate, 0.01);
    autoGainLinearSm.setCurrentAndTargetValue(1.0f);
    smartInputLoudness.prepare(sampleRate);
    smartOutputLoudness.prepare(sampleRate);
    smartWarmupSamplesRemaining = 0;
    smartLoudnessRevision = 0;
    smartLoudnessUpdateCount = 0;
    smartObservationDirty.store(true, std::memory_order_release);
    smartTargetCompDb = 0.0f;
    processedAutoGainMode = -1;
    bandDirtyMask.store((1u << kNumBands) - 1u, std::memory_order_release);
    runtimeBandsConfigured = false;
    lastSidechainConnected = false;
    lastMainChannelCount = 0;
    cachedActiveBandCount = cachedDrivenBandCount = cachedTsActiveBandCount = 0;
    cachedTsDriveActive = cachedTsDetectorNeeded = cachedTsExternalDetectorNeeded = false;
    smartAutoGainLocked.store(false, std::memory_order_release);
    smartAutoGainProgress.store(0.0f, std::memory_order_release);
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
        const auto makeOversampler = [this, order, samplesPerBlock]
        {
            auto result = std::make_unique<juce::dsp::Oversampling<float>>(
                2u, order,
                juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);
            result->initProcessing((size_t)samplesPerBlock);
            return result;
        };
        oversamplers[(size_t)i] = makeOversampler();
        transientOversamplers[(size_t)i] = makeOversampler();
        sustainOversamplers[(size_t)i] = makeOversampler();
    }
}

juce::dsp::Oversampling<float>* DefaultEqualizerAudioProcessor::currentOversamplerPtr() const noexcept
{
    const int order = currentOversamplingOrder;
    if (order <= 0 || order > kNumOversamplingOrders) return nullptr;
    return oversamplers[(size_t)(order - 1)].get();
}

std::uint32_t DefaultEqualizerAudioProcessor::syncBandsFromParams()
{
    const auto dirty = bandDirtyMask.exchange(0u, std::memory_order_acq_rel);
    if (dirty == 0u)
        return 0u;
    for (int index = 0; index < kNumBands; ++index)
    {
        if ((dirty & (1u << index)) == 0u)
            continue;
        const auto& p = bandParams[(size_t)index];
        auto& snapshot = bandSnapshots[(size_t)index];
        snapshot.present = p.present->load(std::memory_order_relaxed) > 0.5f;
        snapshot.on = p.on->load(std::memory_order_relaxed) > 0.5f;
        snapshot.type = (int)p.type->load(std::memory_order_relaxed);
        snapshot.slope = p.slope->load(std::memory_order_relaxed);
        snapshot.placementMode = (int)p.placementMode->load(std::memory_order_relaxed);
        snapshot.placement = p.placement->load(std::memory_order_relaxed);
        snapshot.freq = p.freq->load(std::memory_order_relaxed);
        snapshot.q = p.q->load(std::memory_order_relaxed);
        snapshot.gain = p.gain->load(std::memory_order_relaxed);
        snapshot.drive = p.drive->load(std::memory_order_relaxed);
        snapshot.driveCharacter = p.driveCharacter->load(std::memory_order_relaxed);
        snapshot.driveSecondary = p.driveSecondary->load(std::memory_order_relaxed);
        snapshot.satMode = (int)p.satMode->load(std::memory_order_relaxed);
        snapshot.dynMode = (int)p.dynMode->load(std::memory_order_relaxed);
        snapshot.scSource = (int)p.scSource->load(std::memory_order_relaxed);
        snapshot.dynLookahead = p.dynLookahead->load(std::memory_order_relaxed);
        snapshot.dynThresh = p.dynThresh->load(std::memory_order_relaxed);
        snapshot.dynRange = p.dynRange->load(std::memory_order_relaxed);
        snapshot.dynRatio = p.dynRatio->load(std::memory_order_relaxed);
        snapshot.dynSpeed = p.dynSpeed->load(std::memory_order_relaxed);

    }
    cachedAnyEnabledBand = std::any_of(bandSnapshots.begin(), bandSnapshots.end(),
        [](const auto& snapshot) { return snapshot.enabled(); });
    cachedAnyDynamicBand = false;
    cachedMaxDynamicLookaheadMs = 0.0f;
    for (const auto& snapshot : bandSnapshots)
        if (snapshot.enabled() && snapshot.dynThresh < -0.05f)
        {
            cachedAnyDynamicBand = true;
            cachedMaxDynamicLookaheadMs = std::max(cachedMaxDynamicLookaheadMs,
                                                   snapshot.dynLookahead);
        }
    return dirty;
}

void DefaultEqualizerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const bool shouldAnalyze = analyzerEnabled.load(std::memory_order_acquire);
    if (shouldAnalyze)
    {
        bool transportPlaying = false;
        if (auto* playHead = getPlayHead())
            if (const auto position = playHead->getPosition())
                transportPlaying = position->getIsPlaying();
        if (transportPlaying && !transportWasPlaying)
            transportStartGeneration.fetch_add(1, std::memory_order_release);
        transportWasPlaying = transportPlaying;
    }

    auto mainBuffer = getBusBuffer(buffer, false, 0);
    const int mainChannels = mainBuffer.getNumChannels();
    if (mainChannels < 1) return;

    for (int channel = 0; channel < mainChannels; ++channel)
        sanitizeAndClamp(mainBuffer.getWritePointer(channel),
                         mainBuffer.getNumSamples(), -64.0f, 64.0f);

    const int n = mainBuffer.getNumSamples();
    if (processedLatency != getLatencySamples())
    {
        processedLatency = getLatencySamples();
        latencyTransitionGain.setTargetValue(0.0f);
        latencyFadingOut = true;
        smartInputLoudness.reset();
        smartOutputLoudness.reset();
        smartWarmupSamplesRemaining = processedLatency;
        smartLoudnessRevision = 0;
        smartLoudnessUpdateCount = 0;
        smartAutoGainLocked.store(false, std::memory_order_release);
        smartAutoGainProgress.store(0.0f, std::memory_order_release);
    }
    const int autoGainMode = (int)autoGainModeParam->load(std::memory_order_relaxed);
    const bool autoGainModeChanged = autoGainMode != processedAutoGainMode;
    processedAutoGainMode = autoGainMode;
    if (autoGainMode == 2
        && smartObservationDirty.exchange(false, std::memory_order_acq_rel))
    {
        // Keep the audible value while one new finite observation is taken.
        smartTargetCompDb = autoGainCompDb.load(std::memory_order_relaxed);
        smartInputLoudness.reset();
        smartOutputLoudness.reset();
        smartWarmupSamplesRemaining = std::max(getLatencySamples(), n);
        smartLoudnessRevision = 0;
        smartLoudnessUpdateCount = 0;
        smartAutoGainLocked.store(false, std::memory_order_release);
        smartAutoGainProgress.store(0.0f, std::memory_order_release);
    }

    const auto dirtyBands = syncBandsFromParams();

    const bool pluginEnabled = pluginEnabledParam->load(std::memory_order_relaxed) > 0.5f;
    const bool cleanUnityPath = !cachedAnyEnabledBand && autoGainMode != 2
        && std::abs(outputGainParam->load(std::memory_order_relaxed)) < 0.0001f
        && linearPhaseParam->load(std::memory_order_relaxed) < 0.5f
        && soloBand.load(std::memory_order_relaxed) < 0
        && !shouldAnalyze && getLatencySamples() == 0 && !latencyFadingOut
        && std::abs(autoGainCompDb.load(std::memory_order_relaxed)) < 0.0001f
        && !autoGainLinearSm.isSmoothing()
        && globalBypass.isTransparentEnabledPath(0, pluginEnabled);
    if (cleanUnityPath)
        return;

    const bool transparentBypassPath = globalBypass.isTransparentEnabledPath(
        getLatencySamples(), pluginEnabled);
    if (!transparentBypassPath)
        globalBypass.captureInput(mainBuffer);

    const auto sidechain = getBusCount(true) > 1 ? getBusBuffer(buffer, true, 1)
                                                 : juce::AudioBuffer<float>();
    const float* sidechainL = sidechain.getNumChannels() > 0 ? sidechain.getReadPointer(0) : nullptr;
    const float* sidechainR = sidechain.getNumChannels() > 1 ? sidechain.getReadPointer(1) : sidechainL;
    const bool sidechainConnected = sidechainL != nullptr;
    std::uint32_t configurationDirtyBands = dirtyBands;
    if (!runtimeBandsConfigured || sidechainConnected != lastSidechainConnected
        || mainChannels != lastMainChannelCount)
        configurationDirtyBands = (1u << kNumBands) - 1u;
    runtimeBandsConfigured = true;
    lastSidechainConnected = sidechainConnected;
    lastMainChannelCount = mainChannels;
    const int meterBand = uiMeterBand.load(std::memory_order_relaxed);
    const bool anyDynamicBand = cachedAnyDynamicBand;
    const bool needsDetectorInput = autoGainMode == 2 || anyDynamicBand || meterBand >= 0;
    if (needsDetectorInput)
        for (int channel = 0; channel < 2; ++channel)
            detectorInputBuffer.copyFrom(channel, 0, mainBuffer,
                                         juce::jmin(channel, mainChannels - 1), 0, n);
    if (needsDetectorInput)
        for(int channel=0;channel<2;++channel)
        {
            const float* source=channel==0?sidechainL:sidechainR;
            if(source!=nullptr)externalDetectorInputBuffer.copyFrom(channel,0,source,n);
            else externalDetectorInputBuffer.clear(channel,0,n);
        }

    // Smart Gain alone needs a latency-aligned K-weighted reference stream.
    // Regular is parameter-derived and never touches this per-sample path.
    const int smartReferenceCapacity = smartReferenceDelayBuffer.getNumSamples();
    const int smartReferenceDelay = smartReferenceCapacity > 0
        ? std::clamp(getLatencySamples(), 0, smartReferenceCapacity - 1) : 0;
    if (autoGainMode == 2
        && !smartAutoGainLocked.load(std::memory_order_acquire))
    {
        for (int i = 0; i < n && smartReferenceCapacity > 0; ++i)
        {
            int readPosition = smartReferenceDelayPosition - smartReferenceDelay;
            if (readPosition < 0) readPosition += smartReferenceCapacity;
            const float currentL = detectorInputBuffer.getSample(0, i);
            const float currentR = detectorInputBuffer.getSample(1, i);
            const float alignedL = smartReferenceDelay == 0 ? currentL
                : smartReferenceDelayBuffer.getSample(0, readPosition);
            const float alignedR = smartReferenceDelay == 0 ? currentR
                : smartReferenceDelayBuffer.getSample(1, readPosition);
            smartReferenceDelayBuffer.setSample(0, smartReferenceDelayPosition, currentL);
            smartReferenceDelayBuffer.setSample(1, smartReferenceDelayPosition, currentR);
            if (++smartReferenceDelayPosition >= smartReferenceCapacity)
                smartReferenceDelayPosition = 0;
            smartInputLoudness.pushSample(alignedL, alignedR);
        }
    }

    const int lookahead = juce::roundToInt(sr * cachedMaxDynamicLookaheadMs * 0.001);
    lookaheadMix.setTargetValue(lookahead > 0 ? 1.0f : 0.0f);
    if (lookahead > 0)
        routeLookahead(mainBuffer,sidechainL,sidechainR,lookahead);

    // Get global parameters
    const float amount = amountParam->load(std::memory_order_relaxed);
    const float shiftSemitones = shiftParam->load(std::memory_order_relaxed);
    const float outputGainDb = outputGainParam->load(std::memory_order_relaxed);
    if (outputGainDb != cachedOutputGainDb)
    {
        cachedOutputGainDb = outputGainDb;
        cachedOutputGainLinear = std::pow(10.0f, outputGainDb / 20.0f);
    }
    const float outputGain = cachedOutputGainLinear;
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
            if (auto* os = transientOversamplers[(size_t)(osOrder - 1)].get())
                os->reset();
            if (auto* os = sustainOversamplers[(size_t)(osOrder - 1)].get())
                os->reset();
        }
        currentOversamplingOrder = osOrder;
    }
    juce::dsp::Oversampling<float>* const configuredOS = currentOversamplerPtr();

    // Check if any band is soloed
    const int soloedBand = soloBand.load(std::memory_order_relaxed);

    // Read per-band slope and continuous stereo placement, then set up beginBlock.
    const double effectiveSR = (configuredOS != nullptr)
        ? sr * std::pow(2.0, currentOversamplingOrder)
        : sr;
    auto& activeBands = cachedActiveBands;
    auto& drivenBands = cachedDrivenBands;
    auto& tsActiveBands = cachedTsActiveBands;
    auto& activeBandCount = cachedActiveBandCount;
    auto& drivenBandCount = cachedDrivenBandCount;
    auto& tsActiveBandCount = cachedTsActiveBandCount;
    auto& tsDriveActive = cachedTsDriveActive;
    auto& tsDetectorNeeded = cachedTsDetectorNeeded;
    auto& tsExternalDetectorNeeded = cachedTsExternalDetectorNeeded;

    if (configurationDirtyBands != 0u)
    {
        activeBandCount = drivenBandCount = tsActiveBandCount = 0;
        tsDriveActive = tsDetectorNeeded = tsExternalDetectorNeeded = false;
        for (int i = 0; i < kNumBands; ++i)
        {
            auto& b = bands[(size_t)i];
            const auto& p = bandSnapshots[(size_t)i];
            const bool effectiveEnabled = p.enabled();
            const int placementMode = mainChannels > 1
                ? std::clamp(p.placementMode, 0, 2) : 0;
            const float placement = mainChannels > 1
                ? p.placement * 0.01f : 0.0f;
            const bool configurationDirty = (configurationDirtyBands & (1u << i)) != 0u;

            if (configurationDirty)
            {
                const auto parameterType = deq::filter_types::fromParameterIndex(p.type);
                b.parameterType = parameterType;
                const bool isCut = zl_filter::isClassicCut(parameterType)
                    || zl_filter::isResonantCut(parameterType);
                const float bandGain = p.gain;
                float effectiveQ = p.q;
                if (adaptiveQ)
                    effectiveQ = calculateAdaptiveQ(p.q, bandGain);
                if (zl_filter::isResonantCut(parameterType))
                    effectiveQ = EQBand::amountResonantCutQ(effectiveQ, amount);
                const bool gainBearing = variable_slope::distributesGain(parameterType);
                const float driveDb = p.drive;
                const float driveCharacterRaw = p.driveCharacter;
#if DEFAULT_EQ_FULL
                b.satType = static_cast<SaturationType>(
                    std::clamp(p.satMode, 0, kSaturationModeCount - 1));
#endif
                const bool bipolarCharacter = saturationModeUsesBipolarCharacter(p.satMode);
                b.driveAmount = driveDb > 0.0001f ? driveDb / 36.0f : 0.0f;
                b.driveCharacter = bipolarCharacter
                    ? 0.5f * (std::clamp(driveCharacterRaw, -1.0f, 1.0f) + 1.0f)
                    : std::clamp(driveCharacterRaw, 0.0f, 1.0f);
                b.driveSecondary = p.driveSecondary;
                b.driveAutoGainLinear = deq::drive_auto_gain_table::lookup(p.satMode, driveDb);
                const bool wasDynamic = b.dynEnabled;
                b.dynEnabled = p.dynThresh < -0.05f;
                if (!b.dynEnabled)
                {
                    b.dynGainMod = 0.0f;
                    if (wasDynamic) b.coefficientsValid = false;
                }
                const auto [dynAtk, dynRel] = dynamicsTimingForSpeed(p.dynSpeed);
                b.dynUpward = p.dynMode > 0;
                b.useExternalSidechain = p.scSource > 0 && sidechainConnected;
                b.dynRangeDb = p.dynRange;
                b.dynThreshDb = p.dynThresh;
                b.dynRatio = p.dynRatio;
                b.dynAttackMs = dynAtk;
                b.dynReleaseMs = dynRel;
                b.gainScale = gainBearing ? amount : 1.0f;
                b.globalAmount = gainBearing ? 1.0f
                    : isCut ? EQBand::cutAmountMix(amount)
                            : std::clamp(amount, 0.0f, 1.0f);
                b.driveGlobalAmount = std::max(0.0f, amount);

                float adjustedFrequency = shiftedFrequency(p.freq, shiftSemitones);
                if (isCut)
                {
                    const float neutralEdge = (parameterType == Biquad::Type::LowPass
                                               || parameterType == Biquad::Type::ResLowPass)
                        ? (float)sr * 0.45f : 10.0f;
                    adjustedFrequency = neutralEdge * std::pow(
                        std::max(1.0e-6f, adjustedFrequency / neutralEdge),
                        std::max(0.0f, amount));
                    adjustedFrequency = std::clamp(adjustedFrequency, 10.0f,
                                                    (float)sr * 0.45f);
                }
                const bool linearStereo = linearPhase && !b.dynEnabled
                    && placementMode == 0 && std::abs(placement) < 0.0001f;
                b.routeEffectWeight = 1.0f;
                b.beginBlock(sr, effectiveEnabled,
                             linearStereo ? Biquad::Type::Bell : parameterType,
                             adjustedFrequency, effectiveQ, linearStereo ? 0.0f : bandGain,
                             linearStereo ? 12.0f : p.slope,
                             placementMode == 1, placement, decramp);
            }

            if (effectiveEnabled)
            {
                if(placementMode==2)
                {
                    tsActiveBands[(size_t)tsActiveBandCount++]=i;
                    const bool bandDetectorNeeded = b.dynEnabled || i == meterBand;
                    tsDetectorNeeded = tsDetectorNeeded || bandDetectorNeeded;
                    tsExternalDetectorNeeded = tsExternalDetectorNeeded
                        || (bandDetectorNeeded && b.useExternalSidechain);
                    const auto configureBranch=[&](EQBand& branch,float weight)
                    {
                        branch.parameterType=b.parameterType; branch.gainScale=b.gainScale;
                        branch.globalAmount=b.globalAmount; branch.driveGlobalAmount=b.driveGlobalAmount;
                        branch.routeEffectWeight=weight; branch.driveAmount=b.driveAmount;
                        branch.driveCharacter=b.driveCharacter; branch.driveSecondary=b.driveSecondary;
                        branch.driveAutoGainLinear=b.driveAutoGainLinear; branch.satType=b.satType;
                        branch.dynEnabled=b.dynEnabled; branch.dynUpward=b.dynUpward;
                        branch.useExternalSidechain=b.useExternalSidechain; branch.dynRangeDb=b.dynRangeDb;
                        branch.dynThreshDb=b.dynThreshDb; branch.dynRatio=b.dynRatio;
                        branch.dynAttackMs=b.dynAttackMs; branch.dynReleaseMs=b.dynReleaseMs;
                        branch.beginBlock(sr,true,b.type,b.targetFreqHz,b.targetQ,b.targetGainDb,
                                          b.targetSlopeDbPerOct,false,0.0f,decramp);
                        if (branch.driveActive())
                            branch.prepareDriveRate(configuredOS != nullptr ? effectiveSR : sr);
                    };
                    auto& transientBand = transientBands[(size_t)i];
                    auto& sustainBand = sustainBands[(size_t)i];
                    if (configurationDirty)
                    {
                        configureBranch(transientBand,std::clamp(1.0f-placement,0.0f,1.0f));
                        configureBranch(sustainBand,std::clamp(1.0f+placement,0.0f,1.0f));
                    }
                    tsDriveActive = tsDriveActive || transientBand.driveActive()
                        || sustainBand.driveActive();
                }
                else
                {
                    activeBands[(size_t)activeBandCount++] = i;
                    if (b.driveActive())
                    {
                        drivenBands[(size_t)drivenBandCount++] = i;
                        if (configurationDirty)
                            b.prepareDriveRate(configuredOS != nullptr ? effectiveSR : sr);
                    }
                }
            }
        }
    }
    tsDetectorNeeded = false;
    tsExternalDetectorNeeded = false;
    for (int active = 0; active < tsActiveBandCount; ++active)
    {
        const auto index = (size_t)tsActiveBands[(size_t)active];
        const bool detectorNeeded = transientBands[index].dynEnabled
            || (int)index == meterBand;
        tsDetectorNeeded = tsDetectorNeeded || detectorNeeded;
        tsExternalDetectorNeeded = tsExternalDetectorNeeded
            || (detectorNeeded && transientBands[index].useExternalSidechain);
    }
    if (shouldAnalyze || meterBand >= 0)
    {
        for (int active = 0; active < activeBandCount; ++active)
            bands[(size_t)activeBands[(size_t)active]].beginMeterBlock();
        for (int active = 0; active < tsActiveBandCount; ++active)
        {
            const auto index = (size_t)tsActiveBands[(size_t)active];
            transientBands[index].beginMeterBlock();
            sustainBands[index].beginMeterBlock();
        }
    }
    auto* const activeOS = drivenBandCount > 0 ? configuredOS : nullptr;

    auto* L = mainBuffer.getWritePointer(0);
    auto* R = mainChannels > 1 ? mainBuffer.getWritePointer(1) : L;
    // Push pre-EQ samples to spectrum FIFO
    if (shouldAnalyze) preSpectrumFifo.pushBlock(L, R, n);

    if(tsActiveBandCount>0)
    {
        const float splitStrength=transientStrengthParam->load(std::memory_order_relaxed);
        const float splitBalance=transientBalanceParam->load(std::memory_order_relaxed);
        const float splitHold=transientHoldParam->load(std::memory_order_relaxed);
        const float splitSmooth=transientSmoothParam->load(std::memory_order_relaxed);
        const auto configureSplitter=[&](TransientSplitter& splitter)
        {
            splitter.setParameters(splitStrength,splitBalance,splitHold,splitSmooth);
        };
        if(!transientRoutingWasActive)
        {
            transientSplitter.reset();
            internalDetectorTransientSplitter.reset();
            internalTSDetectorDelayBuffer.clear();
            tsDetectorLookaheadWritePosition=0;
            externalTSDetectorWasActive=false;
            externalTSDetectorWarmupSamplesRemaining=0;
            externalTSDetectorMix.setCurrentAndTargetValue(0.0f);
            transientRoutingWasActive=true;
        }
        if(tsExternalDetectorNeeded&&!externalTSDetectorWasActive)
        {
            // The external splitter is intentionally cold while no T/S band
            // listens to EX SC. Prime its FFT latency before crossfading the
            // detector from the already-valid internal split.
            externalDetectorTransientSplitter.reset();
            externalDetectorTransientBuffer.clear();
            externalDetectorSustainBuffer.clear();
            externalTSDetectorDelayBuffer.clear();
            externalTSDetectorWarmupSamplesRemaining=externalDetectorTransientSplitter.latency();
            externalTSDetectorMix.setCurrentAndTargetValue(0.0f);
            externalTSDetectorWasActive=true;
        }
        else if(!tsExternalDetectorNeeded&&externalTSDetectorWasActive)
        {
            externalTSDetectorWasActive=false;
            externalTSDetectorWarmupSamplesRemaining=0;
            externalTSDetectorMix.setCurrentAndTargetValue(0.0f);
        }
        configureSplitter(transientSplitter);
        transientSplitter.process(mainBuffer,transientBuffer,sustainBuffer,n);
        if (tsDetectorNeeded)
        {
            configureSplitter(internalDetectorTransientSplitter);
            internalDetectorTransientSplitter.process(detectorInputBuffer,internalDetectorTransientBuffer,
                                                       internalDetectorSustainBuffer,n);
        }
        if(tsExternalDetectorNeeded)
        {
            configureSplitter(externalDetectorTransientSplitter);
            externalDetectorTransientSplitter.process(externalDetectorInputBuffer,
                                                       externalDetectorTransientBuffer,
                                                       externalDetectorSustainBuffer,n);
        }
        auto* tL=transientBuffer.getWritePointer(0); auto* tR=transientBuffer.getWritePointer(1);
        auto* sL=sustainBuffer.getWritePointer(0); auto* sR=sustainBuffer.getWritePointer(1);
        const int detectorRingSize=internalTSDetectorDelayBuffer.getNumSamples();
        for(int sample=0;sample<n;++sample)
        {
            if(tsExternalDetectorNeeded&&externalTSDetectorWarmupSamplesRemaining>0
                &&--externalTSDetectorWarmupSamplesRemaining==0)
                externalTSDetectorMix.setTargetValue(1.0f);
            const float externalMix=tsExternalDetectorNeeded
                ?externalTSDetectorMix.getNextValue():0.0f;
            float tl=tL[sample],tr=tR[sample],sl=sL[sample],srSample=sR[sample];
            for(int a=0;a<tsActiveBandCount;++a)
            {
                const int index=tsActiveBands[(size_t)a]; auto& tb=transientBands[(size_t)index]; auto& sb=sustainBands[(size_t)index];
                const bool updateDetector = tb.dynEnabled || index == meterBand;
                const int detectorDelay=lookahead-requestedBandLookaheadSamples(index,lookahead);
                int read=tsDetectorLookaheadWritePosition-detectorDelay;
                if(read<0)read+=detectorRingSize;
                const auto detectorSample=[&](bool external,int channel,const juce::AudioBuffer<float>& current)
                {
                    const auto& ring=external?externalTSDetectorDelayBuffer:internalTSDetectorDelayBuffer;
                    return detectorDelay==0?current.getSample(channel&1,sample):ring.getSample(channel,read);
                };
                if (updateDetector)
                {
                    const float internalTL=detectorSample(false,0,internalDetectorTransientBuffer);
                    const float internalTR=detectorSample(false,1,internalDetectorTransientBuffer);
                    const float internalSL=detectorSample(false,2,internalDetectorSustainBuffer);
                    const float internalSR=detectorSample(false,3,internalDetectorSustainBuffer);
                    const float mix=tb.useExternalSidechain?externalMix:0.0f;
                    const float detectorTL=juce::jmap(mix,internalTL,
                        detectorSample(true,0,externalDetectorTransientBuffer));
                    const float detectorTR=juce::jmap(mix,internalTR,
                        detectorSample(true,1,externalDetectorTransientBuffer));
                    const float detectorSL=juce::jmap(mix,internalSL,
                        detectorSample(true,2,externalDetectorSustainBuffer));
                    const float detectorSR=juce::jmap(mix,internalSR,
                        detectorSample(true,3,externalDetectorSustainBuffer));
                    tb.updateDynamicEnvelope(detectorTL,detectorTR,sr);
                    sb.updateDynamicEnvelope(detectorSL,detectorSR,sr);
                }
                tb.maybeUpdateCoeffs(sr); sb.maybeUpdateCoeffs(sr);
                if(soloedBand<0){tb.processEqualizer(tl,tr);sb.processEqualizer(sl,srSample);}
            }
            tL[sample]=tl; tR[sample]=tr; sL[sample]=sl; sR[sample]=srSample;
            if (tsDetectorNeeded)
            for(int channel=0;channel<2;++channel)
            {
                internalTSDetectorDelayBuffer.setSample(channel,tsDetectorLookaheadWritePosition,
                    internalDetectorTransientBuffer.getSample(channel,sample));
                internalTSDetectorDelayBuffer.setSample(channel+2,tsDetectorLookaheadWritePosition,
                    internalDetectorSustainBuffer.getSample(channel,sample));
                if(tsExternalDetectorNeeded)
                {
                    externalTSDetectorDelayBuffer.setSample(channel,tsDetectorLookaheadWritePosition,
                        externalDetectorTransientBuffer.getSample(channel,sample));
                    externalTSDetectorDelayBuffer.setSample(channel+2,tsDetectorLookaheadWritePosition,
                        externalDetectorSustainBuffer.getSample(channel,sample));
                }
            }
            if (tsDetectorNeeded && ++tsDetectorLookaheadWritePosition>=detectorRingSize)
                tsDetectorLookaheadWritePosition=0;
        }

        if(soloedBand<0&&tsDriveActive)
        {
            const auto processBranchDrive=[&](juce::AudioBuffer<float>& branch,
                                              std::array<EQBand,kNumBands>& branchBands,
                                              juce::dsp::Oversampling<float>* os)
            {
                const auto driveSamples=[&](float* left,float* right,int samples)
                {
                    for(int sample=0;sample<samples;++sample)
                        for(int a=0;a<tsActiveBandCount;++a)
                            branchBands[(size_t)tsActiveBands[(size_t)a]].processDrive(left[sample],right[sample]);
                };
                if(os!=nullptr)
                {
                    juce::dsp::AudioBlock<float> block(branch);
                    auto osBlock=os->processSamplesUp(block);
                    driveSamples(osBlock.getChannelPointer(0),osBlock.getChannelPointer(1),(int)osBlock.getNumSamples());
                    os->processSamplesDown(block);
                }
                else driveSamples(branch.getWritePointer(0),branch.getWritePointer(1),n);
            };
            auto* transientOS=configuredOS!=nullptr
                ?transientOversamplers[(size_t)(currentOversamplingOrder-1)].get():nullptr;
            auto* sustainOS=configuredOS!=nullptr
                ?sustainOversamplers[(size_t)(currentOversamplingOrder-1)].get():nullptr;
            processBranchDrive(transientBuffer,transientBands,transientOS);
            processBranchDrive(sustainBuffer,sustainBands,sustainOS);
        }
        for(int sample=0;sample<n;++sample)
        {
            L[sample]=tL[sample]+sL[sample];
            if(mainChannels>1)R[sample]=tR[sample]+sR[sample];
        }
    }
    else
    {
        transientRoutingWasActive=false;
        externalTSDetectorWasActive=false;
        externalTSDetectorWarmupSamplesRemaining=0;
        externalTSDetectorMix.setCurrentAndTargetValue(0.0f);
    }

    // Static and dynamic filtering stay at base rate. Oversampling is reserved
    // for the nonlinear per-band drive section; de-cramping handles the linear
    // response near Nyquist without multiplying the complete EQ workload.
    auto processEQ = [&](float* left, float* right, int numSamples, double processSR)
    {
        bool allStatic = soloedBand < 0;
        for (int active = 0; active < activeBandCount && allStatic; ++active)
            allStatic = bands[(size_t)activeBands[(size_t)active]].canUseStaticKernel();
        if (allStatic)
        {
            // The static audio kernel skips the per-sample loop, but a selected
            // non-dynamic band must still feed the editor's detector meter.
            for (int active = 0; active < activeBandCount; ++active)
            {
                const int index = activeBands[(size_t)active];
                if (index != meterBand)
                    continue;

                auto& band = bands[(size_t)index];
                const auto& detector = band.useExternalSidechain
                    ? (lookahead > 0 ? externalBandDetectorBuffers[(size_t)index]
                                     : externalDetectorInputBuffer)
                    : (lookahead > 0 ? internalBandDetectorBuffers[(size_t)index]
                                     : detectorInputBuffer);
                for (int sample = 0; sample < numSamples; ++sample)
                    band.updateDynamicEnvelope(detector.getSample(0, sample),
                                               detector.getSample(1, sample), processSR);
                break;
            }

            auto* scratchFirst = staticRoutingScratch.getWritePointer(0);
            auto* scratchSecond = staticRoutingScratch.getWritePointer(1);
            for (int active = 0; active < activeBandCount; ++active)
                bands[(size_t)activeBands[(size_t)active]].processEqualizerStaticBlock(
                    left, right, scratchFirst, scratchSecond, numSamples);
            return;
        }

        if (soloedBand < 0)
        {
            auto* scratchFirst = staticRoutingScratch.getWritePointer(0);
            auto* scratchSecond = staticRoutingScratch.getWritePointer(1);
            for (int active = 0; active < activeBandCount; ++active)
            {
                const int index = activeBands[(size_t)active];
                auto& band = bands[(size_t)index];
                const bool updateDetector = band.dynEnabled || index == meterBand;
                const auto& detector = band.useExternalSidechain
                    ? (lookahead > 0 ? externalBandDetectorBuffers[(size_t)index]
                                     : externalDetectorInputBuffer)
                    : (lookahead > 0 ? internalBandDetectorBuffers[(size_t)index]
                                     : detectorInputBuffer);

                if (band.canUseStaticKernel())
                {
                    if (updateDetector)
                        for (int sample = 0; sample < numSamples; ++sample)
                            band.updateDynamicEnvelope(detector.getSample(0, sample),
                                                       detector.getSample(1, sample), processSR);
                    band.processEqualizerStaticBlock(left, right, scratchFirst, scratchSecond,
                                                     numSamples);
                    continue;
                }

                for (int sample = 0; sample < numSamples; ++sample)
                {
                    if (updateDetector)
                        band.updateDynamicEnvelope(detector.getSample(0, sample),
                                                   detector.getSample(1, sample), processSR);
                    band.maybeUpdateCoeffs(processSR);
                    band.processEqualizer(left[sample], right[sample]);
                }
            }
            return;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            float l = left[i];
            float r = right[i];

            for (int active = 0; active < activeBandCount; ++active)
            {
                const int index=activeBands[(size_t)active];
                auto& b = bands[(size_t)index];
                if (b.dynEnabled || index == meterBand)
                {
                    const auto& detector = b.useExternalSidechain
                        ? (lookahead > 0 ? externalBandDetectorBuffers[(size_t)index]
                                         : externalDetectorInputBuffer)
                        : (lookahead > 0 ? internalBandDetectorBuffers[(size_t)index]
                                         : detectorInputBuffer);
                    b.updateDynamicEnvelope(detector.getSample(0, i), detector.getSample(1, i),
                                            processSR);
                }
                b.maybeUpdateCoeffs(processSR);
                if (soloedBand < 0)
                    b.processEqualizer(l, r);
            }

            if (soloedBand >= 0 && soloedBand < kNumBands)
                bands[(size_t) soloedBand].processAudition(l, r);

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

    if (anyDynamicBand || configurationDirtyBands != 0u)
        for (int i = 0; i < kNumBands; ++i)
        {
            const bool ts=bandSnapshots[(size_t)i].placementMode==2;
            const float placement=bandSnapshots[(size_t)i].placement*0.01f;
            const float transientWeight=std::clamp(1.0f-placement,0.0f,1.0f);
            const float sustainWeight=std::clamp(1.0f+placement,0.0f,1.0f);
            const float weightSum=std::max(1.0f,transientWeight+sustainWeight);
            const float dynamicGain=ts ? (transientWeight*transientBands[(size_t)i].dynGainMod
                                          +sustainWeight*sustainBands[(size_t)i].dynGainMod)/weightSum
                                       : bands[(size_t)i].dynGainMod;
            bandDynamicGainDb[(size_t)i].store(dynamicGain,std::memory_order_relaxed);
        }

    if (shouldAnalyze || meterBand >= 0)
        for (int i = 0; i < kNumBands; ++i)
        {
            const bool ts=bandSnapshots[(size_t)i].placementMode==2;
            const float placement=bandSnapshots[(size_t)i].placement*0.01f;
            const float transientWeight=std::clamp(1.0f-placement,0.0f,1.0f);
            const float sustainWeight=std::clamp(1.0f+placement,0.0f,1.0f);
            const float detectorPeakL=ts ? std::max(transientWeight*transientBands[(size_t)i].detectorPeakL,
                                                    sustainWeight*sustainBands[(size_t)i].detectorPeakL)
                                         : bands[(size_t)i].detectorPeakL;
            const float detectorPeakR=ts ? std::max(transientWeight*transientBands[(size_t)i].detectorPeakR,
                                                    sustainWeight*sustainBands[(size_t)i].detectorPeakR)
                                         : bands[(size_t)i].detectorPeakR;
            bandDetectorLevelDbL[(size_t)i].store(juce::Decibels::gainToDecibels(detectorPeakL,-60.0f),std::memory_order_relaxed);
            bandDetectorLevelDbR[(size_t)i].store(juce::Decibels::gainToDecibels(detectorPeakR,-60.0f),std::memory_order_relaxed);
        }

    // Regular evaluates the combined complex response of the current EQ
    // routing. It performs no signal measurement and recomputes only after a
    // relevant parameter change.
    float compensation = 1.0f;
    if (autoGainMode == 1)
    {
        if (regularAutoGainDirty.exchange(false, std::memory_order_acq_rel))
        {
            std::array<deq::eq_auto_gain::BandParameters, kNumBands> responseBands {};
            for (int index = 0; index < kNumBands; ++index)
            {
                const auto& p = bandParams[(size_t)index];
                auto& band = responseBands[(size_t)index];
                band.enabled = p.present->load(std::memory_order_relaxed) > 0.5f
                    && p.on->load(std::memory_order_relaxed) > 0.5f;
                band.type = std::clamp((int)p.type->load(std::memory_order_relaxed),
                                       0, deq::filter_types::count - 1);
                band.frequencyHz = p.freq->load(std::memory_order_relaxed);
                band.q = p.q->load(std::memory_order_relaxed);
                band.gainDb = p.gain->load(std::memory_order_relaxed);
                band.slopeDbPerOct = p.slope->load(std::memory_order_relaxed);
                band.placementMode = std::clamp(
                    (int)p.placementMode->load(std::memory_order_relaxed), 0, 2);
                band.placementPercent = p.placement->load(std::memory_order_relaxed);
            }
            regularTargetCompDb = deq::eq_auto_gain::combinedResponseCompensationDb(
                responseBands, sr, amount, shiftSemitones, adaptiveQ);
            regularCompensationLinear = std::pow(10.0f, regularTargetCompDb / 20.0f);
        }
        compensation = regularCompensationLinear;
        if (autoGainModeChanged)
        {
            smartAutoGainLocked.store(false, std::memory_order_relaxed);
            smartAutoGainProgress.store(0.0f, std::memory_order_relaxed);
        }
    }
    else if (autoGainMode == 2)
    {
        const float previous = autoGainCompDb.load(std::memory_order_relaxed);
        const float alpha = 1.0f - std::exp(-(float)n / (float)(sr * 0.05));
        const float current = previous + alpha * (smartTargetCompDb - previous);
        autoGainCompDb.store(current, std::memory_order_relaxed);
        compensation = std::pow(10.0f, current / 20.0f);
    }
    else
    {
        if (autoGainModeChanged)
        {
            regularTargetCompDb = smartTargetCompDb = 0.0f;
            regularAutoGainDirty.store(true, std::memory_order_release);
            smartInputLoudness.reset();
            smartOutputLoudness.reset();
            smartWarmupSamplesRemaining = 0;
            smartLoudnessRevision = 0;
            smartLoudnessUpdateCount = 0;
            smartAutoGainLocked.store(false, std::memory_order_relaxed);
            smartAutoGainProgress.store(0.0f, std::memory_order_relaxed);
        }
    }

    autoGainLinearSm.setTargetValue(compensation);
    const bool unityOutputPath = outputGain == 1.0f
        && (autoGainMode != 2
            || smartAutoGainLocked.load(std::memory_order_acquire))
        && !autoGainLinearSm.isSmoothing()
        && autoGainLinearSm.getCurrentValue() == 1.0f
        && autoGainLinearSm.getTargetValue() == 1.0f
        && !latencyTransitionGain.isSmoothing()
        && latencyTransitionGain.getCurrentValue() == 1.0f
        && latencyTransitionGain.getTargetValue() == 1.0f;
    if (unityOutputPath)
    {
        sanitizeAndClamp(L, n, -8.0f, 8.0f);
        if (mainChannels > 1)
            sanitizeAndClamp(R, n, -8.0f, 8.0f);
    }
    else if ((autoGainMode != 2
              || smartAutoGainLocked.load(std::memory_order_acquire))
             && !autoGainLinearSm.isSmoothing()
             && !latencyTransitionGain.isSmoothing()
             && latencyTransitionGain.getCurrentValue() == 1.0f
             && latencyTransitionGain.getTargetValue() == 1.0f)
    {
        const float combinedGain = outputGain * autoGainLinearSm.getCurrentValue();
        scaleSanitizeAndClamp(L, n, combinedGain, -8.0f, 8.0f);
        if (mainChannels > 1)
            scaleSanitizeAndClamp(R, n, combinedGain, -8.0f, 8.0f);
    }
    else
        for (int i = 0; i < n; ++i)
        {
            const float sourceL = L[i];
            const float sourceR = R[i];
            if (autoGainMode == 2
                && !smartAutoGainLocked.load(std::memory_order_acquire))
                smartOutputLoudness.pushSample(sourceL, sourceR);

            const float combinedGain = outputGain * autoGainLinearSm.getNextValue();
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
    autoGainCompDb.store(juce::Decibels::gainToDecibels(
                             std::max(1.0e-7f, autoGainLinearSm.getCurrentValue())),
                         std::memory_order_relaxed);

    if (autoGainMode == 2 && smartWarmupSamplesRemaining > 0)
    {
        smartWarmupSamplesRemaining = std::max<int64_t>(0, smartWarmupSamplesRemaining - n);
        smartInputLoudness.clearWindow();
        smartOutputLoudness.clearWindow();
        smartAutoGainProgress.store(0.0f, std::memory_order_relaxed);
    }
    else if (autoGainMode == 2)
    {
        // Establish the first LUFS result after 400 ms, refine it three times
        // at 100 ms intervals, then stop measuring until audible state changes.
        constexpr int smartTargetUpdates = 4;
        constexpr double smartWindowSeconds = 0.4;
        const int64_t windowFrames = (int64_t)(sr * smartWindowSeconds);
        const float initialObservationProgress = std::min(
            1.0f, (float)smartInputLoudness.getFrameCount()
                / (float)std::max<int64_t>(1, windowFrames));
        smartAutoGainProgress.store(
            ((float)smartLoudnessUpdateCount + initialObservationProgress)
                / (float)smartTargetUpdates,
            std::memory_order_relaxed);
        const int64_t completedRevision = std::min(
            smartInputLoudness.getCompletedSegmentCount(),
            smartOutputLoudness.getCompletedSegmentCount());
        if (smartInputLoudness.hasCompleteWindow()
            && smartOutputLoudness.hasCompleteWindow()
            && completedRevision > smartLoudnessRevision)
        {
            smartLoudnessRevision = completedRevision;
            // Each refinement uses all samples collected in this one finite
            // observation, avoiding independent momentary-window jumps.
            const float inputLufs = smartInputLoudness.getIntegratedLoudnessLufs();
            const float outputLufs = smartOutputLoudness.getIntegratedLoudnessLufs();
            if (std::isfinite(inputLufs) && std::isfinite(outputLufs)
                && inputLufs > -70.0f)
                smartTargetCompDb = std::clamp(inputLufs - outputLufs,
                                               -36.0f, 36.0f);

            // Silence or another unusable LUFS result must not leave the
            // analyser running forever. It consumes the same finite revision
            // while retaining the last valid compensation.
            ++smartLoudnessUpdateCount;
            if (smartLoudnessUpdateCount >= smartTargetUpdates)
            {
                smartAutoGainLocked.store(true, std::memory_order_release);
                smartAutoGainProgress.store(1.0f, std::memory_order_release);
            }
            else
                smartAutoGainProgress.store(
                    (float)smartLoudnessUpdateCount / (float)smartTargetUpdates,
                    std::memory_order_release);
        }
    }

    // Push post-EQ samples to spectrum FIFO
    if (shouldAnalyze) spectrumFifo.pushBlock(L, R, n);

    if (!transparentBypassPath)
        globalBypass.processOutput(mainBuffer, getLatencySamples(), pluginEnabled);
}


double DefaultEqualizerAudioProcessor::getTailLengthSeconds() const
{
    // Only the optional linear-phase FIR can outlive the input boundary.
    if (sr <= 0.0)
        return 0.0;

    const bool linPhase    = apvts.getRawParameterValue("linear_phase")->load() > 0.5f;
    return linPhase ? (double)(currentLinearPhaseLatency() * 2) / sr : 0.0;
}

juce::AudioProcessorEditor* DefaultEqualizerAudioProcessor::createEditor()
{
    return new DefaultEqualizerAudioProcessorEditor(*this);
}


// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DefaultEqualizerAudioProcessor();
}
