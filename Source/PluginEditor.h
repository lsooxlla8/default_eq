#pragma once

#include <limits>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Config.h"
#include "PluginProcessor.h"
#include "UI/ResponseCurveComponent.h"
#include "UI/DefaultFamilyUI.h"

class ResettableSlider : public juce::Slider
{
public:
    void setPersistentTextFormatter(std::function<juce::String(double)> formatter)
    {
        persistentTextFormatter = std::move(formatter);
        updateText();
    }

    juce::String getTextFromValue(double value) override
    {
        return persistentTextFormatter ? persistentTextFormatter(value)
                                       : juce::Slider::getTextFromValue(value);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if ((event.mods.isPopupMenu() || event.mods.isRightButtonDown()) && isEnabled())
        {
            setValue(getDoubleClickReturnValue(), juce::sendNotificationSync);
            return;
        }
        juce::Slider::mouseDown(event);
    }

private:
    std::function<juce::String(double)> persistentTextFormatter;
};

class VerticalDragSlider : public ResettableSlider
{
public:
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

private:
    double dragStartValue = 0.0;
};

class TwoAxisDragSlider final : public ResettableSlider
{
public:
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

private:
    double dragStartValue = 0.0;
};

class DriveCharacterSlider final : public ResettableSlider
{
public:
    void setSaturationMode(int newMode);
    juce::String getTextFromValue(double value) override;

private:
    int saturationMode = 0;
};

class ThresholdMeterSlider final : public VerticalDragSlider
{
public:
    ThresholdMeterSlider();
    void setInputLevelsDb(float leftDb, float rightDb);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void lookAndFeelChanged() override;

private:
    void updateText();
    float displayedLevelDbL = -60.0f, displayedLevelDbR = -60.0f;
    juce::Label valueLabel;
    bool updating = false;
};

class NumericValueControl final : public ResettableSlider
{
public:
    explicit NumericValueControl(juce::String labelText = {});
    void setFormatter(std::function<juce::String(double)> formatter,
                      std::function<double(const juce::String&)> parser);
    void setValueVisible(bool shouldShowValue);
    void refreshText() { updateText(); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void lookAndFeelChanged() override;

private:
    void updateText();
    juce::Label valueLabel;
    std::function<juce::String(double)> formatValue;
    std::function<double(const juce::String&)> parseValue;
    bool updating = false;
    bool showValue = false;
};

using FamilyLookAndFeel = default_family::LookAndFeel;

class DefaultEqualizerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                    private juce::Timer,
                                                    private juce::Slider::Listener
{
public:
    explicit DefaultEqualizerAudioProcessorEditor(DefaultEqualizerAudioProcessor&);
    ~DefaultEqualizerAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed(const juce::KeyPress&) override;
    void mouseDown(const juce::MouseEvent&) override;
    int getControlParameterIndex(juce::Component&) override;

private:
    void timerCallback() override;
    void selectBand(int, bool updateGraphSelection = true);
    void rebindBandControls();
    void updateBandControlEnablement(bool bandSelected);
    void updateHeaderText();
    void initParameter(juce::Slider&, const juce::String& name);
    void applySliderPalette();
    void updateDriveControls(bool resetModeDefaults = false);
    void updateAnalyzerLifecycle();
    void sliderValueChanged(juce::Slider*) override;
    void sliderDragStarted(juce::Slider*) override;
    void sliderDragEnded(juce::Slider*) override;
    juce::String groupSuffixForSlider(const juce::Slider*) const;
    void applyAbsoluteToSelectedBands(const juce::String& suffix, float value,
                                      bool includePrimary = true);
    void beginGroupSliderEdit(juce::Slider&);
    void endGroupSliderEdit(juce::Slider&);

    DefaultEqualizerAudioProcessor& proc;
    ResponseCurveComponent responseCurve;
    FamilyLookAndFeel familyLook;
    std::unique_ptr<juce::PropertiesFile> uiPreferences;
    bool darkTheme = false;
    int selectedBand = 0;
    juce::Random brandRandom;
    double nextBrandGlitchTimeMs = 0.0;
    int displayedDriveMode = -1;
    int displayedFilterType = -1;
    bool saturationMouseInteraction = false;
    bool typeMouseInteraction = false;
    bool placementModeMouseInteraction = false;
    bool applyingGroupEdit = false;
    juce::Slider* activeGroupSlider = nullptr;
    juce::String activeGroupSuffix;
    float groupPrimaryStart = 0.0f;
    std::array<float, kNumBands> groupParameterStarts {};
    std::array<juce::RangedAudioParameter*, kNumBands> groupParameters {};
    bool driveFormatPending = false;
    std::uint64_t lastTransportStartGeneration = 0;
    float displayedShiftSemitones = std::numeric_limits<float>::quiet_NaN();

    // Header: product identity, selected object, global actions and power.
    default_family::WordmarkButton themeBtn { "default_eq" };
    default_family::SmartGainButton autoGainBtn { "AUTO GAIN" };
    juce::ToggleButton powerBtn { "ON" };

    // Band page. Frequency/gain/Q intentionally live on the graph only.
    juce::ToggleButton bandOn { "ON" }, bandSolo { "SOLO" }, adaptiveQBtn { "ADAPTIVE Q" };
    juce::ComboBox placementModeBox, typeBox;
    TwoAxisDragSlider placementSlider;
    NumericValueControl freqField { "FREQ" }, gainField { "GAIN" },
                        qField { "Q" }, slopeField { "SLOPE" };

    // Dynamic page.
    ResettableSlider dynLookahead, dynRange, dynSpeed;
    NumericValueControl dynRatio { "RATIO" };
    ThresholdMeterSlider dynThreshold;
    juce::TextButton dynModeBtn { "DOWN" }, sidechainBtn { "IN SC" };

    // Drive controls share the Band workspace.
    ResettableSlider driveSlider;
    DriveCharacterSlider driveCharacterSlider;
    juce::ComboBox saturationBox;
    ResettableSlider oversamplingSlider;

    // Global controls. Analyzer settings remain internal preferences.
    juce::ComboBox phaseModeBox;
    NumericValueControl amountSlider { "AMOUNT" }, shiftSlider { "SHIFT" }, outputSlider { "OUT" };

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<ButtonAttachment> powerAtt, bandOnAtt, adaptiveQAtt;
    std::unique_ptr<ComboAttachment> typeAtt, placementModeAtt, saturationAtt;
    std::unique_ptr<SliderAttachment> freqAtt, gainAtt, qAtt, slopeAtt, placementAtt,
        dynLookaheadAtt, dynThresholdAtt, dynRangeAtt,
        dynRatioAtt, dynSpeedAtt, driveAtt, driveCharacterAtt,
        amountAtt, shiftAtt, outputAtt, oversamplingAtt;

    juce::TooltipWindow tooltipWindow { this, 450 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DefaultEqualizerAudioProcessorEditor)
};
