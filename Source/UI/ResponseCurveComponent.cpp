#include "ResponseCurveComponent.h"
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

// ── Constructor ────────────────────────────────────────────────────
ResponseCurveComponent::ResponseCurveComponent(DefaultEqualizerAudioProcessor& processor)
    : proc(processor)
{
    // Initialize smoothed spectrum to silence (-100 dB) so the max()-based
    // peak-hold logic works correctly with negative dB values.
    std::fill(std::begin(smoothedInputSpectrum), std::end(smoothedInputSpectrum), -100.0f);
    std::fill(std::begin(smoothedOutputSpectrum), std::end(smoothedOutputSpectrum), -100.0f);
    std::fill(std::begin(peakInputSpectrum), std::end(peakInputSpectrum), -120.0f);
    std::fill(std::begin(peakOutputSpectrum), std::end(peakOutputSpectrum), -120.0f);

    startTimerHz(30);
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
    selectedBand = juce::jlimit(0, kNumBands - 1, band);
    selection.fill(false);
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

float ResponseCurveComponent::dbToY(float db) const
{
    const float h = (float)getHeight();
    // 0 dB is at center
    return h * 0.5f * (1.0f - db / maxDb);
}

float ResponseCurveComponent::yToDb(float y) const
{
    const float h = (float)getHeight();
    return maxDb * (1.0f - 2.0f * y / h);
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
void ResponseCurveComponent::updateResponseCurve()
{
    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    const float scale = proc.apvts.getRawParameterValue("scale")->load();

    // Clear composite
    std::fill(std::begin(magnitudes), std::end(magnitudes), 0.0f);

    for (int b = 0; b < kNumBands; ++b)
    {
        const int idx = b + 1;
        const bool on = proc.apvts.getRawParameterValue(bandId(idx, "on"))->load() > 0.5f;

        if (!on)
        {
            std::fill(std::begin(perBandMagnitudes[b]), std::end(perBandMagnitudes[b]), 0.0f);
            continue;
        }

        const int t = (int)proc.apvts.getRawParameterValue(bandId(idx, "type"))->load();
        const float freq = proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load();
        float q = proc.apvts.getRawParameterValue(bandId(idx, "q"))->load();
        float gain = proc.apvts.getRawParameterValue(bandId(idx, "gain"))->load() * scale;
        if (proc.apvts.getRawParameterValue("adaptive_q")->load() > 0.5f)
            q = DefaultEqualizerAudioProcessor::calculateAdaptiveQ(q, gain);
        if (proc.apvts.getRawParameterValue(bandId(idx, "dyn_on"))->load() > 0.5f)
            gain += proc.getBandDynamicGainDb(b);

        Biquad::Type tp = Biquad::Type::Bell;
        switch (t)
        {
            case 0: tp = Biquad::Type::Bell; break;
            case 1: tp = Biquad::Type::LowShelf; break;
            case 2: tp = Biquad::Type::HighShelf; break;
            case 3: tp = Biquad::Type::HighPass; break;
            case 4: tp = Biquad::Type::LowPass; break;
            case 5: tp = Biquad::Type::Bandpass; break;
            case 6: tp = Biquad::Type::Notch; break;
        }

        const float slopeStages = std::clamp(
            proc.apvts.getRawParameterValue(bandId(idx, "slope"))->load() / 12.0f,
            0.0f, 4.0f);
        const float stageGain = gain / std::max(1.0f, slopeStages);
        const int fullStages = (int)std::floor(slopeStages);
        const float fractional = slopeStages - (float)fullStages;

        // Build a temporary biquad with current coefficients
        Biquad tempBq;
        if (proc.apvts.getRawParameterValue("decramp")->load() > 0.5f)
            tempBq.setMatched(tp, sr, freq, q, stageGain);
        else
            tempBq.set(tp, sr, freq, q, stageGain);

        for (int i = 0; i < numPoints; ++i)
        {
            const float logMin = std::log10(minFreq);
            const float logMax = std::log10(maxFreq);
            const float f = std::pow(10.0f, logMin + (float)i / (float)(numPoints - 1) * (logMax - logMin));

            if (tp == Biquad::Type::HighPass || tp == Biquad::Type::LowPass)
            {
                const double ratio = tp == Biquad::Type::HighPass ? (double)freq / f : f / (double)freq;
                const double exponent = std::max(0.9966, (double)proc.apvts.getRawParameterValue(bandId(idx, "slope"))->load() / 3.01029995664);
                const float mag = (float)(-10.0 * std::log10(1.0 + std::pow(ratio, exponent)));
                perBandMagnitudes[b][i] = mag;
                magnitudes[i] += mag;
                continue;
            }
            const float baseMag = computeMagnitudeDb(tempBq, f, sr);
            float fractionalMag = 0.0f;
            if (fractional > 0.0001f)
            {
                const double omega = 2.0 * kPi * f / sr;
                const std::complex<double> z1(std::cos(omega), -std::sin(omega));
                const auto z2 = z1 * z1;
                const std::complex<double> h = (tempBq.b0 + tempBq.b1 * z1 + tempBq.b2 * z2)
                    / (1.0 + tempBq.a1 * z1 + tempBq.a2 * z2);
                const auto mixed = std::complex<double>(1.0 - fractional, 0.0) + (double)fractional * h;
                fractionalMag = (float)(20.0 * std::log10(std::max(std::abs(mixed), 1.0e-15)));
            }
            const float mag = baseMag * (float)fullStages + fractionalMag;
            perBandMagnitudes[b][i] = mag;
            magnitudes[i] += mag;
        }
    }
}

// ── Timer callback ─────────────────────────────────────────────────
void ResponseCurveComponent::timerCallback()
{
    updateResponseCurve();
    repaint();
}

void ResponseCurveComponent::resized() {}

// ── Paint ──────────────────────────────────────────────────────────
void ResponseCurveComponent::paint(juce::Graphics& g)
{
    const auto fg = darkMode ? juce::Colour(0xfff6f6f6) : juce::Colour(0xff050505);
    const auto bg = darkMode ? juce::Colour(0xff050505) : juce::Colour(0xfff6f6f6);
    g.fillAll(bg);

    paintGrid(g);
    if (analyzerVisible) paintSpectrum(g);
    paintMatchPreview(g);
    paintBandCurves(g);
    paintResponseCurve(g);
    paintNodes(g);
    paintHoverCard(g);
#if DEFAULT_EQUALIZER_FULL
    paintCollisionWarnings(g);
#endif

    // Border
    g.setColour(fg);
    g.drawRect(getLocalBounds(), 2);
}

void ResponseCurveComponent::paintMatchPreview(juce::Graphics& g)
{
    if (!proc.matchEQ.isMatchActive()) return;
    const auto* correction = proc.matchEQ.getCorrectionDb();
    const int bins = proc.matchEQ.getNumBins();
    const double sr = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
    juce::Path path;
    bool started = false;
    for (int bin = 1; bin < bins; bin += 4)
    {
        const float frequency = (float)(bin * sr / MatchEQ::fftSize);
        if (frequency < minFreq || frequency > maxFreq) continue;
        const float x = freqToX(frequency);
        const float y = dbToY(std::clamp(correction[bin], minDb, maxDb));
        if (!started) { path.startNewSubPath(x, y); started = true; }
        else path.lineTo(x, y);
    }
    if (!started) return;
    const auto fg = darkMode ? juce::Colour(0xfff6f6f6) : juce::Colour(0xff050505);
    const float dash[] { 5.0f, 4.0f };
    juce::Path dashed;
    juce::PathStrokeType(1.0f).createDashedStroke(dashed, path, dash, 2);
    g.setColour(fg.withAlpha(0.55f));
    g.strokePath(dashed, juce::PathStrokeType(1.0f));
}

// ── Grid ───────────────────────────────────────────────────────────
void ResponseCurveComponent::paintGrid(juce::Graphics& g)
{
    const float h = (float)getHeight();
    const auto fg = darkMode ? juce::Colour(0xfff6f6f6) : juce::Colour(0xff050505);
    g.setColour(fg.withAlpha(0.13f));

    // Frequency grid lines
    const float freqLines[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    for (float f : freqLines)
    {
        const float x = freqToX(f);
        g.drawVerticalLine((int)x, 0.0f, h);
    }

    // dB grid lines
    const float dbLines[] = { -18, -12, -6, 0, 6, 12, 18 };
    for (float db : dbLines)
    {
        const float y = dbToY(db);
        if (db == 0.0f)
        {
            g.setColour(fg.withAlpha(0.24f));
            g.drawHorizontalLine((int)y, 0.0f, (float)getWidth());
            g.setColour(fg.withAlpha(0.13f));
        }
        else
        {
            g.drawHorizontalLine((int)y, 0.0f, (float)getWidth());
        }
    }

    // Frequency labels
    g.setColour(fg.withAlpha(0.55f));
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain)));

    const std::pair<float, const char*> freqLabels[] = {
        { 100,  "100" }, { 1000, "1k" }, { 10000, "10k" }
    };
    for (auto& [f, label] : freqLabels)
    {
        const float x = freqToX(f);
        g.drawText(label, (int)x - 15, (int)h - 14, 30, 12, juce::Justification::centred);
    }

    // dB labels
    const std::pair<float, const char*> dbLabels[] = {
        { 12, "+12" }, { 6, "+6" }, { 0, "0" }, { -6, "-6" }, { -12, "-12" }
    };
    for (auto& [db, label] : dbLabels)
    {
        const float y = dbToY(db);
        g.drawText(label, 2, (int)y - 6, 28, 12, juce::Justification::left);
    }
}

// ── Spectrum analyzer ──────────────────────────────────────────────
void ResponseCurveComponent::pushSpectrumData(const float* mags, int numBins, double sr, bool input)
{
    if (spectrumFrozen) return;
    const int n = std::min(numBins, maxSpectrumBins);
    std::copy(mags, mags + n, input ? inputSpectrum : outputSpectrum);
    currentSpectrumSize = n;
    spectrumSampleRate = sr;
}

void ResponseCurveComponent::paintSpectrum(juce::Graphics& g)
{
    if (currentSpectrumSize == 0)
        return;

    const float w = (float)getWidth();
    const float h = (float)getHeight();
    const float binWidth = (float)(spectrumSampleRate / (2.0 * currentSpectrumSize));

    const auto draw = [&](const float* source, float* smoothed, float* peaks, float alpha, float fillAlpha)
    {
        juce::Path outline;
        juce::Path peakPath;
        bool started = false;
        bool peakStarted = false;
        float lastY = h, lastPeakY = h;
        for (int i = 1; i < currentSpectrumSize; i += analyzerStride)
        {
            const float target = source[i];
            smoothed[i] += analyzerAveraging * (target - smoothed[i]);
            smoothed[i] = std::max(target, smoothed[i] - analyzerDecayDb);
            const float freq = (float)i * binWidth;
            if (freq < minFreq || freq > maxFreq) continue;
            const float tilted = smoothed[i] + analyzerTiltDbPerOct * std::log2(freq / 1000.0f);
            const float ceiling = analyzerFloorDb + analyzerRangeDb;
            const float db = juce::jlimit(analyzerFloorDb, ceiling, tilted);
            const float x = freqToX(freq);
            const float y = dbToY(juce::jmap(db, analyzerFloorDb, ceiling, minDb, 0.0f));
            if (!started) { outline.startNewSubPath(0.0f, y); outline.lineTo(x, y); started = true; }
            else outline.lineTo(x, y);
            lastY = y;
            if (peakHold)
            {
                peaks[i] = std::max(peaks[i], tilted);
                const float py = dbToY(juce::jmap(juce::jlimit(analyzerFloorDb, ceiling, peaks[i]), analyzerFloorDb, ceiling, minDb, 0.0f));
                if (!peakStarted) { peakPath.startNewSubPath(0.0f, py); peakPath.lineTo(x, py); peakStarted = true; }
                else peakPath.lineTo(x, py);
                lastPeakY = py;
            }
        }
        if (!started) return;
        outline.lineTo(w, lastY);
        if (peakStarted) peakPath.lineTo(w, lastPeakY);
        auto fill = outline;
        fill.lineTo(w, h); fill.lineTo(0.0f, h); fill.closeSubPath();
        const auto fg = darkMode ? juce::Colour(0xfff6f6f6) : juce::Colour(0xff050505);
        g.setColour(fg.withAlpha(fillAlpha)); g.fillPath(fill);
        g.setColour(fg.withAlpha(alpha)); g.strokePath(outline, juce::PathStrokeType(alpha > 0.5f ? 1.5f : 1.0f));
        if (peakStarted) { g.setColour(fg.withAlpha(alpha * 0.48f)); g.strokePath(peakPath, juce::PathStrokeType(1.0f)); }
    };
    if (showInputSpectrum) draw(inputSpectrum, smoothedInputSpectrum, peakInputSpectrum, 0.30f, 0.035f);
    if (showOutputSpectrum) draw(outputSpectrum, smoothedOutputSpectrum, peakOutputSpectrum, 0.82f, 0.10f);
}

// ── Per-band curves (subtle fills) ─────────────────────────────────
void ResponseCurveComponent::paintBandCurves(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float zeroY = dbToY(0.0f);

    for (int b = 0; b < kNumBands; ++b)
    {
        const int idx = b + 1;
        const bool on = proc.apvts.getRawParameterValue(bandId(idx, "on"))->load() > 0.5f;
        if (!on) continue;

        const bool isSelected = selection[(size_t)b];
        const bool isPrimary = (b == selectedBand);
        const float alpha = isSelected ? 0.15f : 0.06f;

        juce::Path fillPath;
        fillPath.startNewSubPath(0.0f, zeroY);

        for (int i = 0; i < numPoints; ++i)
        {
            const float x = (float)i / (float)(numPoints - 1) * w;
            const float y = dbToY(perBandMagnitudes[b][i]);
            fillPath.lineTo(x, y);
        }

        fillPath.lineTo(w, zeroY);
        fillPath.closeSubPath();

        const auto fg = darkMode ? juce::Colour(0xfff6f6f6) : juce::Colour(0xff050505);
        g.setColour(fg.withAlpha(alpha));
        g.fillPath(fillPath);
    }
}

// ── Composite response curve ───────────────────────────────────────
void ResponseCurveComponent::paintResponseCurve(juce::Graphics& g)
{
    const float w = (float)getWidth();

    juce::Path curvePath;
    for (int i = 0; i < numPoints; ++i)
    {
        const float x = (float)i / (float)(numPoints - 1) * w;
        const float y = dbToY(magnitudes[i]);

        if (i == 0)
            curvePath.startNewSubPath(x, y);
        else
            curvePath.lineTo(x, y);
    }

    const auto fg = darkMode ? juce::Colour(0xfff6f6f6) : juce::Colour(0xff050505);
    g.setColour(fg.withAlpha(0.98f));
    g.strokePath(curvePath, juce::PathStrokeType(2.0f));
}

// ── Band nodes ─────────────────────────────────────────────────────
void ResponseCurveComponent::paintNodes(juce::Graphics& g)
{
    for (int b = 0; b < kNumBands; ++b)
    {
        const int idx = b + 1;
        const bool on = proc.apvts.getRawParameterValue(bandId(idx, "on"))->load() > 0.5f;
        if (!on) continue;

        const float freq = proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load();
        const float gain = proc.apvts.getRawParameterValue(bandId(idx, "gain"))->load()
                           * proc.apvts.getRawParameterValue("scale")->load();

        const float x = freqToX(freq);
        const float y = dbToY(gain);
        const float r = nodeRadius;

        const bool isSelected = selection[(size_t) b];
        const bool isPrimary  = (b == selectedBand);
        const bool isHovered  = (b == hoveredBand);

        const auto colour = darkMode ? juce::Colour(0xfff6f6f6) : juce::Colour(0xff050505);

        if (isSelected)
        {
            g.setColour(colour.withAlpha(0.22f));
            g.fillRect(x - r * 2.0f, y - r * 2.0f, r * 4.0f, r * 4.0f);
            if (!isPrimary)
            {
                g.setColour(colour.withAlpha(0.95f));
                g.drawRect(x - r * 1.55f, y - r * 1.55f, r * 3.1f, r * 3.1f, 1.0f);
            }
        }

        g.setColour(colour.withAlpha(isHovered || isSelected ? 1.0f : 0.7f));
        g.fillRect(x - r, y - r, r * 2.0f, r * 2.0f);

        g.setColour(darkMode ? juce::Colour(0xff050505) : juce::Colour(0xfff6f6f6));
        g.drawRect(x - r, y - r, r * 2.0f, r * 2.0f, 1.5f);

        // Band number label
        g.setColour(darkMode ? juce::Colour(0xff050505) : juce::Colour(0xfff6f6f6));
        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::bold)));
        g.drawText(juce::String(idx), (int)(x - r), (int)(y - r), (int)(r * 2.0f), (int)(r * 2.0f),
                   juce::Justification::centred);

        const bool midSide = proc.apvts.getRawParameterValue(bandId(idx, "placement_mode"))->load() > 0.5f;
        const float placement = proc.apvts.getRawParameterValue(bandId(idx, "placement"))->load();
        const auto label = std::abs(placement) < 1.0f ? (midSide ? "MS" : "LR")
                         : placement < 0.0f ? (midSide ? "M" : "L") : (midSide ? "S" : "R");
        g.setColour(colour.withAlpha(0.9f));
        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 8.0f, juce::Font::bold)));
        g.drawText(label, (int)(x - r), (int)(y + r + 1), (int)(r * 2.0f), 10,
                   juce::Justification::centred);
    }
}

void ResponseCurveComponent::paintHoverCard(juce::Graphics& g)
{
    const int band = hoveredBand >= 0 ? hoveredBand : (dragging ? selectedBand : -1);
    if (band < 0) return;

    const int idx = band + 1;
    const float freq = proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load();
    const float gain = proc.apvts.getRawParameterValue(bandId(idx, "gain"))->load();
    const float q = proc.apvts.getRawParameterValue(bandId(idx, "q"))->load();
    const float slope = proc.apvts.getRawParameterValue(bandId(idx, "slope"))->load();
    const float drive = proc.apvts.getRawParameterValue(bandId(idx, "drive"))->load();
    const bool driveOn = proc.apvts.getRawParameterValue(bandId(idx, "drive_on"))->load() > 0.5f;
    const float x = freqToX(freq);
    const float y = dbToY(gain * proc.apvts.getRawParameterValue("scale")->load());

    const int cardW = juce::jmin(254, getWidth() - 12);
    const bool dynamic = proc.apvts.getRawParameterValue(bandId(idx, "dyn_on"))->load() > 0.5f;
    const float dynNow = proc.getBandDynamicGainDb(band);
    const float threshold = proc.apvts.getRawParameterValue(bandId(idx, "dyn_thresh"))->load();
    const float range = proc.apvts.getRawParameterValue(bandId(idx, "dyn_range"))->load();
    const int cardH = dynamic ? 78 : 63;
    int cardX = juce::jlimit(6, getWidth() - cardW - 6, (int)x - cardW / 2);
    int cardY = (int)y - cardH - 20;
    if (cardY < 8) cardY = (int)y + 20;

    const auto fg = darkMode ? juce::Colour(0xfff6f6f6) : juce::Colour(0xff050505);
    const auto bg = darkMode ? juce::Colour(0xff050505) : juce::Colour(0xfff6f6f6);
    g.setColour(fg);
    g.fillRect(cardX, cardY, cardW, cardH);
    g.setColour(bg);
    g.fillRect(cardX + 2, cardY + 2, cardW - 4, cardH - 4);
    g.setColour(fg);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::bold)));

    const auto freqText = freq >= 1000.0f ? juce::String(freq / 1000.0f, 2) + " kHz"
                                          : juce::String(freq, 1) + " Hz";
    const auto cleanGain = std::abs(gain) < 0.005f ? 0.0f : gain;
    g.drawText("B" + juce::String(idx) + "  " + freqText + "  " + juce::String(cleanGain, 2) + " dB",
               cardX + 9, cardY + 6, cardW - 18, 15, juce::Justification::centredLeft);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain)));
    g.setColour(fg.withAlpha(0.72f));
    const bool adaptive = proc.apvts.getRawParameterValue("adaptive_q")->load() > 0.5f;
    const float actualQ = adaptive ? DefaultEqualizerAudioProcessor::calculateAdaptiveQ(q, gain) : q;
    const auto qText = adaptive ? "Q " + juce::String(q, 3) + " > " + juce::String(actualQ, 3)
                                : "Q " + juce::String(q, 3);
    g.drawText(qText + "   SLOPE " + juce::String(slope, 1) + " dB/oct",
               cardX + 9, cardY + 25, cardW - 18, 13, juce::Justification::centredLeft);
    g.drawText("DRIVE " + juce::String(drive, 1) + " dB  " + (driveOn ? "ON" : "OFF"),
               cardX + 9, cardY + 41, cardW - 18, 13, juce::Justification::centredLeft);
    if (dynamic)
        g.drawText("DYN  THR " + juce::String(threshold, 1) + "  RNG " + juce::String(range, 1)
                   + "  NOW " + juce::String(dynNow, 2) + " dB",
                   cardX + 9, cardY + 56, cardW - 18, 13, juce::Justification::centredLeft);
}

// ── Hit-testing ────────────────────────────────────────────────────
int ResponseCurveComponent::hitTestNode(float mx, float my) const
{
    const float hitRadius = nodeRadius + 5.0f;

    for (int b = 0; b < kNumBands; ++b)
    {
        const int idx = b + 1;
        const bool on = proc.apvts.getRawParameterValue(bandId(idx, "on"))->load() > 0.5f;
        if (!on) continue;

        const float freq = proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load();
        const float gain = proc.apvts.getRawParameterValue(bandId(idx, "gain"))->load()
                           * proc.apvts.getRawParameterValue("scale")->load();

        const float nx = freqToX(freq);
        const float ny = dbToY(gain);

        const float dx = mx - nx;
        const float dy = my - ny;
        if (dx * dx + dy * dy <= hitRadius * hitRadius)
            return b;
    }

    return -1;
}

// ── Mouse interaction ──────────────────────────────────────────────
void ResponseCurveComponent::mouseDown(const juce::MouseEvent& e)
{
    if (numericEditor.isVisible() && !numericEditor.getBounds().contains(e.getPosition()))
        dismissNumericEditor();
    const int hit = hitTestNode((float)e.x, (float)e.y);

    if (e.mods.isPopupMenu() && hit >= 0)
    {
        // Right-click context menu
        if (!selection[(size_t)hit]) { selection.fill(false); selection[(size_t)hit] = true; }
        selectedBand = hit;
        const int idx = hit + 1;

        juce::PopupMenu menu;
        menu.setLookAndFeel(&getLookAndFeel());
        menu.addItem(1, "Enable/Disable Band " + juce::String(idx));
        menu.addSeparator();
        menu.addItem(10, "Bell");
        menu.addItem(11, "Low Shelf");
        menu.addItem(12, "High Shelf");
        menu.addItem(13, "High Pass");
        menu.addItem(14, "Low Pass");
        menu.addItem(15, "Bandpass");
        menu.addItem(16, "Notch");
        menu.addSeparator();
        menu.addItem(20, "Enter frequency...");
        menu.addItem(21, "Enter gain...");
        menu.addItem(22, "Enter Q...");
        menu.addItem(23, "Enter slope...");
        menu.addSeparator();
        menu.addItem(40, "Placement mode: L/R");
        menu.addItem(41, "Placement mode: M/S");
        menu.addItem(42, "Placement: first channel");
        menu.addItem(43, "Placement: center");
        menu.addItem(44, "Placement: second channel");
        menu.addSeparator();
        menu.addItem(2, "Delete band");
        menu.addItem(3, "Reset band");
        const int selectedCount = (int) std::count(selection.begin(), selection.end(), true);
        if (selectedCount > 1)
        {
            menu.addSeparator();
            menu.addItem(30, "Bypass selected (" + juce::String(selectedCount) + ")");
            menu.addItem(31, "Delete selected (" + juce::String(selectedCount) + ")");
        }

        menu.showMenuAsync(juce::PopupMenu::Options(), [this, idx, px = (float)e.x, py = (float)e.y](int result)
        {
            if (result == 1)
            {
                auto* p = proc.apvts.getRawParameterValue(bandId(idx, "on"));
                auto* param = proc.apvts.getParameter(bandId(idx, "on"));
                if (param) param->setValueNotifyingHost(p->load() > 0.5f ? 0.0f : 1.0f);
            }
            else if (result == 2)
            {
                proc.undoManager.beginNewTransaction("Delete EQ band");
                proc.resetBandToDefaults(idx - 1, false);
            }
            else if (result == 3)
            {
                proc.undoManager.beginNewTransaction("Reset EQ band");
                proc.resetBandToDefaults(idx - 1, true);
            }
            else if (result >= 10 && result <= 16)
            {
                auto* param = proc.apvts.getParameter(bandId(idx, "type"));
                if (param)
                    param->setValueNotifyingHost(param->convertTo0to1((float)(result - 10)));
            }
            else if (result >= 20 && result <= 23)
            {
                static const char* suffixes[] = { "freq", "gain", "q", "slope" };
                showNumericEditor(idx - 1, suffixes[result - 20], px, py);
            }
            else if (result == 30 || result == 31)
            {
                proc.undoManager.beginNewTransaction(result == 30 ? "Bypass selected bands" : "Delete selected bands");
                for (int b = 0; b < kNumBands; ++b)
                    if (selection[(size_t)b])
                    {
                        if (result == 31)
                        {
                            proc.resetBandToDefaults(b, false);
                            continue;
                        }
                        if (auto* parameter = proc.apvts.getParameter(bandId(b + 1, "on")))
                        {
                            parameter->beginChangeGesture();
                            parameter->setValueNotifyingHost(0.0f);
                            parameter->endChangeGesture();
                        }
                    }
            }
            else if (result >= 40 && result <= 44)
            {
                proc.undoManager.beginNewTransaction("Set selected band placement");
                for (int b = 0; b < kNumBands; ++b)
                    if (selection[(size_t)b])
                    {
                        const bool modeChange = result <= 41;
                        auto* parameter = proc.apvts.getParameter(bandId(b + 1,
                            modeChange ? "placement_mode" : "placement"));
                        if (parameter != nullptr)
                        {
                            parameter->beginChangeGesture();
                            const float value = modeChange ? (result == 41 ? 1.0f : 0.0f)
                                : result == 42 ? -100.0f : result == 44 ? 100.0f : 0.0f;
                            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
                            parameter->endChangeGesture();
                        }
                    }
            }
        });
        return;
    }

    if (hit >= 0 && e.mods.isShiftDown())
    {
        selection[(size_t)hit] = !selection[(size_t)hit];
        if (selection[(size_t)hit]) selectedBand = hit;
        if (std::none_of(selection.begin(), selection.end(), [](bool v) { return v; }))
            selection[(size_t)hit] = true;
        if (!selection[(size_t)selectedBand])
            for (int b = 0; b < kNumBands; ++b)
                if (selection[(size_t)b]) { selectedBand = b; break; }
        repaint();
        return;
    }

    if (hit >= 0 && !selection[(size_t)hit])
    {
        selection.fill(false);
        selection[(size_t)hit] = true;
    }
    if (hit < 0) return;
    selectedBand = hit;
    if (e.mods.isCommandDown())
    {
        dragging = driveDragging = true;
        proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
            ? "Adjust selected band drive" : "Adjust band drive");
        for (int b = 0; b < kNumBands; ++b)
        {
            dragDriveParams[(size_t)b] = nullptr;
            if (!selection[(size_t)b]) continue;
            dragStartDrive[(size_t)b] = proc.apvts.getRawParameterValue(bandId(b + 1, "drive"))->load();
            dragDriveParams[(size_t)b] = proc.apvts.getParameter(bandId(b + 1, "drive"));
            if (auto* driveEnabled = proc.apvts.getParameter(bandId(b + 1, "drive_on")))
                driveEnabled->setValueNotifyingHost(1.0f);
            if (dragDriveParams[(size_t)b]) dragDriveParams[(size_t)b]->beginChangeGesture();
        }
        return;
    }
    if (hit >= 0)
    {
        dragging = true;
        proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
            ? "Move selected EQ bands" : "Move EQ band");
        groupAnchorFreq = proc.apvts.getRawParameterValue(bandId(hit + 1, "freq"))->load();
        groupAnchorGain = proc.apvts.getRawParameterValue(bandId(hit + 1, "gain"))->load();
        for (int b = 0; b < kNumBands; ++b)
        {
            dragFreqParams[(size_t)b] = dragGainParams[(size_t)b] = nullptr;
            if (!selection[(size_t)b]) continue;
            dragStartFreq[(size_t)b] = proc.apvts.getRawParameterValue(bandId(b + 1, "freq"))->load();
            dragStartGain[(size_t)b] = proc.apvts.getRawParameterValue(bandId(b + 1, "gain"))->load();
            dragFreqParams[(size_t)b] = proc.apvts.getParameter(bandId(b + 1, "freq"));
            dragGainParams[(size_t)b] = proc.apvts.getParameter(bandId(b + 1, "gain"));
            if (dragFreqParams[(size_t)b]) dragFreqParams[(size_t)b]->beginChangeGesture();
            if (dragGainParams[(size_t)b]) dragGainParams[(size_t)b]->beginChangeGesture();
        }
    }
}

void ResponseCurveComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragging || selectedBand < 0) return;

    if (driveDragging)
    {
        const float delta = -(float)e.getDistanceFromDragStartY() * 0.18f;
        for (int b = 0; b < kNumBands; ++b)
            if (auto* parameter = dragDriveParams[(size_t)b])
            {
                const float value = juce::jlimit(0.0f, 36.0f, dragStartDrive[(size_t)b] + delta);
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            }
        repaint();
        return;
    }

    const int idx = selectedBand + 1;

    const float anchorFreq = std::clamp(xToFreq((float)e.x), minFreq, maxFreq);
    const float ratio = anchorFreq / std::max(1.0f, groupAnchorFreq);
    const float scale = proc.apvts.getRawParameterValue("scale")->load();
    float anchorGain = std::clamp(yToDb((float)e.y), minDb, maxDb);
    if (scale > 0.001f) anchorGain /= scale;
    const float gainDelta = anchorGain - groupAnchorGain;
    for (int b = 0; b < kNumBands; ++b)
    {
        if (!selection[(size_t)b]) continue;
        const float newFreq = std::clamp(dragStartFreq[(size_t)b] * ratio, minFreq, maxFreq);
        const float newGain = std::clamp(dragStartGain[(size_t)b] + gainDelta, -24.0f, 24.0f);
        if (auto* p = dragFreqParams[(size_t)b]) p->setValueNotifyingHost(p->convertTo0to1(newFreq));
        if (auto* p = dragGainParams[(size_t)b]) p->setValueNotifyingHost(p->convertTo0to1(newGain));
    }
}

void ResponseCurveComponent::mouseUp(const juce::MouseEvent&)
{
    for (int b = 0; b < kNumBands; ++b)
    {
        if (dragFreqParams[(size_t)b]) dragFreqParams[(size_t)b]->endChangeGesture();
        if (dragGainParams[(size_t)b]) dragGainParams[(size_t)b]->endChangeGesture();
        dragFreqParams[(size_t)b] = dragGainParams[(size_t)b] = nullptr;
        if (dragDriveParams[(size_t)b]) dragDriveParams[(size_t)b]->endChangeGesture();
        dragDriveParams[(size_t)b] = nullptr;
    }
    dragging = driveDragging = false;
}

void ResponseCurveComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int existing = hitTestNode((float)e.x, (float)e.y);
    if (existing >= 0)
    {
        proc.undoManager.beginNewTransaction("Delete EQ band");
        proc.resetBandToDefaults(existing, false);
        selection[(size_t) existing] = false;
        selectedBand = -1;
        repaint();
        return;
    }

    int freeBand = -1;
    for (int b = 0; b < kNumBands; ++b)
        if (proc.apvts.getRawParameterValue(bandId(b + 1, "on"))->load() < 0.5f)
        {
            freeBand = b;
            break;
        }
    if (freeBand < 0)
        return;

    proc.undoManager.beginNewTransaction("Create EQ band");
    proc.resetBandToDefaults(freeBand, true,
        std::clamp(xToFreq((float)e.x), minFreq, maxFreq),
        std::clamp(yToDb((float)e.y), minDb, maxDb));
    selectedBand = freeBand;
    repaint();
}

void ResponseCurveComponent::showNumericEditor(int band, const juce::String& suffix, float x, float y)
{
    numericSuffix = suffix;
    numericParameter = proc.apvts.getParameter(bandId(band + 1, suffix.toRawUTF8()));
    if (numericParameter == nullptr) return;
    const float value = proc.apvts.getRawParameterValue(bandId(band + 1, suffix.toRawUTF8()))->load();
    juce::String text;
    if (suffix == "freq") text = value >= 1000.0f ? juce::String(value / 1000.0f, 3) + " kHz" : juce::String(value, 2) + " Hz";
    else if (suffix == "gain") text = juce::String(std::abs(value) < 0.005f ? 0.0f : value, 2) + " dB";
    else if (suffix == "slope") text = juce::String(value, 1) + " dB/oct";
    else text = juce::String(value, 3);
    numericEditor.setText(text, false);
    const int width = 150, height = 30;
    numericEditor.setBounds(juce::jlimit(4, getWidth() - width - 4, (int)x - width / 2),
                            juce::jlimit(4, getHeight() - height - 4, (int)y - 44), width, height);
    numericEditor.setVisible(true);
    numericEditor.toFront(true);
    numericEditor.grabKeyboardFocus();
}

void ResponseCurveComponent::commitNumericEditor()
{
    if (numericParameter == nullptr) return;
    auto source = numericEditor.getText().trim().toLowerCase().replaceCharacter(',', '.');
    double multiplier = 1.0;
    if (numericSuffix == "freq" && (source.contains("khz") || source.endsWithChar('k'))) multiplier = 1000.0;
    const auto digits = source.retainCharacters("-+0123456789.e");
    if (digits.isNotEmpty())
    {
        const double value = digits.getDoubleValue() * multiplier;
        if (std::isfinite(value))
        {
            proc.undoManager.beginNewTransaction("Enter band " + numericSuffix);
            numericParameter->beginChangeGesture();
            numericParameter->setValueNotifyingHost(numericParameter->convertTo0to1((float)value));
            numericParameter->endChangeGesture();
        }
    }
    numericParameter = nullptr;
    numericEditor.setVisible(false);
    grabKeyboardFocus();
}

void ResponseCurveComponent::dismissNumericEditor()
{
    numericParameter = nullptr;
    numericEditor.setVisible(false);
}

void ResponseCurveComponent::mouseMove(const juce::MouseEvent& e)
{
    const int hit = hitTestNode((float)e.x, (float)e.y);
    if (hit != hoveredBand)
    {
        hoveredBand = hit;
        setMouseCursor(hit >= 0 ? juce::MouseCursor::PointingHandCursor
                                : juce::MouseCursor::CrosshairCursor);
        repaint();
    }
}

void ResponseCurveComponent::mouseExit(const juce::MouseEvent&)
{
    hoveredBand = -1;
    repaint();
}

void ResponseCurveComponent::mouseWheelMove(const juce::MouseEvent& e,
                                             const juce::MouseWheelDetails& wheel)
{
    const int hit = hitTestNode((float)e.x, (float)e.y);
    if (hit < 0 || std::abs(wheel.deltaY) < 0.0001f) return;

    selectedBand = hit;
    if (!selection[(size_t)hit]) { selection.fill(false); selection[(size_t)hit] = true; }
    const float oldQ = proc.apvts.getRawParameterValue(bandId(hit + 1, "q"))->load();
    const float factor = std::pow(2.0f, wheel.deltaY * 0.9f);
    juce::ignoreUnused(oldQ);
    proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
        ? "Adjust selected band Q" : "Adjust band Q");
    for (int b = 0; b < kNumBands; ++b)
        if (selection[(size_t)b])
            if (auto* parameter = proc.apvts.getParameter(bandId(b + 1, "q")))
            {
                const float q = proc.apvts.getRawParameterValue(bandId(b + 1, "q"))->load();
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->convertTo0to1(std::clamp(q * factor, 0.1f, 24.0f)));
                parameter->endChangeGesture();
            }
    repaint();
}

#if DEFAULT_EQUALIZER_FULL
// ── Collision detection (warn when bands overlap within 1/3 octave) ──
void ResponseCurveComponent::paintCollisionWarnings(juce::Graphics& g)
{
    // Gather active band frequencies
    struct BandInfo { int index; float freq; };
    std::vector<BandInfo> active;
    active.reserve(kNumBands);

    for (int b = 0; b < kNumBands; ++b)
    {
        const int idx = b + 1;
        const bool on = proc.apvts.getRawParameterValue(bandId(idx, "on"))->load() > 0.5f;
        if (!on) continue;
        const float freq = proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load();
        active.push_back({ b, freq });
    }

    // Check each pair for 1/3-octave proximity
    const float thirdOctaveRatio = std::pow(2.0f, 1.0f / 3.0f); // ~1.26

    for (size_t i = 0; i < active.size(); ++i)
    {
        for (size_t j = i + 1; j < active.size(); ++j)
        {
            float ratio = active[i].freq / active[j].freq;
            if (ratio < 1.0f) ratio = 1.0f / ratio;

            if (ratio < thirdOctaveRatio)
            {
                // Draw amber warning ring on both nodes
                auto drawWarning = [&](const BandInfo& bi)
                {
                    const float gain = proc.apvts.getRawParameterValue(
                        bandId(bi.index + 1, "gain"))->load()
                        * proc.apvts.getRawParameterValue("scale")->load();
                    const float x = freqToX(bi.freq);
                    const float y = dbToY(gain);
                    const float wr = nodeRadius + 4.0f;

                    const auto fg = darkMode ? juce::Colour(0xfff6f6f6) : juce::Colour(0xff050505);
                    g.setColour(fg.withAlpha(0.6f));
                    g.drawRect(x - wr, y - wr, wr * 2.0f, wr * 2.0f, 2.0f);
                };

                drawWarning(active[i]);
                drawWarning(active[j]);
            }
        }
    }
}
#endif
