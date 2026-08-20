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

class ResponseCurveComponent : public juce::Component,
                               public juce::Timer
{
public:
    explicit ResponseCurveComponent(DefaultEqualizerAudioProcessor& processor);
    ~ResponseCurveComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

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
    void setSelectedBand(int band);
    void setDarkMode(bool shouldBeDark) { darkMode = shouldBeDark; repaint(); }
    void setAnalyzerSources(bool input, bool output) { showInputSpectrum = input; showOutputSpectrum = output; repaint(); }
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
    void setAnalyzerSettings(float floorDb, float averagingSeconds, float tilt)
    {
        analyzerFloorDb = floorDb;
        analyzerDecayDb = 1.5f;
        analyzerAveraging = averagingSeconds <= 0.0f
            ? 1.0f
            : 1.0f - std::exp(-1.0f / (30.0f * averagingSeconds));
        analyzerTiltDbPerOct = tilt;
        repaint();
    }

    // Band colors
    static juce::Colour getBandColour(int bandIndex);
    static int defaultTypeForNewBand(float frequencyHz, float gainDb) noexcept;
    static bool typeDefaultsToMidSide(int type) noexcept { return type >= 1 && type <= 4; }
    static float analyzerLevelToY(float db, float floorDb, float rangeDb, float height) noexcept
    {
        const float ceiling = floorDb + std::max(1.0f, rangeDb);
        const float normalized = juce::jmap(juce::jlimit(floorDb, ceiling, db),
                                             floorDb, ceiling, 0.0f, 1.0f);
        return height * (1.0f - normalized);
    }
    static bool isCutType(int type) noexcept { return type == 3 || type == 4; }
    static bool usesQVerticalDrag(int type) noexcept { return type >= 3 && type <= 6; }
    static float cutQFromVerticalDrag(float startQ, float deltaY) noexcept
    {
        return std::clamp(startQ * std::pow(2.0f, -deltaY / 80.0f), 0.1f, 24.0f);
    }

private:
    DefaultEqualizerAudioProcessor& proc;

    // Frequency response calculation
    static constexpr int numPoints = 512;
    float magnitudes[numPoints] = {};
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
    bool dragging = false;
    bool driveDragging = false;
    bool thresholdDragging = false;
    bool commandGesturePending = false;
    bool shiftGesturePending = false;
    bool momentarySoloActive = false;
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
    float groupAnchorFreq = 1000.0f, groupAnchorGain = 0.0f;
    float displayMaxDb = 12.0f;
    bool rangeExpansionAvailable = true;
    bool darkMode = true;
    bool showInputSpectrum = true, showOutputSpectrum = true;
    float analyzerFloorDb = -90.0f, analyzerDecayDb = 1.5f;
    float analyzerAveraging = 0.402f, analyzerTiltDbPerOct = 4.5f;

    // Coordinate mapping
    float freqToX(float freqHz) const;
    float xToFreq(float x) const;
    float dbToY(float db) const;
    float yToDb(float y) const;

    // Compute magnitude response for a single biquad at a given frequency
    static float computeMagnitudeDb(const Biquad& bq, double freq, double sampleRate);

    // Update all magnitude arrays from current processor state
    void updateResponseCurve();

    // Paint helpers
    void paintGrid(juce::Graphics& g);
    void paintSpectrum(juce::Graphics& g);
    void paintMatchPreview(juce::Graphics& g);
    void paintResponseCurve(juce::Graphics& g);
    void paintBandCurves(juce::Graphics& g);
    void paintNodes(juce::Graphics& g);
    void paintHoverCard(juce::Graphics& g);
#if DEFAULT_EQUALIZER_FULL
    void paintCollisionWarnings(juce::Graphics& g);
#endif

    // Hit test for band nodes
    int hitTestNode(float x, float y) const;
    int createBandAt(float x, float y, std::int64_t eventTimeMs);
    void beginStaticBandDrag(int band, bool beginUndoTransaction);
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
