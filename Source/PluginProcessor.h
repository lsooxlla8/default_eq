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

    // Output metering (read from UI thread)
    std::atomic<float> meterPeakL { 0.0f };
    std::atomic<float> meterPeakR { 0.0f };
    std::atomic<float> meterRmsL  { 0.0f };
    std::atomic<float> meterRmsR  { 0.0f };

    // Linear phase engine (public for UI to check state)
    LinearPhaseEngine linearPhaseEngine;

    // Match EQ (public for UI capture/match control)
    MatchEQ matchEQ;

    // ── A/B comparison ────────────────────────────────────────────
    juce::ValueTree snapshotA, snapshotB;
    bool isSlotA = true;  // true = editing slot A

    void storeSnapshot(bool slotA)
    {
        auto state = apvts.copyState();
        if (slotA) snapshotA = state;
        else       snapshotB = state;
    }

    void recallSnapshot(bool slotA)
    {
        auto& snap = slotA ? snapshotA : snapshotB;
        if (snap.isValid())
            apvts.replaceState(snap);
    }

    void copySnapshot(bool fromAtoB)
    {
        if (fromAtoB) snapshotB = snapshotA.createCopy();
        else          snapshotA = snapshotB.createCopy();
    }

    // ── Auto-gain bypass ───────────────────────────────────────────
    std::atomic<float> autoGainCompDb { 0.0f }; // smoothed compensation in dB
    std::atomic<int> sidechainAuditionBand { -1 }; // transient UI action; never serialized
    std::atomic<int> soloBand { -1 }; // transient UI action; never serialized
    std::atomic<bool> smartAutoGainLocked { false };
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
        const int quality = std::clamp((int)apvts.getRawParameterValue("linear_quality")->load(), 0, 2);
        return latencies[quality];
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();

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
    // dedicated background juce::Thread owns the rebuild, writes into the
    // engine's inactive kernel buffer, and atomically swaps activeKernelIdx.
    class LinPhaseRebuildThread;
    std::unique_ptr<LinPhaseRebuildThread> linPhaseRebuildThread;
    void requestLinearPhaseRebuild();

    void syncBandsFromParams();
    void buildAllOversamplers(double sampleRate, int samplesPerBlock);
    juce::dsp::Oversampling<float>* currentOversamplerPtr() const noexcept;
    void buildLinearPhaseMagnitude();        // runs on background thread only
    void updateReportedLatency() noexcept;

    // Band linking.
    //
    // parameterChanged can be entered concurrently from more than one thread:
    // hosts may drive automation from their own thread, and pluginval's
    // "Parameter thread safety" test does so deliberately. The propagation guard
    // must therefore be atomic. A plain bool let two threads past the check, and
    // let one thread clear the flag while another was still propagating; the
    // resulting setValueNotifyingHost call re-entered the APVTS/UndoManager
    // write path and glibc aborted with EDEADLK (recursive lock, exit 134).
    // macOS did not diagnose it, so the defect only surfaced on Linux.
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    std::atomic<bool> propagatingLink { false };
    std::atomic<float> lastLinkedFreq[kNumBands] {};
    std::atomic<float> lastLinkedGain[kNumBands] {};
    std::atomic<float> lastLinkedQ[kNumBands] {};
    void initLinkTracking();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DefaultEqualizerAudioProcessor)
};
