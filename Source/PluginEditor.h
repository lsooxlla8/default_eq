#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Config.h"
#include "PluginProcessor.h"
#include "UI/ResponseCurveComponent.h"
#include "UI/DefaultFamilyUI.h"

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
    enum class WorkspacePage { Band, Dynamic, Analyzer, Match };
    void timerCallback() override;
    void selectBand(int, bool updateGraphSelection = true);
    void rebindBandControls();
    void setWorkspacePage(WorkspacePage);
    void setWorkspaceExpanded(bool shouldExpand, bool resizeWindow);
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
    bool workspaceExpanded = false;
    int expandedWindowHeight = 620;

    // Header: product identity, selected object, global actions and power.
    default_family::WordmarkButton themeBtn { "default_equalizer" };
    default_family::SmartGainButton autoGainBtn { "AUTO GAIN: OFF" };
    juce::ToggleButton powerBtn { "ON" };
    juce::TextButton workspaceToggleBtn { "ADVANCED" };

    // Workspace navigation. Only the selected layer is visible.
    default_family::PageRail pageRail;

    // Band page. Frequency/gain/Q intentionally live on the graph only.
    juce::ToggleButton bandOn { "ON" }, bandSolo { "SOLO" }, adaptiveQBtn { "ADAPTIVE Q" };
    juce::ToggleButton placementModeBtn { "L/R" };
    juce::ComboBox typeBox;
    juce::Slider slopeSlider, placementSlider;

    // Dynamic page.
    juce::ToggleButton sidechainAudition { "SC LISTEN" };
    juce::Slider dynLookahead, dynThreshold, dynRange, dynRatio, dynAttack, dynRelease;
    juce::ComboBox dynModeBox, sidechainBox;

    // Drive controls share the Band workspace.
    juce::ToggleButton driveOn { "DRIVE ON" }, driveAutoGain { "DRIVE AUTO" };
    juce::Slider driveSlider, driveCharacterSlider, driveMixSlider, driveOutputSlider;
    juce::ComboBox saturationBox;
    juce::Slider oversamplingSlider;

    // Analyzer/global page.
    juce::ToggleButton analyzerVisible { "SPECTRUM" }, spectrumFreeze { "FREEZE" };
    juce::ToggleButton analyzerPeakHold { "PEAK HOLD" };
    juce::ComboBox analyzerResolutionBox;
    juce::Slider analyzerRange, analyzerFloor, analyzerSpeed, analyzerAveraging, analyzerTilt;
    juce::ComboBox phaseModeBox;
    juce::ToggleButton linearPhaseBtn { "LINEAR PHASE" }, decrampBtn { "DE-CRAMP" };
    juce::TextButton matchCaptureBtn { "LEARN TARGET" }, matchApplyBtn { "ANALYZE INPUT" },
                     matchCommitBtn { "APPLY 8 BANDS" }, matchClearBtn { "RESET MATCH" };
    juce::Slider matchAmount, matchSmoothing, matchLow, matchHigh, matchTime;
    juce::Slider outputSlider;

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<ButtonAttachment> powerAtt, bandOnAtt, placementModeAtt,
        adaptiveQAtt, driveOnAtt, driveAutoGainAtt, linearPhaseAtt, decrampAtt;
    std::unique_ptr<ComboAttachment> typeAtt, dynModeAtt, sidechainAtt,
        saturationAtt;
    std::unique_ptr<SliderAttachment> slopeAtt, placementAtt, dynLookaheadAtt, dynThresholdAtt, dynRangeAtt,
        dynRatioAtt, dynAttackAtt, dynReleaseAtt, driveAtt, driveCharacterAtt,
        driveMixAtt, driveOutputAtt, outputAtt, oversamplingAtt;

    juce::TooltipWindow tooltipWindow { this, 450 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DefaultEqualizerAudioProcessorEditor)
};
