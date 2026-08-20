#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Config.h"
#include "DSP/EQBand.h"
#include "DSP/SpectrumFIFO.h"
#include "DSP/LinearPhaseEngine.h"
#include "DSP/MatchEQ.h"
#include "DSP/GlobalBypass.h"
#include <array>
#include <atomic>
#include <memory>

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

    // Match EQ (public for UI capture/match control)
    MatchEQ matchEQ;
    std::atomic<bool> matchUseSidechain { false }; // transient editor choice; never serialized

    // ── Auto-gain bypass ───────────────────────────────────────────
    std::atomic<float> autoGainCompDb { 0.0f }; // smoothed compensation in dB
    std::atomic<int> sidechainAuditionBand { -1 }; // transient UI action; never serialized
    std::atomic<int> soloBand { -1 }; // transient UI action; never serialized
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
        std::atomic<float>* dynAttack = nullptr; std::atomic<float>* dynRelease = nullptr;
    };
    std::array<BandParameterPointers, kNumBands> bandParams {};
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* amountParam = nullptr;
    std::atomic<float>* adaptiveQParam = nullptr;
    std::atomic<float>* linearPhaseParam = nullptr;
    std::atomic<float>* linearQualityParam = nullptr;
    std::atomic<float>* oversamplingParam = nullptr;
    std::atomic<float>* autoGainModeParam = nullptr;
    std::atomic<float>* pluginEnabledParam = nullptr;
    void cacheParameterPointers();
    std::atomic<bool> analyzerEnabled { false };
    bool transportWasPlaying = false; // audio-thread owned

    std::array<EQBand, kNumBands> bands;
    GlobalBypass globalBypass;
    double sr = 44100.0;
    int maxBlockSize = 512;
    static constexpr double lookaheadSeconds = 0.005;
    juce::AudioBuffer<float> lookaheadDelayBuffer, detectorInputBuffer;
    int lookaheadWritePosition = 0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lookaheadMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> latencyTransitionGain;
    int processedLatency = 0;
    bool latencyFadingOut = false;
    std::array<std::atomic<float>, kNumBands> bandDynamicGainDb {};
    double smartInputEnergy = 0.0, smartOutputEnergy = 0.0;
    int64_t smartEnergySamples = 0;
    std::uint64_t smartParameterSignature = 0;
    void applyLookaheadDelay(juce::AudioBuffer<float>&, int delaySamples) noexcept;
    int requestedLookaheadSamples() const noexcept;

    // ── Oversampling pool (A1) ────────────────────────────────────
    // All four orders (0=1x, 1=2x, 2=4x, 3=8x) are built in prepareToPlay;
    // processBlock just indexes into the pool. Zero heap allocation on the
    // audio thread when the user changes the oversampling factor.
    // oversamplers[i] covers order (i+1); order 0 (1x) uses the direct path.
    static constexpr int kNumOversamplingOrders = 3;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, kNumOversamplingOrders> oversamplers;
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
