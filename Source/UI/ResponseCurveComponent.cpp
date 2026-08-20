#include "ResponseCurveComponent.h"
#include "../DSP/VariableSlope.h"
#include "../PluginProcessor.h"
#include <complex>

static juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}

namespace
{
class ChoiceRow final : public juce::PopupMenu::CustomComponent
{
public:
    ChoiceRow(int count, int selected, bool dark, std::function<void(int)> callback,
              std::function<void(juce::Graphics&, juce::Rectangle<float>, int, juce::Colour)> painter)
        : juce::PopupMenu::CustomComponent(false), count(count), selected(selected), dark(dark),
          callback(std::move(callback)), painter(std::move(painter)) {}

    void getIdealSize(int& width, int& height) override
    {
        width = count == 5 ? 180 : 280;
        height = 34;
    }

    void paint(juce::Graphics& g) override
    {
        const auto fg = dark ? juce::Colour(0xfff6f6f6) : juce::Colour(0xff050505);
        const auto bg = dark ? juce::Colour(0xff050505) : juce::Colour(0xfff6f6f6);
        g.fillAll(bg);
        const float cellW = (float)getWidth() / (float)count;
        for (int i = 0; i < count; ++i)
        {
            auto cell = juce::Rectangle<float>(cellW * i, 0.0f, cellW, (float)getHeight()).reduced(1.0f);
            if (i == selected)
            {
                g.setColour(fg); g.fillRect(cell);
                painter(g, cell.reduced(4.0f), i, bg);
            }
            else
            {
                g.setColour(fg.withAlpha(i == hovered ? 0.20f : 0.08f)); g.fillRect(cell);
                painter(g, cell.reduced(4.0f), i, fg);
            }
        }
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const int next = choiceAt(e.x);
        if (next != hovered) { hovered = next; repaint(); }
    }
    void mouseExit(const juce::MouseEvent&) override { hovered = -1; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const int choice = choiceAt(e.x);
        if (choice >= 0 && callback) callback(choice);
        triggerMenuItem();
    }

private:
    int choiceAt(int x) const noexcept
    {
        return juce::jlimit(0, count - 1, x * count / juce::jmax(1, getWidth()));
    }
    int count = 0, selected = 0, hovered = -1;
    bool dark = false;
    std::function<void(int)> callback;
    std::function<void(juce::Graphics&, juce::Rectangle<float>, int, juce::Colour)> painter;
};

void paintFilterIcon(juce::Graphics& g, juce::Rectangle<float> r, int type, juce::Colour colour)
{
    r = r.withSizeKeepingCentre(juce::jmin(26.0f, r.getWidth()),
                                juce::jmin(16.0f, r.getHeight()));
    juce::Path p;
    const float x0 = r.getX(), x1 = r.getRight(), y0 = r.getY(), y1 = r.getBottom();
    const float midX = r.getCentreX(), midY = r.getCentreY();
    switch (type)
    {
        case 0: p.startNewSubPath(x0, midY); p.cubicTo(midX - 8, midY, midX - 7, y0 + 3, midX, y0 + 3);
                p.cubicTo(midX + 7, y0 + 3, midX + 8, midY, x1, midY); break;
        case 1: p.startNewSubPath(x0, y0 + 4); p.lineTo(midX - 5, y0 + 4); p.cubicTo(midX, y0 + 4, midX, midY, midX + 5, midY); p.lineTo(x1, midY); break;
        case 2: p.startNewSubPath(x0, midY); p.lineTo(midX - 5, midY); p.cubicTo(midX, midY, midX, y0 + 4, midX + 5, y0 + 4); p.lineTo(x1, y0 + 4); break;
        case 3: p.startNewSubPath(x0, y1); p.cubicTo(x0 + 6, y1 - 9, midX - 5, midY, midX + 2, midY); p.lineTo(x1, midY); break;
        case 4: p.startNewSubPath(x0, midY); p.lineTo(midX - 2, midY); p.cubicTo(midX + 5, midY, x1 - 6, y1 - 9, x1, y1); break;
        case 5: p.startNewSubPath(x0, y1 - 3); p.cubicTo(midX - 8, y1 - 3, midX - 7, y0 + 3, midX, y0 + 3); p.cubicTo(midX + 7, y0 + 3, midX + 8, y1 - 3, x1, y1 - 3); break;
        case 6: p.startNewSubPath(x0, midY); p.cubicTo(midX - 7, midY, midX - 6, y1 - 3, midX, y1 - 3); p.cubicTo(midX + 6, y1 - 3, midX + 7, midY, x1, midY); break;
        case 7: p.startNewSubPath(x0, y1 - 4); p.lineTo(x1, y0 + 4); break;
    }
    const auto pathBounds = p.getBounds();
    p.applyTransform(juce::AffineTransform::translation(
        r.getCentreX() - pathBounds.getCentreX(), r.getCentreY() - pathBounds.getCentreY()));
    g.setColour(colour);
    g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
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
        return gainDb >= 0.0f ? 1 : 3; // low shelf / low cut
    if (frequencyHz >= 5000.0f)
        return gainDb >= 0.0f ? 2 : 4; // high shelf / high cut
    return 0;
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
void ResponseCurveComponent::updateResponseCurve()
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
    displayMaxDb = std::max(displayMaxDb, requiredDisplayDb);

    std::uint64_t signature = 1469598103934665603ULL;
    const auto hash = [&signature](float value)
    { signature = (signature ^ (std::uint64_t)std::llround(value * 1000.0f)) * 1099511628211ULL; };
    hash(amount); hash(proc.apvts.getRawParameterValue("adaptive_q")->load());
    for (int band = 1; band <= kNumBands; ++band)
    {
        for (auto* suffix : { "present", "on", "type", "freq", "q", "gain", "slope" })
            hash(proc.apvts.getRawParameterValue(bandId(band, suffix))->load());
        hash(proc.getBandDynamicGainDb(band - 1));
    }
    if (responseGridSampleRate == sr && responseSignature == signature)
        return;
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
        const float freq = proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load();
        float q = proc.apvts.getRawParameterValue(bandId(idx, "q"))->load();
        float gain = proc.apvts.getRawParameterValue(bandId(idx, "gain"))->load();
        if (proc.apvts.getRawParameterValue("adaptive_q")->load() > 0.5f)
            q = DefaultEqualizerAudioProcessor::calculateAdaptiveQ(q, gain);
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
            case 7: tp = Biquad::Type::Tilt; break;
        }

        const bool gainBearing = variable_slope::distributesGain(tp);
        if (gainBearing)
            gain *= amount;

        const bool cut = tp == Biquad::Type::HighPass || tp == Biquad::Type::LowPass;
        float responseFreq = freq;
        if (cut && amount > 0.0f)
        {
            const float neutralEdge = tp == Biquad::Type::LowPass ? (float)sr * 0.45f : 10.0f;
            responseFreq = neutralEdge * std::pow(std::max(1.0e-6f, freq / neutralEdge), amount);
            responseFreq = std::clamp(responseFreq, 10.0f, (float)sr * 0.45f);
        }

        const float slope = proc.apvts.getRawParameterValue(bandId(idx, "slope"))->load();
        constexpr bool decramp = true;

        for (int i = 0; i < numPoints; ++i)
        {
            const float f = responseFrequencies[i];

            const auto rawResponse = variable_slope::response(tp, sr, responseFreq, q, gain,
                                                              slope, decramp, f);
            const auto response = gainBearing || (cut && amount > 0.0f)
                ? rawResponse
                : cut ? std::complex<double>(1.0, 0.0)
                      : std::complex<double>(1.0, 0.0)
                          + (double)std::clamp(amount, 0.0f, 1.0f)
                              * (rawResponse - std::complex<double>(1.0, 0.0));
            const float mag = (float)(20.0 * std::log10(std::max(std::abs(response), 1.0e-15)));
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
    paintSpectrum(g);
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
    g.drawRect(getLocalBounds(), 3);
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
        const float y = dbToY(std::clamp(correction[bin], -displayMaxDb, displayMaxDb));
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

    // Keep the density stable as the working range expands: +/-12 uses 6 dB
    // divisions, while +/-24 and +/-36 use 12 dB divisions.
    const float dbStep = displayMaxDb <= 12.0f ? 6.0f : 12.0f;
    for (float db = -displayMaxDb; db <= displayMaxDb + 0.01f; db += dbStep)
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
    for (float db = displayMaxDb; db >= -displayMaxDb - 0.01f; db -= dbStep)
    {
        const float y = dbToY(db);
        const int labelY = juce::jlimit(1, juce::jmax(1, getHeight() - 13),
                                        juce::roundToInt(y) - 6);
        const juce::String label = db > 0.0f ? "+" + juce::String((int)db)
                                             : juce::String((int)db);
        g.drawText(label, 2, labelY, 30, 12, juce::Justification::left);
    }
}

// ── Spectrum analyzer ──────────────────────────────────────────────
void ResponseCurveComponent::pushSpectrumData(const float* mags, int numBins, double sr, bool input)
{
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
        for (int i = 1; i < currentSpectrumSize; ++i)
        {
            const float target = source[i];
            smoothed[i] += analyzerAveraging * (target - smoothed[i]);
            smoothed[i] = std::max(target, smoothed[i] - analyzerDecayDb);
            const float freq = (float)i * binWidth;
            if (freq < minFreq || freq > maxFreq) continue;
            const float tilted = smoothed[i] + analyzerTiltDbPerOct * std::log2(freq / 1000.0f);
            constexpr float ceiling = 0.0f;
            const float analyzerRangeDb = -analyzerFloorDb;
            const float db = juce::jlimit(analyzerFloorDb, ceiling, tilted);
            const float x = freqToX(freq);
            const float y = analyzerLevelToY(db, analyzerFloorDb, analyzerRangeDb, h);
            if (!started) { outline.startNewSubPath(0.0f, y); outline.lineTo(x, y); started = true; }
            else outline.lineTo(x, y);
            lastY = y;
            peaks[i] = std::max(peaks[i], tilted);
            const float py = analyzerLevelToY(
                juce::jlimit(analyzerFloorDb, ceiling, peaks[i]),
                analyzerFloorDb, analyzerRangeDb, h);
            if (!peakStarted) { peakPath.startNewSubPath(0.0f, py); peakPath.lineTo(x, py); peakStarted = true; }
            else peakPath.lineTo(x, py);
            lastPeakY = py;
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
        const bool present = proc.apvts.getRawParameterValue(bandId(idx, "present"))->load() > 0.5f;
        if (!present) continue;
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
        const bool present = proc.apvts.getRawParameterValue(bandId(idx, "present"))->load() > 0.5f;
        if (!present) continue;
        const bool on = proc.apvts.getRawParameterValue(bandId(idx, "on"))->load() > 0.5f;

        const float freq = proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load();
        const float gain = proc.apvts.getRawParameterValue(bandId(idx, "gain"))->load();

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

        const auto inverse = darkMode ? juce::Colour(0xff050505) : juce::Colour(0xfff6f6f6);
        if (on)
        {
            g.setColour(colour.withAlpha(isHovered || isSelected ? 1.0f : 0.7f));
            g.fillRect(x - r, y - r, r * 2.0f, r * 2.0f);
            g.setColour(inverse);
            g.drawRect(x - r, y - r, r * 2.0f, r * 2.0f, 1.5f);
        }
        else
        {
            g.setColour(inverse);
            g.fillRect(x - r, y - r, r * 2.0f, r * 2.0f);
            g.setColour(colour.withAlpha(isHovered || isSelected ? 0.85f : 0.45f));
            g.drawRect(x - r, y - r, r * 2.0f, r * 2.0f, 2.0f);
        }

        // Band number label
        g.setColour(on ? inverse : colour.withAlpha(isHovered || isSelected ? 0.85f : 0.45f));
        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::bold)));
        g.drawText(juce::String(idx), (int)(x - r), (int)(y - r), (int)(r * 2.0f), (int)(r * 2.0f),
                   juce::Justification::centred);

        const bool midSide = proc.apvts.getRawParameterValue(bandId(idx, "placement_mode"))->load() > 0.5f;
        const float placement = proc.apvts.getRawParameterValue(bandId(idx, "placement"))->load();
        const auto label = std::abs(placement) < 1.0f ? (midSide ? "MS" : "LR")
                         : placement < 0.0f ? (midSide ? "M" : "L") : (midSide ? "S" : "R");
        g.setColour(colour.withAlpha(on ? 0.9f : 0.42f));
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
    const float x = freqToX(freq);
    const float y = dbToY(gain);

    const float threshold = proc.apvts.getRawParameterValue(bandId(idx, "dyn_thresh"))->load();
    const bool midSide = proc.apvts.getRawParameterValue(bandId(idx, "placement_mode"))->load() > 0.5f;
    const float placement = proc.apvts.getRawParameterValue(bandId(idx, "placement"))->load();
    const bool adaptive = proc.apvts.getRawParameterValue("adaptive_q")->load() > 0.5f;
    const float actualQ = adaptive ? DefaultEqualizerAudioProcessor::calculateAdaptiveQ(q, gain) : q;
    const auto freqText = freq >= 1000.0f ? juce::String(freq / 1000.0f, 2) + " kHz"
                                          : juce::String(freq, 1) + " Hz";
    const auto cleanGain = std::abs(gain) < 0.005f ? 0.0f : gain;
    const auto headline = "B" + juce::String(idx) + "  " + freqText + "  "
        + juce::String(cleanGain, 2) + " dB";
    const auto qText = adaptive ? "Q " + juce::String(q, 3) + " > " + juce::String(actualQ, 3)
                                : "Q " + juce::String(q, 3);
    const auto qLine = qText + "   SLOPE " + juce::String(slope, 1) + " dB/oct";
    const auto driveLine = "DRIVE " + juce::String(drive, 1) + " dB";
    const auto placementText = std::abs(placement) < 0.05f
        ? juce::String(midSide ? "M/S CENTER" : "L/R CENTER")
        : juce::String(midSide ? "M/S " : "L/R ")
            + (placement < 0.0f ? (midSide ? "M " : "L ") : (midSide ? "S " : "R "))
            + juce::String(std::abs(placement), 0) + "%";
    const auto thresholdLine = "THR " + juce::String(threshold, 1) + " dB   " + placementText;
    const juce::Font headlineFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f,
                                                     juce::Font::bold));
    const juce::Font bodyFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f,
                                                 juce::Font::plain));
    const float widest = std::max({ juce::TextLayout::getStringWidth(headlineFont, headline),
                                    juce::TextLayout::getStringWidth(bodyFont, qLine),
                                    juce::TextLayout::getStringWidth(bodyFont, driveLine),
                                    juce::TextLayout::getStringWidth(bodyFont, thresholdLine) });
    const int cardW = juce::jlimit(120, getWidth() - 12, juce::roundToInt(std::ceil(widest)) + 18);
    const int cardH = 76;
    const auto clampCard = [this, cardW, cardH](int candidateX, int candidateY)
    {
        return juce::Rectangle<int>(juce::jlimit(6, getWidth() - cardW - 6, candidateX),
                                    juce::jlimit(8, getHeight() - cardH - 8, candidateY),
                                    cardW, cardH);
    };
    const std::array<juce::Rectangle<int>, 4> candidates {
        clampCard((int)x - cardW / 2, (int)y - cardH - 20),
        clampCard((int)x - cardW / 2, (int)y + 20),
        clampCard((int)x - cardW - 20, (int)y - cardH / 2),
        clampCard((int)x + 20, (int)y - cardH / 2)
    };
    const auto obstructionScore = [this, band, x, y](juce::Rectangle<int> card)
    {
        // Prefer a side that does not contain the node and intersects the
        // selected band's measured response as little as possible. A small
        // margin preserves enough visible curve around the card edge to read
        // the filter shape while dragging.
        const auto guarded = card.expanded(7);
        float score = guarded.contains(juce::roundToInt(x), juce::roundToInt(y)) ? 100000.0f : 0.0f;
        for (int point = 0; point < numPoints; point += 2)
        {
            const float curveX = (float)point / (float)(numPoints - 1) * (float)getWidth();
            const float curveY = dbToY(perBandMagnitudes[band][point]);
            if (guarded.contains(juce::roundToInt(curveX), juce::roundToInt(curveY)))
            {
                const float distanceFromNode = std::abs(curveX - x) / juce::jmax(1.0f, (float)getWidth());
                score += 2.0f - juce::jmin(1.0f, distanceFromNode);
            }
        }
        return score;
    };
    auto card = candidates.front();
    float bestScore = obstructionScore(card);
    for (size_t candidate = 1; candidate < candidates.size(); ++candidate)
    {
        const float score = obstructionScore(candidates[candidate]);
        if (score < bestScore)
        {
            bestScore = score;
            card = candidates[candidate];
        }
    }
    const int cardX = card.getX();
    const int cardY = card.getY();

    const auto fg = darkMode ? juce::Colour(0xfff6f6f6) : juce::Colour(0xff050505);
    const auto bg = darkMode ? juce::Colour(0xff050505) : juce::Colour(0xfff6f6f6);
    g.setColour(fg);
    g.fillRect(cardX, cardY, cardW, cardH);
    g.setColour(bg);
    g.fillRect(cardX + 2, cardY + 2, cardW - 4, cardH - 4);
    g.setColour(fg);
    g.setFont(headlineFont);
    g.drawText(headline,
               cardX + 9, cardY + 6, cardW - 18, 15, juce::Justification::centredLeft);
    g.setFont(bodyFont);
    g.setColour(fg.withAlpha(0.72f));
    g.drawText(qLine,
               cardX + 9, cardY + 25, cardW - 18, 13, juce::Justification::centredLeft);
    g.drawText(driveLine,
               cardX + 9, cardY + 41, cardW - 18, 13, juce::Justification::centredLeft);
    g.drawText(thresholdLine,
               cardX + 9, cardY + 56, cardW - 18, 13, juce::Justification::centredLeft);
}

// ── Hit-testing ────────────────────────────────────────────────────
int ResponseCurveComponent::hitTestNode(float mx, float my) const
{
    const float hitRadius = nodeRadius + 5.0f;

    for (int b = 0; b < kNumBands; ++b)
    {
        const int idx = b + 1;
        const bool present = proc.apvts.getRawParameterValue(bandId(idx, "present"))->load() > 0.5f;
        if (!present) continue;

        const float freq = proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load();
        const float gain = proc.apvts.getRawParameterValue(bandId(idx, "gain"))->load();

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
    int hit = hitTestNode((float)e.x, (float)e.y);

    if (e.mods.isPopupMenu() && hit < 0)
    {
        selectedBand = -1;
        selection.fill(false);
        repaint();
        return;
    }

    if (e.mods.isPopupMenu() && e.mods.isCommandDown() && hit >= 0)
    {
        proc.undoManager.beginNewTransaction("Reset band slope");
        if (auto* parameter = proc.apvts.getParameter(bandId(hit + 1, "slope")))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
            parameter->endChangeGesture();
        }
        repaint();
        return;
    }

    if (hit < 0 && e.mods.isLeftButtonDown() && !e.mods.isCommandDown()
        && !e.mods.isShiftDown() && !e.mods.isAltDown())
    {
        hit = createBandAt((float)e.x, (float)e.y, e.eventTime.toMilliseconds());
        if (hit >= 0)
        {
            // Creation and the immediately following move remain one Undo step.
            beginStaticBandDrag(hit, false);
            repaint();
        }
        return;
    }

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
        const int selectedType = juce::jlimit(0, 7,
            (int)proc.apvts.getRawParameterValue(bandId(idx, "type"))->load());
        menu.addCustomItem(100, std::make_unique<ChoiceRow>(8, selectedType, darkMode,
            [this](int type)
            {
                proc.undoManager.beginNewTransaction("Set selected band filter type");
                for (int b = 0; b < kNumBands; ++b)
                    if (selection[(size_t)b])
                    {
                        if (auto* parameter = proc.apvts.getParameter(bandId(b + 1, "type")))
                        {
                            parameter->beginChangeGesture();
                            parameter->setValueNotifyingHost(parameter->convertTo0to1((float)type));
                            parameter->endChangeGesture();
                        }
                        if (ResponseCurveComponent::typeDefaultsToMidSide(type))
                            if (auto* mode = proc.apvts.getParameter(bandId(b + 1, "placement_mode")))
                            {
                                mode->beginChangeGesture();
                                mode->setValueNotifyingHost(mode->convertTo0to1(1.0f));
                                mode->endChangeGesture();
                            }
                    }
            }, paintFilterIcon), nullptr, "Filter type");
        menu.addSeparator();
        const bool selectedMS = proc.apvts.getRawParameterValue(bandId(idx, "placement_mode"))->load() > 0.5f;
        const float selectedPlacement = proc.apvts.getRawParameterValue(bandId(idx, "placement"))->load();
        const int selectedRoute = std::abs(selectedPlacement) <= 1.0f ? 1
            : selectedMS ? (selectedPlacement < 0.0f ? 3 : 4)
                         : (selectedPlacement < 0.0f ? 0 : 2);
        menu.addCustomItem(101, std::make_unique<ChoiceRow>(5, selectedRoute, darkMode,
            [this](int route)
            {
                static constexpr float placements[] { -100.0f, 0.0f, 100.0f, -100.0f, 100.0f };
                proc.undoManager.beginNewTransaction("Set selected band placement");
                for (int b = 0; b < kNumBands; ++b)
                    if (selection[(size_t)b])
                    {
                        if (auto* mode = proc.apvts.getParameter(bandId(b + 1, "placement_mode")))
                        {
                            mode->beginChangeGesture();
                            mode->setValueNotifyingHost(mode->convertTo0to1(route >= 3 ? 1.0f : 0.0f));
                            mode->endChangeGesture();
                        }
                        if (auto* placement = proc.apvts.getParameter(bandId(b + 1, "placement")))
                        {
                            placement->beginChangeGesture();
                            placement->setValueNotifyingHost(placement->convertTo0to1(placements[route]));
                            placement->endChangeGesture();
                        }
                    }
            }, [](juce::Graphics& g, juce::Rectangle<float> r, int route, juce::Colour colour)
            {
                static constexpr const char* labels[] { "L", "C", "R", "M", "S" };
                g.setColour(colour);
                g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::bold)));
                g.drawText(labels[route], r, juce::Justification::centred);
            }), nullptr, "Placement");
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

        auto menuOptions = juce::PopupMenu::Options()
            .withTargetComponent(*this)
            .withMousePosition()
            .withItemThatMustBeVisible(101);
        menu.showMenuAsync(menuOptions, [this, idx](int result)
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
                selection[(size_t)(idx - 1)] = false;
                if (selectedBand == idx - 1) selectedBand = -1;
            }
            else if (result == 3)
            {
                proc.undoManager.beginNewTransaction("Reset EQ band");
                proc.resetBandToDefaults(idx - 1, true);
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
                            if (selectedBand == b) selectedBand = -1;
                            continue;
                        }
                        if (auto* parameter = proc.apvts.getParameter(bandId(b + 1, "on")))
                        {
                            parameter->beginChangeGesture();
                            parameter->setValueNotifyingHost(0.0f);
                            parameter->endChangeGesture();
                        }
                    }
                if (result == 31) selection.fill(false);
            }
            repaint();
        });
        return;
    }

    if (hit >= 0 && e.mods.isAltDown())
    {
        if (!selection[(size_t)hit]) { selection.fill(false); selection[(size_t)hit] = true; }
        selectedBand = hit;
        momentarySoloActive = true;
        proc.soloBand.store(hit, std::memory_order_release);
        repaint();
        return;
    }

    if (hit >= 0 && e.mods.isCommandDown())
    {
        if (!selection[(size_t)hit]) { selection.fill(false); selection[(size_t)hit] = true; }
        selectedBand = hit;
        commandGesturePending = true;
        modifierGestureBand = hit;
        repaint();
        return;
    }

    if (hit >= 0 && e.mods.isShiftDown())
    {
        shiftGesturePending = true;
        modifierGestureBand = hit;
        return;
    }

    if (hit >= 0 && !selection[(size_t)hit])
    {
        selection.fill(false);
        selection[(size_t)hit] = true;
    }
    if (hit < 0) return;
    selectedBand = hit;
    beginStaticBandDrag(hit, true);
}

int ResponseCurveComponent::createBandAt(float x, float y, std::int64_t eventTimeMs)
{
    int freeBand = -1;
    for (int b = 0; b < kNumBands; ++b)
        if (proc.apvts.getRawParameterValue(bandId(b + 1, "present"))->load() < 0.5f)
        {
            freeBand = b;
            break;
        }
    if (freeBand < 0)
        return -1;

    const float frequency = std::clamp(xToFreq(x), minFreq, maxFreq);
    const float gain = std::clamp(yToDb(y), minBandGainDb, maxBandGainDb);
    proc.undoManager.beginNewTransaction("Create EQ band");
    proc.resetBandToDefaults(freeBand, true, frequency, gain);
    const int newType = defaultTypeForNewBand(frequency, gain);
    if (auto* type = proc.apvts.getParameter(bandId(freeBand + 1, "type")))
    {
        type->beginChangeGesture();
        type->setValueNotifyingHost(type->convertTo0to1((float)newType));
        type->endChangeGesture();
    }
    if (typeDefaultsToMidSide(newType))
        if (auto* mode = proc.apvts.getParameter(bandId(freeBand + 1, "placement_mode")))
        {
            mode->beginChangeGesture();
            mode->setValueNotifyingHost(mode->convertTo0to1(1.0f));
            mode->endChangeGesture();
        }

    selectedBand = freeBand;
    selection.fill(false);
    selection[(size_t)freeBand] = true;
    mostRecentlyCreatedBand = freeBand;
    mostRecentCreationTimeMs = eventTimeMs;
    return freeBand;
}

void ResponseCurveComponent::beginStaticBandDrag(int hit, bool beginUndoTransaction)
{
    if (hit < 0 || hit >= kNumBands)
        return;

    dragging = true;
    rangeExpansionAvailable = true;
    if (beginUndoTransaction)
    {
        proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
            ? "Move selected EQ bands" : "Move EQ band");
    }
    groupAnchorFreq = proc.apvts.getRawParameterValue(bandId(hit + 1, "freq"))->load();
    groupAnchorGain = proc.apvts.getRawParameterValue(bandId(hit + 1, "gain"))->load();
    for (int b = 0; b < kNumBands; ++b)
    {
        dragFreqParams[(size_t)b] = dragGainParams[(size_t)b] = dragQParams[(size_t)b] = nullptr;
        if (!selection[(size_t)b]) continue;
        const int type = (int)proc.apvts.getRawParameterValue(bandId(b + 1, "type"))->load();
        dragStartFreq[(size_t)b] = proc.apvts.getRawParameterValue(bandId(b + 1, "freq"))->load();
        dragStartGain[(size_t)b] = proc.apvts.getRawParameterValue(bandId(b + 1, "gain"))->load();
        dragStartQ[(size_t)b] = proc.apvts.getRawParameterValue(bandId(b + 1, "q"))->load();
        dragFreqParams[(size_t)b] = proc.apvts.getParameter(bandId(b + 1, "freq"));
        if (usesQVerticalDrag(type))
        {
            dragQParams[(size_t)b] = proc.apvts.getParameter(bandId(b + 1, "q"));
            dragGainParams[(size_t)b] = proc.apvts.getParameter(bandId(b + 1, "gain"));
        }
        else
            dragGainParams[(size_t)b] = proc.apvts.getParameter(bandId(b + 1, "gain"));
        if (dragFreqParams[(size_t)b]) dragFreqParams[(size_t)b]->beginChangeGesture();
        if (dragGainParams[(size_t)b]) dragGainParams[(size_t)b]->beginChangeGesture();
        if (dragQParams[(size_t)b]) dragQParams[(size_t)b]->beginChangeGesture();
    }
}

void ResponseCurveComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (commandGesturePending && std::abs(e.getDistanceFromDragStartY()) + std::abs(e.getDistanceFromDragStartX()) > 3)
    {
        commandGesturePending = false;
        dragging = driveDragging = true;
        proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
            ? "Adjust selected band drive" : "Adjust band drive");
        for (int b = 0; b < kNumBands; ++b)
        {
            dragDriveParams[(size_t)b] = nullptr;
            if (!selection[(size_t)b]) continue;
            dragStartDrive[(size_t)b] = proc.apvts.getRawParameterValue(bandId(b + 1, "drive"))->load();
            dragDriveParams[(size_t)b] = proc.apvts.getParameter(bandId(b + 1, "drive"));
            if (dragDriveParams[(size_t)b]) dragDriveParams[(size_t)b]->beginChangeGesture();
        }
    }
    if (shiftGesturePending && std::abs(e.getDistanceFromDragStartY()) + std::abs(e.getDistanceFromDragStartX()) > 3)
    {
        shiftGesturePending = false;
        if (!selection[(size_t)modifierGestureBand])
        {
            selection.fill(false);
            selection[(size_t)modifierGestureBand] = true;
        }
        selectedBand = modifierGestureBand;
        dragging = thresholdDragging = true;
        proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
            ? "Adjust selected band thresholds" : "Adjust band threshold");
        for (int b = 0; b < kNumBands; ++b)
        {
            dragThresholdParams[(size_t)b] = nullptr;
            if (!selection[(size_t)b]) continue;
            dragStartThreshold[(size_t)b] = proc.apvts.getRawParameterValue(bandId(b + 1, "dyn_thresh"))->load();
            dragThresholdParams[(size_t)b] = proc.apvts.getParameter(bandId(b + 1, "dyn_thresh"));
            if (dragThresholdParams[(size_t)b]) dragThresholdParams[(size_t)b]->beginChangeGesture();
        }
    }
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

    if (thresholdDragging)
    {
        const float delta = -(float)e.getDistanceFromDragStartY() * 0.30f;
        for (int b = 0; b < kNumBands; ++b)
            if (auto* parameter = dragThresholdParams[(size_t)b])
            {
                const float value = juce::jlimit(-60.0f, 0.0f, dragStartThreshold[(size_t)b] + delta);
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            }
        repaint();
        return;
    }

    const bool hasGainDrag = std::any_of(dragGainParams.begin(), dragGainParams.end(),
                                        [](const auto* parameter) { return parameter != nullptr; });
    if (hasGainDrag && (e.y < 0 || e.y >= getHeight()) && rangeExpansionAvailable
        && displayMaxDb < maxDisplayDb)
    {
        displayMaxDb = displayMaxDb < 24.0f ? 24.0f : maxDisplayDb;
        rangeExpansionAvailable = false; // at most one range step per drag
        repaint();
    }

    const float anchorFreq = std::clamp(xToFreq((float)e.x), minFreq, maxFreq);
    const float ratio = anchorFreq / std::max(1.0f, groupAnchorFreq);
    // The current visible range is also the maximum reachable range for this
    // gesture. This prevents the first edge crossing (+/-12 -> +/-24) from
    // writing a >24 dB value and making the timer immediately jump to +/-36.
    float anchorGain = std::clamp(yToDb((float)e.y), -displayMaxDb, displayMaxDb);
    anchorGain = std::clamp(anchorGain, minBandGainDb, maxBandGainDb);
    const float gainDelta = anchorGain - groupAnchorGain;
    // Gainless filters retain gain only as the node's visual Y coordinate;
    // vertical movement edits Q in a logarithmic domain.
    // domain, while horizontal movement continues to edit frequency. Mixed
    // selections retain the normal gain gesture on non-cut bands.
    for (int b = 0; b < kNumBands; ++b)
    {
        if (!selection[(size_t)b]) continue;
        const float newFreq = std::clamp(dragStartFreq[(size_t)b] * ratio, minFreq, maxFreq);
        const float newGain = std::clamp(dragStartGain[(size_t)b] + gainDelta,
                                         minBandGainDb, maxBandGainDb);
        if (auto* p = dragFreqParams[(size_t)b]) p->setValueNotifyingHost(p->convertTo0to1(newFreq));
        if (auto* p = dragGainParams[(size_t)b]) p->setValueNotifyingHost(p->convertTo0to1(newGain));
        if (auto* p = dragQParams[(size_t)b])
        {
            const float newQ = cutQFromVerticalDrag(dragStartQ[(size_t)b],
                                                    (float)e.getDistanceFromDragStartY());
            p->setValueNotifyingHost(p->convertTo0to1(newQ));
        }
    }
}

void ResponseCurveComponent::mouseUp(const juce::MouseEvent&)
{
    if (momentarySoloActive)
    {
        proc.soloBand.store(-1, std::memory_order_release);
        momentarySoloActive = false;
    }
    if (commandGesturePending && modifierGestureBand >= 0)
    {
        proc.undoManager.beginNewTransaction("Toggle EQ band");
        if (auto* parameter = proc.apvts.getParameter(bandId(modifierGestureBand + 1, "on")))
        {
            const bool enabled = proc.apvts.getRawParameterValue(bandId(modifierGestureBand + 1, "on"))->load() > 0.5f;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(enabled ? 0.0f : 1.0f);
            parameter->endChangeGesture();
        }
    }
    if (shiftGesturePending && modifierGestureBand >= 0)
    {
        const int hit = modifierGestureBand;
        proc.undoManager.beginNewTransaction("Reset band placement");
        if (auto* parameter = proc.apvts.getParameter(bandId(hit + 1, "placement")))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(0.0f));
            parameter->endChangeGesture();
        }
        selection.fill(false);
        selection[(size_t)hit] = true;
        selectedBand = hit;
    }
    for (int b = 0; b < kNumBands; ++b)
    {
        if (dragFreqParams[(size_t)b]) dragFreqParams[(size_t)b]->endChangeGesture();
        if (dragGainParams[(size_t)b]) dragGainParams[(size_t)b]->endChangeGesture();
        if (dragQParams[(size_t)b]) dragQParams[(size_t)b]->endChangeGesture();
        dragFreqParams[(size_t)b] = dragGainParams[(size_t)b] = dragQParams[(size_t)b] = nullptr;
        if (dragDriveParams[(size_t)b]) dragDriveParams[(size_t)b]->endChangeGesture();
        dragDriveParams[(size_t)b] = nullptr;
        if (dragThresholdParams[(size_t)b]) dragThresholdParams[(size_t)b]->endChangeGesture();
        dragThresholdParams[(size_t)b] = nullptr;
    }
    dragging = driveDragging = thresholdDragging = false;
    rangeExpansionAvailable = true;
    commandGesturePending = shiftGesturePending = false;
    modifierGestureBand = -1;
    repaint();
}

void ResponseCurveComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int existing = hitTestNode((float)e.x, (float)e.y);
    if (existing >= 0)
    {
        const auto elapsedSinceCreation = e.eventTime.toMilliseconds() - mostRecentCreationTimeMs;
        if (existing == mostRecentlyCreatedBand
            && elapsedSinceCreation >= 0 && elapsedSinceCreation < 700)
        {
            // This is the second half of the click sequence that created the
            // band, not an intentional delete gesture on an older node.
            mostRecentlyCreatedBand = -1;
            return;
        }
        proc.undoManager.beginNewTransaction("Delete EQ band");
        proc.resetBandToDefaults(existing, false);
        selection[(size_t) existing] = false;
        selectedBand = -1;
        mostRecentlyCreatedBand = -1;
        repaint();
    }
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
    if (e.mods.isCommandDown())
    {
        proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
            ? "Adjust selected band slopes" : "Adjust band slope");
        for (int b = 0; b < kNumBands; ++b)
            if (selection[(size_t)b])
                if (auto* parameter = proc.apvts.getParameter(bandId(b + 1, "slope")))
                {
                    const float slope = proc.apvts.getRawParameterValue(bandId(b + 1, "slope"))->load();
                    parameter->beginChangeGesture();
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(
                        juce::jlimit(3.0f, 48.0f, slope + wheel.deltaY * 15.0f)));
                    parameter->endChangeGesture();
                }
        repaint();
        return;
    }
    if (e.mods.isShiftDown())
    {
        const float rawStep = wheel.deltaY * 120.0f;
        const float minimumStep = wheel.isSmooth ? 2.5f : 12.0f;
        const float placementStep = std::copysign(
            std::max(std::abs(rawStep), minimumStep), rawStep);
        proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
            ? "Adjust selected band placement" : "Adjust band placement");
        for (int b = 0; b < kNumBands; ++b)
            if (selection[(size_t)b])
                if (auto* parameter = proc.apvts.getParameter(bandId(b + 1, "placement")))
                {
                    const float placement = proc.apvts.getRawParameterValue(bandId(b + 1, "placement"))->load();
                    parameter->beginChangeGesture();
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(
                        juce::jlimit(-100.0f, 100.0f, placement + placementStep)));
                    parameter->endChangeGesture();
                }
        repaint();
        return;
    }
    const float factor = std::pow(2.0f, wheel.deltaY * 0.9f);
    proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
        ? "Adjust selected band widths" : "Adjust band width");
    for (int b = 0; b < kNumBands; ++b)
        if (selection[(size_t)b])
        {
            const int type = (int)proc.apvts.getRawParameterValue(bandId(b + 1, "type"))->load();
            const auto suffix = isCutType(type) ? "slope" : "q";
            if (auto* parameter = proc.apvts.getParameter(bandId(b + 1, suffix)))
            {
                const float current = proc.apvts.getRawParameterValue(bandId(b + 1, suffix))->load();
                const float value = isCutType(type)
                    ? juce::jlimit(3.0f, 48.0f, current + wheel.deltaY * 15.0f)
                    : std::clamp(current * factor, 0.1f, 24.0f);
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
                parameter->endChangeGesture();
            }
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
                        bandId(bi.index + 1, "gain"))->load();
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
