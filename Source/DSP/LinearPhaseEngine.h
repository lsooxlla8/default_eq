#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Biquad.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>

// Composite linear-phase FIR followed by JUCE's zero-additional-latency,
// uniformly partitioned convolution engine. The previous implementation did
// one 8192-point forward and inverse FFT for every host block, so a 64-sample
// block was disproportionately expensive. Partitioning makes the workload
// depend on the signal duration and FIR length rather than host block count.
class LinearPhaseEngine
{
public:
    static constexpr int firLength = 4096;
    static constexpr int fftOrder = 13;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int latency = firLength / 2;

    LinearPhaseEngine() = default;
    ~LinearPhaseEngine()
    {
        delete pendingImpulse.exchange(nullptr, std::memory_order_acq_rel);
        drainRetiredImpulses();
    }

    void prepare(double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)std::max(1, maxBlockSize), 2 };
        convolution.prepare(spec);
        convolution.reset();
        delete pendingImpulse.exchange(nullptr, std::memory_order_acq_rel);
        drainRetiredImpulses();
        kernelHasBeenLoaded.store(false, std::memory_order_release);
        needsRebuild.store(true, std::memory_order_release);
    }

    void reset() { convolution.reset(); }

    // prepareToPlay-only: loading before the final prepare makes JUCE finish
    // building the initial partition set synchronously, so the first processed
    // impulse cannot leak through while the internal queue catches up.
    void activatePendingBeforeProcessing(int maxBlockSize)
    {
        if (auto* pending = pendingImpulse.exchange(nullptr, std::memory_order_acq_rel))
        {
            convolution.loadImpulseResponse(std::move(*pending), sr,
                juce::dsp::Convolution::Stereo::no,
                juce::dsp::Convolution::Trim::no,
                juce::dsp::Convolution::Normalise::no);
            delete pending;
            juce::dsp::ProcessSpec spec { sr, (juce::uint32)std::max(1, maxBlockSize), 2 };
            convolution.prepare(spec);
            kernelHasBeenLoaded.store(true, std::memory_order_release);
        }
    }

    // Background-thread only. Builds a symmetric time-domain FIR and publishes
    // ownership through a single atomic pointer. Superseded unpublished kernels
    // are deleted here, never on the audio thread.
    void rebuildFromMagnitude(const float* magnitudeDb, int numBins,
                              int requestedFirLength = firLength)
    {
        drainRetiredImpulses();
        const int activeLength = std::clamp(requestedFirLength, 1024, firLength);
        std::fill(rebuildFrequency.begin(), rebuildFrequency.end(), 0.0f);
        rebuildFrequency[0] = std::pow(10.0f, magnitudeDb[0] / 20.0f);
        for (int bin = 1; bin < fftSize / 2; ++bin)
        {
            const float fraction = (float)bin / (float)(fftSize / 2);
            const int source = std::min((int)(fraction * (float)(numBins - 1)), numBins - 1);
            rebuildFrequency[(size_t)(bin * 2)] = std::pow(10.0f, magnitudeDb[source] / 20.0f);
        }
        rebuildFrequency[1] = std::pow(10.0f, magnitudeDb[numBins - 1] / 20.0f);
        rebuildFft.performRealOnlyInverseTransform(rebuildFrequency.data());

        auto impulse = std::make_unique<juce::AudioBuffer<float>>(1, activeLength);
        auto* destination = impulse->getWritePointer(0);
        for (int tap = 0; tap < activeLength; ++tap)
        {
            const int source = (tap - activeLength / 2 + fftSize) % fftSize;
            const float window = 0.5f * (1.0f - std::cos(
                2.0f * (float)kPi * (float)tap / (float)(activeLength - 1)));
            destination[tap] = rebuildFrequency[(size_t)source] * window;
        }

        auto* superseded = pendingImpulse.exchange(impulse.release(), std::memory_order_acq_rel);
        delete superseded;
        needsRebuild.store(false, std::memory_order_release);
    }

    void processBlock(float* left, float* right, int numSamples)
    {
        // JUCE requires load/process calls to be serialised, but its load is
        // explicitly wait-free. The worker allocates the buffer; audio merely
        // transfers ownership and retires the now-empty wrapper for deletion
        // back on the worker.
        if (retirementQueueHasSpace())
        {
            if (auto* pending = pendingImpulse.exchange(nullptr, std::memory_order_acq_rel))
            {
                convolution.loadImpulseResponse(std::move(*pending), sr,
                    juce::dsp::Convolution::Stereo::no,
                    juce::dsp::Convolution::Trim::no,
                    juce::dsp::Convolution::Normalise::no);
                retireImpulseFromAudio(pending);
                kernelHasBeenLoaded.store(true, std::memory_order_release);
            }
        }
        if (!kernelHasBeenLoaded.load(std::memory_order_acquire)) return;

        float* channels[] { left, right };
        const size_t channelCount = left == right ? 1u : 2u;
        juce::dsp::AudioBlock<float> block(channels, channelCount, (size_t)numSamples);
        juce::dsp::ProcessContextReplacing<float> context(block);
        convolution.process(context);
    }

    bool getNeedsRebuild() const { return needsRebuild.load(std::memory_order_acquire); }
    bool isKernelReady() const
    {
        return kernelHasBeenLoaded.load(std::memory_order_acquire)
            || pendingImpulse.load(std::memory_order_acquire) != nullptr;
    }

private:
    static constexpr unsigned retiredQueueSize = 8;

    bool retirementQueueHasSpace() const noexcept
    {
        const auto write = retiredWrite.load(std::memory_order_relaxed);
        const auto next = (write + 1u) % retiredQueueSize;
        return next != retiredRead.load(std::memory_order_acquire);
    }

    void retireImpulseFromAudio(juce::AudioBuffer<float>* impulse) noexcept
    {
        const auto write = retiredWrite.load(std::memory_order_relaxed);
        const auto next = (write + 1u) % retiredQueueSize;
        jassert(next != retiredRead.load(std::memory_order_acquire));
        retiredImpulses[write] = impulse;
        retiredWrite.store(next, std::memory_order_release);
    }

    void drainRetiredImpulses() noexcept
    {
        auto read = retiredRead.load(std::memory_order_relaxed);
        const auto write = retiredWrite.load(std::memory_order_acquire);
        while (read != write)
        {
            delete retiredImpulses[read];
            retiredImpulses[read] = nullptr;
            read = (read + 1u) % retiredQueueSize;
        }
        retiredRead.store(read, std::memory_order_release);
    }

    juce::dsp::FFT rebuildFft { fftOrder };
    juce::dsp::Convolution convolution;
    std::array<float, fftSize * 2> rebuildFrequency {};
    std::atomic<juce::AudioBuffer<float>*> pendingImpulse { nullptr };
    std::array<juce::AudioBuffer<float>*, retiredQueueSize> retiredImpulses {};
    std::atomic<unsigned> retiredWrite { 0 };
    std::atomic<unsigned> retiredRead { 0 };
    std::atomic<bool> kernelHasBeenLoaded { false };
    std::atomic<bool> needsRebuild { true };
    double sr = 44100.0;
};
