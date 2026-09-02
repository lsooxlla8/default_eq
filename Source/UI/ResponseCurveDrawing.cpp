#include "ResponseCurveComponent.h"
#include "DriveCharacterFormatting.h"
#include "../DSP/FilterTypes.h"
#include "../DSP/VariableSlope.h"
#include "../PluginProcessor.h"
#include <complex>

namespace
{
juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}
}

void ResponseCurveComponent::paint(juce::Graphics& g)
{
    ensureStaticLayer();
    g.drawImageAt(staticLayer, 0, 0);
    paintSpectrum(g);
    paintBandCurves(g);
    paintResponseCurve(g);
    paintNodes(g);
    paintMarquee(g);
    paintHoverCard(g);
#if DEFAULT_EQ_FULL
    paintCollisionWarnings(g);
#endif

}

void ResponseCurveComponent::ensureStaticLayer()
{
    if (!staticLayerDirty && staticLayer.getWidth() == getWidth()
        && staticLayer.getHeight() == getHeight())
        return;
    if (getWidth() <= 0 || getHeight() <= 0)
        return;
    staticLayer = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true);
    juce::Graphics staticGraphics(staticLayer);
    const auto foreground = darkMode ? juce::Colour(0xfff6f6f6) : juce::Colour(0xff050505);
    const auto background = darkMode ? juce::Colour(0xff050505) : juce::Colour(0xfff6f6f6);
    staticGraphics.fillAll(background);
    paintGrid(staticGraphics);
    staticGraphics.setColour(foreground);
    staticGraphics.drawRect(getLocalBounds(), 3);
    staticLayerDirty = false;
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
