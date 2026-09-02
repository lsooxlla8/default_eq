#include "../Source/PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#if JUCE_MAC
 #include <mach/mach.h>
#elif JUCE_WINDOWS
 #include <windows.h>
 #include <psapi.h>
#elif JUCE_LINUX
 #include <unistd.h>
#endif

namespace
{
constexpr std::uint64_t kProcessorObjectBudgetBytes = 1130000;
constexpr std::uint64_t kPreparedPerInstanceBudgetBytes = 40ull * 1024ull * 1024ull;
constexpr int kMeasuredInstances = 5;

std::uint64_t residentBytes()
{
#if JUCE_MAC
    mach_task_basic_info_data_t info {};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS)
        return (std::uint64_t)info.resident_size;
#elif JUCE_WINDOWS
    PROCESS_MEMORY_COUNTERS_EX counters {};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                             sizeof(counters)) != FALSE)
        return (std::uint64_t)counters.WorkingSetSize;
#elif JUCE_LINUX
    if (auto* file = std::fopen("/proc/self/statm", "r"))
    {
        unsigned long totalPages = 0;
        unsigned long residentPages = 0;
        const int fields = std::fscanf(file, "%lu %lu", &totalPages, &residentPages);
        std::fclose(file);
        if (fields == 2)
            return (std::uint64_t)residentPages * (std::uint64_t)sysconf(_SC_PAGESIZE);
    }
#endif
    return 0;
}

void touchPreparedProcessor(DefaultEqualizerAudioProcessor& processor)
{
    constexpr int blockSize = 512;
    processor.setAnalyzerEnabled(true);
    processor.prepareToPlay(48000.0, blockSize);
    juce::AudioBuffer<float> audio(2, blockSize);
    juce::MidiBuffer midi;
    double phase = 0.0;
    for (int block = 0; block < 12; ++block)
    {
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const float value = 0.05f * std::sin((float)phase);
            phase += juce::MathConstants<double>::twoPi * 997.0 / 48000.0;
            audio.setSample(0, sample, value);
            audio.setSample(1, sample, value * 0.91f);
        }
        processor.processBlock(audio, midi);
    }
}
}

int runMemoryRegression()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    int failures = 0;
    const auto objectBytes = (std::uint64_t)sizeof(DefaultEqualizerAudioProcessor);
    std::printf("processor_object_bytes=%llu budget=%llu\n",
                (unsigned long long)objectBytes,
                (unsigned long long)kProcessorObjectBudgetBytes);
    if (objectBytes > kProcessorObjectBudgetBytes)
    {
        std::printf("FAIL: fixed processor object exceeds its regression budget\n");
        ++failures;
    }

    const auto rssBefore = residentBytes();
    std::vector<std::unique_ptr<DefaultEqualizerAudioProcessor>> processors;
    processors.reserve(kMeasuredInstances);
    std::array<std::uint64_t, kMeasuredInstances> rssAfter {};
    for (int i = 0; i < kMeasuredInstances; ++i)
    {
        auto processor = std::make_unique<DefaultEqualizerAudioProcessor>();
        touchPreparedProcessor(*processor);
        processors.push_back(std::move(processor));
        rssAfter[(size_t)i] = residentBytes();
    }

    if (rssBefore == 0 || rssAfter.back() == 0 || rssAfter.back() < rssAfter.front())
    {
        std::printf("FAIL: platform resident-memory measurement is unavailable\n");
        ++failures;
    }
    else
    {
        // Ignore the first instance so code pages and one-time JUCE runtime setup
        // are not charged to every plug-in. The retained-instance slope includes
        // the processor object, oversampling pool, T/S splitters, linear-phase
        // engine, lookahead/detector workspaces, and their allocator overhead.
        const auto preparedPerInstance =
            (rssAfter.back() - rssAfter.front()) / (std::uint64_t)(kMeasuredInstances - 1);
        std::printf("rss_before_bytes=%llu first_prepared_rss_bytes=%llu final_rss_bytes=%llu\n",
                    (unsigned long long)rssBefore,
                    (unsigned long long)rssAfter.front(),
                    (unsigned long long)rssAfter.back());
        std::printf("prepared_rss_per_instance_bytes=%llu budget=%llu instances=%d\n",
                    (unsigned long long)preparedPerInstance,
                    (unsigned long long)kPreparedPerInstanceBudgetBytes,
                    kMeasuredInstances);
        if (preparedPerInstance > kPreparedPerInstanceBudgetBytes)
        {
            std::printf("FAIL: prepared per-instance RSS exceeds its regression budget\n");
            ++failures;
        }
    }

    if (failures == 0)
        std::printf("Memory regression: PASS\n");
    return failures == 0 ? 0 : 1;
}
