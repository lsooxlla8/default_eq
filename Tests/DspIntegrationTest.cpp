#include "../Source/PluginProcessor.h"
#include <cmath>
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
    enableSidechain(p);
    setPlain(p, id(1, "type"), 0.0f);
    setPlain(p, id(1, "freq"), 1000.0f);
    setPlain(p, id(1, "gain"), 12.0f);
    setPlain(p, id(1, "q"), 2.0f);
    setPlain(p, id(1, "dyn_on"), 1.0f);
    setPlain(p, id(1, "dyn_mode"), 0.0f);
    setPlain(p, id(1, "sc_source"), 1.0f);
    setPlain(p, id(1, "dyn_thresh"), -45.0f);
    setPlain(p, id(1, "dyn_range"), 12.0f);
    setPlain(p, id(1, "dyn_attack"), 0.1f);
    setPlain(p, id(1, "dyn_release"), 20.0f);
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

double measureDriveAlias(int oversamplingOrder)
{
    constexpr int fftOrder = 12, fftSize = 1 << fftOrder;
    constexpr double sampleRate = 48000.0;
    constexpr int fundamentalBin = 85;
    const double frequency = fundamentalBin * sampleRate / fftSize;
    DefaultEqualizerAudioProcessor p;
    for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
    setPlain(p, id(1, "on"), 1.0f); setPlain(p, id(1, "gain"), 0.0f);
    setPlain(p, id(1, "drive_on"), 1.0f); setPlain(p, id(1, "drive"), 36.0f);
    setPlain(p, id(1, "drive_mix"), 100.0f); setPlain(p, id(1, "sat_mode"), 1.0f);
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
    enableSidechain(p);
    for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
    setPlain(p, id(1, "freq"), 1000.0f); setPlain(p, id(1, "gain"), 10.0f);
    setPlain(p, id(1, "dyn_on"), 1.0f); setPlain(p, id(1, "sc_source"), 1.0f);
    setPlain(p, id(1, "dyn_thresh"), -35.0f); setPlain(p, id(1, "dyn_attack"), 4.0f);
    setPlain(p, id(1, "dyn_release"), 80.0f); p.prepareToPlay(48000.0, blockSize);
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
    for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
    setPlain(p, id(1, "type"), 3.0f); setPlain(p, id(1, "freq"), 1000.0f);
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
}

int main()
{
    CHECK(kNumBands == 8, "product exposes exactly eight bands");
    CHECK(std::abs(DefaultEqualizerAudioProcessor::calculateAdaptiveQ(1.25f, 8.0f) - 2.45f) < 1.0e-6f,
          "Adaptive Q follows the documented deterministic formula");

    // Neutral 8-band centered L/R and M/S placement must both be unity.
    for (bool midSide : { false, true })
    {
        DefaultEqualizerAudioProcessor p;
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

    const float quietSC = runDynamic(false);
    const float loudSC = runDynamic(true);
    CHECK(std::abs(loudSC - quietSC) > quietSC * 0.08f, "external sidechain changes selected dynamic band");
    std::printf("external sidechain accumulated output: silent %.3f, active %.3f\n", quietSC, loudSC);

    const double aliasOff = measureDriveAlias(0);
    const double alias8x = measureDriveAlias(3);
    CHECK(alias8x < aliasOff * 0.8, "8x oversampling measurably reduces nonlinear alias energy");
    std::printf("drive alias energy: off %.6g, 8x %.6g (%.2f%%)\n",
                aliasOff, alias8x, 100.0 * alias8x / aliasOff);

    const auto renderDriveLevel = [](bool enabled, bool compensated)
    {
        DefaultEqualizerAudioProcessor p;
        for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
        setPlain(p, id(1, "freq"), 1000.0f); setPlain(p, id(1, "q"), 1.4f);
        setPlain(p, id(1, "gain"), 0.0f); setPlain(p, id(1, "sat_mode"), 1.0f);
        setPlain(p, id(1, "drive_on"), enabled ? 1.0f : 0.0f);
        setPlain(p, id(1, "drive"), enabled ? 18.0f : 0.0f);
        setPlain(p, id(1, "drive_mix"), 100.0f);
        setPlain(p, id(1, "drive_auto_gain"), compensated ? 1.0f : 0.0f);
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
    const double driveCleanLevel = renderDriveLevel(false, false);
    const double driveRawLevel = renderDriveLevel(true, false);
    const double driveCompLevel = renderDriveLevel(true, true);
    CHECK(std::abs(driveCompLevel - driveCleanLevel) < std::abs(driveRawLevel - driveCleanLevel),
          "per-band table Auto Gain moves driven level toward the clean reference");

    // Solo is a frequency-window audition, and per-band drive only returns
    // nonlinear energy from that same window instead of saturating broadband.
    const auto renderBandTone = [](double frequency, bool drive, bool solo)
    {
        DefaultEqualizerAudioProcessor p;
        for (int b = 2; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
        setPlain(p, id(1, "freq"), 8000.0f); setPlain(p, id(1, "q"), 4.0f);
        setPlain(p, id(1, "drive_on"), drive ? 1.0f : 0.0f);
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
            CHECK(level <= previous * 1.01, "variable low-cut slope increases rejection monotonically");
            previous = level;
        }
    }

    // Latency contract: clean minimum-phase is exactly zero; a band's 5 ms
    // lookahead and linear phase are additive and reported to the host.
    {
        DefaultEqualizerAudioProcessor p;
        for (int b = 1; b <= kNumBands; ++b) setPlain(p, id(b, "drive"), 0.0f);
        p.prepareToPlay(48000.0, 256);
        CHECK(p.getLatencySamples() == 0, "clean minimum-phase reports zero samples");
        setPlain(p, id(2, "dyn_on"), 1.0f);
        setPlain(p, id(2, "dyn_lookahead"), 5.0f);
        CHECK(p.getLatencySamples() == 240, "5 ms lookahead reports 240 samples at 48 kHz");
        setPlain(p, "linear_phase", 1.0f);
        CHECK(p.getLatencySamples() == 2048 + 240,
              "linear phase and lookahead latency are additive");
        setPlain(p, "linear_quality", 0.0f);
        CHECK(p.getLatencySamples() == 512 + 240, "Economy linear phase reports 512 samples");
        setPlain(p, "linear_quality", 1.0f);
        CHECK(p.getLatencySamples() == 1024 + 240, "Balanced linear phase reports 1024 samples");
        setPlain(p, "linear_quality", 2.0f);
        CHECK(p.getLatencySamples() == 2048 + 240, "Maximum linear phase reports 2048 samples");
    }

    // Global oversampling includes the dynamic path and reports its measured
    // anti-alias filter latency whether or not drive is currently non-zero.
    {
        DefaultEqualizerAudioProcessor p;
        p.prepareToPlay(48000.0, 256);
        setPlain(p, "oversampling", 3.0f);
        const int globalOsLatency = p.getLatencySamples();
        CHECK(globalOsLatency > 0, "global 8x oversampling reports anti-alias filter latency");
        setPlain(p, id(1, "drive_on"), 1.0f);
        setPlain(p, id(1, "drive"), 18.0f);
        CHECK(p.getLatencySamples() == globalOsLatency,
              "drive activation does not change global oversampling latency");
    }

    {
        DefaultEqualizerAudioProcessor p;
        for (int b = 1; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
        setPlain(p, id(2, "on"), 1.0f); setPlain(p, id(2, "dyn_on"), 1.0f);
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
        std::printf("linear Economy impulse peak: actual %d, reported %d\n", peak, p.getLatencySamples());
        CHECK(std::abs(peak - p.getLatencySamples()) <= 1,
              "linear-phase impulse peak matches reported Economy latency within even-FIR half-sample rounding");
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

    // Current state round-trip preserves continuous slope and per-band M/S placement.
    {
        DefaultEqualizerAudioProcessor source;
        setPlain(source, id(3, "slope"), 37.3f);
        setPlain(source, id(3, "placement_mode"), 1.0f);
        setPlain(source, id(3, "placement"), 73.5f);
        setPlain(source, id(3, "drive_mix"), 42.5f);
        setPlain(source, id(3, "dyn_lookahead"), 5.0f);
        juce::MemoryBlock state;
        source.getStateInformation(state);
        DefaultEqualizerAudioProcessor restored;
        restored.setStateInformation(state.getData(), (int)state.getSize());
        CHECK(std::abs(restored.apvts.getRawParameterValue(id(3, "slope"))->load() - 37.3f) < 0.02f,
              "continuous slope survives state round-trip");
        CHECK(restored.apvts.getRawParameterValue(id(3, "placement_mode"))->load() > 0.5f
              && std::abs(restored.apvts.getRawParameterValue(id(3, "placement"))->load() - 73.5f) < 0.02f,
              "per-band continuous M/S placement survives state round-trip");
        CHECK(std::abs(restored.apvts.getRawParameterValue(id(3, "drive_mix"))->load() - 42.5f) < 0.02f,
              "drive Mix survives state round-trip");
        CHECK(std::abs(restored.apvts.getRawParameterValue(id(3, "dyn_lookahead"))->load() - 5.0f) < 0.01f,
              "per-band lookahead survives state round-trip");
    }

    // Schema-v5 discrete routes and percent-based drive controls migrate to
    // schema-v6 placement and algorithm-native normalized controls.
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
        juce::ValueTree root("DEFAULT_EQUALIZER_STATE");
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
        for (int b = 2; b <= kNumBands; ++b) setPlain(source, id(b, "on"), 0.0f);
        setPlain(source, id(1, "freq"), 1370.0f);
        setPlain(source, id(1, "gain"), 8.5f);
        setPlain(source, id(1, "q"), 1.7f);
        setPlain(source, id(1, "slope"), 31.5f);
        setPlain(source, id(1, "drive_on"), 1.0f);
        setPlain(source, id(1, "drive"), 24.0f);
        setPlain(source, id(1, "drive_mix"), 23.0f);
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
        setPlain(p, id(1, "dyn_on"), 1.0f);
        setPlain(p, id(1, "dyn_thresh"), -41.0f);
        setPlain(p, id(1, "drive"), 31.0f);
        setPlain(p, id(1, "drive_mix"), 27.0f);
        setPlain(p, id(1, "placement_mode"), 1.0f);
        setPlain(p, id(1, "placement"), 100.0f);
        p.resetBandToDefaults(0, false);
        p.resetBandToDefaults(0, true, 777.0f, -3.5f);
        CHECK(p.apvts.getRawParameterValue(id(1, "dyn_on"))->load() < 0.5f,
              "recreated band resets dynamic processing");
        CHECK(std::abs(p.apvts.getRawParameterValue(id(1, "drive"))->load()) < 0.01f,
              "recreated band resets drive amount");
        CHECK(std::abs(p.apvts.getRawParameterValue(id(1, "drive_mix"))->load() - 100.0f) < 0.01f,
              "recreated band restores default drive mix");
        CHECK(p.apvts.getRawParameterValue(id(1, "placement_mode"))->load() < 0.5f
              && std::abs(p.apvts.getRawParameterValue(id(1, "placement"))->load()) < 0.01f,
              "recreated band restores centered L/R placement");
        CHECK(std::abs(p.apvts.getRawParameterValue(id(1, "freq"))->load() - 777.0f) < 0.1f,
              "recreated band applies requested graph frequency");
    }

    // Schema-v3 A/B projects migrate the audible slot into schema-v6's single
    // unambiguous audio state.
    {
        DefaultEqualizerAudioProcessor legacy;
        setPlain(legacy, id(1, "gain"), 7.0f);
        auto a = legacy.apvts.copyState(); a.setProperty("snapshotSlot", "A", nullptr);
        setPlain(legacy, id(1, "gain"), -5.0f);
        auto b = legacy.apvts.copyState(); b.setProperty("snapshotSlot", "B", nullptr);
        juce::ValueTree root("DEFAULT_EQUALIZER_STATE");
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

    std::printf(failures == 0 ? "ALL DSP INTEGRATION TESTS PASSED\n" : "%d DSP INTEGRATION TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
