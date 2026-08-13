#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Config.h"
#include "PluginProcessor.h"
#include "UI/ResponseCurveComponent.h"
#include "UI/LevelMeter.h"

class FamilyLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    FamilyLookAndFeel();
    void setDark(bool);
    bool isDark() const noexcept { return dark; }
    juce::Colour paper() const noexcept { return juce::Colour(0xfff6f6f6); }
    juce::Colour ink() const noexcept { return juce::Colour(0xff050505); }
    juce::Colour foreground() const noexcept { return dark ? paper() : ink(); }
    juce::Colour background() const noexcept { return dark ? ink() : paper(); }
    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getLabelFont(juce::Label&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
private:
    bool dark = true;
};

class DefaultEqualizerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                    private juce::Timer
{
public:
    explicit DefaultEqualizerAudioProcessorEditor(DefaultEqualizerAudioProcessor&);
    ~DefaultEqualizerAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    enum class WorkspacePage { Band, Dynamic, Drive, Analyzer, Match };
    void timerCallback() override;
    void selectBand(int, bool updateGraphSelection = true);
    void rebindBandControls();
    void setWorkspacePage(WorkspacePage);
    void updateHeaderText();
    void initParameter(juce::Slider&, const juce::String& name);
    void applySliderPalette();
    void applyMatchToBands();
    void updateAnalyzerSettings();

    DefaultEqualizerAudioProcessor& proc;
    ResponseCurveComponent responseCurve;
    LevelMeter levelMeter;
    FamilyLookAndFeel familyLook;
    std::unique_ptr<juce::PropertiesFile> uiPreferences;
    bool darkTheme = false;
    int selectedBand = 0;
    WorkspacePage workspacePage = WorkspacePage::Band;
    juce::Random brandRandom;
    double nextBrandGlitchTimeMs = 0.0;

    // Header: product identity, selected object, global actions and power.
    juce::TextButton themeBtn { "default_equalizer" };
    juce::TextButton autoGainBtn { "AUTO OFF" };
    juce::ToggleButton powerBtn { "ON" };

    // Workspace navigation. Only the selected layer is visible.
    juce::TextButton bandPageBtn { "BAND" }, dynamicPageBtn { "DYNAMIC" };
    juce::TextButton drivePageBtn { "DRIVE" }, analyzerPageBtn { "RTA" }, matchPageBtn { "MATCH" };

    // Band page. Frequency/gain/Q intentionally live on the graph only.
    juce::ToggleButton bandOn { "ON" }, bandSolo { "SOLO" }, adaptiveQBtn { "ADAPTIVE Q" };
    juce::TextButton deleteBandBtn { "DELETE" };
    juce::ComboBox typeBox, channelBox;
    juce::Slider slopeSlider;

    // Dynamic page.
    juce::ToggleButton dynOn { "DYN ON" }, sidechainAudition { "SC LISTEN" };
    juce::Slider dynLookahead, dynThreshold, dynRange, dynRatio, dynAttack, dynRelease;
    juce::ComboBox dynModeBox, sidechainBox;

    // Drive page.
    juce::ToggleButton driveOn { "DRIVE ON" };
    juce::Slider driveSlider, driveCharacterSlider, driveSecondarySlider, driveToneSlider,
                 driveMixSlider, driveOutputSlider;
    juce::ComboBox saturationBox, oversamplingBox;

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
    std::unique_ptr<ButtonAttachment> powerAtt, bandOnAtt,
        adaptiveQAtt, dynOnAtt, driveOnAtt, linearPhaseAtt, decrampAtt;
    std::unique_ptr<ComboAttachment> typeAtt, channelAtt, dynModeAtt, sidechainAtt,
        saturationAtt, oversamplingAtt;
    std::unique_ptr<SliderAttachment> slopeAtt, dynLookaheadAtt, dynThresholdAtt, dynRangeAtt,
        dynRatioAtt, dynAttackAtt, dynReleaseAtt, driveAtt, driveCharacterAtt,
        driveSecondaryAtt, driveToneAtt, driveMixAtt, driveOutputAtt, outputAtt;

    juce::TooltipWindow tooltipWindow { this, 450 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DefaultEqualizerAudioProcessorEditor)
};
