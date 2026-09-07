#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSP/Biquad.h"
#include "../Config.h"
#include "../DSP/EQBand.h"
#include "../DSP/SpectrumFIFO.h"
#include <algorithm>
#include <cmath>

class DefaultEqualizerAudioProcessor;

struct PrototypeContextMenuModel
{
    int bandNumber = 1;
    int selectedType = 0;
    int selectedRoute = 1;
    int selectedSaturation = 0;
    int selectedCount = 1;
    std::function<void()> toggleBand;
    std::function<void(int)> chooseFilter;
    std::function<void(int)> chooseRoute;
    std::function<void(int)> chooseSaturation;
    std::function<void()> resetEqualizer;
    std::function<void()> bypassSelected;
};

struct SpectralStatistics
{
    float centroidHz = 0.0f;
    float averageTiltDbPerOct = 0.0f;
    std::array<float, 10> tonalPercent {};
    bool valid = false;
};

class ResponseCurveComponent : public juce::Component
{
public:
    explicit ResponseCurveComponent(DefaultEqualizerAudioProcessor& processor);
    ~ResponseCurveComponent() override = default;

    std::function<void(juce::Point<int>, PrototypeContextMenuModel)> onContextMenuRequest;
    std::function<void()> onContextMenuDismissRequest;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool refreshForTimer(bool spectrumFrameArrived);

    // Mouse interaction for draggable nodes
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    // Spectrum analyzer data - call from the editor to push FFT magnitudes
    void pushSpectrumData(const float* magnitudes, int numBins, double sampleRate, bool input);

    int getSelectedBand() const { return selectedBand; }
    const std::array<bool, kNumBands>& getSelection() const noexcept { return selection; }
    int getSelectionCount() const noexcept
    { return (int)std::count(selection.begin(), selection.end(), true); }
    bool isBandSelected(int band) const noexcept
    { return band >= 0 && band < kNumBands && selection[(size_t)band]; }
    void setSelectedBand(int band);
    bool deleteSelectedBands();
    bool resetBandThreshold(int band);
    void setDarkMode(bool shouldBeDark)
    {
        if (darkMode == shouldBeDark) return;
        darkMode = shouldBeDark;
        staticLayerDirty = true;
        repaint();
    }
    void setThemeColours(juce::Colour lightBackground, juce::Colour lightForeground,
                         juce::Colour darkBackground, juce::Colour darkForeground)
    {
        lightBackgroundColour = lightBackground.withAlpha(1.0f);
        lightForegroundColour = lightForeground.withAlpha(1.0f);
        darkBackgroundColour = darkBackground.withAlpha(1.0f);
        darkForegroundColour = darkForeground.withAlpha(1.0f);
        staticLayerDirty = true;
        repaint();
    }
    void setAnalyzerSources(bool input, bool output)
    {
        if (showInputSpectrum == input && showOutputSpectrum == output) return;
        showInputSpectrum = input;
        showOutputSpectrum = output;
        repaint();
    }
    void resetPeakHold()
    {
        std::fill(std::begin(peakInputSpectrum), std::end(peakInputSpectrum), -120.0f);
        std::fill(std::begin(peakOutputSpectrum), std::end(peakOutputSpectrum), -120.0f);
        repaint();
    }
    void dismissNumericEditor();
    bool isNumericEditorComponent(const juce::Component* component) const noexcept
    {
        return component == &numericEditor || numericEditor.isParentOf(component);
    }
    void setAnalyzerSettings(float floorDb, float averagingSeconds, float tilt, float ceilingDb = 0.0f);
    void setHoverTooltipEnabled(bool enabled)
    {
        if (showHoverTooltip == enabled) return;
        showHoverTooltip = enabled;
        repaint();
    }
    bool isHoverTooltipEnabled() const noexcept { return showHoverTooltip; }
    void setGainRangeMode(int mode);
    int getGainRangeMode() const noexcept { return gainRangeMode; }
    float getDisplayMaxDb() const noexcept { return displayMaxDb; }
    void resetAutoRtaRangeForOpen();
    SpectralStatistics calculateSpectralStatistics() const { return spectralStatistics; }

    // Band colors
    static juce::Colour getBandColour(int bandIndex);
    static int defaultTypeForNewBand(float frequencyHz, float gainDb) noexcept;
    static int shiftClickTypeForNewBand(float frequencyHz, float gainDb) noexcept;
    static bool typeDefaultsToMidSide(int) noexcept { return false; }
    static float analyzerLevelToY(float db, float floorDb, float rangeDb, float height) noexcept
    {
        const float ceiling = floorDb + std::max(1.0f, rangeDb);
        const float normalized = juce::jmap(juce::jlimit(floorDb, ceiling, db),
                                             floorDb, ceiling, 0.0f, 1.0f);
        return height * (1.0f - normalized);
    }
    static bool isCutType(int type) noexcept { return type == 0 || type == 1 || type == 8 || type == 9; }
    static bool isClassicCutType(int type) noexcept { return type == 8 || type == 9; }
    static bool usesQVerticalDrag(int type) noexcept { return isCutType(type) || type == 2 || type == 4; }
    static bool usesGainVerticalDrag(int type) noexcept { return !usesQVerticalDrag(type); }
    enum class WheelAction { widthOrClassicSlope, slope, character, placement };
    static WheelAction wheelActionForModifiers(bool commandDown, bool altDown,
                                               bool shiftDown) noexcept
    {
        if (commandDown) return WheelAction::slope;
        if (altDown) return WheelAction::character;
        if (shiftDown) return WheelAction::placement;
        return WheelAction::widthOrClassicSlope;
    }
    static float increasingSlopeWheelStep(float platformDeltaY) noexcept
    {
        // JUCE reports an upward wheel gesture as a negative delta on the
        // host/platform combination used by this plug-in.
        return -platformDeltaY;
    }
    static float slopeAfterWheelStep(int type, float slope,
                                     float increasingStep) noexcept
    {
        if (isClassicCutType(type))
            return juce::jlimit(3.0f, 96.0f, slope + increasingStep * 18.0f);

        static constexpr float discrete[] { 6, 12, 24, 36, 48, 72, 96 };
        const int first = (type == 2 || type == 4 || type == 5) ? 1 : 0;
        int currentIndex = first;
        for (int index = first + 1; index < 7; ++index)
            if (std::abs(slope - discrete[index])
                < std::abs(slope - discrete[currentIndex]))
                currentIndex = index;
        return discrete[juce::jlimit(first, 6,
            currentIndex + (increasingStep > 0.0f ? 1 : -1))];
    }
    static float qResetValueForType(int type) noexcept
    {
        return deq::filter_types::isResonantCutIndex(type)
            ? deq::filter_types::resonantCutDefaultQ : 1.0f;
    }
    static constexpr float bandHitRadius() noexcept { return 20.0f; }
    static bool isWithinBandHitArea(float deltaX, float deltaY) noexcept
    {
        const float radius = bandHitRadius();
        return deltaX * deltaX + deltaY * deltaY <= radius * radius;
    }
    static float cutQFromVerticalDrag(float startQ, float deltaY) noexcept
    {
        return std::clamp(startQ * std::pow(2.0f, -deltaY / 80.0f), 0.1f, 24.0f);
    }
    static bool marqueeContains(juce::Rectangle<float> marquee,
                                juce::Point<float> point) noexcept
    { return marquee.getSmallestIntegerContainer().contains(point.roundToInt()); }

private:
    friend int runEditorLayoutRegression();

    DefaultEqualizerAudioProcessor& proc;

    // Frequency response calculation
    static constexpr int numPoints = 512;
    float magnitudes[numPoints] = {};
    float staticMagnitudes[numPoints] = {};
    float perBandMagnitudes[kNumBands][numPoints] = {};
    float responseFrequencies[numPoints] = {};
    double responseGridSampleRate = 0.0;
    std::uint64_t responseSignature = 0;

    // Spectrum analyzer display data
    static constexpr int maxSpectrumBins = SpectrumFIFO::numBins;
    float inputSpectrum[maxSpectrumBins] = {}, outputSpectrum[maxSpectrumBins] = {};
    float smoothedInputSpectrum[maxSpectrumBins] = {}, smoothedOutputSpectrum[maxSpectrumBins] = {};
    float peakInputSpectrum[maxSpectrumBins] = {}, peakOutputSpectrum[maxSpectrumBins] = {};
    int currentSpectrumSize = 0;
    double spectrumSampleRate = 44100.0;

    // Draggable nodes
    int selectedBand = -1;   // Currently selected band (-1 = none)
    int hoveredBand = -1;    // Band under cursor
    int hoverCardBand = -1;
    int hoverCardPlacement = -1;
    bool dragging = false;
    bool driveDragging = false;
    bool thresholdDragging = false;
    bool dynamicRangeDragging = false;
    bool commandGesturePending = false;
    bool shiftGesturePending = false;
    bool momentarySoloActive = false;
    bool marqueePending = false;
    bool marqueeDragging = false;
    juce::Point<float> marqueeStart, marqueeCurrent;
    int modifierGestureBand = -1;
    int mostRecentlyCreatedBand = -1;
    std::int64_t mostRecentCreationTimeMs = 0;
    std::array<bool, kNumBands> selection {};
    std::array<float, kNumBands> dragStartFreq {}, dragStartGain {}, dragStartQ {};
    std::array<float, kNumBands> dragStartDrive {};
    std::array<float, kNumBands> dragStartThreshold {};
    std::array<juce::RangedAudioParameter*, kNumBands> dragFreqParams {}, dragGainParams {}, dragQParams {};
    std::array<juce::RangedAudioParameter*, kNumBands> dragDriveParams {};
    std::array<juce::RangedAudioParameter*, kNumBands> dragThresholdParams {};
    juce::RangedAudioParameter* dynamicRangeDragParam = nullptr;
    float groupAnchorFreq = 1000.0f, groupAnchorGain = 0.0f;
    float displayMaxDb = 6.0f;
    int gainRangeMode = 0;
    bool rangeExpansionAvailable = true;
    bool darkMode = true;
    juce::Colour lightBackgroundColour { 0xfff6f6f6 };
    juce::Colour lightForegroundColour { 0xff050505 };
    juce::Colour darkBackgroundColour { 0xff050505 };
    juce::Colour darkForegroundColour { 0xfff6f6f6 };
    juce::Colour foregroundColour() const noexcept
    { return darkMode ? darkForegroundColour : lightForegroundColour; }
    juce::Colour backgroundColour() const noexcept
    { return darkMode ? darkBackgroundColour : lightBackgroundColour; }
    bool showInputSpectrum = true, showOutputSpectrum = true;
    bool showHoverTooltip = true;
    float analyzerCeilingDb = 0.0f;
    float analyzerFloorDb = -90.0f, analyzerDecayDb = 1.5f;
    SpectralStatistics spectralStatistics;
    SpectralStatistics calculateRawSpectralStatistics() const;
    float analyzerAveragingSeconds = 0.065f, analyzerTiltDbPerOct = 4.5f;
    juce::Image staticLayer;
    bool staticLayerDirty = true;

    // Coordinate mapping
    float freqToX(float freqHz) const;
    float xToFreq(float x) const;
    float displayedBandFrequency(float baseFrequency) const;
    float baseBandFrequency(float displayedFrequency) const;
    float dbToY(float db) const;
    float yToDb(float y) const;

    // Compute magnitude response for a single biquad at a given frequency
    static float computeMagnitudeDb(const Biquad& bq, double freq, double sampleRate);

    // Update all magnitude arrays from current processor state
    bool updateResponseCurve();
    void advanceSpectrumFrame();
    void ensureStaticLayer();

    // Paint helpers
    void paintGrid(juce::Graphics& g);
    void paintGridLabels(juce::Graphics& g);
    void paintSpectrum(juce::Graphics& g);
    void paintResponseCurve(juce::Graphics& g);
    void paintBandCurves(juce::Graphics& g);
    void paintNodes(juce::Graphics& g);
    void paintHoverCard(juce::Graphics& g);
    void paintMarquee(juce::Graphics& g);
#if DEFAULT_EQ_FULL
    void paintCollisionWarnings(juce::Graphics& g);
#endif

    // Hit test for band nodes
    int hitTestNode(float x, float y) const;
    int createBandAt(float x, float y, std::int64_t eventTimeMs, int forcedType = -1);
    float bandResponseDb(int band, float modulation, float probeFrequency) const;
    float dynamicRangeTargetDb(float range) const;
    juce::Rectangle<float> dynamicRangeHandleBounds() const;
    bool expandAutoRtaRangeNearEdge(int localPointerY);
    void beginStaticBandDrag(int band, bool beginUndoTransaction);
    void updateMarqueeSelection();
    void showNumericEditor(int band, const juce::String& suffix, float x, float y);
    void commitNumericEditor();
    juce::TextEditor numericEditor;
    juce::RangedAudioParameter* numericParameter = nullptr;
    juce::String numericSuffix;

    // Display range
    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;
    static constexpr float minBandGainDb = -36.0f;
    static constexpr float maxBandGainDb = 36.0f;
    static constexpr float maxDisplayDb = 36.0f;
    static constexpr float nodeRadius = 7.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResponseCurveComponent)
};
