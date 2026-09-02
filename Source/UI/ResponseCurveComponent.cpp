#include "ResponseCurveComponent.h"
#include "DriveCharacterFormatting.h"
#include "../DSP/FilterTypes.h"
#include "../DSP/VariableSlope.h"
#include "../PluginProcessor.h"
#include <complex>

static juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}


// Family UI is intentionally monochrome. Selection is also encoded by shape.
juce::Colour ResponseCurveComponent::getBandColour(int i)
{
    juce::ignoreUnused(i);
    return juce::Colour(0xfff6f6f6);
}

int ResponseCurveComponent::defaultTypeForNewBand(float frequencyHz, float gainDb) noexcept
{
    if (frequencyHz <= 100.0f)
        return gainDb >= 0.0f ? deq::filter_types::lowShelf
                              : deq::filter_types::highCut;
    if (frequencyHz >= 5000.0f)
        return gainDb >= 0.0f ? deq::filter_types::highShelf
                              : deq::filter_types::lowCut;
    return deq::filter_types::bell;
}

int ResponseCurveComponent::shiftClickTypeForNewBand(float frequencyHz, float gainDb) noexcept
{
    if (gainDb < 0.0f && frequencyHz <= 100.0f) return deq::filter_types::resHighCut;
    if (gainDb < 0.0f && frequencyHz >= 5000.0f) return deq::filter_types::resLowCut;
    return deq::filter_types::tilt;
}

// ── Constructor ────────────────────────────────────────────────────
ResponseCurveComponent::ResponseCurveComponent(DefaultEqualizerAudioProcessor& processor)
    : proc(processor)
{
    setName("ResponseCurve");
    // Initialize smoothed spectrum to silence (-100 dB) so the max()-based
    // peak-hold logic works correctly with negative dB values.
    std::fill(std::begin(smoothedInputSpectrum), std::end(smoothedInputSpectrum), -100.0f);
    std::fill(std::begin(smoothedOutputSpectrum), std::end(smoothedOutputSpectrum), -100.0f);
    std::fill(std::begin(peakInputSpectrum), std::end(peakInputSpectrum), -120.0f);
    std::fill(std::begin(peakOutputSpectrum), std::end(peakOutputSpectrum), -120.0f);

    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    numericEditor.setVisible(false);
    numericEditor.setSelectAllWhenFocused(true);
    numericEditor.setJustification(juce::Justification::centred);
    numericEditor.onReturnKey = [this] { commitNumericEditor(); };
    numericEditor.onEscapeKey = [this]
    {
        numericParameter = nullptr;
        numericEditor.setVisible(false);
        grabKeyboardFocus();
    };
    numericEditor.onFocusLost = [this]
    {
        numericParameter = nullptr;
        numericEditor.setVisible(false);
    };
    addChildComponent(numericEditor);
}

void ResponseCurveComponent::setSelectedBand(int band)
{
    selection.fill(false);
    selectedBand = band >= 0 && band < kNumBands ? band : -1;
    if (selectedBand >= 0)
        selection[(size_t)selectedBand] = true;
    repaint();
}

// ── Coordinate mapping ─────────────────────────────────────────────
float ResponseCurveComponent::freqToX(float freqHz) const
{
    const float w = (float)getWidth();
    const float logMin = std::log10(minFreq);
    const float logMax = std::log10(maxFreq);
    return w * (std::log10(freqHz) - logMin) / (logMax - logMin);
}

float ResponseCurveComponent::xToFreq(float x) const
{
    const float w = (float)getWidth();
    const float logMin = std::log10(minFreq);
    const float logMax = std::log10(maxFreq);
    return std::pow(10.0f, logMin + (x / w) * (logMax - logMin));
}

float ResponseCurveComponent::displayedBandFrequency(float baseFrequency) const
{
    const float shift = proc.apvts.getRawParameterValue("shift")->load();
    return DefaultEqualizerAudioProcessor::shiftedFrequency(baseFrequency, shift);
}

float ResponseCurveComponent::baseBandFrequency(float displayedFrequency) const
{
    const float shift = proc.apvts.getRawParameterValue("shift")->load();
    return displayedFrequency
        / DefaultEqualizerAudioProcessor::frequencyShiftRatio(shift);
}

float ResponseCurveComponent::dbToY(float db) const
{
    const float h = (float)getHeight();
    // 0 dB is at center
    return h * 0.5f * (1.0f - db / displayMaxDb);
}

float ResponseCurveComponent::yToDb(float y) const
{
    const float h = (float)getHeight();
    return displayMaxDb * (1.0f - 2.0f * y / h);
}

// ── Magnitude response from biquad coefficients ────────────────────
float ResponseCurveComponent::computeMagnitudeDb(const Biquad& bq, double freq, double sampleRate)
{
    // H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
    // Evaluate at z = e^(j*omega), omega = 2*pi*freq/sampleRate
    const double omega = 2.0 * kPi * freq / sampleRate;
    const double cosw  = std::cos(omega);
    const double cos2w = std::cos(2.0 * omega);
    const double sinw  = std::sin(omega);
    const double sin2w = std::sin(2.0 * omega);

    const double numReal = bq.b0 + bq.b1 * cosw + bq.b2 * cos2w;
    const double numImag = -(bq.b1 * sinw + bq.b2 * sin2w);
    const double denReal = 1.0 + bq.a1 * cosw + bq.a2 * cos2w;
    const double denImag = -(bq.a1 * sinw + bq.a2 * sin2w);

    const double numMagSq = numReal * numReal + numImag * numImag;
    const double denMagSq = denReal * denReal + denImag * denImag;

    if (denMagSq < 1e-30)
        return 0.0f;

    const double magSq = numMagSq / denMagSq;
    return (float)(10.0 * std::log10(std::max(magSq, 1e-30)));
}

// ── Update response curve from processor state ─────────────────────
bool ResponseCurveComponent::updateResponseCurve()
{
    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    const float amount = proc.apvts.getRawParameterValue("scale")->load();

    // The graph opens at +/-12 dB, but restored/automated bands must never be
    // stranded outside it. Range only grows during an editor session; deleting
    // a loud band does not make the graph jump back under the pointer.
    float requiredDisplayDb = 12.0f;
    for (int band = 1; band <= kNumBands; ++band)
    {
        if (proc.apvts.getRawParameterValue(bandId(band, "present"))->load() < 0.5f)
            continue;
        const float visibleGain = std::abs(
            proc.apvts.getRawParameterValue(bandId(band, "gain"))->load());
        if (visibleGain > 24.001f) requiredDisplayDb = 36.0f;
        else if (visibleGain > 12.001f) requiredDisplayDb = std::max(requiredDisplayDb, 24.0f);
    }
    const float previousDisplayMaxDb = displayMaxDb;
    displayMaxDb = std::max(displayMaxDb, requiredDisplayDb);
    if (displayMaxDb != previousDisplayMaxDb)
        staticLayerDirty = true;

    std::uint64_t signature = 1469598103934665603ULL;
    const auto hash = [&signature](float value)
    { signature = (signature ^ (std::uint64_t)std::llround(value * 1000.0f)) * 1099511628211ULL; };
    hash(amount); hash(proc.apvts.getRawParameterValue("shift")->load());
    hash(proc.apvts.getRawParameterValue("adaptive_q")->load());
    for (int band = 1; band <= kNumBands; ++band)
    {
        for (auto* suffix : { "present", "on", "type", "freq", "q", "gain", "slope" })
            hash(proc.apvts.getRawParameterValue(bandId(band, suffix))->load());
        hash(proc.getBandDynamicGainDb(band - 1));
    }
    if (responseGridSampleRate == sr && responseSignature == signature)
        return false;
    responseSignature = signature;
    if (responseGridSampleRate != sr)
    {
        responseGridSampleRate = sr;
        const float logMin = std::log10(minFreq), logMax = std::log10(maxFreq);
        for (int i = 0; i < numPoints; ++i)
            responseFrequencies[i] = std::pow(10.0f, logMin
                + (float)i / (float)(numPoints - 1) * (logMax - logMin));
    }

    // Clear composite
    std::fill(std::begin(magnitudes), std::end(magnitudes), 0.0f);

    for (int b = 0; b < kNumBands; ++b)
    {
        const int idx = b + 1;
        const bool on = proc.apvts.getRawParameterValue(bandId(idx, "present"))->load() > 0.5f
            && proc.apvts.getRawParameterValue(bandId(idx, "on"))->load() > 0.5f;

        if (!on)
        {
            std::fill(std::begin(perBandMagnitudes[b]), std::end(perBandMagnitudes[b]), 0.0f);
            continue;
        }

        const int t = (int)proc.apvts.getRawParameterValue(bandId(idx, "type"))->load();
        const float freq = displayedBandFrequency(
            proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load());
        float q = proc.apvts.getRawParameterValue(bandId(idx, "q"))->load();
        float gain = proc.apvts.getRawParameterValue(bandId(idx, "gain"))->load();
        if (proc.apvts.getRawParameterValue("adaptive_q")->load() > 0.5f)
            q = DefaultEqualizerAudioProcessor::calculateAdaptiveQ(q, gain);
        const float dynamicMod = proc.getBandDynamicGainDb(b);

        const auto tp = deq::filter_types::fromParameterIndex(t);

        const bool gainBearing = variable_slope::distributesGain(tp);
        const bool classicCut = zl_filter::isClassicCut(tp);
        const bool resonantCut = zl_filter::isResonantCut(tp);
        const bool bandPass = tp == Biquad::Type::Bandpass;
        if (resonantCut) q = EQBand::amountResonantCutQ(q, amount);
        if (gainBearing) gain += dynamicMod;
        if (resonantCut) q = EQBand::dynamicResonantCutQ(q, dynamicMod);
        if (bandPass) q = EQBand::dynamicBandPassQ(q, dynamicMod);
        if (gainBearing)
            gain *= amount;

        const bool cut = classicCut || resonantCut;
        float responseFreq = freq;
        if (cut)
        {
            const float neutralEdge = (tp == Biquad::Type::LowPass || tp == Biquad::Type::ResLowPass)
                ? (float)sr * 0.45f : 10.0f;
            responseFreq = neutralEdge * std::pow(std::max(1.0e-6f, freq / neutralEdge),
                                                   std::max(0.0f, amount));
            responseFreq = std::clamp(responseFreq, 10.0f, (float)sr * 0.45f);
        }

        const float baseSlope = proc.apvts.getRawParameterValue(bandId(idx, "slope"))->load();
        const float slope = classicCut
            ? EQBand::dynamicClassicCutSlope(baseSlope, dynamicMod) : baseSlope;
        constexpr bool decramp = true;

        for (int i = 0; i < numPoints; ++i)
        {
            const float f = responseFrequencies[i];

            const auto rawResponse = variable_slope::response(tp, sr, responseFreq, q, gain,
                                                              slope, decramp, f);
            const double mix = cut ? (double)EQBand::cutAmountMix(amount)
                                   : (double)std::clamp(amount, 0.0f, 1.0f);
            const auto response = gainBearing ? rawResponse
                : std::complex<double>(1.0, 0.0)
                    + mix * (rawResponse - std::complex<double>(1.0, 0.0));
            const float mag = (float)(20.0 * std::log10(std::max(std::abs(response), 1.0e-15)));
            perBandMagnitudes[b][i] = mag;
            magnitudes[i] += mag;
        }
    }
    return true;
}

void ResponseCurveComponent::advanceSpectrumFrame()
{
    if (currentSpectrumSize <= 0 || spectrumSampleRate <= 0.0)
        return;
    const float frameSeconds = (float)SpectrumFIFO::publishHop / (float)spectrumSampleRate;
    const float averaging = analyzerAveragingSeconds <= 0.0f ? 1.0f
        : 1.0f - std::exp(-frameSeconds / analyzerAveragingSeconds);
    const float decay = analyzerDecayDb * frameSeconds * 30.0f;
    const float binWidth = (float)(spectrumSampleRate / (2.0 * currentSpectrumSize));
    const auto advance = [&](const float* source, float* smoothed, float* peaks)
    {
        for (int bin = 1; bin < currentSpectrumSize; ++bin)
        {
            const float target = source[bin];
            smoothed[bin] += averaging * (target - smoothed[bin]);
            smoothed[bin] = std::max(target, smoothed[bin] - decay);
            const float frequency = (float)bin * binWidth;
            if (frequency >= minFreq && frequency <= maxFreq)
                peaks[bin] = std::max(peaks[bin], smoothed[bin]
                    + analyzerTiltDbPerOct * std::log2(frequency / 1000.0f));
        }
    };
    advance(inputSpectrum, smoothedInputSpectrum, peakInputSpectrum);
    advance(outputSpectrum, smoothedOutputSpectrum, peakOutputSpectrum);
}

bool ResponseCurveComponent::refreshForTimer(bool spectrumFrameArrived)
{
    const bool responseChanged = updateResponseCurve();
    if (spectrumFrameArrived)
        advanceSpectrumFrame();
    if (responseChanged || spectrumFrameArrived)
        repaint();
    return responseChanged || spectrumFrameArrived;
}

void ResponseCurveComponent::resized()
{
    staticLayerDirty = true;
}
