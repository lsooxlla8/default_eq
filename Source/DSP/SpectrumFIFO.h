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
    static constexpr int fftOrder  = 12;             // 4096-point FFT
    static constexpr int fftSize   = 1 << fftOrder;  // 4096
    static constexpr int numBins   = fftSize / 2;    // 2048
    static constexpr int numSlots  = 3;

    SpectrumFIFO()
        : fftLow(10), fftMedium(11), fftHigh(12)
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
        fifoWriteIndex.store(0, std::memory_order_relaxed);
    }

    // Reset the FIFO state (call from prepareToPlay or when going offline/online).
    void reset()
    {
        fifoWriteIndex.store(0, std::memory_order_relaxed);
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
        int idx = fifoWriteIndex.load(std::memory_order_relaxed);
        for (int i = 0; i < numSamples; ++i)
        {
            slots[(size_t)writeSlot][(size_t)idx] = data[i];
            if (++idx >= fftSize)
            {
                idx = 0;
                writerFlip();
            }
        }
        fifoWriteIndex.store(idx, std::memory_order_relaxed);
    }

    // Call from audio thread: push a stereo buffer (sum to mono).
    void pushBlock(const float* L, const float* R, int numSamples)
    {
        int idx = fifoWriteIndex.load(std::memory_order_relaxed);
        for (int i = 0; i < numSamples; ++i)
        {
            slots[(size_t)writeSlot][(size_t)idx] = (L[i] + R[i]) * 0.5f;
            if (++idx >= fftSize)
            {
                idx = 0;
                writerFlip();
            }
        }
        fifoWriteIndex.store(idx, std::memory_order_relaxed);
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
        const int activeOrder = resolution == 0 ? 10 : (resolution == 1 ? 11 : 12);
        const int activeSize = 1 << activeOrder;
        currentBins = activeSize / 2;
        const auto& src = slots[(size_t)readSlot];
        std::fill(fftData.begin(), fftData.end(), 0.0f);
        const int sourceOffset = fftSize - activeSize;
        for (int i = 0; i < activeSize; ++i)
        {
            const float phase = (float)i / (float)(activeSize - 1);
            const float hann = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * phase));
            fftData[(size_t)i] = src[(size_t)(sourceOffset + i)] * hann;
        }
        if (activeOrder == 10) fftLow.performFrequencyOnlyForwardTransform(fftData.data());
        else if (activeOrder == 11) fftMedium.performFrequencyOnlyForwardTransform(fftData.data());
        else fftHigh.performFrequencyOnlyForwardTransform(fftData.data());

        for (int i = 0; i < currentBins; ++i)
        {
            const float mag = fftData[(size_t)i] / (float)activeSize;
            outputMagnitudes[(size_t)i] = 20.0f * std::log10(std::max(mag, 1e-7f));
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

    juce::dsp::FFT fftLow, fftMedium, fftHigh;

    std::array<std::array<float, fftSize>, numSlots> slots {};
    std::array<float, fftSize * 2>                   fftData {};
    std::array<float, numBins>                       outputMagnitudes {};

    std::atomic<int>  fifoWriteIndex { 0 };
    std::atomic<int>  midSlot        { 1 };
    std::atomic<bool> fresh          { false };
    std::atomic<int> resolutionIndex { 2 };
    int currentBins = numBins;

    // Producer-local / consumer-local slot indices. Only touched by their
    // respective owning thread; never read from the other side.
    int writeSlot = 0;   // audio-thread only
    int readSlot  = 2;   // UI-thread only
};
