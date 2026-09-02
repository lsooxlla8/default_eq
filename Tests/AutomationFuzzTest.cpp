#include "../Source/PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace
{
constexpr int kMutationsPerBlock = 12;
constexpr float kMaximumOutputMagnitude = 2.0f;
constexpr float kMaximumAdjacentStep = 0.5f;

int failures = 0;

struct Rng
{
    explicit Rng(std::uint32_t seed) : state(seed) {}
    std::uint32_t next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    float normalised(int mutation)
    {
        if (mutation % 11 == 0) return 0.0f;
        if (mutation % 13 == 0) return 1.0f;
        return (float)(next() & 0x00ffffffu) / (float)0x00ffffffu;
    }
    std::uint32_t state;
};

juce::String bandId(int band, const char* suffix)
{
    return "b" + juce::String(band) + "_" + suffix;
}

float plain(const DefaultEqualizerAudioProcessor& processor, const juce::String& id)
{
    if (auto* parameter = processor.apvts.getParameter(id))
        return parameter->convertFrom0to1(parameter->getValue());
    std::printf("FAIL: missing parameter %s\n", id.toRawUTF8());
    ++failures;
    return 0.0f;
}

int transientSplitterLatency(double sampleRate)
{
    const int order = sampleRate <= 50000.0 ? 10
                    : sampleRate <= 100000.0 ? 11
                    : sampleRate <= 200000.0 ? 12 : 13;
    const int fftSize = 1 << order;
    return fftSize + 2 * (fftSize / 4);
}

std::array<int, 4> oversamplingLatencies(int blockSize)
{
    std::array<int, 4> result {};
    for (int order = 1; order <= 3; ++order)
    {
        juce::dsp::Oversampling<float> oversampler(
            2u, order, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);
        oversampler.initProcessing((size_t)blockSize);
        result[(size_t)order] = juce::roundToInt(oversampler.getLatencyInSamples());
    }
    return result;
}

int expectedLatency(const DefaultEqualizerAudioProcessor& processor, double sampleRate,
                    const std::array<int, 4>& osLatencies)
{
    const bool linear = plain(processor, "linear_phase") > 0.5f;
    const int quality = std::clamp(juce::roundToInt(plain(processor, "linear_quality")), 0, 2);
    const int linearLatency[] { 512, 1024, 2048 };
    const int order = std::clamp(juce::roundToInt(plain(processor, "oversampling")), 0, 3);
    bool normalDriven = false;
    bool transientDriven = false;
    bool transientRouting = false;
    float maximumLookaheadMs = 0.0f;

    for (int band = 1; band <= kNumBands; ++band)
    {
        const bool active = plain(processor, bandId(band, "present")) > 0.5f
                         && plain(processor, bandId(band, "on")) > 0.5f;
        const bool transient = active
                            && juce::roundToInt(plain(processor, bandId(band, "placement_mode"))) == 2;
        const bool driven = active && plain(processor, bandId(band, "drive")) > 0.0001f;
        transientRouting = transientRouting || transient;
        normalDriven = normalDriven || (driven && !transient);
        transientDriven = transientDriven || (driven && transient);

        if (active && plain(processor, bandId(band, "dyn_thresh")) < -0.05f)
            maximumLookaheadMs = std::max(maximumLookaheadMs,
                                          plain(processor, bandId(band, "dyn_lookahead")));
    }

    const int osPasses = (normalDriven ? 1 : 0) + (transientDriven ? 1 : 0);
    return (linear ? linearLatency[quality] : 0)
         + osPasses * osLatencies[(size_t)order]
         + (transientRouting ? transientSplitterLatency(sampleRate) : 0)
         + juce::roundToInt(sampleRate * maximumLookaheadMs * 0.001);
}

bool enableExternalSidechain(DefaultEqualizerAudioProcessor& processor)
{
    auto layout = processor.getBusesLayout();
    if (layout.inputBuses.size() < 2)
        return false;
    layout.inputBuses.set(1, juce::AudioChannelSet::stereo());
    return processor.setBusesLayout(layout);
}

std::uint64_t hashBlock(const juce::MemoryBlock& block)
{
    constexpr std::uint64_t offset = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    auto hash = offset;
    const auto* bytes = static_cast<const std::uint8_t*>(block.getData());
    for (size_t i = 0; i < block.getSize(); ++i)
    {
        hash ^= bytes[i];
        hash *= prime;
    }
    return hash;
}

struct ScenarioResult
{
    std::uint64_t stateHash = 0;
    size_t stateBytes = 0;
    float maximumMagnitude = 0.0f;
    float maximumStep = 0.0f;
    int parameterChanges = 0;
};

ScenarioResult runScenario(double sampleRate, int blockSize, std::uint32_t seed)
{
    auto processor = std::make_unique<DefaultEqualizerAudioProcessor>();
    if (!enableExternalSidechain(*processor))
    {
        std::printf("FAIL: external stereo sidechain layout rejected\n");
        ++failures;
    }
    processor->prepareToPlay(sampleRate, blockSize);

    auto parameters = processor->getParameters();
    std::vector<int> sweep((size_t)parameters.size());
    for (int i = 0; i < parameters.size(); ++i) sweep[(size_t)i] = i;
    Rng rng(seed);
    for (size_t i = sweep.size(); i > 1; --i)
        std::swap(sweep[i - 1], sweep[(size_t)(rng.next() % (std::uint32_t)i)]);

    const auto osLatencies = oversamplingLatencies(blockSize);
    juce::AudioBuffer<float> audio(4, blockSize);
    juce::MidiBuffer midi;
    ScenarioResult result;
    std::vector<bool> touched((size_t)parameters.size(), false);
    size_t sweepPosition = 0;
    float previousL = 0.0f;
    float previousR = 0.0f;
    double phase = 0.0;
    const int requiredBlocks = (parameters.size() + kMutationsPerBlock - 1) / kMutationsPerBlock;
    const int totalBlocks = requiredBlocks + 18;

    for (int block = 0; block < totalBlocks; ++block)
    {
        for (int mutation = 0; mutation < kMutationsPerBlock; ++mutation)
        {
            int index = 0;
            if (sweepPosition < sweep.size())
                index = sweep[sweepPosition++];
            else
                index = (int)(rng.next() % (std::uint32_t)parameters.size());
            auto* parameter = parameters[index];
            parameter->setValueNotifyingHost(rng.normalised(result.parameterChanges));
            touched[(size_t)index] = true;
            ++result.parameterChanges;
        }

        const int before = processor->getLatencySamples();
        const int expected = expectedLatency(*processor, sampleRate, osLatencies);
        if (before != expected)
        {
            std::printf("FAIL: %.0f Hz/%d block %d latency expected %d, got %d\n",
                        sampleRate, blockSize, block, expected, before);
            ++failures;
        }

        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto noise = ((float)(rng.next() & 0xffffu) / 32767.5f - 1.0f) * 0.005f;
            const auto signal = 0.025f * std::sin((float)phase) + noise;
            phase += juce::MathConstants<double>::twoPi * 997.0 / sampleRate;
            audio.setSample(0, sample, signal);
            audio.setSample(1, sample, signal * 0.91f);
            audio.setSample(2, sample, signal * 8.0f);
            audio.setSample(3, sample, -signal * 6.0f);
        }
        processor->processBlock(audio, midi);
        if (processor->getLatencySamples() != before)
        {
            std::printf("FAIL: %.0f Hz/%d block %d changed reported latency inside processBlock\n",
                        sampleRate, blockSize, block);
            ++failures;
        }

        for (int sample = 0; sample < blockSize; ++sample)
        {
            const float left = audio.getSample(0, sample);
            const float right = audio.getSample(1, sample);
            if (!std::isfinite(left) || !std::isfinite(right))
            {
                std::printf("FAIL: %.0f Hz/%d block %d sample %d produced non-finite output\n",
                            sampleRate, blockSize, block, sample);
                ++failures;
                break;
            }
            result.maximumMagnitude = std::max(result.maximumMagnitude,
                                               std::max(std::abs(left), std::abs(right)));
            result.maximumStep = std::max(result.maximumStep,
                                          std::max(std::abs(left - previousL), std::abs(right - previousR)));
            previousL = left;
            previousR = right;
        }
    }

    if (!std::all_of(touched.begin(), touched.end(), [](bool value) { return value; }))
    {
        std::printf("FAIL: %.0f Hz/%d did not automate every parameter\n", sampleRate, blockSize);
        ++failures;
    }
    if (result.maximumMagnitude > kMaximumOutputMagnitude)
    {
        std::printf("FAIL: %.0f Hz/%d output magnitude %.6f exceeds %.6f\n",
                    sampleRate, blockSize, result.maximumMagnitude, kMaximumOutputMagnitude);
        ++failures;
    }
    if (result.maximumStep > kMaximumAdjacentStep)
    {
        std::printf("FAIL: %.0f Hz/%d adjacent step %.6f exceeds %.6f\n",
                    sampleRate, blockSize, result.maximumStep, kMaximumAdjacentStep);
        ++failures;
    }

    juce::MemoryBlock state;
    processor->getStateInformation(state);
    result.stateHash = hashBlock(state);
    result.stateBytes = state.getSize();
    const int finalLatency = processor->getLatencySamples();

    auto restored = std::make_unique<DefaultEqualizerAudioProcessor>();
    enableExternalSidechain(*restored);
    restored->prepareToPlay(sampleRate, blockSize);
    restored->setStateInformation(state.getData(), (int)state.getSize());
    juce::MemoryBlock roundTrip;
    restored->getStateInformation(roundTrip);
    if (roundTrip != state)
    {
        std::printf("FAIL: %.0f Hz/%d state is not byte-stable after round trip\n",
                    sampleRate, blockSize);
        ++failures;
    }
    if (restored->getLatencySamples() != finalLatency)
    {
        std::printf("FAIL: %.0f Hz/%d restored latency expected %d, got %d\n",
                    sampleRate, blockSize, finalLatency, restored->getLatencySamples());
        ++failures;
    }
    return result;
}

std::vector<ScenarioResult> runSuite()
{
    struct Case { double sampleRate; int blockSize; std::uint32_t seed; };
    const Case cases[] {
        { 44100.0, 17, 0x11a0cafeu },
        { 48000.0, 64, 0x22b1cafeu },
        { 96000.0, 257, 0x33c2cafeu },
        { 192000.0, 512, 0x44d3cafeu }
    };
    std::vector<ScenarioResult> results;
    for (const auto& item : cases)
    {
        const auto result = runScenario(item.sampleRate, item.blockSize, item.seed);
        std::printf("  %.0f Hz / %d: %d changes, max |out| %.4f, max step %.4f, state %zu bytes\n",
                    item.sampleRate, item.blockSize, result.parameterChanges,
                    result.maximumMagnitude, result.maximumStep, result.stateBytes);
        results.push_back(result);
    }
    return results;
}
}

int runAutomationFuzz()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("Automation fuzz pass 1\n");
    const auto first = runSuite();
    std::printf("Automation fuzz pass 2 (determinism)\n");
    const auto second = runSuite();
    if (first.size() != second.size())
    {
        std::printf("FAIL: deterministic suite result count changed\n");
        ++failures;
    }
    for (size_t i = 0; i < std::min(first.size(), second.size()); ++i)
        if (first[i].stateHash != second[i].stateHash || first[i].stateBytes != second[i].stateBytes)
        {
            std::printf("FAIL: scenario %zu ended with non-deterministic state\n", i);
            ++failures;
        }

    if (failures == 0)
        std::printf("Automation fuzz: PASS\n");
    return failures == 0 ? 0 : 1;
}
