#include "ResponseCurveComponent.h"
#include "DefaultFamilyUI.h"
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

float graphUiScale(const juce::Component& component) noexcept
{
    return juce::jlimit(0.85f, 4.0f,
        juce::jmin((float)component.getWidth() / 744.0f,
                   (float)component.getHeight() / 254.0f));
}

struct BandResponseModel
{
    Biquad::Type type {};
    double sampleRate = 44100.0;
    float responseFrequency = 1000.0f;
    float q = 1.0f;
    float baseGain = 0.0f;
    float slope = 12.0f;
    float amount = 1.0f;
    bool gainBearing = false;
    bool classicCut = false;
    bool resonantCut = false;
    bool bandPass = false;

    float responseDb(float modulation, float probeFrequency) const
    {
        const float responseQ = resonantCut ? EQBand::dynamicResonantCutQ(q, modulation)
            : bandPass ? EQBand::dynamicBandPassQ(q, modulation) : q;
        const float responseGain = gainBearing
            ? (baseGain + modulation) * amount : baseGain;
        const float responseSlope = classicCut
            ? EQBand::dynamicClassicCutSlope(slope, modulation) : slope;
        const auto raw = variable_slope::response(type, sampleRate, responseFrequency,
            responseQ, responseGain, responseSlope, probeFrequency);
        const double mix = (classicCut || resonantCut)
            ? (double)EQBand::cutAmountMix(amount)
            : (double)std::clamp(amount, 0.0f, 1.0f);
        const auto response = gainBearing ? raw : std::complex<double>(1.0, 0.0)
            + mix * (raw - std::complex<double>(1.0, 0.0));
        return (float)(20.0 * std::log10(std::max(std::abs(response), 1.0e-15)));
    }
};

BandResponseModel makeBandResponseModel(const DefaultEqualizerAudioProcessor& processor,
                                        int parameterIndex, float displayedFrequency,
                                        double sampleRate)
{
    BandResponseModel model;
    const auto parameter = [parameterIndex, &processor](const char* suffix)
    {
        return processor.apvts.getRawParameterValue(bandId(parameterIndex, suffix))->load();
    };
    model.type = deq::filter_types::fromParameterIndex((int)parameter("type"));
    model.sampleRate = sampleRate;
    model.responseFrequency = displayedFrequency;
    model.baseGain = parameter("gain");
    model.q = parameter("q");
    model.slope = parameter("slope");
    model.amount = processor.apvts.getRawParameterValue("scale")->load();
    if (processor.apvts.getRawParameterValue("adaptive_q")->load() > 0.5f)
        model.q = DefaultEqualizerAudioProcessor::calculateAdaptiveQ(model.q, model.baseGain);
    model.gainBearing = variable_slope::distributesGain(model.type);
    model.classicCut = zl_filter::isClassicCut(model.type);
    model.resonantCut = zl_filter::isResonantCut(model.type);
    model.bandPass = model.type == Biquad::Type::Bandpass;
    if (model.resonantCut)
        model.q = EQBand::amountResonantCutQ(model.q, model.amount);
    if (model.classicCut || model.resonantCut)
    {
        const float neutral = (model.type == Biquad::Type::LowPass
                               || model.type == Biquad::Type::ResLowPass)
            ? (float)sampleRate * 0.45f : 10.0f;
        model.responseFrequency = std::clamp(neutral * std::pow(
            std::max(1.0e-6f, displayedFrequency / neutral), std::max(0.0f, model.amount)),
            10.0f, (float)sampleRate * 0.45f);
    }
    return model;
}
}

void ResponseCurveComponent::paint(juce::Graphics& g)
{
    ensureStaticLayer();
    g.drawImageAt(staticLayer, 0, 0);
    // Text must bypass the 1x raster cache. Otherwise a Retina host scales the
    // already-rasterised glyphs and the gain/frequency labels become blurry.
    paintGridLabels(g);
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
    const auto foreground = foregroundColour();
    const auto background = backgroundColour();
    staticGraphics.fillAll(background);
    paintGrid(staticGraphics);
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
    const auto colour = foregroundColour();
    g.setColour(colour.withAlpha(0.05f));
    g.fillRect(area);
    g.setColour(colour);
    const float uiScale = graphUiScale(*this);
    const float dash[] { 5.0f * uiScale, 4.0f * uiScale };
    juce::Path outline;
    outline.addRectangle(area);
    juce::Path dashed;
    juce::PathStrokeType(1.25f * uiScale).createDashedStroke(
        dashed, outline, dash, 2);
    g.fillPath(dashed);
}

// ── Grid ───────────────────────────────────────────────────────────
void ResponseCurveComponent::paintGrid(juce::Graphics& g)
{
    const float uiScale = graphUiScale(*this);
    const float h = (float)getHeight();
    const auto fg = foregroundColour();

    // Frequency grid lines
    const float freqLines[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (float f : freqLines)
    {
        const float x = freqToX(f);
        g.setColour(fg.withAlpha(0.12f));
        g.drawLine(x, 0.0f, x, h, 1.0f * uiScale);
    }

    for (float db : { -displayMaxDb, -displayMaxDb * 0.5f, 0.0f,
                       displayMaxDb * 0.5f, displayMaxDb })
    {
        const float y = dbToY(db);
        g.setColour(fg.withAlpha(db == 0.0f ? 0.88f : 0.12f));
        g.drawLine(0.0f, y, (float)getWidth(), y,
                   (db == 0.0f ? 2.0f : 1.0f) * uiScale);
    }
}

void ResponseCurveComponent::paintGridLabels(juce::Graphics& g)
{
    const float uiScale = graphUiScale(*this);
    const float scaleX = (float)getWidth() / 1200.0f;
    const float scaleY = (float)getHeight() / 420.0f;
    const auto fg = foregroundColour();
    const std::pair<float, const char*> freqLabels[] = {
        { 100,  "100" }, { 1000, "1k" }, { 10000, "10k" }
    };
    for (auto& [f, label] : freqLabels)
    {
        g.setColour(fg.withAlpha(0.72f));
        g.setFont(default_family::mono(9.0f * uiScale, true));
        const int centreX = juce::roundToInt(freqToX(f));
        const int baselineY = juce::roundToInt(408.0f * scaleY);
        g.drawSingleLineText(label, centreX, baselineY,
                             juce::Justification::horizontallyCentred);
    }

    for (float db : { -displayMaxDb, -displayMaxDb * 0.5f, 0.0f,
                       displayMaxDb * 0.5f, displayMaxDb })
    {
        const float sourceY = juce::jlimit(20.0f, 412.0f,
            dbToY(db) / scaleY - 7.0f);
        const juce::String label = db > 0.0f ? "+" + juce::String((int)db)
                                             : juce::String((int)db);
        g.setColour(fg.withAlpha(0.72f));
        g.setFont(default_family::mono(9.0f * uiScale, true));
        g.drawSingleLineText(label, juce::roundToInt(8.0f * scaleX),
                             juce::roundToInt(sourceY * scaleY));
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

    const auto draw = [&](const float* source, float* smoothed, float* peaks,
                          float alpha, float fillAlpha, float strokeWidth)
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
        const auto fg = foregroundColour();
        g.setColour(fg.withAlpha(fillAlpha)); g.fillPath(fill);
        g.setColour(fg.withAlpha(alpha));
        g.strokePath(outline, juce::PathStrokeType(strokeWidth * graphUiScale(*this)));
        juce::ignoreUnused(source, peakPath, peakStarted);
    };
    if (showInputSpectrum)
        draw(inputSpectrum, smoothedInputSpectrum, peakInputSpectrum, 0.20f, 0.015f, 1.0f);
    if (showOutputSpectrum)
        draw(outputSpectrum, smoothedOutputSpectrum, peakOutputSpectrum, 0.48f, 0.035f, 1.25f);
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
        const float alpha = isSelected ? 0.065f : 0.025f;

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

        const auto fg = foregroundColour();
        g.setColour(fg.withAlpha(alpha));
        g.fillPath(fillPath);

        if (isPrimary && proc.apvts.getRawParameterValue(bandId(idx, "dyn_thresh"))->load() < -0.05f)
        {
            const float range = proc.apvts.getRawParameterValue(bandId(idx, "dyn_range"))->load();
            const bool upward = proc.apvts.getRawParameterValue(bandId(idx, "dyn_mode"))->load() > 0.5f;
            const float dynamicMod = upward ? range : -range;
            const float frequency = displayedBandFrequency(
                proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load());
            const double sampleRate = responseGridSampleRate > 0.0
                ? responseGridSampleRate : (proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0);
            const auto responseModel = makeBandResponseModel(proc, idx, frequency, sampleRate);

            juce::Path basePath, targetPath, dynamicFill;
            std::array<float, numPoints> targetYs {};
            for (int point = 0; point < numPoints; ++point)
            {
                const float x = (float)point / (float)(numPoints - 1) * w;
                const float baseResponse = responseModel.responseDb(0.0f, responseFrequencies[point]);
                const float targetResponse = responseModel.responseDb(dynamicMod, responseFrequencies[point]);
                const float baseY = dbToY(staticMagnitudes[point]);
                const float targetY = dbToY(
                    staticMagnitudes[point] + targetResponse - baseResponse);
                targetYs[(size_t)point] = targetY;
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
                dynamicFill.lineTo(x, targetYs[(size_t)point]);
            }
            dynamicFill.closeSubPath();
            const float uiScale = graphUiScale(*this);
            g.setColour(fg.withAlpha(0.055f)); g.fillPath(dynamicFill);
            g.setColour(fg.withAlpha(0.45f));
            const float dash[] { 6.0f * uiScale, 5.0f * uiScale };
            juce::Path dashed;
            juce::PathStrokeType(1.25f * uiScale).createDashedStroke(
                dashed, targetPath, dash, 2);
            g.fillPath(dashed);
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

    const auto fg = foregroundColour();
    g.setColour(fg);
    g.strokePath(curvePath, juce::PathStrokeType(2.5f * graphUiScale(*this)));
}

// ── Band nodes ─────────────────────────────────────────────────────
void ResponseCurveComponent::paintNodes(juce::Graphics& g)
{
    const float scaleX = (float)getWidth() / 1200.0f;
    const float scaleY = (float)getHeight() / 420.0f;
    const float uiScale = graphUiScale(*this);
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
        const bool isSelected = selection[(size_t) b];
        const bool isPrimary  = (b == selectedBand);

        const auto colour = foregroundColour();
        const auto inverse = backgroundColour();

        const auto snappedRect = [x, y, scaleX, scaleY](float width, float height)
        {
            const int left = juce::roundToInt(x - width * scaleX * 0.5f);
            const int right = juce::roundToInt(x + width * scaleX * 0.5f);
            const int top = juce::roundToInt(y - height * scaleY * 0.5f);
            const int bottom = juce::roundToInt(y + height * scaleY * 0.5f);
            return juce::Rectangle<int>::leftTopRightBottom(
                left, top, juce::jmax(left + 1, right), juce::jmax(top + 1, bottom));
        };

        const auto fillFrame = [&g](juce::Rectangle<int> bounds,
                                    juce::Colour frameColour, int thickness)
        {
            thickness = juce::jlimit(1,
                                     juce::jmax(1, juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2),
                                     thickness);
            g.setColour(frameColour);
            g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), thickness);
            g.fillRect(bounds.getX(), bounds.getBottom() - thickness,
                       bounds.getWidth(), thickness);
            g.fillRect(bounds.getX(), bounds.getY() + thickness,
                       thickness, juce::jmax(0, bounds.getHeight() - thickness * 2));
            g.fillRect(bounds.getRight() - thickness, bounds.getY() + thickness,
                       thickness, juce::jmax(0, bounds.getHeight() - thickness * 2));
        };

        if (isSelected)
        {
            g.setColour(colour.withAlpha(0.07f));
            g.fillRect(snappedRect(42.0f, 42.0f));
            if (!isPrimary)
                fillFrame(snappedRect(38.0f, 38.0f), colour,
                          juce::jmax(1, juce::roundToInt(uiScale)));
        }

        const auto body = snappedRect(28.0f, 28.0f);
        const int bodyFrame = juce::jmax(1, juce::roundToInt(uiScale));
        g.setColour(on ? inverse : colour);
        g.fillRect(body);
        const auto bodyInterior = body.reduced(bodyFrame);
        if (!bodyInterior.isEmpty())
        {
            g.setColour(on ? colour : inverse);
            g.fillRect(bodyInterior);
        }

        default_family::drawPrototypeBaselineText(
            g, juce::String(idx), { x, y + 5.0f * scaleY },
            15.0f, true, 0.0f, on ? inverse : colour,
            default_family::PrototypeTextAlign::centre, scaleX, scaleY);

        const int routeMode=std::clamp((int)proc.apvts.getRawParameterValue(bandId(idx,"placement_mode"))->load(),0,2);
        const float placement = proc.apvts.getRawParameterValue(bandId(idx, "placement"))->load();
        const auto label = std::abs(placement)<1.0f ? (routeMode==0?"LR":routeMode==1?"MS":"TS")
                         : placement<0.0f ? (routeMode==0?"L":routeMode==1?"M":"T")
                                          : (routeMode==0?"R":"S");
        default_family::drawPrototypeBaselineText(
            g, label, { x, y + 28.0f * scaleY },
            10.0f, true, 0.0f, colour.withAlpha(0.84f),
            default_family::PrototypeTextAlign::centre, scaleX, scaleY);
    }

    const auto handle = dynamicRangeHandleBounds();
    if (!handle.isEmpty())
    {
        const auto colour = foregroundColour();
        const auto inverse = backgroundColour();
        const auto visual = handle.withSizeKeepingCentre(12.0f * scaleX, 12.0f * scaleY);
        g.setColour(colour); g.fillRect(visual);
        g.setColour(inverse); g.drawRect(visual, 1.25f * uiScale);
    }
}

float ResponseCurveComponent::bandResponseDb(int band, float modulation,
                                              float probeFrequency) const
{
    if (band < 0 || band >= kNumBands)
        return 0.0f;

    const int idx = band + 1;
    if (proc.apvts.getRawParameterValue(bandId(idx, "present"))->load() < 0.5f
        || proc.apvts.getRawParameterValue(bandId(idx, "on"))->load() < 0.5f)
        return 0.0f;

    const float frequency = displayedBandFrequency(
        proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load());
    const double sampleRate = responseGridSampleRate > 0.0
        ? responseGridSampleRate : (proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0);
    return makeBandResponseModel(proc, idx, frequency, sampleRate)
        .responseDb(modulation, probeFrequency);
}

float ResponseCurveComponent::dynamicRangeTargetDb(float range) const
{
    if (selectedBand < 0 || selectedBand >= kNumBands || getWidth() <= 0)
        return 0.0f;

    const int idx = selectedBand + 1;
    const float frequency = displayedBandFrequency(
        proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load());
    const float point = juce::jlimit(0.0f, (float)(numPoints - 1),
        freqToX(frequency) / (float)getWidth() * (float)(numPoints - 1));
    const int lower = juce::jlimit(0, numPoints - 1, (int)std::floor(point));
    const int upper = juce::jmin(numPoints - 1, lower + 1);
    const float staticComposite = juce::jmap(point - (float)lower,
        staticMagnitudes[lower], staticMagnitudes[upper]);
    const bool upward = proc.apvts.getRawParameterValue(bandId(idx, "dyn_mode"))->load() > 0.5f;
    const float modulation = upward ? range : -range;
    return staticComposite
        + bandResponseDb(selectedBand, modulation, frequency)
        - bandResponseDb(selectedBand, 0.0f, frequency);
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
    const float range = proc.apvts.getRawParameterValue(bandId(idx, "dyn_range"))->load();
    const float y = dbToY(dynamicRangeTargetDb(range));
    return juce::Rectangle<float>(freqToX(frequency) - 5.0f, y - 5.0f, 10.0f, 10.0f);
}

void ResponseCurveComponent::paintHoverCard(juce::Graphics& g)
{
    if (!showHoverTooltip)
    {
        hoverCardBand = hoverCardPlacement = -1;
        return;
    }
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
    const float uiScale = graphUiScale(*this);
    const float textWidth = juce::jmax(
        default_family::prototypeTextWidth(driveLine, 9.0f, false, 0.0f, uiScale),
        default_family::prototypeTextWidth(thresholdLine, 9.0f, false, 0.0f, uiScale));
    const int cardW = juce::jlimit(1, getWidth() - juce::roundToInt(12.0f * uiScale),
                                   juce::roundToInt(textWidth + 20.0f * uiScale));
    const int cardH = juce::roundToInt(37.0f * uiScale);
    const auto clampCard = [this, cardW, cardH, uiScale](int candidateX, int candidateY)
    {
        const int horizontal = juce::roundToInt(6.0f * uiScale);
        const int vertical = juce::roundToInt(8.0f * uiScale);
        return juce::Rectangle<int>(
            juce::jlimit(horizontal, getWidth() - cardW - horizontal, candidateX),
            juce::jlimit(vertical, getHeight() - cardH - vertical, candidateY),
            cardW, cardH);
    };
    const std::array<juce::Rectangle<int>, 4> candidates {
        clampCard((int)x - cardW / 2, (int)y - cardH - juce::roundToInt(20.0f * uiScale)),
        clampCard((int)x - cardW / 2, (int)y + juce::roundToInt(20.0f * uiScale)),
        clampCard((int)x - cardW - juce::roundToInt(20.0f * uiScale), (int)y - cardH / 2),
        clampCard((int)x + juce::roundToInt(20.0f * uiScale), (int)y - cardH / 2)
    };
    const auto obstructionScore = [this, band, x, y, uiScale](juce::Rectangle<int> card)
    {
        const auto guarded = card.expanded(juce::roundToInt(7.0f * uiScale));
        float score = guarded.contains(juce::roundToInt(x), juce::roundToInt(y))
            ? 100000.0f : 0.0f;
        for (int point = 0; point < numPoints; point += 2)
        {
            const float curveX = (float)point / (float)(numPoints - 1) * (float)getWidth();
            const float curveY = dbToY(perBandMagnitudes[band][point]);
            if (guarded.contains(juce::roundToInt(curveX), juce::roundToInt(curveY)))
            {
                const float distanceFromNode = std::abs(curveX - x)
                    / juce::jmax(1.0f, (float)getWidth());
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

    const auto fg = foregroundColour();
    const auto bg = backgroundColour();
    g.setColour(bg.withAlpha(0.82f));
    g.fillRect(cardX, cardY, cardW, cardH);
    g.setColour(fg.withAlpha(0.82f));
    g.drawRect(cardX, cardY, cardW, cardH, juce::roundToInt(2.0f * uiScale));
    default_family::drawPrototypeText(g, driveLine,
        { cardX + 8.0f * uiScale, cardY + 4.0f * uiScale,
          cardW - 16.0f * uiScale, 13.0f * uiScale },
        9.0f, false, 0.0f, fg.withAlpha(0.82f),
        default_family::PrototypeTextAlign::left, uiScale);
    default_family::drawPrototypeText(g, thresholdLine,
        { cardX + 8.0f * uiScale, cardY + 17.0f * uiScale,
          cardW - 16.0f * uiScale, 13.0f * uiScale },
        9.0f, false, 0.0f, fg.withAlpha(0.82f),
        default_family::PrototypeTextAlign::left, uiScale);
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

                    const auto fg = foregroundColour();
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
