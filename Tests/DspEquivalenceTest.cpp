#include "../Source/PluginProcessor.h"
#include "../Source/DSP/FilterTypes.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>

namespace
{
constexpr std::uint32_t kFileMagic = 0x44514551u; // DQEQ
constexpr float kMaximumSampleDelta = 1.0e-5f;
constexpr double kMaximumRmsDelta = 1.0e-6;

juce::String bandId(int band, const char* suffix)
{
    return "b" + juce::String(band) + "_" + suffix;
}

void setPlain(DefaultEqualizerAudioProcessor& processor, const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void activateBand(DefaultEqualizerAudioProcessor& processor, int band,
                  int type, float frequency, float q, float gain)
{
    setPlain(processor, bandId(band, "present"), 1.0f);
    setPlain(processor, bandId(band, "on"), 1.0f);
    setPlain(processor, bandId(band, "type"), (float)type);
    setPlain(processor, bandId(band, "freq"), frequency);
    setPlain(processor, bandId(band, "q"), q);
    setPlain(processor, bandId(band, "gain"), gain);
}

void enableSidechain(DefaultEqualizerAudioProcessor& processor)
{
    auto layout = processor.getBusesLayout();
    if (layout.inputBuses.size() > 1)
    {
        layout.inputBuses.set(1, juce::AudioChannelSet::stereo());
        processor.setBusesLayout(layout);
    }
}

struct Noise
{
    std::uint32_t state = 0x6d2b79f5u;
    float next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return ((float)(state & 0xffffu) / 32767.5f - 1.0f);
    }
};

void appendScenario(std::vector<float>& result, int scenario)
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 127;
    constexpr int blocks = 72;
    auto processor = std::make_unique<DefaultEqualizerAudioProcessor>();
    setPlain(*processor, "auto_gain_mode", 0.0f);

    if (scenario == 0)
    {
        activateBand(*processor, 1, deq::filter_types::highCut, 55.0f, 0.8f, 0.0f);
        setPlain(*processor, bandId(1, "slope"), 30.0f);
        activateBand(*processor, 2, deq::filter_types::lowShelf, 180.0f, 0.9f, 5.5f);
        activateBand(*processor, 3, deq::filter_types::bell, 730.0f, 2.4f, -7.0f);
        activateBand(*processor, 4, deq::filter_types::tilt, 1800.0f, 1.1f, 4.0f);
        activateBand(*processor, 5, deq::filter_types::notch, 3100.0f, 5.0f, 0.0f);
        activateBand(*processor, 6, deq::filter_types::highShelf, 7200.0f, 0.7f, -3.5f);
        setPlain(*processor, "scale", 0.83f);
        setPlain(*processor, "shift", 2.75f);
        setPlain(*processor, "adaptive_q", 1.0f);
    }
    else if (scenario == 1)
    {
        enableSidechain(*processor);
        activateBand(*processor, 1, deq::filter_types::bell, 900.0f, 2.1f, 10.0f);
        setPlain(*processor, bandId(1, "drive"), 18.0f);
        setPlain(*processor, bandId(1, "sat_mode"), 3.0f);
        setPlain(*processor, bandId(1, "drive_character"), -0.37f);
        setPlain(*processor, bandId(1, "dyn_thresh"), -32.0f);
        setPlain(*processor, bandId(1, "dyn_range"), 9.0f);
        setPlain(*processor, bandId(1, "dyn_ratio"), 6.0f);
        setPlain(*processor, bandId(1, "dyn_speed"), 61.0f);
        setPlain(*processor, bandId(1, "dyn_lookahead"), 3.25f);
        setPlain(*processor, bandId(1, "sc_source"), 1.0f);
        activateBand(*processor, 2, deq::filter_types::lowShelf, 240.0f, 0.8f, -8.0f);
        setPlain(*processor, bandId(2, "placement_mode"), 1.0f);
        setPlain(*processor, bandId(2, "placement"), -55.0f);
        setPlain(*processor, bandId(2, "drive"), 11.0f);
        setPlain(*processor, bandId(2, "sat_mode"), 4.0f);
        setPlain(*processor, "oversampling", 2.0f);
    }
    else if (scenario == 2)
    {
        activateBand(*processor, 1, deq::filter_types::bell, 2200.0f, 1.6f, 8.0f);
        setPlain(*processor, bandId(1, "placement_mode"), 2.0f);
        setPlain(*processor, bandId(1, "placement"), -68.0f);
        setPlain(*processor, bandId(1, "drive"), 13.0f);
        setPlain(*processor, bandId(1, "sat_mode"), 5.0f);
        activateBand(*processor, 2, deq::filter_types::highShelf, 6200.0f, 0.75f, -6.0f);
        setPlain(*processor, bandId(2, "placement_mode"), 2.0f);
        setPlain(*processor, bandId(2, "placement"), 72.0f);
        setPlain(*processor, "oversampling", 1.0f);
        setPlain(*processor, "transient_split_strength", 83.0f);
        setPlain(*processor, "transient_split_balance", -17.0f);
        setPlain(*processor, "transient_split_hold", 64.0f);
        setPlain(*processor, "transient_split_smooth", 37.0f);
    }
    else
    {
        activateBand(*processor, 1, deq::filter_types::lowShelf, 170.0f, 0.8f, 6.0f);
        activateBand(*processor, 2, deq::filter_types::bell, 1250.0f, 3.2f, -9.0f);
        activateBand(*processor, 3, deq::filter_types::highShelf, 7600.0f, 0.9f, 4.5f);
        setPlain(*processor, "linear_quality", 0.0f);
        setPlain(*processor, "linear_phase", 1.0f);
    }

    processor->prepareToPlay(sampleRate, blockSize);
    const int channels = scenario == 1 ? 4 : 2;
    juce::AudioBuffer<float> audio(channels, blockSize);
    juce::MidiBuffer midi;
    Noise noise;
    double phaseA = 0.0;
    double phaseB = 0.0;

    result.push_back((float)scenario);
    result.push_back((float)processor->getLatencySamples());
    for (int block = 0; block < blocks; ++block)
    {
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const float transient = ((block * blockSize + sample) % 701 == 0) ? 0.6f : 0.0f;
            const float left = 0.07f * std::sin((float)phaseA)
                             + 0.035f * std::sin((float)phaseB) + 0.008f * noise.next() + transient;
            const float right = 0.065f * std::sin((float)(phaseA + 0.31))
                              + 0.03f * std::sin((float)(phaseB - 0.19)) + 0.008f * noise.next() - transient * 0.4f;
            phaseA += juce::MathConstants<double>::twoPi * 317.0 / sampleRate;
            phaseB += juce::MathConstants<double>::twoPi * 2231.0 / sampleRate;
            audio.setSample(0, sample, left);
            audio.setSample(1, sample, right);
            if (channels == 4)
            {
                audio.setSample(2, sample, left * 4.0f + 0.03f * noise.next());
                audio.setSample(3, sample, right * 3.0f + 0.03f * noise.next());
            }
        }
        processor->processBlock(audio, midi);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            result.push_back(audio.getSample(0, sample));
            result.push_back(audio.getSample(1, sample));
        }
    }
}

std::vector<float> render()
{
    std::vector<float> result;
    result.reserve(4 * (2 + 72 * 127 * 2));
    for (int scenario = 0; scenario < 4; ++scenario)
        appendScenario(result, scenario);
    return result;
}

bool writeBaseline(const char* path, const std::vector<float>& samples)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    const auto count = (std::uint64_t)samples.size();
    stream.write(reinterpret_cast<const char*>(&kFileMagic), sizeof(kFileMagic));
    stream.write(reinterpret_cast<const char*>(&count), sizeof(count));
    stream.write(reinterpret_cast<const char*>(samples.data()),
                 (std::streamsize)(samples.size() * sizeof(float)));
    return stream.good();
}

bool readBaseline(const char* path, std::vector<float>& samples)
{
    std::ifstream stream(path, std::ios::binary);
    std::uint32_t magic = 0;
    std::uint64_t count = 0;
    stream.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    stream.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!stream.good() || magic != kFileMagic || count > 1000000)
        return false;
    samples.resize((size_t)count);
    stream.read(reinterpret_cast<char*>(samples.data()),
                (std::streamsize)(samples.size() * sizeof(float)));
    return stream.good();
}

int compare(const std::vector<float>& reference, const std::vector<float>& actual)
{
    if (reference.size() != actual.size())
    {
        std::printf("FAIL: DSP render size changed from %zu to %zu floats\n",
                    reference.size(), actual.size());
        return 1;
    }
    float maximum = 0.0f;
    double squared = 0.0;
    size_t worst = 0;
    for (size_t i = 0; i < actual.size(); ++i)
    {
        if (!std::isfinite(actual[i]))
        {
            std::printf("FAIL: non-finite DSP sample at %zu\n", i);
            return 1;
        }
        const float delta = std::abs(actual[i] - reference[i]);
        squared += (double)delta * delta;
        if (delta > maximum) { maximum = delta; worst = i; }
    }
    const double rms = std::sqrt(squared / (double)std::max<size_t>(1, actual.size()));
    std::printf("DSP equivalence: %zu floats, max delta %.9g at %zu, RMS %.9g\n",
                actual.size(), maximum, worst, rms);
    if (maximum > kMaximumSampleDelta || rms > kMaximumRmsDelta)
    {
        std::printf("FAIL: DSP render exceeds equivalence tolerances\n");
        return 1;
    }
    return 0;
}
}

int runDspEquivalence(const char* mode, const char* path)
{
    const auto actual = render();
    if (std::strcmp(mode, "self") == 0)
        return compare(actual, render());
    if (path == nullptr)
    {
        std::printf("FAIL: DSP equivalence mode requires a baseline path\n");
        return 2;
    }
    if (std::strcmp(mode, "write") == 0)
    {
        if (!writeBaseline(path, actual))
        {
            std::printf("FAIL: could not write DSP baseline %s\n", path);
            return 1;
        }
        std::printf("DSP baseline: wrote %zu floats to %s\n", actual.size(), path);
        return 0;
    }
    if (std::strcmp(mode, "compare") == 0)
    {
        std::vector<float> reference;
        if (!readBaseline(path, reference))
        {
            std::printf("FAIL: could not read DSP baseline %s\n", path);
            return 1;
        }
        return compare(reference, actual);
    }
    std::printf("FAIL: unknown DSP equivalence mode %s\n", mode);
    return 2;
}
