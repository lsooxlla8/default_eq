#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Config.h"
#include "DSP/EQBand.h"
#include "DSP/SpectrumFIFO.h"
#include "DSP/LinearPhaseEngine.h"
#include "DSP/GlobalBypass.h"
#include "DSP/TransientSplitter.h"
#include <array>
#include <atomic>
#include <memory>
#include <utility>

class DefaultEqualizerAudioProcessor : public juce::AudioProcessor,
                               public juce::AudioProcessorValueTreeState::Listener
{
public:
    DefaultEqualizerAudioProcessor();
    ~DefaultEqualizerAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return kProductName; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override;

    // The default_* family deliberately has no factory-program/preset concept.
    // Hosts still persist the complete versioned processor state via
    // getStateInformation()/setStateInformation(). Returning zero prevents AU
    // and VST3 wrappers from advertising a synthetic "Untitled" preset.
    int getNumPrograms() override { return 0; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Undo manager (shared with APVTS)
    juce::UndoManager undoManager { 30000, 30 };
    juce::AudioProcessorValueTreeState apvts;

    // Spectrum analyzer FIFO (post-EQ by default)
    SpectrumFIFO spectrumFifo;
    SpectrumFIFO preSpectrumFifo;
    void setAnalyzerEnabled(bool enabled) noexcept { analyzerEnabled.store(enabled, std::memory_order_release); }

    // Output metering (read from UI thread)
    std::atomic<float> meterPeakL { 0.0f };
    std::atomic<float> meterPeakR { 0.0f };
    std::atomic<float> meterRmsL  { 0.0f };
    std::atomic<float> meterRmsR  { 0.0f };

    // Linear phase engine (public for UI to check state)
    LinearPhaseEngine linearPhaseEngine;

    // ── Auto-gain bypass ───────────────────────────────────────────
    std::atomic<float> autoGainCompDb { 0.0f }; // smoothed compensation in dB
    std::atomic<int> soloBand { -1 }; // transient UI action; never serialized
    std::atomic<int> uiMeterBand { -1 }; // selected editor band; avoids idle detector work
    std::atomic<bool> smartAutoGainLocked { false };
    std::atomic<float> smartAutoGainProgress { 0.0f };
    std::atomic<std::uint64_t> transportStartGeneration { 0 };
    // Restores every parameter owned by one band. New graph nodes always call
    // this before assigning frequency/gain, so a recycled slot cannot inherit
    // deleted dynamic, routing or drive settings.
    void resetBandToDefaults(int zeroBasedBand, bool enable = false,
                             float frequency = -1.0f, float gainDb = 0.0f);
    float getBandDynamicGainDb(int index) const noexcept
    {
        return index >= 0 && index < kNumBands
            ? bandDynamicGainDb[(size_t) index].load(std::memory_order_relaxed) : 0.0f;
    }
    std::pair<float, float> getBandDetectorLevelsDb(int index) const noexcept
    {
        if (index < 0 || index >= kNumBands) return { -60.0f, -60.0f };
        return { bandDetectorLevelDbL[(size_t)index].load(std::memory_order_relaxed),
                 bandDetectorLevelDbR[(size_t)index].load(std::memory_order_relaxed) };
    }
    float getBandDetectorLevelDb(int index) const noexcept
    {
        const auto levels = getBandDetectorLevelsDb(index);
        return std::max(levels.first, levels.second);
    }
    static std::pair<float, float> dynamicsTimingForSpeed(float speed) noexcept;
    static float frequencyShiftRatio(float semitones) noexcept
    { return std::pow(2.0f, semitones / 12.0f); }
    static float shiftedFrequency(float baseFrequency, float semitones) noexcept
    { return baseFrequency * frequencyShiftRatio(semitones); }
    static float calculateAdaptiveQ(float baseQ, float gainDb) noexcept
    {
        return std::clamp(baseQ * (1.0f + std::abs(gainDb) * 0.12f), 0.1f, 24.0f);
    }
    int currentLinearPhaseLatency() const noexcept
    {
        static constexpr int latencies[] { 512, 1024, 2048 };
        const int quality = std::clamp(linearQualityParam != nullptr ? (int)linearQualityParam->load() : 2, 0, 2);
        return latencies[quality];
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();

    struct BandParameterPointers
    {
        std::atomic<float>* present = nullptr; std::atomic<float>* on = nullptr;
        std::atomic<float>* type = nullptr; std::atomic<float>* slope = nullptr;
        std::atomic<float>* placementMode = nullptr; std::atomic<float>* placement = nullptr;
        std::atomic<float>* freq = nullptr; std::atomic<float>* q = nullptr; std::atomic<float>* gain = nullptr;
        std::atomic<float>* drive = nullptr;
        std::atomic<float>* driveCharacter = nullptr; std::atomic<float>* driveSecondary = nullptr;
        std::atomic<float>* satMode = nullptr;
        std::atomic<float>* dynMode = nullptr; std::atomic<float>* scSource = nullptr;
        std::atomic<float>* dynLookahead = nullptr; std::atomic<float>* dynThresh = nullptr;
        std::atomic<float>* dynRange = nullptr; std::atomic<float>* dynRatio = nullptr;
        std::atomic<float>* dynSpeed = nullptr;
    };
    std::array<BandParameterPointers, kNumBands> bandParams {};
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* amountParam = nullptr;
    std::atomic<float>* shiftParam = nullptr;
    std::atomic<float>* adaptiveQParam = nullptr;
    std::atomic<float>* linearPhaseParam = nullptr;
    std::atomic<float>* linearQualityParam = nullptr;
    std::atomic<float>* oversamplingParam = nullptr;
    std::atomic<float>* autoGainModeParam = nullptr;
    std::atomic<float>* pluginEnabledParam = nullptr;
    std::atomic<float>* transientStrengthParam = nullptr;
    std::atomic<float>* transientBalanceParam = nullptr;
    std::atomic<float>* transientHoldParam = nullptr;
    std::atomic<float>* transientSmoothParam = nullptr;
    void cacheParameterPointers();
    std::atomic<bool> analyzerEnabled { false };
    bool transportWasPlaying = false; // audio-thread owned

    std::array<EQBand, kNumBands> bands;
    std::array<EQBand, kNumBands> transientBands, sustainBands;
    TransientSplitter transientSplitter;
    TransientSplitter internalDetectorTransientSplitter, externalDetectorTransientSplitter;
    juce::AudioBuffer<float> transientBuffer, sustainBuffer;
    juce::AudioBuffer<float> internalDetectorTransientBuffer, internalDetectorSustainBuffer;
    juce::AudioBuffer<float> externalDetectorTransientBuffer, externalDetectorSustainBuffer;
    juce::AudioBuffer<float> externalDetectorInputBuffer;
    std::array<juce::AudioBuffer<float>, kNumBands> internalBandDetectorBuffers;
    std::array<juce::AudioBuffer<float>, kNumBands> externalBandDetectorBuffers;
    GlobalBypass globalBypass;
    double sr = 44100.0;
    int maxBlockSize = 512;
    static constexpr double lookaheadSeconds = 0.005;
    juce::AudioBuffer<float> lookaheadDelayBuffer, externalLookaheadDelayBuffer, detectorInputBuffer;
    juce::AudioBuffer<float> internalTSDetectorDelayBuffer, externalTSDetectorDelayBuffer;
    int lookaheadWritePosition = 0;
    int tsDetectorLookaheadWritePosition = 0;
    bool transientRoutingWasActive = false;
    bool externalTSDetectorWasActive = false;
    int externalTSDetectorWarmupSamplesRemaining = 0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> externalTSDetectorMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lookaheadMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> latencyTransitionGain;
    int processedLatency = 0;
    bool latencyFadingOut = false;
    std::array<std::atomic<float>, kNumBands> bandDynamicGainDb {};
    std::array<std::atomic<float>, kNumBands> bandDetectorLevelDbL {}, bandDetectorLevelDbR {};
    std::atomic<bool> regularAutoGainDirty { true };
    float regularTargetCompDb = 0.0f;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> autoGainLinearSm;
    double smartInputEnergy = 0.0, smartOutputEnergy = 0.0;
    float smartInputPeak = 0.0f, smartOutputPeak = 0.0f;
    int64_t smartEnergySamples = 0;
    int64_t smartWarmupSamplesRemaining = 0;
    std::uint64_t smartParameterSignature = 0;
    float smartTargetCompDb = 0.0f;
    juce::AudioBuffer<float> smartReferenceDelayBuffer;
    int smartReferenceDelayPosition = 0;
    void routeLookahead(juce::AudioBuffer<float>&, const float* sidechainL,
                        const float* sidechainR, int maximumDelaySamples) noexcept;
    int requestedLookaheadSamples() const noexcept;
    int requestedBandLookaheadSamples(int index, int maximumDelaySamples) const noexcept;

    // ── Oversampling pool (A1) ────────────────────────────────────
    // All four orders (0=1x, 1=2x, 2=4x, 3=8x) are built in prepareToPlay;
    // processBlock just indexes into the pool. Zero heap allocation on the
    // audio thread when the user changes the oversampling factor.
    // oversamplers[i] covers order (i+1); order 0 (1x) uses the direct path.
    static constexpr int kNumOversamplingOrders = 3;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, kNumOversamplingOrders> oversamplers;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, kNumOversamplingOrders> transientOversamplers;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, kNumOversamplingOrders> sustainOversamplers;
    int currentOversamplingOrder = 0;

    // Pre-allocated buffer for buildLinearPhaseMagnitude (avoids heap alloc on audio thread)
    std::vector<float> linPhaseMagBuf;

    // Linear phase dirty flag — only rebuild the FIR when params actually change
    std::atomic<bool> linPhaseDirty { true };

    // ── Linear-phase rebuild worker (A5) ──────────────────────────
    // buildLinearPhaseMagnitude + rebuildFromMagnitude used to run on the
    // audio thread when linPhaseDirty was seen; it is a ~2k-bin
    // magnitude evaluation plus an 8192-pt FFT per dirty block. Now a
    // dedicated background juce::Thread owns FIR construction; the audio
    // callback adopts ready IR ownership with JUCE's wait-free load and runs
    // zero-additional-latency uniform partitioned convolution.
    class LinPhaseRebuildThread;
    std::unique_ptr<LinPhaseRebuildThread> linPhaseRebuildThread;
    void requestLinearPhaseRebuild();

    void syncBandsFromParams();
    void buildAllOversamplers(double sampleRate, int samplesPerBlock);
    juce::dsp::Oversampling<float>* currentOversamplerPtr() const noexcept;
    void buildLinearPhaseMagnitude();        // runs on background thread only
    void updateReportedLatency() noexcept;

    void parameterChanged(const juce::String& parameterID, float newValue) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DefaultEqualizerAudioProcessor)
};
