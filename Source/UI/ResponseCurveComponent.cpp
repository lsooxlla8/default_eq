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

static constexpr std::array<const char*, kSaturationModeCount> saturationModeNames {
    "Soft Clip", "Diode", "Triode", "Transistor",
    "Tape", "Odd / Even", "Phase Distortion", "Sine Erosion"
};

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
    switch (juce::jlimit(0, 9, type))
    {
        case 0: // resonant low-pass
            p.startNewSubPath(x0, midY); p.lineTo(midX - 5.0f, midY);
            p.cubicTo(midX - 2.0f, midY, midX - 1.0f, y0 + 1.0f, midX + 2.0f, y0 + 2.0f);
            p.cubicTo(midX + 6.0f, y0 + 3.0f, x1 - 4.0f, y1 - 3.0f, x1, y1); break;
        case 1: // resonant high-pass
            p.startNewSubPath(x0, y1); p.cubicTo(x0 + 4.0f, y1 - 3.0f, midX - 6.0f, y0 + 3.0f, midX - 2.0f, y0 + 2.0f);
            p.cubicTo(midX + 1.0f, y0 + 1.0f, midX + 2.0f, midY, midX + 5.0f, midY);
            p.lineTo(x1, midY); break;
        case 2: // true notch
            p.startNewSubPath(x0, midY); p.lineTo(midX - 3.0f, midY);
            p.lineTo(midX, y1); p.lineTo(midX + 3.0f, midY); p.lineTo(x1, midY); break;
        case 3: // tilt
            p.startNewSubPath(x0, y1 - 3.0f); p.lineTo(x1, y0 + 3.0f); break;
        case 4: // band-pass
            p.startNewSubPath(x0, y1 - 2.0f); p.cubicTo(midX - 7.0f, y1 - 2.0f, midX - 6.0f, y0 + 2.0f, midX, y0 + 2.0f);
            p.cubicTo(midX + 6.0f, y0 + 2.0f, midX + 7.0f, y1 - 2.0f, x1, y1 - 2.0f); break;
        case 5: // classic parametric bell symbol
        {
            const auto oval = juce::Rectangle<float>(midX - 6.0f, midY - 4.0f, 12.0f, 8.0f);
            p.addEllipse(oval);
            p.startNewSubPath(x0, midY); p.lineTo(oval.getX(), midY);
            p.startNewSubPath(oval.getRight(), midY); p.lineTo(x1, midY);
            break;
        }
        case 6: // low shelf
            p.startNewSubPath(x0, y0 + 3.0f); p.lineTo(midX - 5.0f, y0 + 3.0f);
            p.cubicTo(midX, y0 + 3.0f, midX, midY, midX + 5.0f, midY); p.lineTo(x1, midY); break;
        case 7: // high shelf
            p.startNewSubPath(x0, midY); p.lineTo(midX - 5.0f, midY);
            p.cubicTo(midX, midY, midX, y0 + 3.0f, midX + 5.0f, y0 + 3.0f); p.lineTo(x1, y0 + 3.0f); break;
        case 8: // low-pass
            p.startNewSubPath(x0, midY); p.lineTo(midX - 2.0f, midY);
            p.cubicTo(midX + 5.0f, midY, x1 - 6.0f, y1 - 9.0f, x1, y1); break;
        case 9: // high-pass
            p.startNewSubPath(x0, y1); p.cubicTo(x0 + 6.0f, y1 - 9.0f, midX - 5.0f, midY, midX + 2.0f, midY);
            p.lineTo(x1, midY); break;
    }
    // Cut, Bell and Notch symbols are authored around midY. Keep that common
    // baseline exact instead of visually recentering their asymmetric curves.
    if (type == 3 || type == 4 || type == 6 || type == 7)
    {
        const auto pathBounds = p.getBounds();
        p.applyTransform(juce::AffineTransform::translation(
            r.getCentreX() - pathBounds.getCentreX(), r.getCentreY() - pathBounds.getCentreY()));
    }
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
    hash(amount); hash(proc.apvts.getRawParameterValue("shift")->load());
    hash(proc.apvts.getRawParameterValue("adaptive_q")->load());
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
    paintBandCurves(g);
    paintResponseCurve(g);
    paintNodes(g);
    paintMarquee(g);
    paintHoverCard(g);
#if DEFAULT_EQ_FULL
    paintCollisionWarnings(g);
#endif

    // Border
    g.setColour(fg);
    g.drawRect(getLocalBounds(), 3);
}

void ResponseCurveComponent::paintMarquee(juce::Graphics& g)
{
    if (!marqueeDragging) return;
    const auto area = juce::Rectangle<float>::leftTopRightBottom(
        std::min(marqueeStart.x, marqueeCurrent.x),
        std::min(marqueeStart.y, marqueeCurrent.y),
        std::max(marqueeStart.x, marqueeCurrent.x),
        std::max(marqueeStart.y, marqueeCurrent.y));
    const auto colour = darkMode ? juce::Colour(0xfff6f6f6) : juce::Colour(0xff050505);
    g.setColour(colour.withAlpha(0.10f));
    g.fillRect(area);
    g.setColour(colour.withAlpha(0.85f));
    g.drawRect(area, 1.0f);
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
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::plain)));

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

        if (isPrimary && proc.apvts.getRawParameterValue(bandId(idx, "dyn_thresh"))->load() < -0.05f)
        {
            const int typeIndex = (int)proc.apvts.getRawParameterValue(bandId(idx, "type"))->load();
            const auto type = deq::filter_types::fromParameterIndex(typeIndex);
            const float frequency = displayedBandFrequency(
                proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load());
            float q = proc.apvts.getRawParameterValue(bandId(idx, "q"))->load();
            const float baseGain = proc.apvts.getRawParameterValue(bandId(idx, "gain"))->load();
            const float slope = proc.apvts.getRawParameterValue(bandId(idx, "slope"))->load();
            const float range = proc.apvts.getRawParameterValue(bandId(idx, "dyn_range"))->load();
            const bool upward = proc.apvts.getRawParameterValue(bandId(idx, "dyn_mode"))->load() > 0.5f;
            const float dynamicMod = upward ? range : -range;
            const float amount = proc.apvts.getRawParameterValue("scale")->load();
            const double curveSampleRate = responseGridSampleRate > 0.0
                ? responseGridSampleRate : (proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0);
            if (proc.apvts.getRawParameterValue("adaptive_q")->load() > 0.5f)
                q = DefaultEqualizerAudioProcessor::calculateAdaptiveQ(q, baseGain);
            const bool gainBearing = variable_slope::distributesGain(type);
            const bool classicCut = zl_filter::isClassicCut(type);
            const bool resonantCut = zl_filter::isResonantCut(type);
            const bool bandPass = type == Biquad::Type::Bandpass;
            if (resonantCut) q = EQBand::amountResonantCutQ(q, amount);

            const auto responseAt = [&](float modulation, float probe)
            {
                float responseQ = resonantCut ? EQBand::dynamicResonantCutQ(q, modulation)
                    : bandPass ? EQBand::dynamicBandPassQ(q, modulation) : q;
                float responseGain = gainBearing ? (baseGain + modulation) * amount : baseGain;
                const float responseSlope = classicCut
                    ? EQBand::dynamicClassicCutSlope(slope, modulation) : slope;
                float responseFrequency = frequency;
                if (classicCut || resonantCut)
                {
                    const float neutral = (type == Biquad::Type::LowPass || type == Biquad::Type::ResLowPass)
                        ? (float)curveSampleRate * 0.45f : 10.0f;
                    responseFrequency = std::clamp(neutral * std::pow(
                        std::max(1.0e-6f, frequency / neutral), std::max(0.0f, amount)),
                        10.0f, (float)curveSampleRate * 0.45f);
                }
                const auto raw = variable_slope::response(type, curveSampleRate,
                    responseFrequency, responseQ, responseGain, responseSlope, true, probe);
                const double mix = (classicCut || resonantCut)
                    ? (double)EQBand::cutAmountMix(amount)
                    : (double)std::clamp(amount, 0.0f, 1.0f);
                const auto response = gainBearing ? raw : std::complex<double>(1.0, 0.0)
                    + mix * (raw - std::complex<double>(1.0, 0.0));
                return (float)(20.0 * std::log10(std::max(std::abs(response), 1.0e-15)));
            };

            juce::Path basePath, targetPath, dynamicFill;
            for (int point = 0; point < numPoints; ++point)
            {
                const float x = (float)point / (float)(numPoints - 1) * w;
                const float baseY = dbToY(responseAt(0.0f, responseFrequencies[point]));
                const float targetY = dbToY(responseAt(dynamicMod, responseFrequencies[point]));
                if (point == 0)
                {
                    basePath.startNewSubPath(x, baseY);
                    targetPath.startNewSubPath(x, targetY);
                    dynamicFill.startNewSubPath(x, baseY);
                }
                else
                {
                    basePath.lineTo(x, baseY);
                    targetPath.lineTo(x, targetY);
                    dynamicFill.lineTo(x, baseY);
                }
            }
            for (int point = numPoints - 1; point >= 0; --point)
            {
                const float x = (float)point / (float)(numPoints - 1) * w;
                dynamicFill.lineTo(x, dbToY(responseAt(dynamicMod, responseFrequencies[point])));
            }
            dynamicFill.closeSubPath();
            g.setColour(fg.withAlpha(0.13f)); g.fillPath(dynamicFill);
            g.setColour(fg.withAlpha(0.52f));
            const float dash[] { 4.0f, 3.0f };
            juce::Path dashed;
            juce::PathStrokeType(1.0f).createDashedStroke(dashed, targetPath, dash, 2);
            g.strokePath(dashed, juce::PathStrokeType(1.0f));
        }
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

        const float freq = displayedBandFrequency(
            proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load());
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
        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::bold)));
        g.drawText(juce::String(idx), (int)(x - r), (int)(y - r), (int)(r * 2.0f), (int)(r * 2.0f),
                   juce::Justification::centred);

        const int routeMode=std::clamp((int)proc.apvts.getRawParameterValue(bandId(idx,"placement_mode"))->load(),0,2);
        const float placement = proc.apvts.getRawParameterValue(bandId(idx, "placement"))->load();
        const auto label = std::abs(placement)<1.0f ? (routeMode==0?"LR":routeMode==1?"MS":"TS")
                         : placement<0.0f ? (routeMode==0?"L":routeMode==1?"M":"T")
                                          : (routeMode==0?"R":"S");
        g.setColour(colour.withAlpha(on ? 0.9f : 0.42f));
        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::bold)));
        g.drawText(label, (int)(x - r), (int)(y + r + 1), (int)(r * 2.0f), 10,
                   juce::Justification::centred);
    }

    const auto handle = dynamicRangeHandleBounds();
    if (!handle.isEmpty())
    {
        const auto colour = darkMode ? juce::Colour(0xfff6f6f6) : juce::Colour(0xff050505);
        const auto inverse = darkMode ? juce::Colour(0xff050505) : juce::Colour(0xfff6f6f6);
        g.setColour(colour); g.fillRect(handle);
        g.setColour(inverse); g.drawRect(handle, 1.5f);
    }
}

juce::Rectangle<float> ResponseCurveComponent::dynamicRangeHandleBounds() const
{
    if (selectedBand < 0 || selectedBand >= kNumBands) return {};
    const int idx = selectedBand + 1;
    if (proc.apvts.getRawParameterValue(bandId(idx, "present"))->load() < 0.5f
        || proc.apvts.getRawParameterValue(bandId(idx, "on"))->load() < 0.5f
        || proc.apvts.getRawParameterValue(bandId(idx, "dyn_thresh"))->load() >= -0.05f)
        return {};
    const float frequency = displayedBandFrequency(
        proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load());
    const float gain = proc.apvts.getRawParameterValue(bandId(idx, "gain"))->load();
    const float range = proc.apvts.getRawParameterValue(bandId(idx, "dyn_range"))->load();
    const bool upward = proc.apvts.getRawParameterValue(bandId(idx, "dyn_mode"))->load() > 0.5f;
    const float y = dbToY(gain + (upward ? range : -range));
    return juce::Rectangle<float>(freqToX(frequency) - 5.0f, y - 5.0f, 10.0f, 10.0f);
}

void ResponseCurveComponent::paintHoverCard(juce::Graphics& g)
{
    const int band = hoveredBand >= 0 ? hoveredBand : (dragging ? selectedBand : -1);
    if (band < 0)
    {
        hoverCardBand = hoverCardPlacement = -1;
        return;
    }
    if (hoverCardBand != band)
    {
        hoverCardBand = band;
        hoverCardPlacement = -1;
    }

    const int idx = band + 1;
    const float freq = displayedBandFrequency(
        proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load());
    const float gain = proc.apvts.getRawParameterValue(bandId(idx, "gain"))->load();
    const float drive = proc.apvts.getRawParameterValue(bandId(idx, "drive"))->load();
    const float character = proc.apvts.getRawParameterValue(bandId(idx, "drive_character"))->load();
    const int saturationMode = std::clamp((int)proc.apvts.getRawParameterValue(
        bandId(idx, "sat_mode"))->load(), 0, kSaturationModeCount - 1);
    const float x = freqToX(freq);
    const float y = dbToY(gain);

    const float threshold = proc.apvts.getRawParameterValue(bandId(idx, "dyn_thresh"))->load();
    const int routeMode=std::clamp((int)proc.apvts.getRawParameterValue(bandId(idx,"placement_mode"))->load(),0,2);
    const float placement = proc.apvts.getRawParameterValue(bandId(idx, "placement"))->load();
    const auto driveLine = "DRIVE " + juce::String(drive, 1) + "dB   CHAR "
        + deq::ui::formatDriveCharacter(saturationMode, character);
    const auto placementText = std::abs(placement) < 0.05f
        ? juce::String(routeMode==0?"L/R CENTER":routeMode==1?"M/S CENTER":"T/S CENTER")
        : juce::String(routeMode==0?"L/R ":routeMode==1?"M/S ":"T/S ")
            + (placement < 0.0f ? (routeMode==0?"L ":routeMode==1?"M ":"T ")
                                : (routeMode==0?"R ":"S "))
            + juce::String(std::abs(placement), 0) + "%";
    const auto thresholdLine = "THR " + juce::String(threshold, 1) + "dB   " + placementText;
    const juce::Font bodyFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 15.0f,
                                                 juce::Font::plain));
    const int textWidth = juce::jmax(juce::GlyphArrangement::getStringWidthInt(bodyFont, driveLine),
                                     juce::GlyphArrangement::getStringWidthInt(bodyFont, thresholdLine));
    const int cardW = juce::jlimit(136, getWidth() - 12, textWidth + 16);
    const int cardH = 37;
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
    if (hoverCardPlacement < 0)
    {
        hoverCardPlacement = 0;
        float bestScore = obstructionScore(candidates.front());
        for (size_t candidate = 1; candidate < candidates.size(); ++candidate)
        {
            const float score = obstructionScore(candidates[candidate]);
            if (score < bestScore)
            {
                bestScore = score;
                hoverCardPlacement = (int)candidate;
            }
        }
    }
    const auto card = candidates[(size_t)juce::jlimit(0, 3, hoverCardPlacement)];
    const int cardX = card.getX();
    const int cardY = card.getY();

    const auto fg = darkMode ? juce::Colour(0xfff6f6f6) : juce::Colour(0xff050505);
    const auto bg = darkMode ? juce::Colour(0xff050505) : juce::Colour(0xfff6f6f6);
    g.setColour(bg.withAlpha(0.60f));
    g.fillRect(cardX, cardY, cardW, cardH);
    g.setColour(fg.withAlpha(0.82f));
    g.drawRect(cardX, cardY, cardW, cardH, 2);
    g.setFont(bodyFont);
    g.setColour(fg.withAlpha(0.82f));
    g.drawText(driveLine,
               cardX + 8, cardY + 5, cardW - 16, 13, juce::Justification::centredLeft);
    g.drawText(thresholdLine,
               cardX + 8, cardY + 20, cardW - 16, 13, juce::Justification::centredLeft);
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

        const float freq = displayedBandFrequency(
            proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load());
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

    if (e.mods.isLeftButtonDown() && !e.mods.isCommandDown() && !e.mods.isShiftDown()
        && !e.mods.isAltDown() && dynamicRangeHandleBounds().contains((float)e.x, (float)e.y))
    {
        const int idx = selectedBand + 1;
        dynamicRangeDragParam = proc.apvts.getParameter(bandId(idx, "dyn_range"));
        dynamicRangeDragBaseGain = proc.apvts.getRawParameterValue(bandId(idx, "gain"))->load();
        dynamicRangeDragging = dynamicRangeDragParam != nullptr;
        dragging = dynamicRangeDragging;
        if (dynamicRangeDragParam)
        {
            proc.undoManager.beginNewTransaction("Adjust dynamic range");
            dynamicRangeDragParam->beginChangeGesture();
        }
        return;
    }

    if (hit >= 0 && e.mods.isLeftButtonDown() && e.mods.isShiftDown()
        && e.mods.isCommandDown())
    {
        proc.undoManager.beginNewTransaction("Reset band placement");
        if (auto* parameter = proc.apvts.getParameter(bandId(hit + 1, "placement")))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(0.0f));
            parameter->endChangeGesture();
        }
        repaint();
        return;
    }

    const bool shiftMarquee = hit < 0 && e.mods.isLeftButtonDown()
        && e.mods.isShiftDown() && !e.mods.isCommandDown() && !e.mods.isAltDown();
    const bool popupMarquee = hit < 0 && e.mods.isPopupMenu();
    if (shiftMarquee || popupMarquee)
    {
        marqueePending = true;
        marqueeDragging = false;
        marqueeCreatesShiftFilterOnClick = shiftMarquee;
        marqueeStart = marqueeCurrent = e.position;
        return;
    }

    if (e.mods.isPopupMenu() && e.mods.isShiftDown() && hit >= 0)
    {
        selection.fill(false);
        selection[(size_t)hit] = true;
        selectedBand = hit;
        proc.undoManager.beginNewTransaction("Center band placement");
        if (auto* placement = proc.apvts.getParameter(bandId(hit + 1, "placement")))
        {
            placement->beginChangeGesture();
            placement->setValueNotifyingHost(placement->convertTo0to1(0.0f));
            placement->endChangeGesture();
        }
        repaint();
        return;
    }

    if (e.mods.isPopupMenu() && e.mods.isAltDown() && hit >= 0)
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

    if (e.mods.isPopupMenu() && e.mods.isCommandDown() && hit >= 0)
    {
        if (!selection[(size_t)hit])
        {
            selection.fill(false);
            selection[(size_t)hit] = true;
        }
        selectedBand = hit;
        proc.undoManager.beginNewTransaction("Reset selected band drive and character");
        for (int b = 0; b < kNumBands; ++b)
            if (selection[(size_t)b])
            {
                const int mode = std::clamp((int)proc.apvts.getRawParameterValue(
                    bandId(b + 1, "sat_mode"))->load(), 0, kSaturationModeCount - 1);
                const float characterDefault = mode == static_cast<int>(SaturationType::Tape)
                    || mode == static_cast<int>(SaturationType::PhaseDistortion)
                    || mode == static_cast<int>(SaturationType::SineErosion) ? 0.5f : 0.0f;
                if (auto* drive = proc.apvts.getParameter(bandId(b + 1, "drive")))
                {
                    drive->beginChangeGesture();
                    drive->setValueNotifyingHost(drive->getDefaultValue());
                    drive->endChangeGesture();
                }
                if (auto* character = proc.apvts.getParameter(bandId(b + 1, "drive_character")))
                {
                    character->beginChangeGesture();
                    character->setValueNotifyingHost(character->convertTo0to1(characterDefault));
                    character->endChangeGesture();
                }
            }
        repaint();
        return;
    }

    if (hit < 0 && e.mods.isLeftButtonDown() && !e.mods.isCommandDown()
        && !e.mods.isAltDown() && !e.mods.isShiftDown())
    {
        hit = createBandAt((float)e.x, (float)e.y, e.eventTime.toMilliseconds(), -1);
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
        const int selectedType = juce::jlimit(0, 9,
            (int)proc.apvts.getRawParameterValue(bandId(idx, "type"))->load());
        menu.addCustomItem(100, std::make_unique<ChoiceRow>(10, selectedType, darkMode,
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
                        if (deq::filter_types::isResonantCutIndex(type))
                            if (auto* q = proc.apvts.getParameter(bandId(b + 1, "q")))
                            {
                                q->beginChangeGesture();
                                q->setValueNotifyingHost(q->convertTo0to1(
                                    deq::filter_types::resonantCutDefaultQ));
                                q->endChangeGesture();
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
        const int selectedMode = std::clamp((int)proc.apvts.getRawParameterValue(bandId(idx, "placement_mode"))->load(),0,2);
        const float selectedPlacement = proc.apvts.getRawParameterValue(bandId(idx, "placement"))->load();
        const int selectedRoute = std::abs(selectedPlacement) <= 1.0f ? 1
            : selectedMode == 2 ? (selectedPlacement < 0.0f ? 5 : 6)
            : selectedMode == 1 ? (selectedPlacement < 0.0f ? 3 : 4)
                                : (selectedPlacement < 0.0f ? 0 : 2);
        menu.addCustomItem(101, std::make_unique<ChoiceRow>(7, selectedRoute, darkMode,
            [this](int route)
            {
                static constexpr float placements[] { -100.0f, 0.0f, 100.0f, -100.0f, 100.0f, -100.0f, 100.0f };
                static constexpr float modes[] { 0,0,0,1,1,2,2 };
                proc.undoManager.beginNewTransaction("Set selected band placement");
                for (int b = 0; b < kNumBands; ++b)
                    if (selection[(size_t)b])
                    {
                        if (auto* mode = proc.apvts.getParameter(bandId(b + 1, "placement_mode")))
                        {
                            mode->beginChangeGesture();
                            mode->setValueNotifyingHost(mode->convertTo0to1(modes[route]));
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
                static constexpr const char* labels[] { "L", "C", "R", "M", "S", "T", "S" };
                g.setColour(colour);
                g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::bold)));
                g.drawText(labels[route], r, juce::Justification::centred);
            }), nullptr, "Placement");
        menu.addSeparator();
        const int selectedSaturation = std::clamp((int)proc.apvts.getRawParameterValue(
            bandId(idx, "sat_mode"))->load(), 0, kSaturationModeCount - 1);
        juce::PopupMenu saturationMenu;
        for (int mode = 0; mode < kSaturationModeCount; ++mode)
            saturationMenu.addItem(200 + mode, saturationModeNames[(size_t)mode], true,
                                   mode == selectedSaturation);
        menu.addSubMenu("Saturation", saturationMenu);
        menu.addSeparator();
        menu.addItem(3, "Reset equalizer");
        const int selectedCount = (int) std::count(selection.begin(), selection.end(), true);
        if (selectedCount > 1)
        {
            menu.addSeparator();
            menu.addItem(30, "Bypass selected (" + juce::String(selectedCount) + ")");
        }

        const auto mousePosition = juce::Desktop::getMousePosition();
        const auto* mouseDisplay = juce::Desktop::getInstance().getDisplays()
            .getDisplayForPoint(mousePosition.toFloat());
        const bool mouseIsOnLeft = mouseDisplay == nullptr
            ? mousePosition.x < getScreenBounds().getCentreX()
            : mousePosition.x < mouseDisplay->userBounds.getCentreX();
        auto menuOptions = juce::PopupMenu::Options()
            .withTargetComponent(*this)
            .withMousePosition()
            .withPreferredSubmenuDirection(mouseIsOnLeft
                ? juce::PopupMenu::Options::SubmenuDirection::towardsLeft
                : juce::PopupMenu::Options::SubmenuDirection::towardsRight);
        menu.showMenuAsync(menuOptions, [this, idx](int result)
        {
            if (result == 1)
            {
                auto* p = proc.apvts.getRawParameterValue(bandId(idx, "on"));
                auto* param = proc.apvts.getParameter(bandId(idx, "on"));
                if (param) param->setValueNotifyingHost(p->load() > 0.5f ? 0.0f : 1.0f);
            }
            else if (result == 3)
            {
                proc.undoManager.beginNewTransaction("Reset equalizer");
                for (int band = 0; band < kNumBands; ++band)
                    proc.resetBandToDefaults(band, false);
                selection.fill(false);
                selectedBand = -1;
            }
            else if (result == 30)
            {
                proc.undoManager.beginNewTransaction("Bypass selected bands");
                for (int b = 0; b < kNumBands; ++b)
                    if (selection[(size_t)b])
                    {
                        if (auto* parameter = proc.apvts.getParameter(bandId(b + 1, "on")))
                        {
                            parameter->beginChangeGesture();
                            parameter->setValueNotifyingHost(0.0f);
                            parameter->endChangeGesture();
                        }
                    }
            }
            else if (result >= 200 && result < 200 + kSaturationModeCount)
            {
                const int mode = result - 200;
                proc.undoManager.beginNewTransaction("Set selected band saturation");
                const float characterDefault = mode == static_cast<int>(SaturationType::Tape)
                    || mode == static_cast<int>(SaturationType::PhaseDistortion)
                    || mode == static_cast<int>(SaturationType::SineErosion) ? 0.5f : 0.0f;
                const float secondaryDefault = mode == static_cast<int>(SaturationType::Tape) ? 0.5f : 0.0f;
                for (int b = 0; b < kNumBands; ++b)
                    if (selection[(size_t)b])
                        for (const auto& change : {
                            std::pair<const char*, float>{ "sat_mode", (float)mode },
                            { "drive_character", characterDefault },
                            { "drive_secondary", secondaryDefault } })
                            if (auto* parameter = proc.apvts.getParameter(
                                    bandId(b + 1, change.first)))
                            {
                                parameter->beginChangeGesture();
                                parameter->setValueNotifyingHost(
                                    parameter->convertTo0to1(change.second));
                                parameter->endChangeGesture();
                            }
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

int ResponseCurveComponent::createBandAt(float x, float y, std::int64_t eventTimeMs, int forcedType)
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

    const float displayedFrequency = std::clamp(xToFreq(x), minFreq, maxFreq);
    const float frequency = std::clamp(baseBandFrequency(displayedFrequency), minFreq, maxFreq);
    const float gain = std::clamp(yToDb(y), minBandGainDb, maxBandGainDb);
    proc.undoManager.beginNewTransaction("Create EQ band");
    proc.resetBandToDefaults(freeBand, true, frequency, gain);
    const int newType = forcedType >= 0 ? forcedType : defaultTypeForNewBand(displayedFrequency, gain);
    if (auto* type = proc.apvts.getParameter(bandId(freeBand + 1, "type")))
    {
        type->beginChangeGesture();
        type->setValueNotifyingHost(type->convertTo0to1((float)newType));
        type->endChangeGesture();
    }
    if (deq::filter_types::isResonantCutIndex(newType))
        if (auto* q = proc.apvts.getParameter(bandId(freeBand + 1, "q")))
        {
            q->beginChangeGesture();
            q->setValueNotifyingHost(q->convertTo0to1(deq::filter_types::resonantCutDefaultQ));
            q->endChangeGesture();
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
    groupAnchorFreq = displayedBandFrequency(
        proc.apvts.getRawParameterValue(bandId(hit + 1, "freq"))->load());
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
            dragQParams[(size_t)b] = proc.apvts.getParameter(bandId(b + 1, "q"));
        else if (usesGainVerticalDrag(type))
            dragGainParams[(size_t)b] = proc.apvts.getParameter(bandId(b + 1, "gain"));
        if (dragFreqParams[(size_t)b]) dragFreqParams[(size_t)b]->beginChangeGesture();
        if (dragGainParams[(size_t)b]) dragGainParams[(size_t)b]->beginChangeGesture();
        if (dragQParams[(size_t)b]) dragQParams[(size_t)b]->beginChangeGesture();
    }
}

void ResponseCurveComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (marqueePending)
    {
        marqueeCurrent = e.position;
        if (!marqueeDragging
            && std::abs(e.getDistanceFromDragStartX()) + std::abs(e.getDistanceFromDragStartY()) > 3)
            marqueeDragging = true;
        if (marqueeDragging) updateMarqueeSelection();
        repaint();
        return;
    }
    if (dynamicRangeDragging && dynamicRangeDragParam != nullptr)
    {
        const float range = juce::jlimit(0.0f, 24.0f,
            std::abs(yToDb((float)e.y) - dynamicRangeDragBaseGain));
        dynamicRangeDragParam->setValueNotifyingHost(dynamicRangeDragParam->convertTo0to1(range));
        repaint();
        return;
    }
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
    // Cut, Notch and Band Pass vertical movement edit Q in a logarithmic domain while
    // horizontal movement edits frequency. It must not also write Gain: doing
    // both made Band Pass change level during a Q gesture. Other filter types
    // retain the normal vertical Gain gesture.
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

void ResponseCurveComponent::mouseUp(const juce::MouseEvent& e)
{
    if (marqueePending)
    {
        if (marqueeDragging)
        {
            marqueeCurrent = e.position;
            updateMarqueeSelection();
        }
        else if (marqueeCreatesShiftFilterOnClick)
        {
            const float frequency = std::clamp(xToFreq(marqueeStart.x), minFreq, maxFreq);
            const float gain = std::clamp(yToDb(marqueeStart.y), minBandGainDb, maxBandGainDb);
            createBandAt(marqueeStart.x, marqueeStart.y, e.eventTime.toMilliseconds(),
                         shiftClickTypeForNewBand(frequency, gain));
        }
        else
        {
            selection.fill(false);
            selectedBand = -1;
        }
        marqueePending = marqueeDragging = marqueeCreatesShiftFilterOnClick = false;
        repaint();
        return;
    }
    if (momentarySoloActive)
    {
        proc.soloBand.store(-1, std::memory_order_release);
        momentarySoloActive = false;
    }
    if (dynamicRangeDragParam)
    {
        dynamicRangeDragParam->endChangeGesture();
        dynamicRangeDragParam = nullptr;
    }
    if (commandGesturePending && modifierGestureBand >= 0)
    {
        proc.undoManager.beginNewTransaction("Toggle EQ band");
        if (auto* parameter = proc.apvts.getParameter(bandId(modifierGestureBand + 1, "on")))
        {
            const bool enabled = proc.apvts.getRawParameterValue(
                bandId(modifierGestureBand + 1, "on"))->load() > 0.5f;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(enabled ? 0.0f : 1.0f);
            parameter->endChangeGesture();
        }
    }
    if (shiftGesturePending && modifierGestureBand >= 0)
    {
        const int hit = modifierGestureBand;
        selection[(size_t)hit] = !selection[(size_t)hit];
        if (selection[(size_t)hit])
            selectedBand = hit;
        else if (selectedBand == hit)
        {
            selectedBand = -1;
            for (int band = 0; band < kNumBands; ++band)
                if (selection[(size_t)band]) { selectedBand = band; break; }
        }
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
    dragging = driveDragging = thresholdDragging = dynamicRangeDragging = false;
    rangeExpansionAvailable = true;
    commandGesturePending = shiftGesturePending = false;
    modifierGestureBand = -1;
    repaint();
}

void ResponseCurveComponent::updateMarqueeSelection()
{
    const auto area = juce::Rectangle<float>::leftTopRightBottom(
        std::min(marqueeStart.x, marqueeCurrent.x),
        std::min(marqueeStart.y, marqueeCurrent.y),
        std::max(marqueeStart.x, marqueeCurrent.x),
        std::max(marqueeStart.y, marqueeCurrent.y));
    selection.fill(false);
    for (int band = 0; band < kNumBands; ++band)
    {
        const int index = band + 1;
        if (proc.apvts.getRawParameterValue(bandId(index, "present"))->load() < 0.5f)
            continue;
        const auto point = juce::Point<float>(
            freqToX(displayedBandFrequency(
                proc.apvts.getRawParameterValue(bandId(index, "freq"))->load())),
            dbToY(proc.apvts.getRawParameterValue(bandId(index, "gain"))->load()));
        selection[(size_t)band] = marqueeContains(area, point);
    }
    if (selectedBand < 0 || !selection[(size_t)selectedBand])
    {
        selectedBand = -1;
        for (int band = 0; band < kNumBands; ++band)
            if (selection[(size_t)band]) { selectedBand = band; break; }
    }
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
    if (dynamicRangeHandleBounds().contains((float)e.x, (float)e.y))
    {
        hoveredBand = selectedBand;
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        repaint();
        return;
    }
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
    if (e.mods.isAltDown())
    {
        proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
            ? "Adjust selected band slopes" : "Adjust band slope");
        for (int b = 0; b < kNumBands; ++b)
            if (selection[(size_t)b])
                if (auto* parameter = proc.apvts.getParameter(bandId(b + 1, "slope")))
                {
                    const float slope = proc.apvts.getRawParameterValue(bandId(b + 1, "slope"))->load();
                    parameter->beginChangeGesture();
                    const int type=(int)proc.apvts.getRawParameterValue(bandId(b+1,"type"))->load();
                    float value=juce::jlimit(3.0f,96.0f,slope+wheel.deltaY*18.0f);
                    if(!isClassicCutType(type))
                    {
                        static constexpr float discrete[]={6,12,24,36,48,72,96};
                        const int first=(type==2 || type==4 || type==5) ? 1 : 0;
                        int currentIndex=first;
                        for(int i=first+1;i<7;++i) if(std::abs(slope-discrete[i])<std::abs(slope-discrete[currentIndex])) currentIndex=i;
                        value=discrete[juce::jlimit(first,6,currentIndex+(wheel.deltaY>0?1:-1))];
                    }
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
                    parameter->endChangeGesture();
                }
        repaint();
        return;
    }
    if (e.mods.isCommandDown())
    {
        proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
            ? "Adjust selected band characters" : "Adjust band character");
        // JUCE reports an upward wheel gesture with a negative delta on the
        // host/platform combination used by this plug-in.
        const float rawStep = -wheel.deltaY * 0.3f;
        const float minimumStep = wheel.isSmooth ? 0.01f : 0.05f;
        const float step = std::copysign(std::max(std::abs(rawStep), minimumStep), rawStep);
        for (int b = 0; b < kNumBands; ++b)
            if (selection[(size_t)b])
                if (auto* parameter = proc.apvts.getParameter(bandId(b + 1, "drive_character")))
                {
                    const int mode = std::clamp((int)proc.apvts.getRawParameterValue(
                        bandId(b + 1, "sat_mode"))->load(), 0, kSaturationModeCount - 1);
                    const float minimum = saturationModeUsesBipolarCharacter(mode) ? -1.0f : 0.0f;
                    const float current = proc.apvts.getRawParameterValue(
                        bandId(b + 1, "drive_character"))->load();
                    parameter->beginChangeGesture();
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(
                        juce::jlimit(minimum, 1.0f, current + step)));
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
    proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
        ? "Adjust selected band widths" : "Adjust band width");
    for (int b = 0; b < kNumBands; ++b)
        if (selection[(size_t)b])
        {
            const int type = (int)proc.apvts.getRawParameterValue(bandId(b + 1, "type"))->load();
            const float wheelDirection = (type == deq::filter_types::resLowCut
                                          || type == deq::filter_types::resHighCut)
                ? -wheel.deltaY : wheel.deltaY;
            const float factor = std::pow(2.0f, wheelDirection * 0.9f);
            const auto suffix = isClassicCutType(type) ? "slope" : "q";
            if (auto* parameter = proc.apvts.getParameter(bandId(b + 1, suffix)))
            {
                const float current = proc.apvts.getRawParameterValue(bandId(b + 1, suffix))->load();
                const float value = isClassicCutType(type)
                    ? juce::jlimit(3.0f, 96.0f, current + wheel.deltaY * 18.0f)
                    : std::clamp(current * factor, 0.1f, 24.0f);
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
                parameter->endChangeGesture();
            }
        }
    repaint();
}

#if DEFAULT_EQ_FULL
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
        const float freq = displayedBandFrequency(
            proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load());
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
