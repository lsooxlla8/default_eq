#include "../Source/PluginProcessor.h"
#include "../Source/DSP/FilterTypes.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace
{
juce::String bandId(int band, const char* suffix)
{
    return "b" + juce::String(band) + "_" + suffix;
}

void setPlain(DefaultEqualizerAudioProcessor& processor,
              const juce::String& parameterId, float value)
{
    if (auto* parameter = processor.apvts.getParameter(parameterId))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void activateBell(DefaultEqualizerAudioProcessor& processor, int band)
{
    setPlain(processor, bandId(band, "present"), 1.0f);
    setPlain(processor, bandId(band, "on"), 1.0f);
    setPlain(processor, bandId(band, "type"), (float)deq::filter_types::bell);
    setPlain(processor, bandId(band, "freq"), 70.0f * std::pow(2.0f, (float)band));
    setPlain(processor, bandId(band, "q"), 0.7f + 0.13f * (float)band);
    setPlain(processor, bandId(band, "gain"), band % 2 == 0 ? -6.0f : 6.0f);
}

enum class Scenario
{
    clean,
    outputOnly,
    oneBell,
    eightBells,
    oneBellMidSide,
    eightBellsAnalyzer,
    oneClassicCut48,
    oneTilt,
    oneDynamicBell,
    eightBellsOneDynamic,
    oneDrive1x,
    oneDrive4x
};

const char* scenarioName(Scenario scenario)
{
    switch (scenario)
    {
        case Scenario::clean: return "clean";
        case Scenario::outputOnly: return "output_only";
        case Scenario::oneBell: return "1_bell";
        case Scenario::eightBells: return "8_bells";
        case Scenario::oneBellMidSide: return "1_bell_ms";
        case Scenario::eightBellsAnalyzer: return "8_bells_rta";
        case Scenario::oneClassicCut48: return "1_cut_48";
        case Scenario::oneTilt: return "1_tilt";
        case Scenario::oneDynamicBell: return "1_dynamic";
        case Scenario::eightBellsOneDynamic: return "8_bells_1_dyn";
        case Scenario::oneDrive1x: return "1_drive_1x";
        case Scenario::oneDrive4x: return "1_drive_4x";
    }
    return "unknown";
}

std::unique_ptr<DefaultEqualizerAudioProcessor> makeProcessor(Scenario scenario,
                                                              int blockSize)
{
    auto processor = std::make_unique<DefaultEqualizerAudioProcessor>();
    setPlain(*processor, "auto_gain_mode", 0.0f);
    if (scenario == Scenario::outputOnly)
        setPlain(*processor, "output_gain", 1.0f);
    if (scenario == Scenario::oneBell || scenario == Scenario::oneBellMidSide
        || scenario == Scenario::oneClassicCut48 || scenario == Scenario::oneTilt
        || scenario == Scenario::oneDynamicBell || scenario == Scenario::oneDrive1x
        || scenario == Scenario::oneDrive4x)
        activateBell(*processor, 1);
    if (scenario == Scenario::eightBells || scenario == Scenario::eightBellsAnalyzer
        || scenario == Scenario::eightBellsOneDynamic)
        for (int band = 1; band <= kNumBands; ++band)
            activateBell(*processor, band);
    if (scenario == Scenario::oneBellMidSide)
    {
        setPlain(*processor, bandId(1, "placement_mode"), 1.0f);
        setPlain(*processor, bandId(1, "placement"), 35.0f);
    }
    if (scenario == Scenario::oneClassicCut48)
    {
        setPlain(*processor, bandId(1, "type"), (float)deq::filter_types::highCut);
        setPlain(*processor, bandId(1, "freq"), 6200.0f);
        setPlain(*processor, bandId(1, "slope"), 48.0f);
    }
    if (scenario == Scenario::oneTilt)
    {
        setPlain(*processor, bandId(1, "type"), (float)deq::filter_types::tilt);
        setPlain(*processor, bandId(1, "freq"), 1200.0f);
    }
    if (scenario == Scenario::oneDynamicBell || scenario == Scenario::eightBellsOneDynamic)
    {
        const int dynamicBand = scenario == Scenario::eightBellsOneDynamic ? 4 : 1;
        setPlain(*processor, bandId(dynamicBand, "dyn_thresh"), -30.0f);
        setPlain(*processor, bandId(dynamicBand, "dyn_range"), 9.0f);
        setPlain(*processor, bandId(dynamicBand, "dyn_ratio"), 4.0f);
    }
    if (scenario == Scenario::oneDrive1x || scenario == Scenario::oneDrive4x)
    {
        setPlain(*processor, bandId(1, "drive"), 18.0f);
        setPlain(*processor, bandId(1, "sat_mode"), 0.0f);
        setPlain(*processor, "oversampling",
                 scenario == Scenario::oneDrive4x ? 2.0f : 0.0f);
    }
    processor->setAnalyzerEnabled(scenario == Scenario::eightBellsAnalyzer);
    processor->prepareToPlay(48000.0, blockSize);
    return processor;
}

void fillSource(juce::AudioBuffer<float>& source)
{
    for (int sample = 0; sample < source.getNumSamples(); ++sample)
    {
        const float phase = (float)sample * 0.071f;
        source.setSample(0, sample, 0.07f * std::sin(phase)
                                     + 0.013f * std::sin(phase * 3.17f));
        source.setSample(1, sample, 0.061f * std::sin(phase + 0.37f)
                                     - 0.011f * std::sin(phase * 2.73f));
    }
}

void restore(juce::AudioBuffer<float>& destination,
             const juce::AudioBuffer<float>& source)
{
    for (int channel = 0; channel < 2; ++channel)
        juce::FloatVectorOperations::copy(destination.getWritePointer(channel),
                                          source.getReadPointer(channel),
                                          source.getNumSamples());
}

double median(std::array<double, 7> values)
{
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

double benchmark(Scenario scenario, int blockSize)
{
    constexpr int samplesPerTrial = 1 << 20;
    const int blocksPerTrial = std::max(1, samplesPerTrial / blockSize);
    auto processor = makeProcessor(scenario, blockSize);
    juce::AudioBuffer<float> source(2, blockSize), audio(2, blockSize);
    juce::MidiBuffer midi;
    fillSource(source);

    for (int warmup = 0; warmup < 256; ++warmup)
    {
        restore(audio, source);
        processor->processBlock(audio, midi);
    }

    std::array<double, 7> measured {};
    std::array<double, 7> restoreOnly {};
    volatile float checksum = 0.0f;
    for (size_t trial = 0; trial < measured.size(); ++trial)
    {
        auto start = std::chrono::steady_clock::now();
        for (int block = 0; block < blocksPerTrial; ++block)
        {
            restore(audio, source);
            processor->processBlock(audio, midi);
        }
        measured[trial] = std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - start).count();
        checksum += audio.getSample(0, blockSize - 1);

        start = std::chrono::steady_clock::now();
        for (int block = 0; block < blocksPerTrial; ++block)
            restore(audio, source);
        restoreOnly[trial] = std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - start).count();
        checksum += audio.getSample(1, blockSize - 1);
    }
    if (!std::isfinite(checksum))
        std::printf("invalid benchmark checksum\n");
    return std::max(0.0, median(measured) - median(restoreOnly))
        / (double)(blocksPerTrial * blockSize);
}
}

int main()
{
    constexpr std::array<int, 4> blockSizes { 32, 64, 128, 512 };
    constexpr std::array<Scenario, 12> scenarios {
        Scenario::clean, Scenario::outputOnly, Scenario::oneBell,
        Scenario::eightBells, Scenario::oneBellMidSide,
        Scenario::eightBellsAnalyzer, Scenario::oneClassicCut48,
        Scenario::oneTilt, Scenario::oneDynamicBell, Scenario::eightBellsOneDynamic,
        Scenario::oneDrive1x, Scenario::oneDrive4x
    };
    std::printf("default_eq processor benchmark, 48 kHz stereo, ns/sample\n");
    std::printf("scenario             32       64      128      512\n");
    for (const auto scenario : scenarios)
    {
        std::printf("%-18s", scenarioName(scenario));
        for (const auto blockSize : blockSizes)
            std::printf(" %8.2f", benchmark(scenario, blockSize));
        std::printf("\n");
    }
    return 0;
}
