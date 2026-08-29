/*
    AuditRegressionTest.cpp — verifies the invariants introduced by the
    Milestone-A correctness/RT-safety pass.

    This test is intentionally standalone (no JUCE dependency) so it runs in
    the same CI slot as BiquadTest. It covers the lock-free handoff algorithms
    whose correctness is independent of JUCE's FFT:

      1. SpectrumFIFO ping-pong (A4): under a concurrent producer/consumer,
         each published buffer must equal the exact sequence of samples the
         producer wrote during that wrap. No tearing, no drops of adjacent
         samples inside a published buffer.

    What this test does NOT cover: the FFT math of SpectrumFIFO itself — that
    lives in the juce_dsp library and is covered by JUCE's
    own unit tests and by pluginval at integration time (see milestone B3).
*/

#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#include <algorithm>
#include <chrono>

static int failures = 0;

#define CHECK(cond, msg) \
    do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } } while (0)

// ═══════════════════════════════════════════════════════════════════════
//  1. Triple-buffer SPSC algorithm (mirrors SpectrumFIFO.h exactly)
// ═══════════════════════════════════════════════════════════════════════

// Mirrors the canonical swap-chain triple-buffer in SpectrumFIFO.h.
template <int FFT_SIZE>
struct TripleBuf
{
    static constexpr int NUM_SLOTS = 3;
    std::array<std::array<float, FFT_SIZE>, NUM_SLOTS> slots {};

    std::atomic<int>  fifoWriteIndex { 0 };
    std::atomic<int>  midSlot        { 1 };
    std::atomic<bool> fresh          { false };

    // Owned locally; never touched by the other thread.
    int writeSlot = 0;
    int readSlot  = 2;

    void writerFlip()
    {
        writeSlot = midSlot.exchange(writeSlot, std::memory_order_release);
        fresh.store(true, std::memory_order_release);
    }

    void push(const float* data, int n)
    {
        int idx = fifoWriteIndex.load(std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            slots[(size_t)writeSlot][(size_t)idx] = data[i];
            if (++idx >= FFT_SIZE)
            {
                idx = 0;
                writerFlip();
            }
        }
        fifoWriteIndex.store(idx, std::memory_order_relaxed);
    }

    bool tryConsume(std::array<float, FFT_SIZE>& dest)
    {
        if (!fresh.exchange(false, std::memory_order_acquire)) return false;
        readSlot = midSlot.exchange(readSlot, std::memory_order_acquire);
        dest = slots[(size_t)readSlot];
        return true;
    }
};

static void test_triplebuf_single_thread()
{
    // Deterministic sanity: a single-threaded sequence of pushes produces
    // exactly the published buffers we expect, in order.
    TripleBuf<16> pp;
    std::array<float, 16> snap {};

    float counter = 0.0f;
    std::vector<float> batch(16);

    for (int i = 0; i < 16; ++i) batch[i] = counter++;
    pp.push(batch.data(), 16);
    CHECK(pp.tryConsume(snap), "consume after 1st wrap returns true");
    for (int i = 0; i < 16; ++i)
        CHECK(snap[i] == (float)i, "snap[i] == i after 1st wrap");

    for (int i = 0; i < 16; ++i) batch[i] = counter++;
    pp.push(batch.data(), 16);
    CHECK(pp.tryConsume(snap), "consume after 2nd wrap returns true");
    for (int i = 0; i < 16; ++i)
        CHECK(snap[i] == (float)(16 + i), "snap[i] == 16+i after 2nd wrap");

    CHECK(!pp.tryConsume(snap), "consume returns false when nothing is published");

    for (int i = 0; i < 5; ++i) batch[i] = counter++;
    pp.push(batch.data(), 5);
    CHECK(!pp.tryConsume(snap), "partial write does not publish");
    for (int i = 0; i < 11; ++i) batch[i] = counter++;
    pp.push(batch.data(), 11);
    CHECK(pp.tryConsume(snap), "wrap completes after partial + fill publishes");
    for (int i = 0; i < 16; ++i)
        CHECK(snap[i] == (float)(32 + i), "snap[i] == 32+i after partial+fill");
}

static void test_triplebuf_concurrent_no_tearing()
{
    // Producer writes samples whose value == (absolute stream index) mod MOD,
    // where MOD < 2^23 so every float value round-trips exactly and adjacent
    // deltas are always either +1 or -(MOD-1). A torn buffer (producer
    // overwriting while reader reads) would break that bounded delta pattern.
    //
    // The producer deliberately outpaces the consumer (producer in a tight
    // push loop, consumer simulating FFT work via a short sleep) to stress
    // the case where 2-buffer ping-pong would fail.
    constexpr int MOD = 1 << 14;   // 16384 — well under float's 2^24 limit
    static_assert(MOD > 128, "MOD must exceed FFT so at most one wrap per buffer");
    TripleBuf<128> pp;

    std::atomic<bool> stop { false };
    std::atomic<long long> producedSamples { 0 };
    std::atomic<int> badBuffers { 0 };
    std::atomic<int> consumedBuffers { 0 };

    std::thread producer([&]{
        std::vector<float> batch(40);
        long long counter = 0;
        while (!stop.load(std::memory_order_relaxed))
        {
            for (size_t i = 0; i < batch.size(); ++i)
                batch[i] = (float)((counter + (long long)i) % MOD);
            pp.push(batch.data(), (int)batch.size());
            counter += (long long)batch.size();
            producedSamples.store(counter, std::memory_order_relaxed);
        }
    });

    std::thread consumer([&]{
        std::array<float, 128> snap {};
        auto verify = [&]{
            bool ok = true;
            for (int i = 1; i < 128; ++i)
            {
                const float diff = snap[i] - snap[i - 1];
                // Normal delta is +1. A wrap at MOD gives diff == -(MOD - 1).
                // Any other value means a torn buffer.
                if (diff != 1.0f && diff != -(float)(MOD - 1))
                {
                    ok = false; break;
                }
            }
            if (!ok) badBuffers.fetch_add(1, std::memory_order_relaxed);
            consumedBuffers.fetch_add(1, std::memory_order_relaxed);
        };
        while (!stop.load(std::memory_order_relaxed))
        {
            if (pp.tryConsume(snap))
            {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
                verify();
            }
        }
        while (pp.tryConsume(snap)) verify();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    stop.store(true, std::memory_order_relaxed);
    producer.join();
    consumer.join();

    const int bad = badBuffers.load();
    const int good = consumedBuffers.load();
    CHECK(bad == 0, "no torn / out-of-order buffers observed");
    CHECK(good > 0, "consumer saw at least one buffer");
    std::printf("  (triple-buffer stress: produced %lld samples, consumed %d buffers, %d tears)\n",
                producedSamples.load(), good, bad);
}

// ══════════════════════════════════════════════════════════════════════
//  Main
// ══════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════
//  3. LinearPhaseEngine kernel-handoff invariant (A5)
// ═══════════════════════════════════════════════════════════════════════
//
// The real LinearPhaseEngine class pulls in juce::dsp::FFT, so we don't
// instantiate it directly from this no-JUCE test. We instead mirror just
// its publish/acquire pattern with two 8192-float kernel slots, prove the
// write-swap / read-consume pairing holds under stress, and rely on the
// full-plugin build + JUCE's own FFT tests for the FFT math itself.

// Mirrors LinearPhaseEngine's swap-chain triple buffer for the FIR kernel.
struct KernelSwapChain
{
    static constexpr int KERNEL_LEN = 8192;
    static constexpr int NUM_SLOTS = 3;
    std::array<std::array<float, KERNEL_LEN>, NUM_SLOTS> slots {};
    std::atomic<int>  midSlot     { 1 };
    std::atomic<bool> kernelFresh { false };
    int writeSlot = 0;       // writer-only
    int readSlot  = 2;       // reader-only
    bool loaded   = false;   // reader-only

    void publishKernel(const float* src)
    {
        std::copy(src, src + KERNEL_LEN, slots[(size_t)writeSlot].begin());
        writeSlot = midSlot.exchange(writeSlot, std::memory_order_release);
        kernelFresh.store(true, std::memory_order_release);
    }

    bool tryReadSum(double& outSum)
    {
        if (kernelFresh.exchange(false, std::memory_order_acquire))
        {
            readSlot = midSlot.exchange(readSlot, std::memory_order_acquire);
            loaded = true;
        }
        if (!loaded) return false;
        const auto& src = slots[(size_t)readSlot];
        double s = 0.0;
        for (int i = 0; i < KERNEL_LEN; ++i) s += (double)src[(size_t)i];
        outSum = s;
        return true;
    }
};

static void test_linphase_kernel_handoff()
{
    // Writer pushes a sequence of N kernels whose element value is fixed
    // per-kernel (kernel k has every element = (float)k). Therefore the sum
    // of any valid kernel is KERNEL_LEN * k for some integer k, i.e. an
    // exact multiple of KERNEL_LEN. A torn read would produce some other
    // value. Reader runs concurrently in a tight loop and verifies.
    KernelSwapChain kb;

    std::atomic<bool> stop { false };
    std::atomic<long long> publishedKernels { 0 };
    std::atomic<long long> readKernels { 0 };
    std::atomic<long long> torn { 0 };

    std::thread writer([&]{
        std::array<float, KernelSwapChain::KERNEL_LEN> tmp {};
        int k = 0;
        while (!stop.load(std::memory_order_relaxed))
        {
            const float v = (float)(k % 4096);  // stay within float-exact int range
            std::fill(tmp.begin(), tmp.end(), v);
            kb.publishKernel(tmp.data());
            publishedKernels.fetch_add(1, std::memory_order_relaxed);
            ++k;
        }
    });

    std::thread reader([&]{
        while (!stop.load(std::memory_order_relaxed))
        {
            double sum = 0.0;
            if (!kb.tryReadSum(sum)) continue;
            readKernels.fetch_add(1, std::memory_order_relaxed);
            const double v = sum / (double)KernelSwapChain::KERNEL_LEN;
            if (v < 0.0 || v > 4096.0) { torn.fetch_add(1, std::memory_order_relaxed); continue; }
            if (v != std::floor(v))    { torn.fetch_add(1, std::memory_order_relaxed); }
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    stop.store(true, std::memory_order_relaxed);
    writer.join();
    reader.join();

    const long long t = torn.load();
    CHECK(t == 0, "LinearPhaseEngine kernel handoff: no torn reads");
    std::printf("  (kernel handoff stress: published %lld, read %lld, torn %lld)\n",
                publishedKernels.load(), readKernels.load(), t);
}

int main()
{
    std::printf("── A4: SpectrumFIFO triple-buffer, single-thread ──\n");
    test_triplebuf_single_thread();

    std::printf("── A4: SpectrumFIFO triple-buffer, concurrent stress ──\n");
    test_triplebuf_concurrent_no_tearing();

    std::printf("── A5: LinearPhaseEngine kernel double-buffer handoff ──\n");
    test_linphase_kernel_handoff();

    if (failures == 0)
        std::printf("\nALL AUDIT REGRESSION TESTS PASSED\n");
    else
        std::printf("\n%d TEST(S) FAILED\n", failures);

    return failures == 0 ? 0 : 1;
}
