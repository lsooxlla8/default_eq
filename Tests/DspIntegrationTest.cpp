#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/UI/DriveCharacterFormatting.h"
#include "../Source/DSP/FilterTypes.h"
#include "../Source/UI/ResponseCurveComponent.h"
#include <cmath>
#include <chrono>
#include <cstdio>
#include <memory>
#include <limits>
#include <vector>

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL: %s\n", m); ++failures; } } while (0)

juce::String id(int band, const char* suffix)
{
    return "b" + juce::String(band) + "_" + suffix;
}

void setPlain(DefaultEqualizerAudioProcessor& p, const juce::String& parameterID, float value)
{
    auto* parameter = p.apvts.getParameter(parameterID);
    CHECK(parameter != nullptr, "parameter exists");
    if (parameter != nullptr)
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void activateBand(DefaultEqualizerAudioProcessor& p, int band = 1)
{
    setPlain(p, id(band, "present"), 1.0f);
    setPlain(p, id(band, "on"), 1.0f);
    setPlain(p, id(band, "type"), 5.0f);
}

void enableSidechain(DefaultEqualizerAudioProcessor& p)
{
    auto layout = p.getBusesLayout();
    if (layout.inputBuses.size() > 1)
        layout.inputBuses.set(1, juce::AudioChannelSet::stereo());
    CHECK(p.setBusesLayout(layout), "external sidechain layout accepted");
}

float runDynamic(bool externalSignal)
{
    DefaultEqualizerAudioProcessor p;
    setPlain(p, "auto_gain_mode", 0.0f);
    activateBand(p);
    enableSidechain(p);
    setPlain(p, id(1, "type"), 5.0f);
    setPlain(p, id(1, "freq"), 1000.0f);
    setPlain(p, id(1, "gain"), 12.0f);
    setPlain(p, id(1, "q"), 2.0f);
    // Dynamic processing is structurally active; threshold 0 dB is neutral.
    setPlain(p, id(1, "dyn_mode"), 0.0f);
    setPlain(p, id(1, "sc_source"), 1.0f);
    setPlain(p, id(1, "dyn_thresh"), -45.0f);
    setPlain(p, id(1, "dyn_range"), 12.0f);
    setPlain(p, id(1, "dyn_speed"), 100.0f);
    for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
    p.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer(4, 256);
    juce::MidiBuffer midi;
    double phase = 0.0;
    float result = 0.0f;
    for (int block = 0; block < 120; ++block)
    {
        buffer.clear();
        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float main = 0.05f * std::sin((float)phase);
            const float sc = externalSignal ? 0.9f * std::sin((float)phase) : 0.0f;
            phase += 2.0 * juce::MathConstants<double>::pi * 1000.0 / 48000.0;
            buffer.setSample(0, n, main); buffer.setSample(1, n, main);
            buffer.setSample(2, n, sc); buffer.setSample(3, n, sc);
        }
        p.processBlock(buffer, midi);
        if (block > 100)
            for (int n = 0; n < buffer.getNumSamples(); ++n)
                result += std::abs(buffer.getSample(0, n));
    }
    return result;
}

std::pair<float, float> routedDetectorLevels(int placementMode, float placement,
                                             bool antiPhase, bool leftOnly,
                                             bool externalSidechain = false,
                                             bool rightOnly = false)
{
    DefaultEqualizerAudioProcessor p;
    activateBand(p);
    if (externalSidechain) enableSidechain(p);
    for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
    setPlain(p, id(1, "freq"), 1000.0f);
    setPlain(p, id(1, "q"), 2.0f);
    setPlain(p, id(1, "placement_mode"), (float)placementMode);
    setPlain(p, id(1, "placement"), placement);
    setPlain(p, id(1, "dyn_thresh"), 0.0f);
    setPlain(p, id(1, "sc_source"), externalSidechain ? 1.0f : 0.0f);
    p.uiMeterBand.store(0, std::memory_order_relaxed);
    p.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> block(externalSidechain ? 4 : 2, 256);
    juce::MidiBuffer midi;
    double phase = 0.0;
    for (int pass = 0; pass < 12; ++pass)
    {
        block.clear();
        for (int sample = 0; sample < block.getNumSamples(); ++sample)
        {
            const float x = 0.5f * std::sin((float)phase);
            phase += juce::MathConstants<double>::twoPi * 1000.0 / 48000.0;
            const float l = rightOnly ? 0.0f : x;
            const float r = rightOnly ? x : (leftOnly ? 0.0f : (antiPhase ? -x : x));
            const int offset = externalSidechain ? 2 : 0;
            block.setSample(offset, sample, l);
            block.setSample(offset + 1, sample, r);
        }
        p.processBlock(block, midi);
    }
    return p.getBandDetectorLevelsDb(0);
}

float steadyTSDetectorLevel(float placement, bool externalSidechain)
{
    DefaultEqualizerAudioProcessor p;
    activateBand(p);
    if (externalSidechain) enableSidechain(p);
    for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
    setPlain(p, id(1, "freq"), 1000.0f);
    setPlain(p, id(1, "q"), 2.0f);
    setPlain(p, id(1, "placement_mode"), 2.0f);
    setPlain(p, id(1, "placement"), placement);
    setPlain(p, id(1, "dyn_thresh"), 0.0f);
    setPlain(p, id(1, "sc_source"), externalSidechain ? 1.0f : 0.0f);
    p.uiMeterBand.store(0, std::memory_order_relaxed);
    p.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> block(externalSidechain ? 4 : 2, 256);
    juce::MidiBuffer midi;
    double phase = 0.0;
    double accumulatedLinear = 0.0;
    int measured = 0;
    for (int pass = 0; pass < 100; ++pass)
    {
        block.clear();
        for (int sample = 0; sample < block.getNumSamples(); ++sample)
        {
            const float x = 0.5f * std::sin((float)phase);
            phase += juce::MathConstants<double>::twoPi * 1000.0 / 48000.0;
            const int offset = externalSidechain ? 2 : 0;
            block.setSample(offset, sample, x);
            block.setSample(offset + 1, sample, x);
        }
        p.processBlock(block, midi);
        if (pass >= 80)
        {
            accumulatedLinear += juce::Decibels::decibelsToGain(p.getBandDetectorLevelDb(0));
            ++measured;
        }
    }
    return (float)(accumulatedLinear / std::max(1, measured));
}

double measureDriveAlias(int oversamplingOrder)
{
    constexpr int fftOrder = 12, fftSize = 1 << fftOrder;
    constexpr double sampleRate = 48000.0;
    constexpr int fundamentalBin = 85;
    const double frequency = fundamentalBin * sampleRate / fftSize;
    DefaultEqualizerAudioProcessor p;
    activateBand(p);
    for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
    setPlain(p, id(1, "on"), 1.0f); setPlain(p, id(1, "gain"), 0.0f);
    setPlain(p, id(1, "drive"), 36.0f);
    setPlain(p, id(1, "sat_mode"), (float)SaturationType::FET);
    setPlain(p, "oversampling", (float)oversamplingOrder);
    p.prepareToPlay(sampleRate, 256);
    juce::AudioBuffer<float> block(2, 256); juce::MidiBuffer midi;
    std::array<float, fftSize * 2> fftData {};
    double phase = 0.0;
    int capture = 0;
    for (int blockIndex = 0; blockIndex < 40; ++blockIndex)
    {
        for (int i = 0; i < 256; ++i)
        {
            const float sample = 0.9f * std::sin((float)phase);
            phase += juce::MathConstants<double>::twoPi * frequency / sampleRate;
            block.setSample(0, i, sample); block.setSample(1, i, sample);
        }
        p.processBlock(block, midi);
        if (blockIndex >= 24)
            for (int i = 0; i < 256; ++i) fftData[(size_t)capture++] = block.getSample(0, i);
    }
    juce::dsp::FFT fft(fftOrder);
    fft.performFrequencyOnlyForwardTransform(fftData.data());
    double aliasEnergy = 0.0;
    for (int bin = 2; bin < fftSize / 2; ++bin)
    {
        bool harmonic = false;
        for (int h = 1; h * fundamentalBin < fftSize / 2; ++h)
            if (std::abs(bin - h * fundamentalBin) <= 2) { harmonic = true; break; }
        if (!harmonic) aliasEnergy += (double)fftData[(size_t)bin] * fftData[(size_t)bin];
    }
    return aliasEnergy;
}

double dynamicLevelForBlockSize(int blockSize)
{
    DefaultEqualizerAudioProcessor p;
    activateBand(p);
    enableSidechain(p);
    for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
    setPlain(p, id(1, "freq"), 1000.0f); setPlain(p, id(1, "gain"), 10.0f);
    setPlain(p, id(1, "sc_source"), 1.0f);
    setPlain(p, id(1, "dyn_thresh"), -35.0f); setPlain(p, id(1, "dyn_speed"), 60.0f);
    p.prepareToPlay(48000.0, blockSize);
    juce::AudioBuffer<float> audio(4, blockSize); juce::MidiBuffer midi;
    constexpr int total = 48000; double phase = 0.0, sum = 0.0; int counted = 0;
    for (int offset = 0; offset < total; offset += blockSize)
    {
        const int valid = std::min(blockSize, total - offset); audio.clear();
        for (int i = 0; i < valid; ++i)
        {
            const float main = 0.08f * std::sin((float)phase);
            const float sc = 0.8f * std::sin((float)phase);
            phase += juce::MathConstants<double>::twoPi * 1000.0 / 48000.0;
            audio.setSample(0, i, main); audio.setSample(1, i, main);
            audio.setSample(2, i, sc); audio.setSample(3, i, sc);
        }
        p.processBlock(audio, midi);
        if (offset >= total / 2)
            for (int i = 0; i < valid; ++i) { sum += std::abs(audio.getSample(0, i)); ++counted; }
    }
    return sum / (double)counted;
}

double renderCutLevel(float slope)
{
    DefaultEqualizerAudioProcessor p;
    activateBand(p);
    for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
    setPlain(p, id(1, "type"), 9.0f); setPlain(p, id(1, "freq"), 1000.0f);
    setPlain(p, id(1, "slope"), slope); p.prepareToPlay(48000.0, 127);
    juce::AudioBuffer<float> block(2, 127); juce::MidiBuffer midi;
    double phase = 0.0, energy = 0.0; int count = 0;
    for (int bi = 0; bi < 160; ++bi)
    {
        for (int n = 0; n < 127; ++n)
        {
            const float x = 0.2f * std::sin((float)phase);
            phase += juce::MathConstants<double>::twoPi * 100.0 / 48000.0;
            block.setSample(0, n, x); block.setSample(1, n, x);
        }
        p.processBlock(block, midi);
        if (bi > 120)
            for (int n = 0; n < 127; ++n) { const double x = block.getSample(0, n); energy += x * x; ++count; }
    }
    return std::sqrt(energy / std::max(1, count));
}

double renderStaticBandLevel(int type, float q, float slope, float gainDb, double probeHz,
                             float amount = 1.0f, float shiftSemitones = 0.0f)
{
    DefaultEqualizerAudioProcessor p;
    setPlain(p, "auto_gain_mode", 0.0f);
    activateBand(p);
    for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
    setPlain(p, id(1, "type"), (float)type);
    setPlain(p, id(1, "freq"), 1000.0f);
    setPlain(p, id(1, "q"), q);
    setPlain(p, id(1, "slope"), slope);
    setPlain(p, id(1, "gain"), gainDb);
    setPlain(p, "scale", amount);
    setPlain(p, "shift", shiftSemitones);
    p.prepareToPlay(48000.0, 127);
    juce::AudioBuffer<float> block(2, 127); juce::MidiBuffer midi;
    double phase = 0.0, energy = 0.0; int count = 0;
    for (int bi = 0; bi < 180; ++bi)
    {
        for (int n = 0; n < 127; ++n)
        {
            const float x = 0.1f * std::sin((float)phase);
            phase += juce::MathConstants<double>::twoPi * probeHz / 48000.0;
            block.setSample(0, n, x); block.setSample(1, n, x);
        }
        p.processBlock(block, midi);
        if (bi > 140)
            for (int n = 0; n < 127; ++n)
            {
                const double x = block.getSample(0, n);
                energy += x * x; ++count;
            }
    }
    return std::sqrt(energy / std::max(1, count));
}

struct AutoGainRender
{
    double rms = 0.0;
    double peak = 0.0;
    float compensationDb = 0.0f;
};

enum class TestSpectrum { Balanced, BassHeavy, Bright, DynamicMusic };

struct AutoGainScenario
{
    const char* name;
    int type;
    float frequency;
    float q;
    float slope;
    float gainDb;
    TestSpectrum spectrum;
};

AutoGainRender renderAutoGainScenario(int autoMode, const AutoGainScenario& scenario,
                                      bool neutralReference = false, float outputDb = 0.0f)
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr std::array<double, 9> frequencies {
        40.0, 80.0, 160.0, 320.0, 640.0, 1280.0, 2560.0, 5120.0, 10240.0
    };
    DefaultEqualizerAudioProcessor p;
    setPlain(p, "auto_gain_mode", (float)autoMode);
    setPlain(p, "output_gain", outputDb);
    if (!neutralReference)
    {
        activateBand(p);
        setPlain(p, id(1, "type"), (float)scenario.type);
        setPlain(p, id(1, "freq"), scenario.frequency);
        setPlain(p, id(1, "q"), scenario.q);
        setPlain(p, id(1, "slope"), scenario.slope);
        setPlain(p, id(1, "gain"), scenario.gainDb);
    }
    p.prepareToPlay(sampleRate, blockSize);

    juce::AudioBuffer<float> block(2, blockSize);
    juce::MidiBuffer midi;
    std::array<double, frequencies.size()> phases {};
    double energy = 0.0, peak = 0.0;
    int samplesMeasured = 0;
    const int totalBlocks = autoMode == 2 ? 1500 : 500;
    const int measureStart = totalBlocks - 200;
    for (int blockIndex = 0; blockIndex < totalBlocks; ++blockIndex)
    {
        for (int sample = 0; sample < blockSize; ++sample)
        {
            float value = 0.0f;
            const double absoluteTime = (double)(blockIndex * blockSize + sample) / sampleRate;
            for (size_t tone = 0; tone < frequencies.size(); ++tone)
            {
                float spectralWeight = scenario.spectrum == TestSpectrum::BassHeavy
                    ? std::pow(0.77f, (float)tone)
                    : scenario.spectrum == TestSpectrum::Bright
                        ? std::pow(0.77f, (float)(frequencies.size() - 1 - tone))
                        : 1.0f;
                if (scenario.spectrum == TestSpectrum::DynamicMusic)
                {
                    const float phrase = 0.35f + 0.65f * std::pow(
                        0.5f + 0.5f * std::sin((float)(absoluteTime * (0.55 + 0.07 * tone))), 2.0f);
                    const double beatPhase = std::fmod(absoluteTime, 0.5);
                    const float transient = tone < 3
                        ? (float)std::exp(-beatPhase * (18.0 + 3.0 * tone)) : 0.0f;
                    spectralWeight = std::pow(0.86f, (float)tone)
                        * (phrase + 1.8f * transient);
                }
                value += 0.003f * spectralWeight * std::sin((float)phases[tone]);
                phases[tone] += juce::MathConstants<double>::twoPi
                    * frequencies[tone] / sampleRate;
                if (phases[tone] >= juce::MathConstants<double>::twoPi)
                    phases[tone] -= juce::MathConstants<double>::twoPi;
            }
            block.setSample(0, sample, value);
            block.setSample(1, sample, value);
        }
        p.processBlock(block, midi);
        if (blockIndex >= measureStart)
            for (int sample = 0; sample < blockSize; ++sample)
            {
                const double value = block.getSample(0, sample);
                energy += value * value;
                peak = std::max(peak, std::abs(value));
                ++samplesMeasured;
            }
    }
    return { std::sqrt(energy / std::max(1, samplesMeasured)), peak,
             p.autoGainCompDb.load(std::memory_order_relaxed) };
}

std::pair<double, double> renderLiveAmountChange()
{
    DefaultEqualizerAudioProcessor p;
    activateBand(p);
    for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
    setPlain(p, id(1, "type"), 5.0f);
    setPlain(p, id(1, "freq"), 1000.0f);
    setPlain(p, id(1, "gain"), 12.0f);
    setPlain(p, "scale", 1.0f);
    p.prepareToPlay(48000.0, 127);

    juce::AudioBuffer<float> block(2, 127);
    juce::MidiBuffer midi;
    double phase = 0.0;
    const auto render = [&](int settleBlocks)
    {
        double energy = 0.0;
        int count = 0;
        for (int bi = 0; bi < settleBlocks + 40; ++bi)
        {
            for (int n = 0; n < block.getNumSamples(); ++n)
            {
                const float x = 0.1f * std::sin((float)phase);
                phase += juce::MathConstants<double>::twoPi * 1000.0 / 48000.0;
                block.setSample(0, n, x); block.setSample(1, n, x);
            }
            p.processBlock(block, midi);
            if (bi >= settleBlocks)
                for (int n = 0; n < block.getNumSamples(); ++n)
                {
                    const double x = block.getSample(0, n);
                    energy += x * x;
                    ++count;
                }
        }
        return std::sqrt(energy / std::max(1, count));
    };

    const double full = render(140);
    setPlain(p, "scale", 0.25f);
    const double quarter = render(80);
    return { full, quarter };
}

double renderCutAmountOneToZeroMaximumStep()
{
    DefaultEqualizerAudioProcessor p;
    activateBand(p);
    for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
    setPlain(p, id(1, "type"), (float)deq::filter_types::highCut);
    setPlain(p, id(1, "freq"), 1000.0f);
    setPlain(p, id(1, "slope"), 48.0f);
    setPlain(p, "scale", 0.01f);
    p.prepareToPlay(48000.0, 64);

    juce::AudioBuffer<float> block(2, 64);
    juce::MidiBuffer midi;
    float previous = 0.0f;
    for (int bi = 0; bi < 400; ++bi)
    {
        block.clear();
        for (int n = 0; n < block.getNumSamples(); ++n)
            block.setSample(0, n, 0.1f), block.setSample(1, n, 0.1f);
        p.processBlock(block, midi);
        previous = block.getSample(0, block.getNumSamples() - 1);
    }

    setPlain(p, "scale", 0.0f);
    double maximumStep = 0.0;
    for (int bi = 0; bi < 20; ++bi)
    {
        for (int n = 0; n < block.getNumSamples(); ++n)
            block.setSample(0, n, 0.1f), block.setSample(1, n, 0.1f);
        p.processBlock(block, midi);
        for (int n = 0; n < block.getNumSamples(); ++n)
        {
            const float current = block.getSample(0, n);
            maximumStep = std::max(maximumStep, (double)std::abs(current - previous));
            previous = current;
        }
    }
    return maximumStep;
}

double renderRapidLowBandDragMaximumStep(int type)
{
    constexpr int blockSize = 64;
    DefaultEqualizerAudioProcessor p;
    setPlain(p, "auto_gain_mode", 0.0f);
    activateBand(p);
    for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
    setPlain(p, id(1, "type"), (float)type);
    setPlain(p, id(1, "freq"), 45.0f);
    setPlain(p, id(1, "q"), 8.0f);
    setPlain(p, id(1, "gain"), 30.0f);
    p.prepareToPlay(48000.0, blockSize);

    juce::AudioBuffer<float> block(2, blockSize);
    juce::MidiBuffer midi;
    std::array<double, 3> phase {};
    constexpr std::array<double, 3> frequencies { 31.0, 67.0, 137.0 };
    float previous = 0.0f;
    double maximumStep = 0.0;
    for (int bi = 0; bi < 700; ++bi)
    {
        // Faster than a normal UI update cadence and deliberately reverses
        // both axes. This catches coefficient replacement transients without
        // relying on a DAW or the editor event loop.
        const float motion = 0.5f + 0.5f * std::sin((float)bi * 0.71f);
        setPlain(p, id(1, "freq"), 25.0f * std::pow(16.0f, motion));
        if (type == deq::filter_types::bell)
            setPlain(p, id(1, "gain"), -30.0f + 60.0f * motion);
        else if (ResponseCurveComponent::isClassicCutType(type))
            setPlain(p, id(1, "slope"), 3.0f + 93.0f * motion);
        else
            setPlain(p, id(1, "q"), 0.2f * std::pow(60.0f, motion));
        for (int sample = 0; sample < blockSize; ++sample)
        {
            float value = 0.0f;
            for (size_t tone = 0; tone < frequencies.size(); ++tone)
            {
                value += 0.012f * std::sin((float)phase[tone]);
                phase[tone] += juce::MathConstants<double>::twoPi
                    * frequencies[tone] / 48000.0;
            }
            block.setSample(0, sample, value);
            block.setSample(1, sample, value);
        }
        p.processBlock(block, midi);
        if (bi > 100)
            for (int sample = 0; sample < blockSize; ++sample)
            {
                const float current = block.getSample(0, sample);
                maximumStep = std::max(maximumStep,
                    (double)std::abs(current - previous));
                previous = current;
            }
        else
            previous = block.getSample(0, blockSize - 1);
    }
    return maximumStep;
}

double renderRegularAutoGainChangeMaximumStep()
{
    constexpr int blockSize = 64;
    DefaultEqualizerAudioProcessor p;
    setPlain(p, "auto_gain_mode", 1.0f);
    activateBand(p);
    for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
    setPlain(p, id(1, "type"), (float)deq::filter_types::lowShelf);
    setPlain(p, id(1, "freq"), 200.0f);
    setPlain(p, id(1, "gain"), 0.0f);
    p.prepareToPlay(48000.0, blockSize);

    juce::AudioBuffer<float> block(2, blockSize);
    juce::MidiBuffer midi;
    float previous = 0.01f;
    double maximumStep = 0.0;
    for (int bi = 0; bi < 180; ++bi)
    {
        if (bi == 80) setPlain(p, id(1, "gain"), 36.0f);
        for (int sample = 0; sample < blockSize; ++sample)
            block.setSample(0, sample, 0.01f), block.setSample(1, sample, 0.01f);
        p.processBlock(block, midi);
        if (bi >= 80)
            for (int sample = 0; sample < blockSize; ++sample)
            {
                const float current = block.getSample(0, sample);
                maximumStep = std::max(maximumStep,
                    (double)std::abs(current - previous));
                previous = current;
            }
        else
            previous = block.getSample(0, blockSize - 1);
    }
    return maximumStep;
}

double benchmarkProcessorNsPerSample(int activeBands)
{
    constexpr int blockSize = 512;
    constexpr int blocksPerTrial = 3000;
    DefaultEqualizerAudioProcessor p;
    setPlain(p, "auto_gain_mode", 1.0f);
    for (int band = 1; band <= activeBands; ++band)
    {
        activateBand(p, band);
        setPlain(p, id(band, "type"), (float)deq::filter_types::bell);
        setPlain(p, id(band, "freq"), 80.0f * std::pow(2.0f, (float)band));
        setPlain(p, id(band, "gain"), band % 2 == 0 ? -6.0f : 6.0f);
    }
    p.prepareToPlay(48000.0, blockSize);
    juce::AudioBuffer<float> block(2, blockSize);
    juce::MidiBuffer midi;
    std::array<double, 5> trials {};
    double phase = 0.0;
    for (size_t trial = 0; trial < trials.size(); ++trial)
    {
        const auto start = std::chrono::steady_clock::now();
        for (int bi = 0; bi < blocksPerTrial; ++bi)
        {
            for (int sample = 0; sample < blockSize; ++sample)
            {
                const float value = 0.05f * std::sin((float)phase);
                phase += juce::MathConstants<double>::twoPi * 997.0 / 48000.0;
                block.setSample(0, sample, value);
                block.setSample(1, sample, value * 0.91f);
            }
            p.processBlock(block, midi);
        }
        const auto elapsed = std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - start).count();
        trials[trial] = elapsed / (double)(blocksPerTrial * blockSize);
    }
    std::sort(trials.begin(), trials.end());
    return trials[trials.size() / 2];
}
}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("DSP integration: initialise JUCE\n");
    std::printf("DSP integration: defaults\n");
    CHECK(kNumBands == 8, "product exposes exactly eight bands");
    {
        DefaultEqualizerAudioProcessor fresh;
        CHECK(std::abs(fresh.apvts.getRawParameterValue("auto_gain_mode")->load() - 1.0f) < 0.01f,
              "Regular Auto Gain is the new-instance default");
        CHECK(std::abs(fresh.apvts.getRawParameterValue("scale")->load() - 1.0f) < 0.01f,
              "global Amount defaults to 100 percent");
        CHECK(std::abs(fresh.apvts.getRawParameterValue("shift")->load()) < 0.01f,
              "global frequency Shift defaults to zero semitones");
        if (auto* shift = fresh.apvts.getParameter("shift"))
            CHECK(shift->getText(shift->convertTo0to1(12.0f), 32) == "+12.00",
                  "Shift parameter text contains only its name and numeric value");
        if (auto* amount = fresh.apvts.getParameter("scale"))
            CHECK(amount->getText(amount->convertTo0to1(0.00999996f), 32) == "1%",
                  "Amount text rounds host float noise to an integer percent");
        {
            DefaultEqualizerAudioProcessorEditor editor(fresh);
            NumericValueControl* amountControl = nullptr;
            NumericValueControl* shiftControl = nullptr;
            NumericValueControl* outputControl = nullptr;
            for (int child = 0; child < editor.getNumChildComponents(); ++child)
                if (auto* numeric = dynamic_cast<NumericValueControl*>(editor.getChildComponent(child)))
                {
                    if (numeric->getName() == "AMOUNT") amountControl = numeric;
                    if (numeric->getName() == "SHIFT") shiftControl = numeric;
                    if (numeric->getName() == "OUT") outputControl = numeric;
                }
            auto* amountParameter = fresh.apvts.getParameter("scale");
            auto* shiftParameter = fresh.apvts.getParameter("shift");
            auto* outputParameter = fresh.apvts.getParameter("output_gain");
            CHECK(amountControl != nullptr && amountParameter != nullptr
                      && editor.getControlParameterIndex(*amountControl)
                          == amountParameter->getParameterIndex()
                      && amountControl->getNumChildComponents() > 0
                      && editor.getControlParameterIndex(*amountControl->getChildComponent(0))
                          == amountParameter->getParameterIndex(),
                  "VST3 parameter finder resolves custom controls and their child labels");
            CHECK(shiftControl != nullptr && shiftParameter != nullptr
                      && editor.getControlParameterIndex(*shiftControl)
                          == shiftParameter->getParameterIndex()
                      && outputControl != nullptr && outputParameter != nullptr
                      && editor.getControlParameterIndex(*outputControl)
                          == outputParameter->getParameterIndex(),
                  "VST3 parameter finder resolves the new Shift and relocated Out controls");
        }
        CHECK(std::abs(fresh.apvts.getRawParameterValue(id(1, "dyn_speed"))->load() - 75.0f) < 0.01f,
              "dynamic Speed defaults to 75 percent");
        CHECK(std::abs(fresh.apvts.getRawParameterValue(id(1,"type"))->load()-5.0f)<0.01f,
              "fresh band slots default to Bell in the reordered filter list");
        if (auto* saturation = fresh.apvts.getParameter(id(1, "sat_mode")))
        {
            const auto choices = saturation->getAllValueStrings();
            CHECK(choices.size() == kSaturationModeCount
                      && choices.contains("Soft Clip") && choices.contains("Diode")
                      && choices.contains("Triode") && choices.contains("Transistor")
                      && choices.contains("Tape") && choices.contains("Odd / Even")
                      && choices.contains("Phase Distortion") && choices.contains("Sine Erosion")
                      && !choices.contains("Hard Clip") && !choices.contains("Spectral Clip"),
                  "published saturation parameter exposes exactly the retained eight modes");
        }
        CHECK(std::abs(fresh.apvts.getRawParameterValue("transient_split_strength")->load()-100.0f)<0.01f
              && std::abs(fresh.apvts.getRawParameterValue("transient_split_balance")->load())<0.01f
              && std::abs(fresh.apvts.getRawParameterValue("transient_split_hold")->load()-50.0f)<0.01f
              && std::abs(fresh.apvts.getRawParameterValue("transient_split_smooth")->load()-50.0f)<0.01f,
              "hidden Transient Split parameters expose the requested defaults");
        const auto amountRange = fresh.apvts.getParameterRange("scale");
        CHECK(std::abs(amountRange.start + 2.0f) < 0.01f && std::abs(amountRange.end - 2.0f) < 0.01f,
              "global Amount exposes the full -200 to 200 percent range");
        for (int band = 1; band <= kNumBands; ++band)
            CHECK(fresh.apvts.getRawParameterValue(id(band, "present"))->load() < 0.5f
                      && fresh.apvts.getRawParameterValue(id(band, "on"))->load() < 0.5f,
                  "a fresh instance starts with no graph bands");
    }
    CHECK(std::abs(DefaultEqualizerAudioProcessor::calculateAdaptiveQ(1.25f, 8.0f) - 2.45f) < 1.0e-6f,
          "Adaptive Q follows the documented deterministic formula");
    CHECK(std::abs(DefaultEqualizerAudioProcessor::frequencyShiftRatio(12.0f) - 2.0f) < 1.0e-6f
              && std::abs(DefaultEqualizerAudioProcessor::shiftedFrequency(110.0f, 12.0f) - 220.0f) < 1.0e-4f
              && std::abs(DefaultEqualizerAudioProcessor::shiftedFrequency(220.0f, 12.0f) - 440.0f) < 1.0e-4f,
          "one-octave Shift doubles every band frequency and preserves harmonic ratios");
    CHECK(std::abs(EQBand::amountResonantCutQ(4.0f, 0.0f) - 0.75f) < 1.0e-6f
              && std::abs(EQBand::amountResonantCutQ(0.2f, 0.0f) - 0.75f) < 1.0e-6f
              && std::abs(EQBand::amountResonantCutQ(4.0f, 1.0f) - 4.0f) < 1.0e-6f,
          "resonant-cut Amount approaches Q 0.75 from either side");
    {
        DriveCharacterSlider character;
        character.setSaturationMode((int)SaturationType::SoftClip);
        const auto zeroCharacter = character.getTextFromValue(0.0);
        const auto midCharacter = character.getTextFromValue(0.364);
        CHECK(zeroCharacter == "00%" && midCharacter == "36%",
              "Character percent label has a fixed integer-width zero form");
        character.setSaturationMode((int)SaturationType::SineErosion);
        const auto lowSine = character.getTextFromValue(std::sqrt(0.139876) * 0.5);
        const auto highSine = character.getTextFromValue(0.5 * (1.0 + std::log10(1.25)));
        if (zeroCharacter != "00%" || midCharacter != "36%"
            || lowSine != "140 Hz" || highSine != "1.25 kHz")
            std::printf("character formatter actual: '%s', '%s', '%s', '%s'\n",
                        zeroCharacter.toRawUTF8(), midCharacter.toRawUTF8(),
                        lowSine.toRawUTF8(), highSine.toRawUTF8());
        CHECK(lowSine == "140 Hz" && highSine == "1.25 kHz",
              "Sine Erosion label uses integer Hz below 1 kHz and two-decimal kHz above it");
        CHECK(deq::ui::formatDriveCharacter((int)SaturationType::SoftClip, 0.0400001) == "4%",
              "hover and slider share an integer-only Character formatter");
    }

    std::printf("DSP integration: auto gain\n");
    const auto levelDeltaDb = [](double level, double reference)
    {
        return 20.0 * std::log10(std::max(level, 1.0e-12) / std::max(reference, 1.0e-12));
    };
    const std::array<AutoGainScenario, 17> autoGainScenarios {{
        { "low shelf +12 bass", deq::filter_types::lowShelf, 160.0f, 1.0f, 12.0f, 12.0f, TestSpectrum::BassHeavy },
        { "low shelf +25 bass", deq::filter_types::lowShelf, 160.0f, 1.0f, 12.0f, 25.0f, TestSpectrum::BassHeavy },
        { "low shelf +36 bass", deq::filter_types::lowShelf, 160.0f, 1.0f, 12.0f, 36.0f, TestSpectrum::BassHeavy },
        { "low shelf -25 bass", deq::filter_types::lowShelf, 160.0f, 1.0f, 12.0f, -25.0f, TestSpectrum::BassHeavy },
        { "high shelf +12 bright", deq::filter_types::highShelf, 5000.0f, 1.0f, 12.0f, 12.0f, TestSpectrum::Bright },
        { "high shelf +25 bright", deq::filter_types::highShelf, 5000.0f, 1.0f, 12.0f, 25.0f, TestSpectrum::Bright },
        { "high shelf +36 bright", deq::filter_types::highShelf, 5000.0f, 1.0f, 12.0f, 36.0f, TestSpectrum::Bright },
        { "bell +12 wide", deq::filter_types::bell, 1000.0f, 0.7f, 12.0f, 12.0f, TestSpectrum::Balanced },
        { "bell +25 narrow", deq::filter_types::bell, 1000.0f, 4.0f, 12.0f, 25.0f, TestSpectrum::Balanced },
        { "bell +36 wide", deq::filter_types::bell, 1000.0f, 0.7f, 12.0f, 36.0f, TestSpectrum::Balanced },
        { "bell +36 narrow", deq::filter_types::bell, 1000.0f, 4.0f, 12.0f, 36.0f, TestSpectrum::Balanced },
        { "bell -25 wide", deq::filter_types::bell, 1000.0f, 0.7f, 12.0f, -25.0f, TestSpectrum::Balanced },
        { "tilt +12", deq::filter_types::tilt, 1000.0f, 1.0f, 12.0f, 12.0f, TestSpectrum::Balanced },
        { "low cut 200", deq::filter_types::lowCut, 200.0f, 1.0f, 48.0f, 0.0f, TestSpectrum::BassHeavy },
        { "high cut 3k", deq::filter_types::highCut, 3000.0f, 1.0f, 48.0f, 0.0f, TestSpectrum::Bright },
        { "notch 1k", deq::filter_types::notch, 1000.0f, 2.0f, 12.0f, 0.0f, TestSpectrum::Balanced },
        { "band pass 1k", deq::filter_types::bandPass, 1000.0f, 1.0f, 12.0f, 0.0f, TestSpectrum::Balanced }
    }};
    float worstRegularBoostPeak = 0.0f, worstSmartPositiveResidual = 0.0f;
    for (const auto& scenario : autoGainScenarios)
    {
        const auto clean = renderAutoGainScenario(0, scenario, true);
        const auto uncorrected = renderAutoGainScenario(0, scenario);
        const auto regular = renderAutoGainScenario(1, scenario);
        const auto smart = renderAutoGainScenario(2, scenario);
        const double offRmsDb = levelDeltaDb(uncorrected.rms, clean.rms);
        const double regularRmsDb = levelDeltaDb(regular.rms, clean.rms);
        const double smartRmsDb = levelDeltaDb(smart.rms, clean.rms);
        const double offPeakDb = levelDeltaDb(uncorrected.peak, clean.peak);
        const double regularPeakDb = levelDeltaDb(regular.peak, clean.peak);
        const double smartPeakDb = levelDeltaDb(smart.peak, clean.peak);
        worstSmartPositiveResidual = std::max(worstSmartPositiveResidual,
                                              (float)std::max(smartRmsDb, smartPeakDb));
        if (scenario.gainDb > 0.0f)
            worstRegularBoostPeak = std::max(worstRegularBoostPeak, (float)regularPeakDb);
        std::printf("%-23s RMS off/reg/smart %+6.2f/%+6.2f/%+6.2f, peak %+6.2f/%+6.2f/%+6.2f, comp %+6.2f/%+6.2f dB\n",
                    scenario.name, offRmsDb, regularRmsDb, smartRmsDb,
                    offPeakDb, regularPeakDb, smartPeakDb,
                    regular.compensationDb, smart.compensationDb);
        CHECK(smartRmsDb < 0.35 && smartPeakDb < 0.35,
              "Smart Gain leaves neither measured energy nor peak louder than the input");
        if (scenario.gainDb > 0.0f && offPeakDb > 0.5)
            CHECK(regularPeakDb <= 0.75 && regularPeakDb < offPeakDb,
                  "instant Regular Auto Gain prevents a positive-gain band from making peaks louder");
    }
    std::printf("auto-gain matrix: worst Regular boost peak %+0.2f dB, worst Smart positive residual %.2f dB\n",
                worstRegularBoostPeak, worstSmartPositiveResidual);

    // This scenario changes both spectral balance and crest factor over time.
    // It catches the old stationary-tone false confidence and the former 75%
    // Smart correction that left several dB of error on programme material.
    const AutoGainScenario dynamicMusic {
        "dynamic music low shelf", deq::filter_types::lowShelf, 160.0f, 1.0f,
        12.0f, 36.0f, TestSpectrum::DynamicMusic
    };
    const auto musicClean = renderAutoGainScenario(0, dynamicMusic, true);
    const auto musicOff = renderAutoGainScenario(0, dynamicMusic);
    const auto musicRegular = renderAutoGainScenario(1, dynamicMusic);
    const auto musicSmart = renderAutoGainScenario(2, dynamicMusic);
    const double musicOffPeakDb = levelDeltaDb(musicOff.peak, musicClean.peak);
    const double musicRegularPeakDb = levelDeltaDb(musicRegular.peak, musicClean.peak);
    const double musicSmartPeakDb = levelDeltaDb(musicSmart.peak, musicClean.peak);
    const double musicSmartRmsDb = levelDeltaDb(musicSmart.rms, musicClean.rms);
    std::printf("dynamic music peak off/reg/smart %+0.2f/%+0.2f/%+0.2f dB, Smart RMS %+0.2f dB\n",
                musicOffPeakDb, musicRegularPeakDb, musicSmartPeakDb, musicSmartRmsDb);
    CHECK(musicRegularPeakDb <= 0.75 && musicRegularPeakDb < musicOffPeakDb,
          "instant Regular Auto Gain bounds a changing music-like bass boost");
    CHECK(musicSmartRmsDb < 0.5 && musicSmartPeakDb < 0.5,
          "Smart Gain leaves neither energy nor peak louder after a changing +36 dB music-like boost");
    const auto& outputScenario = autoGainScenarios.front();
    const auto autoGainSmart = renderAutoGainScenario(2, outputScenario);
    const auto smartWithOutput = renderAutoGainScenario(2, outputScenario, false, 6.0f);
    CHECK(std::abs(smartWithOutput.compensationDb - autoGainSmart.compensationDb) < 0.25f
              && std::abs(levelDeltaDb(smartWithOutput.rms, autoGainSmart.rms) - 6.0) < 0.35,
          "Smart Gain leaves the manual Output control outside its compensation loop");

    std::printf("DSP integration: routing and unity\n");
    // Neutral 8-band centered L/R and M/S placement must both be unity.
    for (bool midSide : { false, true })
    {
        DefaultEqualizerAudioProcessor p;
        for (int band = 1; band <= kNumBands; ++band) activateBand(p, band);
        for (int b = 1; b <= kNumBands; ++b)
        {
            setPlain(p, id(b, "gain"), 0.0f);
            setPlain(p, id(b, "drive"), 0.0f);
            setPlain(p, id(b, "placement_mode"), midSide ? 1.0f : 0.0f);
            setPlain(p, id(b, "placement"), 0.0f);
        }
        p.prepareToPlay(48000.0, 257);
        juce::AudioBuffer<float> buffer(2, 257), reference(2, 257);
        for (int c = 0; c < 2; ++c)
            for (int n = 0; n < 257; ++n)
                buffer.setSample(c, n, 0.2f * std::sin(0.017f * (float)(n + c * 13)));
        reference.makeCopyOf(buffer);
        juce::MidiBuffer midi;
        p.processBlock(buffer, midi);
        float worst = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int n = 0; n < 257; ++n)
                worst = std::max(worst, std::abs(buffer.getSample(c, n) - reference.getSample(c, n)));
        CHECK(worst < 2.0e-5f, midSide ? "neutral centered M/S placement is unity" : "neutral centered L/R placement is unity");
    }

    // Continuous placement must preserve center and smoothly select either
    // component at its endpoints without changing the stored EQ settings.
    const auto renderPlacement = [](float placement, bool midSide)
    {
        DefaultEqualizerAudioProcessor p;
        // This isolates the per-band routing from the product-level default
        // compensation mode, which is covered independently above.
        setPlain(p, "auto_gain_mode", 0.0f);
        activateBand(p);
        for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
        setPlain(p, id(1, "freq"), 1000.0f); setPlain(p, id(1, "gain"), 12.0f);
        setPlain(p, id(1, "q"), 1.0f); setPlain(p, id(1, "drive"), 0.0f);
        setPlain(p, id(1, "placement_mode"), midSide ? 1.0f : 0.0f);
        setPlain(p, id(1, "placement"), placement);
        p.prepareToPlay(48000.0, 128);
        juce::AudioBuffer<float> block(2, 128); juce::MidiBuffer midi;
        double phase = 0.0, leftEnergy = 0.0, rightEnergy = 0.0; int count = 0;
        for (int bi = 0; bi < 80; ++bi)
        {
            for (int n = 0; n < block.getNumSamples(); ++n)
            {
                const float sample = 0.1f * std::sin((float)phase);
                phase += juce::MathConstants<double>::twoPi * 1000.0 / 48000.0;
                block.setSample(0, n, sample); block.setSample(1, n, sample);
            }
            p.processBlock(block, midi);
            if (bi >= 40)
                for (int n = 0; n < block.getNumSamples(); ++n)
                {
                    leftEnergy += (double)block.getSample(0, n) * block.getSample(0, n);
                    rightEnergy += (double)block.getSample(1, n) * block.getSample(1, n);
                    ++count;
                }
        }
        return std::pair<double, double>{ std::sqrt(leftEnergy / count), std::sqrt(rightEnergy / count) };
    };
    const auto placedLeft = renderPlacement(-100.0f, false);
    const auto placedCentre = renderPlacement(0.0f, false);
    const auto placedRight = renderPlacement(100.0f, false);
    CHECK(placedLeft.first > placedLeft.second * 1.8 && placedRight.second > placedRight.first * 1.8,
          "continuous L/R placement reaches the correct endpoints");
    CHECK(std::abs(placedCentre.first - placedCentre.second) < 1.0e-5,
          "continuous L/R placement is symmetric at center");
    const auto placedMid = renderPlacement(-100.0f, true);
    const auto placedSide = renderPlacement(100.0f, true);
    CHECK(placedMid.first > placedSide.first * 1.8,
          "continuous M/S placement distinguishes mono Mid from empty Side");

    const auto leftFromLeft = routedDetectorLevels(0, -100.0f, false, true);
    const auto leftFromRight = routedDetectorLevels(0, -100.0f, false, false, false, true);
    CHECK(leftFromLeft.first > -30.0f && leftFromLeft.second < -55.0f
              && std::max(leftFromRight.first, leftFromRight.second) < -55.0f,
          "full-left detector ignores the right channel while retaining the left channel");
    const auto sideFromMono = routedDetectorLevels(1, 100.0f, false, false);
    const auto sideFromAntiPhase = routedDetectorLevels(1, 100.0f, true, false);
    CHECK(std::max(sideFromMono.first, sideFromMono.second) < -55.0f
              && sideFromAntiPhase.second > -30.0f,
          "Side detector rejects Mid and responds to anti-phase Side content");
    const auto externalSideFromMono = routedDetectorLevels(1, 100.0f, false, false, true);
    const auto externalSideFromAntiPhase = routedDetectorLevels(1, 100.0f, true, false, true);
    CHECK(std::max(externalSideFromMono.first, externalSideFromMono.second) < -55.0f
              && externalSideFromAntiPhase.second > -30.0f,
          "external sidechain follows the selected M/S detector domain");

    const auto centreLRMono = routedDetectorLevels(0, 0.0f, false, false);
    const auto fullMidMono = routedDetectorLevels(1, -100.0f, false, false);
    const auto centreLRAnti = routedDetectorLevels(0, 0.0f, true, false);
    const auto fullSideAnti = routedDetectorLevels(1, 100.0f, true, false);
    const auto level = [](std::pair<float,float> value){return std::max(value.first,value.second);};
    CHECK(std::abs(level(centreLRMono)-level(fullMidMono))<0.5f
              &&std::abs(level(centreLRAnti)-level(fullSideAnti))<0.5f,
          "L/R and M/S detector thresholds use the same signal-level calibration");
    const auto externalCentreLRMono = routedDetectorLevels(0, 0.0f, false, false, true);
    const auto externalFullMidMono = routedDetectorLevels(1, -100.0f, false, false, true);
    CHECK(std::abs(level(externalCentreLRMono)-level(externalFullMidMono))<0.5f,
          "external L/R and M/S detector thresholds share the same calibration");

    const float internalTransientSteady = steadyTSDetectorLevel(-100.0f, false);
    const float internalSustainSteady = steadyTSDetectorLevel(100.0f, false);
    const float externalTransientSteady = steadyTSDetectorLevel(-100.0f, true);
    const float externalSustainSteady = steadyTSDetectorLevel(100.0f, true);
    CHECK(internalSustainSteady > internalTransientSteady * 1.5f,
          "T/S internal detector separates steady content before envelope following");
    CHECK(externalSustainSteady > externalTransientSteady * 1.5f,
          "T/S external sidechain is split before envelope following");

    std::printf("DSP integration: analyzer lifecycle\n");
    {
        DefaultEqualizerAudioProcessor p;
        activateBand(p);
        p.prepareToPlay(48000.0, 256);
        juce::AudioBuffer<float> block(2, 256); juce::MidiBuffer midi;
        block.clear();
        p.setAnalyzerEnabled(false);
        for (int i = 0; i < 40; ++i) p.processBlock(block, midi);
        CHECK(!p.spectrumFifo.processIfReady(), "hidden analyzer publishes no audio-side frames");
        p.setAnalyzerEnabled(true);
        for (int i = 0; i < 40; ++i) p.processBlock(block, midi);
        CHECK(p.spectrumFifo.processIfReady(), "visible analyzer publishes lock-free audio frames");
        p.spectrumFifo.reset();
        p.setAnalyzerEnabled(false);
        for (int i = 0; i < 40; ++i) p.processBlock(block, midi);
        CHECK(!p.spectrumFifo.processIfReady(), "disabled analyzer stops publishing immediately");
    }

    std::printf("DSP integration: dynamics and drive\n");
    const float quietSC = runDynamic(false);
    const float loudSC = runDynamic(true);
    CHECK(std::abs(loudSC - quietSC) > quietSC * 0.08f, "external sidechain changes selected dynamic band");
    std::printf("external sidechain accumulated output: silent %.3f, active %.3f\n", quietSC, loudSC);

    const double aliasOff = measureDriveAlias(0);
    const double alias8x = measureDriveAlias(3);
    CHECK(alias8x < aliasOff * 0.8, "8x oversampling measurably reduces nonlinear alias energy");
    std::printf("drive alias energy: off %.6g, 8x %.6g (%.2f%%)\n",
                aliasOff, alias8x, 100.0 * alias8x / aliasOff);

    const auto renderDriveLevel = [](bool enabled)
    {
        DefaultEqualizerAudioProcessor p;
        activateBand(p);
        for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
        setPlain(p, id(1, "freq"), 1000.0f); setPlain(p, id(1, "q"), 1.4f);
        setPlain(p, id(1, "gain"), 0.0f); setPlain(p, id(1, "sat_mode"), 1.0f);
        setPlain(p, id(1, "drive"), enabled ? 18.0f : 0.0f);
        p.prepareToPlay(48000.0, 128);
        juce::AudioBuffer<float> block(2, 128); juce::MidiBuffer midi;
        double phase = 0.0, energy = 0.0; int count = 0;
        for (int bi = 0; bi < 80; ++bi)
        {
            for (int n = 0; n < 128; ++n)
            {
                const float sample = 0.35f * std::sin((float)phase);
                phase += juce::MathConstants<double>::twoPi * 1000.0 / 48000.0;
                block.setSample(0, n, sample); block.setSample(1, n, sample);
            }
            p.processBlock(block, midi);
            if (bi >= 40)
                for (int n = 0; n < 128; ++n) { const double x = block.getSample(0, n); energy += x * x; ++count; }
        }
        return std::sqrt(energy / count);
    };
    const double driveCleanLevel = renderDriveLevel(false);
    const double driveCompLevel = renderDriveLevel(true);
    CHECK(std::isfinite(driveCompLevel) && driveCompLevel > 0.0
          && std::abs(driveCompLevel - driveCleanLevel) < driveCleanLevel,
          "always-on per-band table Auto Gain keeps driven level bounded");

    // Solo is a frequency-window audition, and per-band drive only returns
    // nonlinear energy from that same window instead of saturating broadband.
    const auto renderBandTone = [](double frequency, bool drive, bool solo)
    {
        DefaultEqualizerAudioProcessor p;
        setPlain(p, "auto_gain_mode", 0.0f);
        activateBand(p);
        for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
        setPlain(p, id(1, "freq"), 8000.0f); setPlain(p, id(1, "q"), 4.0f);
        setPlain(p, id(1, "drive"), drive ? 36.0f : 0.0f);
        setPlain(p, id(1, "sat_mode"), 1.0f);
        if (solo) p.soloBand.store(0);
        p.prepareToPlay(48000.0, 128);
        juce::AudioBuffer<float> block(2, 128); juce::MidiBuffer midi;
        double phase = 0.0, energy = 0.0; int count = 0;
        for (int bi = 0; bi < 160; ++bi)
        {
            for (int n = 0; n < 128; ++n)
            {
                const float x = 0.7f * std::sin((float)phase);
                phase += juce::MathConstants<double>::twoPi * frequency / 48000.0;
                block.setSample(0, n, x); block.setSample(1, n, x);
            }
            p.processBlock(block, midi);
            if (bi > 120)
                for (int n = 0; n < 128; ++n) { const double x = block.getSample(0, n); energy += x * x; ++count; }
        }
        return std::sqrt(energy / std::max(1, count));
    };
    const double soloInside = renderBandTone(8000.0, false, true);
    const double soloOutside = renderBandTone(100.0, false, true);
    CHECK(soloInside > soloOutside * 20.0, "band Solo rejects out-of-band program content");
    const double lowClean = renderBandTone(100.0, false, false);
    const double lowDriven = renderBandTone(100.0, true, false);
    const double highClean = renderBandTone(8000.0, false, false);
    const double highDriven = renderBandTone(8000.0, true, false);
    CHECK(std::abs(lowDriven - lowClean) < lowClean * 0.02,
          "high-band drive leaves low-frequency program essentially unchanged");
    CHECK(std::abs(highDriven - highClean) > highClean * 0.05,
          "high-band drive measurably changes in-band program");

    std::printf("DSP integration: block size and latency\n");
    {
        TransientSplitter splitter; splitter.prepare(48000.0,256); splitter.setParameters(100,0,50,50);
        juce::AudioBuffer<float> input(2,256),transient(2,256),sustain(2,256);
        std::array<float,2048> sum{}; int offset=0; double transientEnergy=0.0;
        for(int block=0;block<8;++block)
        {
            input.clear(); if(block==0){input.setSample(0,0,1);input.setSample(1,0,1);}
            splitter.process(input,transient,sustain,256);
            for(int i=0;i<256;++i){transientEnergy+=std::abs(transient.getSample(0,i));sum[(size_t)offset++]=transient.getSample(0,i)+sustain.getSample(0,i);}
        }
        int peak=0; for(int i=1;i<(int)sum.size();++i)if(std::abs(sum[(size_t)i])>std::abs(sum[(size_t)peak]))peak=i;
        CHECK(splitter.latency()==1536 && peak==1536 && std::abs(sum[(size_t)peak]-1.0f)<1.0e-5f,
              "T/S outputs are complementary and aligned to the reported ZL latency");
        CHECK(transientEnergy>0.01,"T/S median mask produces a non-empty transient branch");
    }
    const double dyn64 = dynamicLevelForBlockSize(64);
    const double dyn257 = dynamicLevelForBlockSize(257);
    const double dyn1024 = dynamicLevelForBlockSize(1024);
    const double dynSpread = (std::max({ dyn64, dyn257, dyn1024 }) - std::min({ dyn64, dyn257, dyn1024 })) / dyn257;
    CHECK(dynSpread < 0.015, "dynamic EQ is reproducible across 64/257/1024 sample blocks");
    std::printf("dynamic block-size levels: 64 %.7f, 257 %.7f, 1024 %.7f, spread %.3f%%\n",
                dyn64, dyn257, dyn1024, dynSpread * 100.0);

    // The cut slope is a continuous 3..48 dB/oct control. Each increase must
    // monotonically deepen rejection one decade below the corner.
    {
        double previous = std::numeric_limits<double>::infinity();
        for (float slope : { 3.0f, 6.0f, 9.0f, 12.0f, 18.0f, 24.0f, 36.0f, 48.0f })
        {
            const double level = renderCutLevel(slope);
            std::printf("cut slope %.1f dB/oct: %.9f RMS\n", slope, level);
            CHECK(previous < 2.0e-6 ? level < 2.0e-6 : level <= previous * 1.01,
                  "variable low-cut slope increases rejection monotonically until the numerical floor");
            previous = level;
        }
    }


    // Verify the processor output itself: cut resonance is real and non-cut
    // slope settings must produce externally measurable transfer changes.
    {
        const double neutralCut = renderStaticBandLevel(0, 1.0f, 24.0f, 0.0f, 1000.0);
        const double resonantCut = renderStaticBandLevel(0, 2.0f, 24.0f, 0.0f, 1000.0);
        CHECK(resonantCut > neutralCut * 1.8,
              "low-pass Q produces audible cutoff resonance in the processor path");
        const double bellCenterSoft = renderStaticBandLevel(5, 1.0f, 12.0f, 24.0f, 1000.0);
        const double bellCenterSteep = renderStaticBandLevel(5, 1.0f, 48.0f, 24.0f, 1000.0);
        const double bellShoulderSoft = renderStaticBandLevel(5, 1.0f, 12.0f, 24.0f, 800.0);
        const double bellShoulderSteep = renderStaticBandLevel(5, 1.0f, 48.0f, 24.0f, 800.0);
        const double shiftedBellCenter = renderStaticBandLevel(
            5, 1.0f, 12.0f, 24.0f, 2000.0, 1.0f, 12.0f);
        const double shiftedBellOldCenter = renderStaticBandLevel(
            5, 1.0f, 12.0f, 24.0f, 1000.0, 1.0f, 12.0f);
        CHECK(std::abs(20.0 * std::log10(shiftedBellCenter / bellCenterSoft)) < 0.15
                  && shiftedBellCenter > shiftedBellOldCenter * 2.0,
              "global Shift moves the audible Bell center by the requested octave");
        CHECK(std::abs(bellCenterSoft - bellCenterSteep) / bellCenterSoft < 0.015,
              "Bell variable slope preserves processor-path center gain");
        CHECK(std::abs(bellShoulderSteep - bellShoulderSoft) > bellShoulderSoft * 0.12,
              "Bell slope materially changes the processor-path shoulder");

        for (int type : { 5, 6, 7 })
        {
            double maximumRelativeDelta = 0.0;
            for (double probe : { 500.0, 800.0, 1250.0, 2000.0 })
            {
                const double soft = renderStaticBandLevel(type, 1.0f, 3.0f, 6.0f, probe);
                const double steep = renderStaticBandLevel(type, 1.0f, 48.0f, 6.0f, probe);
                maximumRelativeDelta = std::max(maximumRelativeDelta,
                    std::abs(steep - soft) / std::max(1.0e-9, std::max(soft, steep)));
            }
            CHECK(maximumRelativeDelta > 0.025,
                  "every gain-bearing filter has an audible variable-slope response");
        }
        const double tiltSoft = renderStaticBandLevel(3, 1.0f, 12.0f, 6.0f, 1000.0);
        const double tiltSteep = renderStaticBandLevel(3, 1.0f, 48.0f, 6.0f, 1000.0);
        CHECK(std::abs(tiltSoft - tiltSteep) < 1.0e-6,
              "Flat Tilt processor path is independent of the legacy slope parameter");

        const double unity = 0.1 / std::sqrt(2.0);
        const double cutAtZeroAmount = renderStaticBandLevel(8, 1.0f, 48.0f, 0.0f, 1000.0, 0.0f);
        const double bellAtZeroAmount = renderStaticBandLevel(5, 1.0f, 48.0f, 12.0f, 1000.0, 0.0f);
        const double bellAtMinus25 = renderStaticBandLevel(5, 1.0f, 12.0f, 12.0f, 1000.0, -0.25f);
        const double bellAtMinus50 = renderStaticBandLevel(5, 1.0f, 12.0f, 12.0f, 1000.0, -0.50f);
        const double bellAtMinus100 = renderStaticBandLevel(5, 1.0f, 12.0f, 12.0f, 1000.0, -1.0f);
        const double bellAtMinus200 = renderStaticBandLevel(5, 1.0f, 12.0f, 12.0f, 1000.0, -2.0f);
        const double bellAtPlus25 = renderStaticBandLevel(5, 1.0f, 12.0f, 12.0f, 1000.0, 0.25f);
        const double bellAtPlus50 = renderStaticBandLevel(5, 1.0f, 12.0f, 12.0f, 1000.0, 0.50f);
        const double bellAtPlus100 = renderStaticBandLevel(5, 1.0f, 12.0f, 12.0f, 1000.0, 1.0f);
        const double bellAtPlus200 = renderStaticBandLevel(5, 1.0f, 12.0f, 12.0f, 1000.0, 2.0f);
        const double cutAtMinus100 = renderStaticBandLevel(8, 1.0f, 48.0f, 0.0f, 1000.0, -1.0f);
        const double cutAtPlus25 = renderStaticBandLevel(8, 1.0f, 24.0f, 0.0f, 4000.0, 0.25f);
        const double cutAtPlus50 = renderStaticBandLevel(8, 1.0f, 24.0f, 0.0f, 4000.0, 0.50f);
        const double cutAtPlus100 = renderStaticBandLevel(8, 1.0f, 24.0f, 0.0f, 4000.0, 1.0f);
        const double cutAtPlus200 = renderStaticBandLevel(8, 1.0f, 24.0f, 0.0f, 4000.0, 2.0f);
        const double notchAtPlus100 = renderStaticBandLevel(2, 2.0f, 12.0f, 0.0f, 1000.0, 1.0f);
        const double notchAtPlus200 = renderStaticBandLevel(2, 2.0f, 12.0f, 0.0f, 1000.0, 2.0f);
        CHECK(std::abs(cutAtZeroAmount - unity) < unity * 0.01,
              "Amount 0 percent bypasses cut filters on the audio path");
        CHECK(std::abs(bellAtZeroAmount - unity) < unity * 0.01,
              "Amount 0 percent bypasses gain filters on the audio path");
        CHECK(bellAtMinus25 < unity && bellAtMinus50 < bellAtMinus25
                  && bellAtMinus100 < bellAtMinus50 && bellAtMinus200 < bellAtMinus100,
              "negative Amount scales gain-bearing EQ monotonically without polarity turnaround");
        CHECK(bellAtPlus25 > unity && bellAtPlus50 > bellAtPlus25
                  && bellAtPlus100 > bellAtPlus50 && bellAtPlus200 > bellAtPlus100,
              "positive Amount scales gain-bearing EQ monotonically without phase turnaround");
        CHECK(std::abs(cutAtMinus100 - unity) < unity * 0.01,
              "negative Amount keeps non-invertible cut filters at stable unity");
        CHECK(cutAtPlus25 > cutAtPlus50 && cutAtPlus50 > cutAtPlus100
                  && cutAtPlus100 > cutAtPlus200,
              "positive Amount strengthens cut filtering monotonically without wet/dry reversal");
        CHECK(std::abs(notchAtPlus200 - notchAtPlus100) < unity * 0.01,
              "gainless filter Amount cannot extrapolate past its null and reverse polarity");

        const auto [liveFullAmount, liveQuarterAmount] = renderLiveAmountChange();
        CHECK(liveQuarterAmount < liveFullAmount * 0.75,
              "changing Amount after band creation updates the audible filter coefficients");
        CHECK(renderCutAmountOneToZeroMaximumStep() < 0.002,
              "cut Amount 1-to-0 transition is smoothed without a click-sized discontinuity");
        double rapidDragStep = 0.0;
        for (const int type : { deq::filter_types::bell, deq::filter_types::resHighCut,
                                deq::filter_types::highCut, deq::filter_types::bandPass,
                                deq::filter_types::notch })
        {
            const double typeStep = renderRapidLowBandDragMaximumStep(type);
            std::printf("rapid drag type %d max step %.9g\n", type, typeStep);
            rapidDragStep = std::max(rapidDragStep, typeStep);
        }
        const double regularGainStep = renderRegularAutoGainChangeMaximumStep();
        std::printf("rapid low-band drag max step %.9g, Regular Gain change %.9g\n",
                    rapidDragStep, regularGainStep);
        CHECK(rapidDragStep < 0.02,
              "rapid low-frequency two-axis band movement has no click-sized coefficient discontinuity");
        CHECK(regularGainStep < 0.002,
              "Regular Gain compensation changes continuously instead of at a block boundary");
    }

    // Latency contract: clean minimum-phase is exactly zero; a band's 5 ms
    // lookahead and linear phase are additive and reported to the host.
    {
        DefaultEqualizerAudioProcessor p;
        activateBand(p);
        for (int b = 1; b <= kNumBands; ++b) setPlain(p, id(b, "drive"), 0.0f);
        p.prepareToPlay(48000.0, 256);
        CHECK(p.getLatencySamples() == 0, "clean minimum-phase reports zero samples");
        setPlain(p,id(1,"placement_mode"),2.0f);
        CHECK(p.getLatencySamples()==1536,"an active T/S route reports the shared splitter latency");
        setPlain(p,id(1,"placement_mode"),0.0f);
        setPlain(p, id(1, "dyn_lookahead"), 5.0f);
        CHECK(p.getLatencySamples() == 0,
              "lookahead is inactive while dynamic threshold remains at 0 dB");
        setPlain(p, id(1, "dyn_thresh"), -0.1f);
        CHECK(p.getLatencySamples() == 240, "5 ms lookahead reports 240 samples at 48 kHz");
        setPlain(p, "linear_phase", 1.0f);
        CHECK(p.getLatencySamples() == 2048 + 240,
              "linear phase and lookahead latency are additive");
        setPlain(p, "linear_quality", 0.0f);
        CHECK(p.getLatencySamples() == 512 + 240, "Eco linear phase reports 512 samples");
        setPlain(p, "linear_quality", 1.0f);
        CHECK(p.getLatencySamples() == 1024 + 240, "Balanced linear phase reports 1024 samples");
        setPlain(p, "linear_quality", 2.0f);
        CHECK(p.getLatencySamples() == 2048 + 240, "Maximum linear phase reports 2048 samples");
    }

    // Oversampling is a global quality choice but only enters the nonlinear
    // per-band drive path. Clean linear/dynamic EQ therefore remains zero
    // latency and avoids paying the complete oversampled workload.
    {
        DefaultEqualizerAudioProcessor p;
        activateBand(p);
        p.prepareToPlay(48000.0, 256);
        setPlain(p, "oversampling", 3.0f);
        CHECK(p.getLatencySamples() == 0, "clean EQ ignores nonlinear oversampling latency");
        setPlain(p, id(1, "drive"), 0.01f);
        CHECK(p.getLatencySamples() > 0, "the first positive drive step activates nonlinear processing");
        setPlain(p, id(1, "drive"), 0.0f);
        CHECK(p.getLatencySamples() == 0, "zero drive fully disables nonlinear processing and oversampling latency");
    }

    // T/S drive owns a separate oversampling pass before recombination. If an
    // ordinary driven band follows it, the host latency is the sum of both
    // passes plus the splitter latency.
    {
        DefaultEqualizerAudioProcessor p;
        activateBand(p,1); activateBand(p,2);
        setPlain(p,"oversampling",3.0f);
        setPlain(p,id(1,"drive"),18.0f);
        p.prepareToPlay(48000.0,256);
        const int normalPass=p.getLatencySamples();
        setPlain(p,id(1,"placement_mode"),2.0f);
        const int transientPass=p.getLatencySamples()-1536;
        setPlain(p,id(2,"drive"),18.0f);
        const int twoPasses=p.getLatencySamples()-1536;
        CHECK(normalPass>0&&transientPass==normalPass&&twoPasses==2*normalPass,
              "T/S and ordinary drive report one oversampling latency per serial pass");

        juce::AudioBuffer<float> block(2,256); juce::MidiBuffer midi;
        for(int pass=0;pass<16;++pass)
        {
            for(int sample=0;sample<256;++sample)
            {
                const float x=0.25f*std::sin(0.07f*(float)(pass*256+sample));
                block.setSample(0,sample,x); block.setSample(1,sample,x);
            }
            p.processBlock(block,midi);
        }
        bool finite=true;
        for(int channel=0;channel<2;++channel)
            for(int sample=0;sample<256;++sample)
                finite=finite&&std::isfinite(block.getSample(channel,sample));
        CHECK(finite,"oversampled T/S drive remains finite through split and recombination");
    }

    {
        DefaultEqualizerAudioProcessor p;
        activateBand(p, 2);
        for (int b = 1; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
        setPlain(p, id(2, "on"), 1.0f);
        setPlain(p, id(2, "dyn_thresh"), -0.1f);
        setPlain(p, id(2, "dyn_lookahead"), 5.0f);
        p.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> impulse(2, 512); impulse.clear();
        impulse.setSample(0, 0, 1.0f); impulse.setSample(1, 0, 1.0f);
        juce::MidiBuffer midi; p.processBlock(impulse, midi);
        int peak = 0;
        for (int i = 1; i < 512; ++i)
            if (std::abs(impulse.getSample(0, i)) > std::abs(impulse.getSample(0, peak))) peak = i;
        CHECK(peak == 240, "lookahead DSP delays the program path by the reported 240 samples");
    }

    // The longest band establishes the common audio/host latency, while a
    // shorter band still receives only its own requested detector lead.
    {
        DefaultEqualizerAudioProcessor p;
        activateBand(p,1); activateBand(p,2);
        for(int band=1;band<=2;++band)
        {
            setPlain(p,id(band,"freq"),1000.0f);
            setPlain(p,id(band,"q"),2.0f);
            setPlain(p,id(band,"dyn_thresh"),-45.0f);
            setPlain(p,id(band,"dyn_range"),12.0f);
            setPlain(p,id(band,"dyn_speed"),100.0f);
        }
        setPlain(p,id(1,"dyn_lookahead"),0.0f);
        setPlain(p,id(2,"dyn_lookahead"),5.0f);
        p.prepareToPlay(48000.0,64);
        CHECK(p.getLatencySamples()==240,
              "the maximum per-band lookahead establishes the shared plugin latency");
        juce::AudioBuffer<float> block(2,64); juce::MidiBuffer midi;
        for(int sample=0;sample<64;++sample)
        {
            const float x=0.9f*std::sin(juce::MathConstants<float>::twoPi*1000.0f*sample/48000.0f);
            block.setSample(0,sample,x); block.setSample(1,sample,x);
        }
        p.processBlock(block,midi);
        CHECK(p.getBandDynamicGainDb(1)<-1.0f&&std::abs(p.getBandDynamicGainDb(0))<0.1f,
              "each band receives its own lookahead rather than the global maximum");
    }

    {
        DefaultEqualizerAudioProcessor p;
        for (int b = 1; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
        setPlain(p, "linear_quality", 0.0f); setPlain(p, "linear_phase", 1.0f);
        p.prepareToPlay(48000.0, 256);
        juce::Thread::sleep(120); // test thread waits for the documented worker build
        juce::AudioBuffer<float> block(2, 256); juce::MidiBuffer midi;
        std::array<float, 1536> rendered {};
        for (int blockIndex = 0; blockIndex < 6; ++blockIndex)
        {
            block.clear();
            if (blockIndex == 0) { block.setSample(0, 0, 1.0f); block.setSample(1, 0, 1.0f); }
            p.processBlock(block, midi);
            for (int i = 0; i < 256; ++i) rendered[(size_t)(blockIndex * 256 + i)] = block.getSample(0, i);
        }
        int peak = 0;
        for (int i = 1; i < (int)rendered.size(); ++i)
            if (std::abs(rendered[(size_t)i]) > std::abs(rendered[(size_t)peak])) peak = i;
        std::printf("linear Eco impulse peak: actual %d, reported %d\n", peak, p.getLatencySamples());
        CHECK(std::abs(peak - p.getLatencySamples()) <= 1,
              "linear-phase impulse peak matches reported Eco latency within even-FIR half-sample rounding");
    }

    // Every linear quality must preserve a valid finite impulse and align its
    // main peak with the latency reported to the host on odd block sizes.
    for (int quality = 0; quality < 3; ++quality)
    {
        DefaultEqualizerAudioProcessor p;
        for (int b = 1; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
        setPlain(p, "linear_quality", (float)quality); setPlain(p, "linear_phase", 1.0f);
        p.prepareToPlay(48000.0, 257); juce::Thread::sleep(120);
        constexpr int total = 5000;
        std::vector<float> rendered((size_t)total, 0.0f);
        juce::AudioBuffer<float> block(2, 257); juce::MidiBuffer midi;
        int written = 0;
        while (written < total)
        {
            block.clear();
            if (written == 0) { block.setSample(0, 0, 1.0f); block.setSample(1, 0, 1.0f); }
            p.processBlock(block, midi);
            const int copy = std::min(257, total - written);
            for (int i = 0; i < copy; ++i) rendered[(size_t)(written + i)] = block.getSample(0, i);
            written += copy;
        }
        int peak = 0; double energy = 0.0;
        for (int i = 0; i < total; ++i)
        {
            CHECK(std::isfinite(rendered[(size_t)i]), "linear phase output remains finite");
            energy += (double)rendered[(size_t)i] * rendered[(size_t)i];
            if (std::abs(rendered[(size_t)i]) > std::abs(rendered[(size_t)peak])) peak = i;
        }
        CHECK(energy > 0.1 && energy < 2.0, "linear phase quality produces a sane non-silent impulse");
        CHECK(std::abs(peak - p.getLatencySamples()) <= 1,
              "linear phase quality impulse peak matches reported latency");
    }

    std::printf("DSP integration: state and graph contracts\n");
    // Current state round-trip preserves continuous slope and per-band M/S placement.
    {
        DefaultEqualizerAudioProcessor source;
        activateBand(source);
        setPlain(source, id(3, "slope"), 37.3f);
        setPlain(source, id(3, "placement_mode"), 1.0f);
        setPlain(source, id(3, "placement"), 73.5f);
        setPlain(source, id(3, "dyn_lookahead"), 5.0f);
        setPlain(source, "shift", -17.25f);
        juce::MemoryBlock state;
        source.getStateInformation(state);
        DefaultEqualizerAudioProcessor restored;
        restored.setStateInformation(state.getData(), (int)state.getSize());
        CHECK(std::abs(restored.apvts.getRawParameterValue(id(3, "slope"))->load() - 37.3f) < 0.02f,
              "continuous slope survives state round-trip");
        CHECK(restored.apvts.getRawParameterValue(id(3, "placement_mode"))->load() > 0.5f
              && std::abs(restored.apvts.getRawParameterValue(id(3, "placement"))->load() - 73.5f) < 0.02f,
              "per-band continuous M/S placement survives state round-trip");
        CHECK(std::abs(restored.apvts.getRawParameterValue(id(3, "dyn_lookahead"))->load() - 5.0f) < 0.01f,
              "per-band lookahead survives state round-trip");
        CHECK(std::abs(restored.apvts.getRawParameterValue("shift")->load() + 17.25f) < 0.01f,
              "global frequency Shift survives state round-trip");
    }

    // Schema-v5 discrete routes and percent-based drive controls migrate to
    // schema-v6 placement and algorithm-native normalized controls migrate
    // into the current schema-v8 state.
    {
        DefaultEqualizerAudioProcessor source;
        auto legacyCurrent = source.apvts.copyState();
        legacyCurrent.setProperty("stateRole", "current", nullptr);
        const auto setLegacyValue = [](juce::ValueTree state, const juce::String& paramId, float value)
        {
            for (auto child : state)
                if (child.getProperty("id").toString() == paramId)
                { child.setProperty("value", value, nullptr); return; }
            juce::ValueTree child("PARAM"); child.setProperty("id", paramId, nullptr);
            child.setProperty("value", value, nullptr); state.appendChild(child, nullptr);
        };
        setLegacyValue(legacyCurrent, id(2, "ch"), 4.0f);
        setLegacyValue(legacyCurrent, id(2, "drive"), 50.0f);
        setLegacyValue(legacyCurrent, id(2, "drive_character"), 75.0f);
        setLegacyValue(legacyCurrent, id(2, "drive_secondary"), 25.0f);
        setLegacyValue(legacyCurrent, id(2, "drive_tone"), 80.0f);
        setLegacyValue(legacyCurrent, id(2, "sat_mode"), 4.0f);
        juce::ValueTree root("DEFAULT_EQ_STATE");
        root.setProperty("schemaVersion", 5, nullptr);
        root.appendChild(legacyCurrent, nullptr);
        auto xml = root.createXml(); juce::MemoryBlock encoded;
        juce::AudioProcessor::copyXmlToBinary(*xml, encoded);
        DefaultEqualizerAudioProcessor migrated;
        migrated.setStateInformation(encoded.getData(), (int)encoded.getSize());
        CHECK(migrated.apvts.getRawParameterValue(id(2, "placement_mode"))->load() > 0.5f
              && std::abs(migrated.apvts.getRawParameterValue(id(2, "placement"))->load() - 100.0f) < 0.01f,
              "schema-v5 Side route migrates to the M/S endpoint");
        CHECK(std::abs(migrated.apvts.getRawParameterValue(id(2, "drive"))->load() - 18.0f) < 0.01f
              && std::abs(migrated.apvts.getRawParameterValue(id(2, "drive_character"))->load() - 0.5f) < 0.01f
              && std::abs(migrated.apvts.getRawParameterValue(id(2, "drive_secondary"))->load() - 0.25f) < 0.01f,
              "schema-v5 drive values migrate without resurrecting Tone");
    }

    // A saved project state must reproduce the same rendered audio after both
    // processors have been prepared from the same initial condition.
    {
        DefaultEqualizerAudioProcessor source;
        activateBand(source);
        for (int b = 2; b <= kNumBands; ++b) setPlain(source, id(b, "on"), 0.0f);
        setPlain(source, id(1, "freq"), 1370.0f);
        setPlain(source, id(1, "gain"), 8.5f);
        setPlain(source, id(1, "q"), 1.7f);
        setPlain(source, id(1, "slope"), 31.5f);
        setPlain(source, id(1, "drive"), 24.0f);
        setPlain(source, id(1, "sat_mode"), 4.0f);
        juce::MemoryBlock state;
        source.getStateInformation(state);

        DefaultEqualizerAudioProcessor restored;
        restored.setStateInformation(state.getData(), (int) state.getSize());
        source.prepareToPlay(48000.0, 257);
        restored.prepareToPlay(48000.0, 257);

        juce::AudioBuffer<float> a(2, 257), b(2, 257);
        for (int c = 0; c < 2; ++c)
            for (int n = 0; n < 257; ++n)
                a.setSample(c, n, 0.11f * std::sin(0.021f * (float)(n + 17 * c))
                                   + 0.04f * std::cos(0.067f * (float)(n + 5 * c)));
        b.makeCopyOf(a);
        juce::MidiBuffer midiA, midiB;
        source.processBlock(a, midiA);
        restored.processBlock(b, midiB);
        float worst = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int n = 0; n < 257; ++n)
                worst = std::max(worst, std::abs(a.getSample(c, n) - b.getSample(c, n)));
        CHECK(worst < 1.0e-6f, "project reload reproduces rendered audio within numerical tolerance");
        std::printf("state audio recall worst sample delta: %.9g\n", worst);
    }


    // Reusing a deleted slot must never resurrect its previous routing, dynamic
    // or drive state. A graph-created band starts from parameter defaults.
    {
        DefaultEqualizerAudioProcessor p;
        setPlain(p, id(1, "dyn_thresh"), -41.0f);
        setPlain(p, id(1, "drive"), 31.0f);
        setPlain(p, id(1, "placement_mode"), 1.0f);
        setPlain(p, id(1, "placement"), 100.0f);
        p.resetBandToDefaults(0, false);
        CHECK(p.apvts.getRawParameterValue(id(1, "present"))->load() < 0.5f,
              "deleting a band releases its graph slot");
        p.resetBandToDefaults(0, true, 777.0f, -3.5f);
        CHECK(p.apvts.getRawParameterValue(id(1, "present"))->load() > 0.5f,
              "recreating a band restores its graph slot");
        CHECK(p.apvts.getParameter(id(1, "dyn_on")) == nullptr,
              "dynamic processing publishes no redundant enable parameter");
        CHECK(std::abs(p.apvts.getRawParameterValue(id(1, "drive"))->load()) < 0.01f,
              "recreated band resets drive amount");
        CHECK(p.apvts.getParameter(id(1, "drive_on")) == nullptr
              && p.apvts.getParameter(id(1, "drive_mix")) == nullptr
              && p.apvts.getParameter(id(1, "drive_output")) == nullptr
              && p.apvts.getParameter(id(1, "drive_auto_gain")) == nullptr,
              "removed drive controls are not published to the host");
        CHECK(p.apvts.getRawParameterValue(id(1, "placement_mode"))->load() < 0.5f
              && std::abs(p.apvts.getRawParameterValue(id(1, "placement"))->load()) < 0.01f,
              "recreated band restores centered L/R placement");
        CHECK(std::abs(p.apvts.getRawParameterValue(id(1, "freq"))->load() - 777.0f) < 0.1f,
              "recreated band applies requested graph frequency");
        CHECK(std::abs(p.apvts.getRawParameterValue(id(1, "dyn_thresh"))->load()) < 0.01f,
              "recreated band restores the neutral zero-dB dynamic threshold");
    }

    // Empty-graph double-click creation chooses edge filters by position while
    // preserving Bell in the central editing area.
    CHECK(ResponseCurveComponent::defaultTypeForNewBand(80.0f, 3.0f) == 6,
          "upper low-frequency double-click creates low shelf");
    CHECK(ResponseCurveComponent::defaultTypeForNewBand(80.0f, -3.0f)
              == deq::filter_types::highCut,
          "lower low-frequency click creates the classic high-pass cut");
    CHECK(ResponseCurveComponent::defaultTypeForNewBand(8000.0f, 3.0f) == 7,
          "upper high-frequency double-click creates high shelf");
    CHECK(ResponseCurveComponent::defaultTypeForNewBand(8000.0f, -3.0f)
              == deq::filter_types::lowCut,
          "lower high-frequency click creates the classic low-pass cut");
    CHECK(ResponseCurveComponent::defaultTypeForNewBand(1000.0f, -12.0f) == 5,
          "central double-click creates Bell");
    CHECK(ResponseCurveComponent::shiftClickTypeForNewBand(80.0f, -3.0f)
              == deq::filter_types::resHighCut,
          "lower-left Shift-click creates the resonant high-pass cut");
    CHECK(ResponseCurveComponent::shiftClickTypeForNewBand(8000.0f, -3.0f)
              == deq::filter_types::resLowCut,
          "lower-right Shift-click creates the resonant low-pass cut");
    CHECK(ResponseCurveComponent::shiftClickTypeForNewBand(1000.0f, 0.0f)
              == deq::filter_types::tilt,
          "central Shift-click creates Flat Tilt");
    CHECK(ResponseCurveComponent::isCutType(0) && ResponseCurveComponent::isCutType(1)
              && ResponseCurveComponent::isCutType(8) && ResponseCurveComponent::isCutType(9),
          "all resonant and classic low/high cuts are classified as cuts");
    CHECK(ResponseCurveComponent::usesQVerticalDrag(8)
              && ResponseCurveComponent::usesQVerticalDrag(9)
              && ResponseCurveComponent::usesQVerticalDrag(0)
              && ResponseCurveComponent::usesQVerticalDrag(1)
              && ResponseCurveComponent::usesQVerticalDrag(2)
              && ResponseCurveComponent::usesQVerticalDrag(4)
              && !ResponseCurveComponent::usesQVerticalDrag(5),
          "vertical Q drag covers classic/resonant cuts, Notch and Band Pass");
    CHECK(!ResponseCurveComponent::usesGainVerticalDrag(4)
              && !ResponseCurveComponent::usesGainVerticalDrag(0)
              && !ResponseCurveComponent::usesGainVerticalDrag(2)
              && ResponseCurveComponent::usesGainVerticalDrag(5),
          "Band Pass, Notch and cut Q drags never write Gain, while Bell keeps its Gain drag");
    CHECK(!ResponseCurveComponent::typeDefaultsToMidSide(0)
              && !ResponseCurveComponent::typeDefaultsToMidSide(9),
          "new filter types preserve the selected routing mode");
    CHECK(ResponseCurveComponent::cutQFromVerticalDrag(1.0f, -80.0f) > 1.99f
              && ResponseCurveComponent::cutQFromVerticalDrag(1.0f, 80.0f) < 0.51f,
          "vertical cut drag edits Q logarithmically in both directions");
    CHECK(ResponseCurveComponent::cutQFromVerticalDrag(24.0f, -1000.0f) == 24.0f
              && ResponseCurveComponent::cutQFromVerticalDrag(0.1f, 1000.0f) == 0.1f,
          "vertical cut drag clamps Q to the published parameter range");
    {
        const juce::Rectangle<float> marquee(20.0f, 30.0f, 100.0f, 80.0f);
        CHECK(ResponseCurveComponent::marqueeContains(marquee, { 20.0f, 30.0f })
                  && ResponseCurveComponent::marqueeContains(marquee, { 75.0f, 70.0f })
                  && !ResponseCurveComponent::marqueeContains(marquee, { 121.0f, 70.0f }),
              "marquee selection includes its boundary and rejects nodes outside the rectangle");
    }
    CHECK(deq::filter_types::fromParameterIndex(deq::filter_types::bell) == Biquad::Type::Bell
              && deq::filter_types::fromParameterIndex(deq::filter_types::lowCut) == Biquad::Type::LowPass
              && deq::filter_types::fromParameterIndex(deq::filter_types::highCut) == Biquad::Type::HighPass,
          "the centralized parameter order maps default and edge-created bands to their intended DSP types");
    CHECK(EQBand::dynamicClassicCutSlope(12.0f, -6.0f) > 47.9f
              && EQBand::dynamicClassicCutSlope(12.0f, 6.0f) < 3.1f,
          "downward classic-cut dynamics steepen slope and upward dynamics flatten it");
    CHECK(EQBand::dynamicResonantCutQ(2.0f, -6.0f) < 1.01f
              && EQBand::dynamicResonantCutQ(2.0f, 6.0f) > 3.99f,
          "downward resonant-cut dynamics reduce resonance and upward dynamics increase it");
    CHECK(EQBand::dynamicBandPassQ(2.0f, -6.0f) > 3.99f
              && EQBand::dynamicBandPassQ(2.0f, 6.0f) < 1.01f,
          "downward Band Pass dynamics narrow the passband and upward dynamics widen it");
    {
        DefaultEqualizerAudioProcessor p;
        activateBand(p);
        setPlain(p, id(1, "dyn_thresh"), -20.0f);
        setPlain(p, id(1, "dyn_lookahead"), 5.0f);
        setPlain(p, "auto_gain_mode", 2.0f);
        p.prepareToPlay(48000.0, 128);
        CHECK(p.getLatencySamples() >= 240,
              "dynamic lookahead contributes to reported latency");
        juce::AudioBuffer<float> block(2, 128);
        juce::MidiBuffer midi;
        const auto fillBlock = [&block]
        {
            for (int sample = 0; sample < block.getNumSamples(); ++sample)
            {
                block.setSample(0, sample, 0.25f);
                block.setSample(1, sample, 0.25f);
            }
        };
        fillBlock();
        p.processBlock(block, midi);
        CHECK(p.smartAutoGainProgress.load() == 0.0f,
              "Smart Gain does not measure the first half of a 5 ms latency interval");
        fillBlock();
        p.processBlock(block, midi);
        CHECK(p.smartAutoGainProgress.load() == 0.0f,
              "Smart Gain completes latency warm-up before starting measurement");
        fillBlock();
        p.processBlock(block, midi);
        CHECK(p.smartAutoGainProgress.load() > 0.0f,
              "Smart Gain starts measuring after the latency warm-up");
    }
    {
        const auto slow = DefaultEqualizerAudioProcessor::dynamicsTimingForSpeed(0.0f);
        const auto fast = DefaultEqualizerAudioProcessor::dynamicsTimingForSpeed(100.0f);
        const auto defaultTiming = DefaultEqualizerAudioProcessor::dynamicsTimingForSpeed(75.0f);
        CHECK(std::abs(slow.first - 100.0f) < 0.01f && std::abs(slow.second - 1000.0f) < 0.01f
                  && std::abs(fast.first - 0.1f) < 0.001f && std::abs(fast.second - 15.0f) < 0.01f,
              "Speed spans the agreed 100/1000 ms to 0.1/15 ms endpoints");
        CHECK(defaultTiming.first > fast.first && defaultTiming.first < slow.first,
              "the published 75 percent Speed default lies inside the linked timing range");
    }
    CHECK(std::abs(ResponseCurveComponent::analyzerLevelToY(-90.0f, -90.0f, 90.0f, 400.0f) - 400.0f) < 0.001f
              && std::abs(ResponseCurveComponent::analyzerLevelToY(0.0f, -90.0f, 90.0f, 400.0f)) < 0.001f,
          "RTA floor and fixed 0 dB ceiling occupy the full graph height");

    // The threshold UI reads a detector level even while the neutral 0 dB
    // threshold keeps dynamic processing disabled.
    {
        DefaultEqualizerAudioProcessor p;
        activateBand(p);
        setPlain(p, id(1, "freq"), 1000.0f);
        setPlain(p, id(1, "dyn_thresh"), 0.0f);
        p.uiMeterBand.store(0, std::memory_order_relaxed);
        p.prepareToPlay(48000.0, 256);
        juce::AudioBuffer<float> block(2, 256);
        juce::MidiBuffer midi;
        double phase = 0.0;
        for (int pass = 0; pass < 8; ++pass)
        {
            for (int sample = 0; sample < block.getNumSamples(); ++sample)
            {
                const float value = 0.5f * std::sin((float)phase);
                phase += juce::MathConstants<double>::twoPi * 1000.0 / 48000.0;
                block.setSample(0, sample, value);
                block.setSample(1, sample, value);
            }
            p.processBlock(block, midi);
        }
        CHECK(p.getBandDetectorLevelDb(0) > -30.0f
                  && std::abs(p.getBandDynamicGainDb(0)) < 0.001f,
              "threshold meter receives the band detector signal without enabling compression");

        for (int pass = 0; pass < 8; ++pass)
        {
            block.clear();
            for (int sample = 0; sample < block.getNumSamples(); ++sample)
                block.setSample(0, sample, 0.5f * std::sin((float)(phase +=
                    juce::MathConstants<double>::twoPi * 1000.0 / 48000.0)));
            p.processBlock(block, midi);
        }
        const auto stereoDetector = p.getBandDetectorLevelsDb(0);
        CHECK(stereoDetector.first > -30.0f && stereoDetector.second < -50.0f,
              "threshold meter publishes independent left and right detector levels");
    }

    // Every displayed drive algorithm must remain finite across its published
    // control range. Two source-contract edge cases are checked explicitly:
    // cubic Soft Clip cannot invert a positive over-range sample, and zero-Hz
    // Sine Erosion is transparent.
    {
        bool allFinite = true;
        for (int mode = 0; mode < kSaturationModeCount; ++mode)
            for (float drive : { 0.001f, 0.25f, 1.0f })
                for (float character : { 0.0f, 0.5f, 1.0f })
                {
                    EQBand band;
                    band.reset(48000.0);
                    band.prepareDriveRate(48000.0);
                    band.satType = static_cast<SaturationType>(mode);
                    band.driveAmount = drive;
                    band.driveCharacter = character;
                    band.driveSecondary = 0.5f;
                    for (int sample = 0; sample < 2048; ++sample)
                    {
                        const float input = 1.5f * std::sin(0.071f * (float)sample);
                        const float output = band.saturateOne(input, false);
                        allFinite = allFinite && std::isfinite(output)
                            && std::abs(output) < 16.0f;
                    }
                }
        CHECK(allFinite, "all eight drive algorithms stay finite and bounded across controls");

        EQBand soft;
        soft.reset(48000.0); soft.prepareDriveRate(48000.0);
        soft.satType = SaturationType::SoftClip;
        soft.driveAmount = 1.0f; soft.driveCharacter = 1.0f;
        CHECK(soft.saturateOne(1.0f) > 0.99f,
              "cubic Soft Clip preserves polarity above its clipping threshold");

        EQBand sine;
        sine.reset(48000.0); sine.prepareDriveRate(48000.0);
        sine.satType = SaturationType::SineErosion;
        sine.driveAmount = 1.0f; sine.driveCharacter = 0.0f;
        float transparentError = 0.0f;
        for (int sample = 0; sample < 512; ++sample)
        {
            const float input = std::sin(0.071f * (float)sample);
            transparentError = std::max(transparentError,
                std::abs(sine.saturateOne(input) - input));
        }
        CHECK(transparentError < 1.0e-7f,
              "zero-frequency Sine Erosion is exactly transparent");
    }

    // Phase Distortion follows default_distortion's input-envelope modulated
    // delay contract: zero depth is exact passthrough and non-zero depth is a
    // time-domain transformation rather than an amplitude hard clipper.
    {
        EQBand phase;
        phase.reset(48000.0);
        phase.freqHz = 1000.0f;
        phase.Q = 1.0f;
        phase.prepareDriveRate(48000.0);
        phase.satType = SaturationType::PhaseDistortion;
        phase.driveCharacter = 0.5f;
        phase.driveAmount = 0.0f;
        float zeroDepthError = 0.0f;
        for (int i = 0; i < 512; ++i)
        {
            const float input = 0.8f * std::sin(0.07f * (float)i);
            zeroDepthError = std::max(zeroDepthError,
                std::abs(phase.saturateOne(input, false) - input));
        }
        CHECK(zeroDepthError < 1.0e-6f,
              "Phase Distortion returns a true zero-delay signal at zero Drive");
        phase.reset(48000.0);
        phase.prepareDriveRate(48000.0);
        phase.satType = SaturationType::PhaseDistortion;
        phase.driveCharacter = 0.5f;
        phase.driveAmount = 0.8f;
        float difference = 0.0f, peak = 0.0f;
        for (int i = 0; i < 4096; ++i)
        {
            const float input = 0.8f * std::sin(0.07f * (float)i);
            const float output = phase.saturateOne(input, false);
            difference += std::abs(output - input);
            peak = std::max(peak, std::abs(output));
        }
        CHECK(std::isfinite(difference) && difference > 1.0f && peak < 0.99f,
              "Phase Distortion produces bounded envelope-modulated delay without hard clipping");

        phase.reset(48000.0);
        phase.prepareDriveRate(48000.0);
        phase.satType = SaturationType::PhaseDistortion;
        phase.driveCharacter = 1.0f;
        phase.driveAmount = 0.25f;
        int firstOutputSample = -1;
        for (int i = 0; i < 900; ++i)
            if (std::abs(phase.saturateOne(1.0f, false)) > 0.5f && firstOutputSample < 0)
                firstOutputSample = i;
        CHECK(firstOutputSample == 600,
              "Phase Distortion Depth maps linearly to its 0..50 ms delay range");
    }

    // Bypass and deletion have separate state semantics: bypass preserves the
    // node slot while deletion clears it.
    {
        DefaultEqualizerAudioProcessor p;
        activateBand(p);
        setPlain(p, id(1, "on"), 0.0f);
        CHECK(p.apvts.getRawParameterValue(id(1, "present"))->load() > 0.5f,
              "bypassed band remains present on the graph");
        p.resetBandToDefaults(0, false);
        CHECK(p.apvts.getRawParameterValue(id(1, "present"))->load() < 0.5f,
              "deleted band no longer occupies a graph slot");
    }

    // Schema-v3 A/B projects migrate the audible slot into schema-v8's single
    // unambiguous audio state.
    {
        DefaultEqualizerAudioProcessor legacy;
        setPlain(legacy, id(1, "gain"), 7.0f);
        auto a = legacy.apvts.copyState(); a.setProperty("snapshotSlot", "A", nullptr);
        setPlain(legacy, id(1, "gain"), -5.0f);
        auto b = legacy.apvts.copyState(); b.setProperty("snapshotSlot", "B", nullptr);
        juce::ValueTree root("DEFAULT_EQ_STATE");
        root.setProperty("schemaVersion", 3, nullptr); root.setProperty("activeSlot", "B", nullptr);
        root.appendChild(a, nullptr); root.appendChild(b, nullptr);
        juce::MemoryBlock encoded;
        auto xml = root.createXml();
        juce::AudioProcessor::copyXmlToBinary(*xml, encoded);
        DefaultEqualizerAudioProcessor migrated;
        migrated.setStateInformation(encoded.getData(), (int)encoded.getSize());
        CHECK(std::abs(migrated.apvts.getRawParameterValue(id(1, "gain"))->load() + 5.0f) < 0.01f,
              "schema-v3 migration preserves the previously active audible slot");
        CHECK(migrated.apvts.getParameter(id(1, "link")) == nullptr,
              "obsolete A/B link-group parameter is not published");
    }

    // Corrupt state must be rejected without altering a safe default.
    {
        DefaultEqualizerAudioProcessor p;
        const unsigned char garbage[] { 0xff, 0x00, 0x7f, 0x13, 0x37 };
        p.setStateInformation(garbage, (int)sizeof(garbage));
        CHECK(std::isfinite(p.apvts.getRawParameterValue("output_gain")->load()),
              "corrupt state is rejected safely");
    }

    // Mono is a supported, deterministic neutral layout.
    {
        DefaultEqualizerAudioProcessor p;
        auto layout = p.getBusesLayout();
        layout.inputBuses.set(0, juce::AudioChannelSet::mono());
        layout.outputBuses.set(0, juce::AudioChannelSet::mono());
        CHECK(p.setBusesLayout(layout), "mono bus layout accepted");
        for (int b = 1; b <= kNumBands; ++b) { setPlain(p, id(b, "gain"), 0.0f); setPlain(p, id(b, "drive"), 0.0f); }
        p.prepareToPlay(44100.0, 127);
        juce::AudioBuffer<float> mono(1, 127), original(1, 127);
        for (int i = 0; i < 127; ++i) mono.setSample(0, i, 0.2f * std::sin(0.031f * (float)i));
        original.makeCopyOf(mono); juce::MidiBuffer midi; p.processBlock(mono, midi);
        float worst = 0.0f;
        for (int i = 0; i < 127; ++i) worst = std::max(worst, std::abs(mono.getSample(0, i) - original.getSample(0, i)));
        CHECK(worst < 2.0e-5f, "neutral mono processing is unity");
    }

    std::printf("processor CPU probe: 0 bands %.2f, 1 band %.2f, 8 bands %.2f ns/sample\n",
                benchmarkProcessorNsPerSample(0), benchmarkProcessorNsPerSample(1),
                benchmarkProcessorNsPerSample(8));

    std::printf(failures == 0 ? "ALL DSP INTEGRATION TESTS PASSED\n" : "%d DSP INTEGRATION TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
