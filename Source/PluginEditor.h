#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Config.h"
#include "PluginProcessor.h"
#include "UI/ResponseCurveComponent.h"
#include "UI/DefaultFamilyUI.h"

class ResettableSlider : public juce::Slider
{
public:
    void mouseDown(const juce::MouseEvent& event) override
    {
        if ((event.mods.isPopupMenu() || event.mods.isRightButtonDown()) && isEnabled())
        {
            setValue(getDoubleClickReturnValue(), juce::sendNotificationSync);
            return;
        }
        juce::Slider::mouseDown(event);
    }
};

class NumericValueControl final : public ResettableSlider
{
public:
    explicit NumericValueControl(juce::String labelText = {});
    void setFormatter(std::function<juce::String(double)> formatter,
                      std::function<double(const juce::String&)> parser);
    void setValueVisible(bool shouldShowValue);
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
                                                    private juce::Timer
{
public:
    explicit DefaultEqualizerAudioProcessorEditor(DefaultEqualizerAudioProcessor&);
    ~DefaultEqualizerAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed(const juce::KeyPress&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    enum class WorkspacePage { Band, Match };
    void timerCallback() override;
    void selectBand(int, bool updateGraphSelection = true);
    void rebindBandControls();
    void updateBandControlEnablement(bool bandSelected);
    void setWorkspacePage(WorkspacePage);
    void setWorkspaceExpanded(bool, bool);
    void updateHeaderText();
    void initParameter(juce::Slider&, const juce::String& name);
    void applySliderPalette();
    void updateDriveControls(bool resetModeDefaults = false);
    void applyMatchToBands();
    void updateAnalyzerSettings();
    void updateAnalyzerLifecycle();

    DefaultEqualizerAudioProcessor& proc;
    ResponseCurveComponent responseCurve;
    FamilyLookAndFeel familyLook;
    std::unique_ptr<juce::PropertiesFile> uiPreferences;
    bool darkTheme = false;
    int selectedBand = 0;
    WorkspacePage workspacePage = WorkspacePage::Band;
    juce::Random brandRandom;
    double nextBrandGlitchTimeMs = 0.0;
    int displayedDriveMode = -1;
    bool saturationMouseInteraction = false;
    bool driveFormatPending = false;
    bool workspaceExpanded = true;
    std::uint64_t lastTransportStartGeneration = 0;
    int expandedWindowHeight = 464;

    // Header: product identity, selected object, global actions and power.
    default_family::WordmarkButton themeBtn { "default_eq8" };
    default_family::SmartGainButton autoGainBtn { "AUTO GAIN: OFF" };
    juce::ToggleButton powerBtn { "ON" };
    juce::TextButton workspaceToggleBtn { "MATCH / RTA" };

    // Band page. Frequency/gain/Q intentionally live on the graph only.
    juce::ToggleButton bandOn { "ON" }, bandSolo { "SOLO" }, adaptiveQBtn { "ADAPTIVE Q" };
    juce::ToggleButton placementModeBtn { "L/R" };
    juce::ComboBox typeBox;
    ResettableSlider placementSlider;
    NumericValueControl freqField { "FREQ" }, gainField { "GAIN" },
                        qField { "Q" }, slopeField { "SLOPE" };

    // Dynamic page.
    juce::ToggleButton sidechainAudition { "SC LISTEN" };
    ResettableSlider dynLookahead, dynThreshold, dynRange, dynRatio, dynAttack, dynRelease;
    juce::TextButton dynModeBtn { "DOWN" }, sidechainBtn { "INT SC" };

    // Drive controls share the Band workspace.
    ResettableSlider driveSlider, driveCharacterSlider;
    juce::ComboBox saturationBox;
    ResettableSlider oversamplingSlider;

    // Analyzer/global page.
    ResettableSlider analyzerFloor, analyzerAveraging, analyzerTilt;
    juce::ComboBox phaseModeBox;
    juce::ToggleButton linearPhaseBtn { "LINEAR PHASE" };
    juce::ToggleButton matchSidechainBtn { "SC INPUT" };
    juce::TextButton matchCaptureBtn { "LEARN TARGET" }, matchApplyBtn { "ANALYZE INPUT" },
                     matchCommitBtn { "APPLY 8 BANDS" }, matchClearBtn { "RESET MATCH" };
    ResettableSlider matchAmount, matchSmoothing, matchLow, matchHigh, matchTime;
    NumericValueControl amountSlider { "AMOUNT" }, outputSlider { "OUT" };

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<ButtonAttachment> powerAtt, bandOnAtt, placementModeAtt,
        adaptiveQAtt, linearPhaseAtt;
    std::unique_ptr<ComboAttachment> typeAtt, saturationAtt;
    std::unique_ptr<SliderAttachment> freqAtt, gainAtt, qAtt, slopeAtt, placementAtt,
        dynLookaheadAtt, dynThresholdAtt, dynRangeAtt,
        dynRatioAtt, dynAttackAtt, dynReleaseAtt, driveAtt, driveCharacterAtt,
        amountAtt, outputAtt, oversamplingAtt;

    juce::TooltipWindow tooltipWindow { this, 450 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DefaultEqualizerAudioProcessorEditor)
};
