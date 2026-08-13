#include "../Source/PluginProcessor.h"
#include <cmath>
#include <cstdio>
#include <memory>

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
    setPlain(p, id(1, "drive_on"), 1.0f); setPlain(p, id(1, "drive"), 100.0f);
    setPlain(p, id(1, "drive_mix"), 100.0f); setPlain(p, id(1, "sat_mode"), 5.0f);
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
}

int main()
{
    CHECK(kNumBands == 8, "product exposes exactly eight bands");
    CHECK(std::abs(DefaultEqualizerAudioProcessor::calculateAdaptiveQ(1.25f, 8.0f) - 2.45f) < 1.0e-6f,
          "Adaptive Q follows the documented deterministic formula");

    // Neutral 8-band Stereo and per-band Mid routing must both be unity.
    for (int route : { 0, 3, 4 })
    {
        DefaultEqualizerAudioProcessor p;
        for (int b = 1; b <= kNumBands; ++b)
        {
            setPlain(p, id(b, "gain"), 0.0f);
            setPlain(p, id(b, "drive"), 0.0f);
            setPlain(p, id(b, "ch"), (float)route);
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
        CHECK(worst < 2.0e-5f, route == 0 ? "neutral Stereo is unity" : "neutral Mid/Side route is unity");
    }

    const float quietSC = runDynamic(false);
    const float loudSC = runDynamic(true);
    CHECK(std::abs(loudSC - quietSC) > quietSC * 0.08f, "external sidechain changes selected dynamic band");
    std::printf("external sidechain accumulated output: silent %.3f, active %.3f\n", quietSC, loudSC);

    const double aliasOff = measureDriveAlias(0);
    const double alias8x = measureDriveAlias(3);
    CHECK(alias8x < aliasOff * 0.8, "8x oversampling measurably reduces nonlinear alias energy");
    std::printf("drive alias energy: off %.6g, 8x %.6g (%.2f%%)\n",
                aliasOff, alias8x, 100.0 * alias8x / aliasOff);

    const double dyn64 = dynamicLevelForBlockSize(64);
    const double dyn257 = dynamicLevelForBlockSize(257);
    const double dyn1024 = dynamicLevelForBlockSize(1024);
    const double dynSpread = (std::max({ dyn64, dyn257, dyn1024 }) - std::min({ dyn64, dyn257, dyn1024 })) / dyn257;
    CHECK(dynSpread < 0.015, "dynamic EQ is reproducible across 64/257/1024 sample blocks");
    std::printf("dynamic block-size levels: 64 %.7f, 257 %.7f, 1024 %.7f, spread %.3f%%\n",
                dyn64, dyn257, dyn1024, dynSpread * 100.0);

    // Latency contract: clean minimum-phase is exactly zero; a band's 5 ms
    // lookahead and linear phase are additive and reported to the host.
    {
        DefaultEqualizerAudioProcessor p;
        for (int b = 1; b <= kNumBands; ++b) setPlain(p, id(b, "drive"), 0.0f);
        p.prepareToPlay(48000.0, 256);
        CHECK(p.getLatencySamples() == 0, "clean minimum-phase reports zero samples");
        setPlain(p, id(2, "dyn_on"), 1.0f);
        setPlain(p, id(2, "dyn_lookahead"), 1.0f);
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

    // Nonlinear-only oversampling adds its own measured IIR anti-alias-filter
    // latency, and is dormant when no drive section is actually processing.
    {
        DefaultEqualizerAudioProcessor p;
        p.prepareToPlay(48000.0, 256);
        setPlain(p, "oversampling", 3.0f);
        CHECK(p.getLatencySamples() == 0, "oversampling is zero-latency while nonlinear drive is inactive");
        setPlain(p, id(1, "drive_on"), 1.0f);
        setPlain(p, id(1, "drive"), 50.0f);
        CHECK(p.getLatencySamples() == 5, "8x drive oversampling reports its 5-sample filter latency");
    }

    {
        DefaultEqualizerAudioProcessor p;
        for (int b = 1; b <= kNumBands; ++b) setPlain(p, id(b, "on"), 0.0f);
        setPlain(p, id(2, "on"), 1.0f); setPlain(p, id(2, "dyn_on"), 1.0f);
        setPlain(p, id(2, "dyn_lookahead"), 1.0f);
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

    // v2 state round-trip preserves continuous slope and per-band Side mode.
    {
        DefaultEqualizerAudioProcessor source;
        setPlain(source, id(3, "slope"), 37.3f);
        setPlain(source, id(3, "ch"), 4.0f);
        setPlain(source, id(3, "drive_mix"), 42.5f);
        setPlain(source, id(3, "dyn_lookahead"), 1.0f);
        juce::MemoryBlock state;
        source.getStateInformation(state);
        DefaultEqualizerAudioProcessor restored;
        restored.setStateInformation(state.getData(), (int)state.getSize());
        CHECK(std::abs(restored.apvts.getRawParameterValue(id(3, "slope"))->load() - 37.3f) < 0.02f,
              "continuous slope survives state round-trip");
        CHECK((int)restored.apvts.getRawParameterValue(id(3, "ch"))->load() == 4,
              "per-band Side routing survives state round-trip");
        CHECK(std::abs(restored.apvts.getRawParameterValue(id(3, "drive_mix"))->load() - 42.5f) < 0.02f,
              "drive Mix survives state round-trip");
        CHECK(restored.apvts.getRawParameterValue(id(3, "dyn_lookahead"))->load() > 0.5f,
              "per-band lookahead survives state round-trip");
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
        setPlain(source, id(1, "drive"), 37.0f);
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


    // A/B slots are independent full audio states and both survive project recall.
    {
        DefaultEqualizerAudioProcessor source;
        setPlain(source, id(1, "gain"), 7.25f); source.storeSnapshot(true);
        setPlain(source, id(1, "gain"), -5.5f); source.storeSnapshot(false);
        source.snapshotB = source.apvts.copyState(); source.isSlotA = false;
        juce::MemoryBlock data; source.getStateInformation(data);
        DefaultEqualizerAudioProcessor restored;
        restored.setStateInformation(data.getData(), (int)data.getSize());
        restored.recallSnapshot(true);
        CHECK(std::abs(restored.apvts.getRawParameterValue(id(1, "gain"))->load() - 7.25f) < 0.02f,
              "A/B recall restores slot A exactly");
        restored.recallSnapshot(false);
        CHECK(std::abs(restored.apvts.getRawParameterValue(id(1, "gain"))->load() + 5.5f) < 0.02f,
              "A/B recall restores slot B exactly");
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
