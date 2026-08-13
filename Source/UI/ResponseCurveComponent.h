#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSP/Biquad.h"
#include "../Config.h"
#include "../DSP/EQBand.h"
#include "../DSP/SpectrumFIFO.h"

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
    void setAnalyzerVisible(bool shouldShow) { analyzerVisible = shouldShow; repaint(); }
    void setAnalyzerSources(bool input, bool output) { showInputSpectrum = input; showOutputSpectrum = output; repaint(); }
    void setSpectrumFrozen(bool frozen) { spectrumFrozen = frozen; }
    void dismissNumericEditor();
    bool isNumericEditorComponent(const juce::Component* component) const noexcept
    {
        return component == &numericEditor || numericEditor.isParentOf(component);
    }
    void setAnalyzerSettings(float floorDb, float rangeDb, float speed, float averaging,
                             int resolution, float tilt, bool holdPeaks)
    {
        analyzerFloorDb = floorDb; analyzerRangeDb = rangeDb;
        analyzerDecayDb = juce::jmap(speed, 0.0f, 100.0f, 0.15f, 4.0f);
        analyzerAveraging = juce::jmap(averaging, 0.0f, 100.0f, 1.0f, 0.08f);
        analyzerStride = resolution == 0 ? 4 : (resolution == 1 ? 2 : 1);
        analyzerTiltDbPerOct = tilt; peakHold = holdPeaks;
        if (!peakHold) { std::fill(std::begin(peakInputSpectrum), std::end(peakInputSpectrum), -120.0f);
                         std::fill(std::begin(peakOutputSpectrum), std::end(peakOutputSpectrum), -120.0f); }
        repaint();
    }

    // Band colors
    static juce::Colour getBandColour(int bandIndex);

private:
    DefaultEqualizerAudioProcessor& proc;

    // Frequency response calculation
    static constexpr int numPoints = 512;
    float magnitudes[numPoints] = {};
    float perBandMagnitudes[kNumBands][numPoints] = {};

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
    std::array<bool, kNumBands> selection {};
    std::array<float, kNumBands> dragStartFreq {}, dragStartGain {};
    std::array<juce::RangedAudioParameter*, kNumBands> dragFreqParams {}, dragGainParams {};
    float groupAnchorFreq = 1000.0f, groupAnchorGain = 0.0f;
    bool darkMode = true;
    bool analyzerVisible = true;
    bool showInputSpectrum = true, showOutputSpectrum = true, spectrumFrozen = false;
    float analyzerFloorDb = -90.0f, analyzerRangeDb = 90.0f, analyzerDecayDb = 1.5f;
    float analyzerAveraging = 0.35f, analyzerTiltDbPerOct = 0.0f;
    int analyzerStride = 1;
    bool peakHold = false;

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
    void showNumericEditor(int band, const juce::String& suffix, float x, float y);
    void commitNumericEditor();
    juce::TextEditor numericEditor;
    juce::RangedAudioParameter* numericParameter = nullptr;
    juce::String numericSuffix;

    // Display range
    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;
    static constexpr float minDb = -24.0f;
    static constexpr float maxDb = 24.0f;
    static constexpr float nodeRadius = 7.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResponseCurveComponent)
};
