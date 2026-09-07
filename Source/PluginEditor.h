#pragma once

#include <limits>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "Config.h"
#include "PluginProcessor.h"
#include "UI/ResponseCurveComponent.h"
#include "UI/DefaultFamilyUI.h"

class ResettableSlider : public juce::Slider
{
public:
    void setMixedValue(bool shouldBeMixed)
    {
        if (hasMixedValue() == shouldBeMixed) return;
        getProperties().set("mixedValue", shouldBeMixed);
        updateText();
        repaint();
    }

    bool hasMixedValue() const
    {
        return (bool)getProperties().getWithDefault("mixedValue", false);
    }

    void setPersistentTextFormatter(std::function<juce::String(double)> formatter)
    {
        persistentTextFormatter = std::move(formatter);
        updateText();
    }

    juce::String getTextFromValue(double value) override
    {
        return hasMixedValue() ? juce::String("MULTI")
             : persistentTextFormatter ? persistentTextFormatter(value)
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
    void setMixedValue(bool shouldBeMixed);
    bool hasMixedValue() const noexcept { return mixedValue; }
    juce::String getDisplayedText() const { return valueLabel.getText(); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void lookAndFeelChanged() override;

private:
    void updateText();
    float displayedLevelDbL = -60.0f, displayedLevelDbR = -60.0f;
    juce::Label valueLabel;
    bool updating = false;
    bool mixedValue = false;
};

class NumericValueControl final : public ResettableSlider
{
public:
    explicit NumericValueControl(juce::String labelText = {});
    void setFormatter(std::function<juce::String(double)> formatter,
                      std::function<double(const juce::String&)> parser);
    void setValueVisible(bool shouldShowValue);
    void setVerticalMini(bool shouldUseVerticalMini);
    void setMixedValue(bool shouldBeMixed);
    bool hasMixedValue() const noexcept { return mixedValue; }
    juce::String getDisplayedText() const { return valueLabel.getText(); }
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
    bool verticalMini = false;
    bool mixedValue = false;
};

using FamilyLookAndFeel = default_family::LookAndFeel;

class PrototypeComboBox final : public juce::ComboBox
{
public:
    PrototypeComboBox() { setMouseClickGrabsKeyboardFocus(false); }
    std::function<void(PrototypeComboBox&)> onPickerRequest;

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (isEnabled() && event.mods.isLeftButtonDown() && onPickerRequest)
        {
            onPickerRequest(*this);
            return;
        }
        juce::ComboBox::mouseDown(event);
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        const int code = key.getKeyCode();
        if (isEnabled() && onPickerRequest
            && (code == juce::KeyPress::returnKey || code == juce::KeyPress::spaceKey
                || code == juce::KeyPress::downKey))
        {
            onPickerRequest(*this);
            return true;
        }
        return juce::ComboBox::keyPressed(key);
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override {}
};

class PrototypeSelectMenu final : public juce::Component
{
public:
    std::function<void(PrototypeComboBox&, int)> onChoose;
    void showFor(PrototypeComboBox&, juce::Component& shell, float scale);
    void hide();
    bool isShowingFor(const PrototypeComboBox&) const noexcept;
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    int rowAt(juce::Point<int>) const noexcept;
    PrototypeComboBox* activeBox = nullptr;
    int hoverRow = -1;
    float uiScale = 1.0f;
};

class PrototypeContextMenu final : public juce::Component
{
public:
    void showAt(juce::Point<int>, juce::Component& shell, float scale,
                PrototypeContextMenuModel);
    void hide();
    bool hitTest(int, int) override;
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    juce::Rectangle<int> getMainMenuBounds() const noexcept { return mainBounds; }
    juce::Rectangle<int> getSubmenuBounds() const noexcept { return submenuBounds; }
    bool isSubmenuVisible() const noexcept { return submenuVisible; }

private:
    friend int runEditorLayoutRegression();
    enum class HitKind { none, toggle, filter, route, saturation, saturationChoice, reset, bypass };
    struct Hit { HitKind kind = HitKind::none; int index = -1; };
    Hit itemAt(juce::Point<int>) const noexcept;
    juce::Rectangle<int> scaledRect(int x, int y, int width, int height,
                                    juce::Point<int> origin) const noexcept;
    PrototypeContextMenuModel model;
    juce::Rectangle<int> mainBounds;
    juce::Rectangle<int> submenuBounds;
    Hit hovered;
    float uiScale = 1.0f;
    bool submenuVisible = false;
};

class SettingsOverlay final : public juce::Component,
                              private juce::ChangeListener
{
public:
    struct State
    {
        int themeMode = 0;
        int gainRangeMode = 0;
        int fftSizeMode = 1;
        float rtaCeilingDb = 0.0f;
        float rtaFloorDb = -80.0f;
        float rtaAverageSeconds = 0.065f;
        float rtaSlopeDbPerOct = 4.5f;
        bool showHoverTooltip = true;
        juce::Colour lightBackground { 0xfff6f6f6 };
        juce::Colour lightForeground { 0xff050505 };
        juce::Colour darkBackground { 0xff050505 };
        juce::Colour darkForeground { 0xfff6f6f6 };
    };

    struct Statistics
    {
        SpectralStatistics spectrum;
        float crestDb = 0.0f;
        float correlation = 0.0f;
    };

    std::function<void(const State&)> onStateChange;
    SettingsOverlay();
    ~SettingsOverlay() override;
    void setState(State next);
    const State& getState() const noexcept { return state; }
    void setStatistics(Statistics next);
    void dismissColourEditor();
    void resetNavigation();
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    friend int runEditorLayoutRegression();
    enum class Cell { none, theme, gainRange, fft, hover, ceiling, floor, average, slope, shortcuts,
                      lightBackground, lightForeground, darkBackground, darkForeground };
    Cell cellAt(juce::Point<int>) const noexcept;
    void nudge(Cell, float amount);
    void notifyChanged();
    void showColourEditor(Cell);
    static juce::String gainRangeText(int mode);
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    State state;
    Statistics statistics;
    Cell draggedCell = Cell::none;
    int dragStartY = 0;
    State dragStartState;
    Cell editedColour = Cell::none;
    bool shortcutPageVisible = false;
    std::unique_ptr<juce::ColourSelector> colourSelector;
};

class DefaultEqualizerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                    private juce::Timer,
                                                    private juce::Slider::Listener
{
public:
    explicit DefaultEqualizerAudioProcessorEditor(DefaultEqualizerAudioProcessor&);
    ~DefaultEqualizerAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
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
    void updateMixedSelectionDisplays();
    bool selectedBandsHaveMixedValue(const juce::String& suffix) const;
    void updateAnalyzerLifecycle();
    void sliderValueChanged(juce::Slider*) override;
    void sliderDragStarted(juce::Slider*) override;
    void sliderDragEnded(juce::Slider*) override;
    juce::String groupSuffixForSlider(const juce::Slider*) const;
    void applyAbsoluteToSelectedBands(const juce::String& suffix, float value,
                                      bool includePrimary = true);
    void beginGroupSliderEdit(juce::Slider&);
    void endGroupSliderEdit(juce::Slider&);
    void toggleSelectMenu(PrototypeComboBox&);
    void hideSelectMenu();
    void hideContextMenu();
    void toggleSettingsOverlay();
    void hideSettingsOverlay();
    void applySettings(const SettingsOverlay::State&, bool persist);
    void applyThemeMode(int mode, bool persist);

    DefaultEqualizerAudioProcessor& proc;
    ResponseCurveComponent responseCurve;
    FamilyLookAndFeel familyLook;
    std::unique_ptr<juce::PropertiesFile> uiPreferences;
    bool darkTheme = false;
    int selectedBand = 0;
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
    juce::Point<int> lastLaidOutSize;

    friend int runEditorLayoutRegression();

    // Header: product identity, selected object, global actions and power.
    default_family::WordmarkButton themeBtn { "default_eq" };
    default_family::SmartGainButton autoGainBtn { "AUTO GAIN" };
    juce::ToggleButton powerBtn { "ON" };

    // Band page. Frequency/gain/Q intentionally live on the graph only.
    juce::ToggleButton bandOn { "ON" }, bandSolo { "SOLO" }, adaptiveQBtn { "ADAPTIVE Q" };
    PrototypeComboBox placementModeBox, typeBox;
    TwoAxisDragSlider placementSlider;
    NumericValueControl freqField { "FREQUENCY" }, gainField { "GAIN" },
                        qField { "Q" }, slopeField { "SLOPE" };

    // Dynamic page.
    ResettableSlider dynLookahead, dynRange, dynSpeed;
    NumericValueControl dynRatio { "RATIO" };
    ThresholdMeterSlider dynThreshold;
    juce::TextButton dynModeBtn { "DOWN" }, sidechainBtn { "IN SC" };

    // Drive controls share the Band workspace.
    ResettableSlider driveSlider;
    DriveCharacterSlider driveCharacterSlider;
    PrototypeComboBox saturationBox;

    // Global controls. Analyzer settings remain internal preferences.
    PrototypeComboBox oversamplingBox, phaseModeBox;
    NumericValueControl amountSlider { "AMOUNT" }, shiftSlider { "SHIFT" }, outputSlider { "OUT" };
    PrototypeSelectMenu selectMenu;
    PrototypeContextMenu contextMenu;
    SettingsOverlay settingsOverlay;
    int themeMode = default_family::ThemePreferences::automatic;
    int appliedFftSizeMode = -1;

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<ButtonAttachment> powerAtt, bandOnAtt, adaptiveQAtt;
    std::unique_ptr<ComboAttachment> typeAtt, placementModeAtt, saturationAtt, oversamplingAtt;
    std::unique_ptr<SliderAttachment> freqAtt, gainAtt, qAtt, slopeAtt, placementAtt,
        dynLookaheadAtt, dynThresholdAtt, dynRangeAtt,
        dynRatioAtt, dynSpeedAtt, driveAtt, driveCharacterAtt,
        amountAtt, shiftAtt, outputAtt;

    juce::TooltipWindow tooltipWindow { this, 450 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DefaultEqualizerAudioProcessorEditor)
};
