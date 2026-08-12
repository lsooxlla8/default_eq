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
    void setDark(bool shouldBeDark);
    bool isDark() const noexcept { return dark; }
    juce::Colour paper() const noexcept { return juce::Colour(0xfff6f6f6); }
    juce::Colour ink() const noexcept { return juce::Colour(0xff050505); }
    juce::Colour foreground() const noexcept { return dark ? paper() : ink(); }
    juce::Colour background() const noexcept { return dark ? ink() : paper(); }

    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getLabelFont(juce::Label&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                              bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float,
                          float, juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int,
                      juce::ComboBox&) override;

private:
    bool dark = true;
};

class DefaultEqualizerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::Timer
{
public:
    explicit DefaultEqualizerAudioProcessorEditor(DefaultEqualizerAudioProcessor&);
    ~DefaultEqualizerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    DefaultEqualizerAudioProcessor& proc;
    ResponseCurveComponent responseCurve;
    FamilyLookAndFeel familyLook;
    std::unique_ptr<juce::PropertiesFile> uiPreferences;
    bool darkTheme = true;

    // ── Band selection ───────────────────────────────────────────
    int selectedBand = 0;
    std::array<juce::TextButton, kNumBands> bandBtns;
    void selectBand(int band);
    void rebindBandControls(int band);

    // ── Selected band controls (single set, rebound per selection) ──
    juce::ToggleButton bandOn, bandSolo, dynOn;
    juce::ComboBox typeBox, slopeBox, channelBox, linkBox;
    juce::Slider freqKnob, gainKnob, qKnob, driveKnob;
#if PROEQ8
    juce::ComboBox satModeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> satModeAtt;
#endif
    juce::Slider dynThreshKnob, dynRatioKnob, dynAttackKnob, dynReleaseKnob;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  bandOnAtt, bandSoloAtt, dynOnAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt, slopeAtt, channelAtt, linkAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  freqAtt, gainAtt, qAtt, driveAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  dynThreshAtt, dynRatioAtt, dynAttackAtt, dynReleaseAtt;

    // ── Global controls ──────────────────────────────────────────
    juce::Slider outputGainSlider, scaleSlider;
    juce::ToggleButton adaptiveQBtn, linPhaseBtn, autoGainBtn;
    juce::ComboBox oversamplingBox, procModeBox;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  outputGainAtt, scaleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  adaptiveQAtt, linPhaseAtt, autoGainAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAtt, procModeAtt;

    // ── Toolbar ──────────────────────────────────────────────────
    juce::TextButton undoBtn { "Undo" }, redoBtn { "Redo" };
    juce::TextButton matchCapBtn { "Capture" }, matchAppBtn { "Match" }, matchClrBtn { "Clear" };
    juce::TextButton themeBtn { "default_equalizer" };
    juce::ToggleButton reducedMotionBtn { "REDUCED MOTION" };
    juce::ToggleButton powerBtn { "ON" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;

    // ── Presets ──────────────────────────────────────────────────
    juce::ComboBox presetBox;
    juce::TextButton saveBtn { "Save" }, delBtn { "Del" };
    void refreshPresetList();
    void onPresetSelected();
    void onSaveClicked();
    void onDeleteClicked();

    // ── Spectrum / Meter ─────────────────────────────────────────
    juce::ToggleButton postEqToggle;
    bool showPostSpectrum = true;
    LevelMeter levelMeter;

    // ── A/B comparison ───────────────────────────────────────────
    juce::TextButton abBtn { "A" }, copyABBtn { "A\u2192B" };
    void toggleAB();

    // ── Helpers ──────────────────────────────────────────────────
    void initKnob(juce::Slider& s, juce::Colour c, bool large);

    // Tooltip window — auto-shows tooltips for any child component with setTooltip()
    juce::TooltipWindow tooltipWindow { this, 500 };

    // ── A2: lifetime-safe modal dialogs + async callbacks ─────────────
    // activeDialog is non-null while a modal AlertWindow is up; it's cleared
    // from the ModalCallbackFunction. Using a unique_ptr instead of
    // `new ... + delete dlg` eliminates the prior latent double-free risk
    // (the old code called `delete dlg` inside a callback registered with
    // `deleteWhenDismissed = true`).
    std::unique_ptr<juce::AlertWindow> activeDialog;

    JUCE_DECLARE_WEAK_REFERENCEABLE (DefaultEqualizerAudioProcessorEditor)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DefaultEqualizerAudioProcessorEditor)
};
