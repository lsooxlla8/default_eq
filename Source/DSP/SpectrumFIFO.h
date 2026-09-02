#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

// Lock-free single-producer / single-consumer spectrum FIFO (triple-buffered).
//
// Protocol (classical swap-chain triple buffer):
//
//   - Three contiguous slots {0,1,2} partitioned at all times between:
//       writeSlot  (producer-local index, owned by audio thread)
//       midSlot    (shared atomic, only touched via exchange)
//       readSlot   (consumer-local index, owned by UI thread)
//     These three indices are a permutation of {0,1,2} at every quiescent
//     point, so no two threads ever reference the same slot.
//
//   - Writer wrap:
//       fill slots[writeSlot];
//       writeSlot = midSlot.exchange(writeSlot, release);
//       fresh.store(true, release);
//
//   - Reader consume:
//       if (!fresh.exchange(false, acquire)) return false;
//       readSlot = midSlot.exchange(readSlot, acquire);
//       read slots[readSlot];
//       return true;
//
// Correctness: both sides only ever swap *their own* slot with midSlot via
// a single atomic exchange. Because exchange is atomic, writeSlot / midSlot /
// readSlot remain pairwise distinct. The happens-before edge established by
// the release store to fresh (writer) and acquire load (reader) covers the
// preceding buffer fill. The writer's next wrap cannot touch the reader's
// current slot because that slot is held in readSlot (consumer-local) and is
// not midSlot. If the writer laps the reader (two wraps without a consume),
// the writer simply overwrites the slot it obtained from the last exchange,
// which is the *older* unread frame — the newer frame remains intact in
// midSlot. This is the correct "drop stale frames" semantic for a spectrum
// display.
class SpectrumFIFO
{
public:
    static constexpr int fftOrder  = 13;             // 8192-point maximum FFT
    static constexpr int fftSize   = 1 << fftOrder;
    static constexpr int numBins   = fftSize / 2;
    static constexpr int publishHop = fftSize / 4;
    static constexpr int numSlots  = 3;

    SpectrumFIFO()
        : fftLow(11), fftMedium(12), fftHigh(13)
    {
        for (auto& buf : slots)
            std::fill(buf.begin(), buf.end(), 0.0f);
        std::fill(fftData.begin(),    fftData.end(),    0.0f);
        std::fill(outputMagnitudes.begin(), outputMagnitudes.end(), -100.0f);

        // Initial partition: writer owns 0, midSlot is 1, readSlot is 2.
        // These three are a permutation of {0,1,2} at all times.
        writeSlot = 0;
        midSlot.store(1, std::memory_order_relaxed);
        readSlot  = 2;
        fresh.store(false, std::memory_order_relaxed);
        fifoWriteIndex = 0;
        samplesSincePublish = 0;
        for (int resolution = 0; resolution < 3; ++resolution)
        {
            const int activeSize = 1 << (resolution + 11);
            for (int i = 0; i < activeSize; ++i)
            {
                const float phase = (float)i / (float)(activeSize - 1);
                hannWindows[(size_t)resolution][(size_t)i] = 0.5f * (1.0f - std::cos(
                    juce::MathConstants<float>::twoPi * phase));
            }
            const int bins = activeSize / 2;
            const double octaveWidth = resolution == 0 ? 1.0 / 6.0
                                      : resolution == 1 ? 1.0 / 12.0 : 1.0 / 24.0;
            const double factor = std::pow(2.0, octaveWidth * 0.5);
            for (int i = 0; i < bins; ++i)
            {
                smoothingLo[(size_t)resolution][(size_t)i] = std::clamp(
                    (int)std::floor((double)i / factor), 0, bins - 1);
                smoothingHi[(size_t)resolution][(size_t)i] = std::clamp(
                    (int)std::ceil((double)i * factor) + 1,
                    smoothingLo[(size_t)resolution][(size_t)i] + 1, bins);
            }
        }
    }

    // Reset the FIFO state (call from prepareToPlay or when going offline/online).
    void reset()
    {
        fifoWriteIndex = 0;
        samplesSincePublish = 0;
        writeSlot = 0;
        midSlot.store(1, std::memory_order_relaxed);
        readSlot  = 2;
        fresh.store(false, std::memory_order_relaxed);
        for (auto& buf : slots)
            std::fill(buf.begin(), buf.end(), 0.0f);
        std::fill(outputMagnitudes.begin(), outputMagnitudes.end(), -100.0f);
    }

    // Call from audio thread: push interleaved mono samples.
    void pushSamples(const float* data, int numSamples)
    {
        int idx = fifoWriteIndex;
        for (int i = 0; i < numSamples; ++i)
        {
            capture[(size_t)idx] = data[i];
            if (++idx >= fftSize) idx = 0;
            if (++samplesSincePublish >= publishHop)
            {
                samplesSincePublish = 0;
                snapshotCapture(idx);
                writerFlip();
            }
        }
        fifoWriteIndex = idx;
    }

    // Call from audio thread: push a stereo buffer (sum to mono).
    void pushBlock(const float* L, const float* R, int numSamples)
    {
        int idx = fifoWriteIndex;
        for (int i = 0; i < numSamples; ++i)
        {
            capture[(size_t)idx] = (L[i] + R[i]) * 0.5f;
            if (++idx >= fftSize) idx = 0;
            if (++samplesSincePublish >= publishHop)
            {
                samplesSincePublish = 0;
                snapshotCapture(idx);
                writerFlip();
            }
        }
        fifoWriteIndex = idx;
    }

    // Call from UI thread: returns true if new data was processed.
    bool processIfReady()
    {
        // Atomically consume the "fresh" flag. If it was already false, no
        // new frame has been published since last call.
        if (!fresh.exchange(false, std::memory_order_acquire))
            return false;

        // Swap our private readSlot with midSlot: reader takes what writer
        // published; writer will take reader's old slot on its next flip.
        readSlot = midSlot.exchange(readSlot, std::memory_order_acquire);

        const int resolution = resolutionIndex.load(std::memory_order_relaxed);
        const int activeOrder = resolution == 0 ? 11 : (resolution == 1 ? 12 : 13);
        const int activeSize = 1 << activeOrder;
        currentBins = activeSize / 2;
        const auto& src = slots[(size_t)readSlot];
        std::fill(fftData.begin(), fftData.end(), 0.0f);
        const int sourceOffset = fftSize - activeSize;
        for (int i = 0; i < activeSize; ++i)
        {
            fftData[(size_t)i] = src[(size_t)(sourceOffset + i)]
                * hannWindows[(size_t)resolution][(size_t)i];
        }
        if (activeOrder == 11) fftLow.performFrequencyOnlyForwardTransform(fftData.data());
        else if (activeOrder == 12) fftMedium.performFrequencyOnlyForwardTransform(fftData.data());
        else fftHigh.performFrequencyOnlyForwardTransform(fftData.data());

        // ZLEqualizer-inspired perceptual smoothing: average linear power over
        // a constant fractional-octave width before converting to dB. This
        // avoids the jagged low-resolution trace and the bias caused by
        // averaging already-logarithmic values.
        cumulativePower[0] = 0.0;
        for (int i = 0; i < currentBins; ++i)
        {
            const float normalized = fftData[(size_t)i] * (4.0f / (float)activeSize);
            linearPower[(size_t)i] = normalized * normalized;
            cumulativePower[(size_t)i + 1] = cumulativePower[(size_t)i] + linearPower[(size_t)i];
        }
        for (int i = 0; i < currentBins; ++i)
        {
            const int lo = smoothingLo[(size_t)resolution][(size_t)i];
            const int hi = smoothingHi[(size_t)resolution][(size_t)i];
            const double power = (cumulativePower[(size_t)hi] - cumulativePower[(size_t)lo]) / (double)(hi - lo);
            outputMagnitudes[(size_t)i] = 10.0f * std::log10((float)std::max(power, 1.0e-14));
        }

        return true;
    }

    const float* getMagnitudes() const { return outputMagnitudes.data(); }
    int getNumBins() const { return currentBins; }
    void setResolution(int index) noexcept { resolutionIndex.store(juce::jlimit(0, 2, index), std::memory_order_relaxed); }

private:
    // Writer-only: publish the just-filled writeSlot and take the previous
    // midSlot as the next write target. Called only by the audio thread.
    void writerFlip()
    {
        writeSlot = midSlot.exchange(writeSlot, std::memory_order_release);
        fresh.store(true, std::memory_order_release);
    }

    void snapshotCapture(int oldestSample)
    {
        auto& destination = slots[(size_t) writeSlot];
        const int tailSamples = fftSize - oldestSample;
        std::copy_n(capture.begin() + oldestSample, tailSamples, destination.begin());
        std::copy_n(capture.begin(), oldestSample, destination.begin() + tailSamples);
    }

    juce::dsp::FFT fftLow, fftMedium, fftHigh;

    std::array<std::array<float, fftSize>, numSlots> slots {};
    std::array<float, fftSize>                        capture {};
    std::array<float, fftSize * 2>                   fftData {};
    std::array<float, numBins>                       outputMagnitudes {};
    std::array<float, numBins>                       linearPower {};
    std::array<double, numBins + 1>                  cumulativePower {};
    std::array<std::array<float, fftSize>, 3>        hannWindows {};
    std::array<std::array<int, numBins>, 3>          smoothingLo {}, smoothingHi {};

    int               fifoWriteIndex = 0; // audio-thread only; reset in prepare
    std::atomic<int>  midSlot        { 1 };
    std::atomic<bool> fresh          { false };
    std::atomic<int> resolutionIndex { 2 };
    int samplesSincePublish = 0;
    int currentBins = numBins;

    // Producer-local / consumer-local slot indices. Only touched by their
    // respective owning thread; never read from the other side.
    int writeSlot = 0;   // audio-thread only
    int readSlot  = 2;   // UI-thread only
};
