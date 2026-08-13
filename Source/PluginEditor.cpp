#include "PluginEditor.h"

namespace
{
juce::Font mono(float size, bool bold = false)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size,
        bold ? juce::Font::bold : juce::Font::plain));
}

juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}

double parseUnitValue(juce::String text, bool frequency = false, bool time = false)
{
    auto source = text.trim().toLowerCase().replaceCharacter(',', '.');
    double multiplier = 1.0;
    if (frequency && (source.contains("khz") || source.endsWithChar('k'))) multiplier = 1000.0;
    if (time && source.endsWithChar('s') && !source.endsWith("ms")) multiplier = 1000.0;
    const auto numeric = source.retainCharacters("-+0123456789.e");
    if (numeric.isEmpty()) return std::numeric_limits<double>::quiet_NaN();
    const double value = numeric.getDoubleValue() * multiplier;
    return std::isfinite(value) ? value : std::numeric_limits<double>::quiet_NaN();
}

juce::String cleanDb(double value, int digits = 2)
{
    if (std::abs(value) < std::pow(10.0, -digits) * 0.5) value = 0.0;
    return juce::String(value, digits) + " dB";
}

int workspaceHeightForWidth(int width) noexcept
{
    if (width < 820) return 84;
    if (width < 1080) return 92;
    return 100;
}
}

DefaultEqualizerAudioProcessorEditor::DefaultEqualizerAudioProcessorEditor(DefaultEqualizerAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), proc(p), responseCurve(p)
{
    juce::PropertiesFile::Options options;
    options.applicationName = "default_equalizer";
    options.filenameSuffix = "settings";
    options.folderName = "icanseesounds";
    options.osxLibrarySubFolder = "Application Support";
    uiPreferences = std::make_unique<juce::PropertiesFile>(options);
    darkTheme = !default_family::ThemePreferences::loadLight();
    familyLook.setDark(darkTheme);
    setLookAndFeel(&familyLook);
    responseCurve.setDarkMode(darkTheme);

    // Compact is an editor default, not a global preference inherited by every
    // new plug-in instance.
    workspaceExpanded = false;
    expandedWindowHeight = 486;
    setResizable(true, true);
    addMouseListener(this, true);
    const int initialWidth = 860;
    setResizeLimits(720, workspaceExpanded ? 296 + workspaceHeightForWidth(initialWidth) : 296, 1200, 900);
    const int collapsedHeight = 354;
    setSize(initialWidth, workspaceExpanded ? expandedWindowHeight : collapsedHeight);

    addAndMakeVisible(responseCurve);

    auto addButton = [this](auto& button) { addAndMakeVisible(button); };
    nextBrandGlitchTimeMs = juce::Time::getMillisecondCounterHiRes() + 4000.0;
    addButton(themeBtn); addButton(powerBtn); addButton(workspaceToggleBtn);
    addAndMakeVisible(pageRail);
    addButton(bandOn); addButton(bandSolo); addButton(adaptiveQBtn); addButton(placementModeBtn);
    addButton(sidechainAudition); addButton(driveOn); addButton(driveAutoGain);
    addButton(analyzerVisible); addButton(spectrumFreeze);
    addButton(analyzerPeakHold);
    addButton(decrampBtn);
    addButton(matchCaptureBtn); addButton(matchApplyBtn); addButton(matchCommitBtn); addButton(matchClearBtn);

    auto addCombo = [this](juce::ComboBox& box) { box.setJustificationType(juce::Justification::centred); addAndMakeVisible(box); };
    for (auto* type : { "BELL", "LOW SHELF", "HIGH SHELF", "LOW CUT", "HIGH CUT", "BAND PASS", "NOTCH" }) typeBox.addItem(type, typeBox.getNumItems() + 1);
    dynModeBox.addItemList({ "DOWN", "UP" }, 1);
    sidechainBox.addItemList({ "INT SC", "EXT SC" }, 1);
    saturationBox.addItemList({ "SOFT CLIP", "HARD CLIP", "DIODE CLIPPER", "TRIODE STAGE", "TRANSISTOR / FET",
                                "TAPE HYSTERESIS", "HARMONIC MORPH", "PHASE DISTORTION", "SPECTRAL CLIP", "SINE EROSION" }, 1);
    analyzerResolutionBox.addItemList({ "RTA: LOW", "RTA: MEDIUM", "RTA: HIGH" }, 1);
    phaseModeBox.addItemList({ "MIN PHASE", "LINEAR ECO", "LINEAR MED", "LINEAR HIGH" }, 1);
    addCombo(typeBox); addCombo(dynModeBox); addCombo(sidechainBox);
    addCombo(saturationBox); addCombo(analyzerResolutionBox); addCombo(phaseModeBox);
    addAndMakeVisible(autoGainBtn);

    for (auto* slider : { &slopeSlider, &dynThreshold, &dynRange, &dynRatio, &dynAttack,
                          &dynRelease, &driveSlider, &driveCharacterSlider,
                          &driveMixSlider, &driveOutputSlider, &outputSlider,
                          &matchAmount, &matchSmoothing, &matchLow, &matchHigh, &matchTime,
                          &analyzerRange, &analyzerFloor, &analyzerSpeed, &analyzerAveraging, &analyzerTilt })
        initParameter(*slider, {});
    oversamplingSlider.setName("OVERSAMPLING");
    oversamplingSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    oversamplingSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    oversamplingSlider.setRange(0.0, 3.0, 1.0);
    oversamplingSlider.setDoubleClickReturnValue(true, 0.0);
    addAndMakeVisible(oversamplingSlider);
    dynLookahead.setName("LOOKAHEAD");
    dynLookahead.setSliderStyle(juce::Slider::LinearHorizontal);
    dynLookahead.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    dynLookahead.setDoubleClickReturnValue(true, 0.0);
    addAndMakeVisible(dynLookahead);
    placementSlider.setName("PLACEMENT");
    placementSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    placementSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    placementSlider.setDoubleClickReturnValue(true, 0.0);
    addAndMakeVisible(placementSlider);
    slopeSlider.setName("SLOPE"); dynThreshold.setName("THRESHOLD"); dynRange.setName("RANGE");
    dynRatio.setName("RATIO"); dynAttack.setName("ATTACK"); dynRelease.setName("RELEASE");
    driveSlider.setName("DRIVE"); driveCharacterSlider.setName("CHARACTER");
    driveMixSlider.setName("MIX"); driveOutputSlider.setName("COMP");
    outputSlider.setName("OUTPUT");
    outputSlider.setName("OUTPUT_HDR");
    outputSlider.setSliderStyle(juce::Slider::LinearBarVertical);
    outputSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    outputSlider.setDoubleClickReturnValue(true, 0.0);
    matchAmount.setName("AMOUNT"); matchSmoothing.setName("SMOOTHING");
    matchLow.setName("LOW LIMIT"); matchHigh.setName("HIGH LIMIT");
    matchTime.setName("ANALYSIS");
    analyzerRange.setName("RANGE"); analyzerFloor.setName("FLOOR"); analyzerSpeed.setName("SPEED");
    analyzerAveraging.setName("AVERAGING"); analyzerTilt.setName("TILT");
    analyzerRange.setRange(30.0, 120.0, 1.0); analyzerFloor.setRange(-140.0, -30.0, 1.0);
    analyzerSpeed.setRange(0.0, 100.0, 1.0); analyzerAveraging.setRange(0.0, 100.0, 1.0);
    analyzerTilt.setRange(-6.0, 6.0, 0.1);
    analyzerRange.setValue(uiPreferences->getDoubleValue("analyzerRange", 90.0), juce::dontSendNotification);
    analyzerFloor.setValue(uiPreferences->getDoubleValue("analyzerFloor", -90.0), juce::dontSendNotification);
    analyzerSpeed.setValue(uiPreferences->getDoubleValue("analyzerSpeed", 35.0), juce::dontSendNotification);
    analyzerAveraging.setValue(uiPreferences->getDoubleValue("analyzerAveraging", 65.0), juce::dontSendNotification);
    analyzerTilt.setValue(uiPreferences->getDoubleValue("analyzerTilt", 0.0), juce::dontSendNotification);
    analyzerRange.setDoubleClickReturnValue(true, 90.0);
    analyzerFloor.setDoubleClickReturnValue(true, -90.0);
    analyzerSpeed.setDoubleClickReturnValue(true, 35.0);
    analyzerAveraging.setDoubleClickReturnValue(true, 65.0);
    analyzerTilt.setDoubleClickReturnValue(true, 0.0);
    analyzerRange.textFromValueFunction = [](double v) { return juce::String(v, 0) + " dB"; };
    analyzerFloor.textFromValueFunction = [](double v) { return cleanDb(v, 0); };
    analyzerSpeed.textFromValueFunction = analyzerAveraging.textFromValueFunction = [](double v) { return juce::String(v, 0) + "%"; };
    analyzerTilt.textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB/oct"; };
    matchAmount.setRange(0.0, 200.0, 1.0); matchSmoothing.setRange(0.0, 100.0, 1.0);
    matchLow.setRange(20.0, 2000.0, 1.0); matchHigh.setRange(2000.0, 20000.0, 1.0);
    matchTime.setRange(0.25, 5.0, 0.05);
    matchAmount.setValue(uiPreferences->getDoubleValue("matchAmount", 100.0), juce::dontSendNotification);
    matchSmoothing.setValue(uiPreferences->getDoubleValue("matchSmoothing", 35.0), juce::dontSendNotification);
    matchLow.setValue(uiPreferences->getDoubleValue("matchLow", 20.0), juce::dontSendNotification);
    matchHigh.setValue(uiPreferences->getDoubleValue("matchHigh", 20000.0), juce::dontSendNotification);
    matchTime.setValue(uiPreferences->getDoubleValue("matchTime", 1.0), juce::dontSendNotification);
    matchAmount.setDoubleClickReturnValue(true, 100.0);
    matchSmoothing.setDoubleClickReturnValue(true, 35.0);
    matchLow.setDoubleClickReturnValue(true, 20.0);
    matchHigh.setDoubleClickReturnValue(true, 20000.0);
    matchTime.setDoubleClickReturnValue(true, 1.0);
    matchAmount.textFromValueFunction = matchSmoothing.textFromValueFunction = [](double v) { return juce::String(v, 0) + "%"; };
    matchLow.textFromValueFunction = matchHigh.textFromValueFunction = [](double v) { return v >= 1000.0 ? juce::String(v / 1000.0, 2) + " kHz" : juce::String(v, 0) + " Hz"; };
    matchTime.textFromValueFunction = [](double v) { return juce::String(v, 2) + " s"; };
    matchLow.valueFromTextFunction = matchHigh.valueFromTextFunction = [](const juce::String& s) { return parseUnitValue(s, true); };
    matchAmount.onValueChange = [this] { uiPreferences->setValue("matchAmount", matchAmount.getValue()); };
    matchSmoothing.onValueChange = [this] { uiPreferences->setValue("matchSmoothing", matchSmoothing.getValue()); };
    matchLow.onValueChange = [this] { uiPreferences->setValue("matchLow", matchLow.getValue()); };
    matchHigh.onValueChange = [this] { uiPreferences->setValue("matchHigh", matchHigh.getValue()); };
    matchTime.onValueChange = [this] { uiPreferences->setValue("matchTime", matchTime.getValue()); };

    slopeSlider.textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB/oct"; };
    slopeSlider.valueFromTextFunction = [](const juce::String& s) { return parseUnitValue(s); };
    dynThreshold.textFromValueFunction = [](double v) { return cleanDb(v, 1); };
    dynRange.textFromValueFunction = [](double v) { return cleanDb(v, 1); };
    dynRatio.textFromValueFunction = [](double v) { return juce::String(v, 2) + ":1"; };
    dynAttack.textFromValueFunction = [](double v) { return juce::String(v, 1) + " ms"; };
    dynRelease.textFromValueFunction = [](double v) { return v >= 1000.0 ? juce::String(v / 1000.0, 2) + " s" : juce::String(v, 0) + " ms"; };
    dynAttack.valueFromTextFunction = dynRelease.valueFromTextFunction = [](const juce::String& s) { return parseUnitValue(s, false, true); };
    driveSlider.textFromValueFunction = [](double v) { return cleanDb(v, 1); };
    driveMixSlider.textFromValueFunction = [](double v) { return juce::String(v, 0) + "%"; };
    driveOutputSlider.textFromValueFunction = outputSlider.textFromValueFunction = [](double v) { return cleanDb(v); };
    applySliderPalette();

    themeBtn.setTooltip("Toggle exact paper/ink inversion");
    dynModeBox.setTooltip("Downward or upward dynamic EQ");
    sidechainBox.setTooltip("Internal frequency-filtered detector or host sidechain");
    dynLookahead.setTooltip("Click or drag from 0 to 5 ms; latency is reported to the host");
    sidechainAudition.setTooltip("Momentarily listen to this band's filtered detector signal");
    themeBtn.onClick = [this]
    {
        darkTheme = !darkTheme;
        familyLook.setDark(darkTheme); responseCurve.setDarkMode(darkTheme);
        applySliderPalette();
        default_family::ThemePreferences::saveLight(!darkTheme);
        sendLookAndFeelChange(); repaint();
    };
    autoGainBtn.onClick = [this]
    {
        if (auto* parameter = proc.apvts.getParameter("auto_gain_mode"))
        {
            const int current = (int)proc.apvts.getRawParameterValue("auto_gain_mode")->load();
            const int next = (current + 1) % 3;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1((float)next));
            parameter->endChangeGesture();
        }
    };
    pageRail.onPageChange = [this](int page)
    {
        setWorkspacePage(static_cast<WorkspacePage>(juce::jlimit(0, 3, page)));
    };
    workspaceToggleBtn.onClick = [this] { setWorkspaceExpanded(!workspaceExpanded, true); };
    phaseModeBox.onChange = [this]
    {
        const int selected = phaseModeBox.getSelectedItemIndex();
        if (auto* enabled = proc.apvts.getParameter("linear_phase"))
            enabled->setValueNotifyingHost(enabled->convertTo0to1(selected > 0 ? 1.0f : 0.0f));
        if (selected > 0)
            if (auto* quality = proc.apvts.getParameter("linear_quality"))
                quality->setValueNotifyingHost(quality->convertTo0to1((float)(selected - 1)));
    };
    saturationBox.onChange = [this]
    {
        const int requested = juce::jlimit(0, 9, saturationBox.getSelectedItemIndex());
        const bool userChangedMode = saturationMouseInteraction
            && displayedDriveMode >= 0 && requested != displayedDriveMode;
        saturationMouseInteraction = false;
        displayedDriveMode = requested;
        updateDriveControls(userChangedMode);
    };
    sidechainAudition.setClickingTogglesState(true);
    sidechainAudition.onClick = [this]
    {
        proc.sidechainAuditionBand.store(sidechainAudition.getToggleState() ? selectedBand : -1,
                                         std::memory_order_release);
    };
    bandSolo.setClickingTogglesState(true);
    bandSolo.onClick = [this]
    {
        proc.soloBand.store(bandSolo.getToggleState() ? selectedBand : -1, std::memory_order_release);
    };
    matchCaptureBtn.onClick = [this]
    {
        proc.matchEQ.setAnalysisSeconds(matchTime.getValue(), proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0);
        if (proc.matchEQ.isCapturing()) proc.matchEQ.stopCapture();
        else proc.matchEQ.startCapture();
    };
    matchApplyBtn.onClick = [this]
    {
        proc.matchEQ.setAnalysisSeconds(matchTime.getValue(), proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0);
        proc.matchEQ.setMatchActive(true);
    };
    matchCommitBtn.onClick = [this] { applyMatchToBands(); };
    matchClearBtn.onClick = [this] { proc.matchEQ.clear(); };

    analyzerVisible.setClickingTogglesState(true);
    analyzerVisible.setToggleState(uiPreferences->getBoolValue("analyzerVisible", true), juce::dontSendNotification);
    analyzerVisible.onClick = [this]
    {
        uiPreferences->setValue("analyzerVisible", analyzerVisible.getToggleState());
        responseCurve.setAnalyzerVisible(analyzerVisible.getToggleState());
        updateAnalyzerLifecycle();
    };
    responseCurve.setAnalyzerVisible(analyzerVisible.getToggleState());
    updateAnalyzerLifecycle();
    responseCurve.setAnalyzerSources(true, true);
    spectrumFreeze.setClickingTogglesState(true);
    spectrumFreeze.onClick = [this]
    {
        responseCurve.setSpectrumFrozen(spectrumFreeze.getToggleState());
        updateAnalyzerLifecycle();
    };
    analyzerPeakHold.setClickingTogglesState(true);
    analyzerPeakHold.setToggleState(uiPreferences->getBoolValue("analyzerPeakHold", false), juce::dontSendNotification);
    if (!uiPreferences->getBoolValue("analyzerResolutionV2", false))
    {
        if (uiPreferences->containsKey("analyzerResolution"))
        {
            const int legacy = uiPreferences->getIntValue("analyzerResolution", 3);
            uiPreferences->setValue("analyzerResolution", legacy == 3 ? 2 : std::max(1, legacy - 1));
        }
        else
        {
            // Fresh installs start at the most detailed analyzer mode.
            uiPreferences->setValue("analyzerResolution", 3);
        }
        uiPreferences->setValue("analyzerResolutionV2", true);
    }
    // One-time preference migration for the new product default. Afterwards
    // the user's explicit analyzer choice remains persistent as before.
    if (!uiPreferences->getBoolValue("analyzerResolutionV3", false))
    {
        uiPreferences->setValue("analyzerResolution", 3);
        uiPreferences->setValue("analyzerResolutionV3", true);
    }
    analyzerResolutionBox.setSelectedId(uiPreferences->getIntValue("analyzerResolution", 3), juce::dontSendNotification);
    const auto analyzerChanged = [this] { updateAnalyzerSettings(); };
    analyzerPeakHold.onClick = analyzerChanged;
    analyzerResolutionBox.onChange = analyzerChanged;
    analyzerRange.onValueChange = analyzerChanged; analyzerFloor.onValueChange = analyzerChanged;
    analyzerSpeed.onValueChange = analyzerChanged; analyzerAveraging.onValueChange = analyzerChanged;
    analyzerTilt.onValueChange = analyzerChanged;
    updateAnalyzerSettings();

    powerAtt = std::make_unique<ButtonAttachment>(proc.apvts, "plugin_enabled", powerBtn);
    adaptiveQAtt = std::make_unique<ButtonAttachment>(proc.apvts, "adaptive_q", adaptiveQBtn);
    linearPhaseAtt = std::make_unique<ButtonAttachment>(proc.apvts, "linear_phase", linearPhaseBtn);
    decrampAtt = std::make_unique<ButtonAttachment>(proc.apvts, "decramp", decrampBtn);
    oversamplingAtt = std::make_unique<SliderAttachment>(proc.apvts, "oversampling", oversamplingSlider);
    outputAtt = std::make_unique<SliderAttachment>(proc.apvts, "output_gain", outputSlider);

    const int initialAutoMode = (int)proc.apvts.getRawParameterValue("auto_gain_mode")->load();
    autoGainBtn.setButtonText(initialAutoMode == 0 ? "AUTO GAIN: OFF"
                              : initialAutoMode == 1 ? "AUTO GAIN" : "SMART AUTO GAIN");
    autoGainBtn.setToggleState(initialAutoMode > 0, juce::dontSendNotification);

    setWantsKeyboardFocus(true);
    selectBand(0);
    setWorkspacePage(WorkspacePage::Band);
    setWorkspaceExpanded(workspaceExpanded, false);
    applySliderPalette();
    sendLookAndFeelChange();
    repaint();
    startTimerHz(30);
}

DefaultEqualizerAudioProcessorEditor::~DefaultEqualizerAudioProcessorEditor()
{
    removeMouseListener(this);
    proc.sidechainAuditionBand.store(-1, std::memory_order_release);
    proc.soloBand.store(-1, std::memory_order_release);
    proc.setAnalyzerEnabled(false);
    if (uiPreferences)
    {
        uiPreferences->removeValue("windowWidth");
        uiPreferences->removeValue("windowHeight");
        uiPreferences->removeValue("workspaceExpanded");
        uiPreferences->saveIfNeeded();
    }
    setLookAndFeel(nullptr);
}

void DefaultEqualizerAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    if (event.originalComponent == &saturationBox || saturationBox.isParentOf(event.originalComponent))
        saturationMouseInteraction = true;
    if (!responseCurve.isNumericEditorComponent(event.originalComponent))
        responseCurve.dismissNumericEditor();
}

void DefaultEqualizerAudioProcessorEditor::visibilityChanged()
{
    updateAnalyzerLifecycle();
}

void DefaultEqualizerAudioProcessorEditor::updateAnalyzerLifecycle()
{
    proc.setAnalyzerEnabled(isShowing() && analyzerVisible.getToggleState()
                            && !spectrumFreeze.getToggleState());
}

void DefaultEqualizerAudioProcessorEditor::initParameter(juce::Slider& slider, const juce::String& name)
{
    slider.setName(name);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 16);
    slider.setDoubleClickReturnValue(false, 0.0);
    addAndMakeVisible(slider);
}

void DefaultEqualizerAudioProcessorEditor::applySliderPalette()
{
    const auto fg = familyLook.foreground();
    const auto bg = familyLook.background();
    for (auto* slider : { &slopeSlider, &placementSlider, &dynThreshold, &dynRange, &dynRatio, &dynAttack,
                          &dynRelease, &dynLookahead, &driveSlider, &driveCharacterSlider,
                          &driveMixSlider,
                          &driveOutputSlider, &outputSlider, &matchAmount, &matchSmoothing,
                          &matchLow, &matchHigh, &matchTime, &analyzerRange, &analyzerFloor,
                          &analyzerSpeed, &analyzerAveraging, &analyzerTilt, &oversamplingSlider })
    {
        slider->setColour(juce::Slider::textBoxTextColourId, fg);
        slider->setColour(juce::Slider::textBoxBackgroundColourId, bg);
        slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider->updateText();
    }
}

void DefaultEqualizerAudioProcessorEditor::rebindBandControls()
{
    bandOnAtt.reset(); typeAtt.reset(); placementModeAtt.reset(); placementAtt.reset(); slopeAtt.reset();
    dynLookaheadAtt.reset(); dynModeAtt.reset(); sidechainAtt.reset(); dynThresholdAtt.reset(); dynRangeAtt.reset();
    dynRatioAtt.reset(); dynAttackAtt.reset(); dynReleaseAtt.reset(); driveOnAtt.reset(); driveAtt.reset();
    driveCharacterAtt.reset(); driveMixAtt.reset();
    driveOutputAtt.reset(); driveAutoGainAtt.reset(); saturationAtt.reset();
    const int idx = selectedBand + 1;
    bandOnAtt = std::make_unique<ButtonAttachment>(proc.apvts, bandId(idx, "on"), bandOn);
    typeAtt = std::make_unique<ComboAttachment>(proc.apvts, bandId(idx, "type"), typeBox);
    placementModeAtt = std::make_unique<ButtonAttachment>(proc.apvts, bandId(idx, "placement_mode"), placementModeBtn);
    placementAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "placement"), placementSlider);
    slopeAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "slope"), slopeSlider);
    dynLookaheadAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_lookahead"), dynLookahead);
    dynModeAtt = std::make_unique<ComboAttachment>(proc.apvts, bandId(idx, "dyn_mode"), dynModeBox);
    sidechainAtt = std::make_unique<ComboAttachment>(proc.apvts, bandId(idx, "sc_source"), sidechainBox);
    dynThresholdAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_thresh"), dynThreshold);
    dynRangeAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_range"), dynRange);
    dynRatioAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_ratio"), dynRatio);
    dynAttackAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_attack"), dynAttack);
    dynReleaseAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_release"), dynRelease);
    driveOnAtt = std::make_unique<ButtonAttachment>(proc.apvts, bandId(idx, "drive_on"), driveOn);
    driveAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "drive"), driveSlider);
    driveCharacterAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "drive_character"), driveCharacterSlider);
    driveMixAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "drive_mix"), driveMixSlider);
    driveOutputAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "drive_output"), driveOutputSlider);
    driveAutoGainAtt = std::make_unique<ButtonAttachment>(proc.apvts, bandId(idx, "drive_auto_gain"), driveAutoGain);
    displayedDriveMode = std::clamp((int)proc.apvts.getRawParameterValue(bandId(idx, "sat_mode"))->load(), 0, 9);
    saturationAtt = std::make_unique<ComboAttachment>(proc.apvts, bandId(idx, "sat_mode"), saturationBox);
    updateDriveControls(false);
    driveFormatPending = true;
}

void DefaultEqualizerAudioProcessorEditor::updateDriveControls(bool resetModeDefaults)
{
    static constexpr const char* characterNames[] { "CURVE", "SOFTNESS", "TOPOLOGY", "BIAS", "GATE",
                                                     "HYSTERESIS", "EVEN / ODD", "TONE", "KNEE", "FREQUENCY" };
    const int mode = juce::jlimit(0, 9, displayedDriveMode);
    const bool bipolarCharacter = mode == 3 || mode == 4 || mode == 6;
    driveCharacterSlider.setName(characterNames[mode]);
    driveCharacterSlider.setRange(bipolarCharacter ? -1.0 : 0.0, 1.0, 0.001);
    driveSlider.textFromValueFunction = [](double value) { return cleanDb(value, 1); };
    driveMixSlider.textFromValueFunction = [](double value) { return juce::String(value, 0) + "%"; };
    driveOutputSlider.textFromValueFunction = [](double value) { return cleanDb(value); };
    driveCharacterSlider.textFromValueFunction = [mode, bipolarCharacter](double raw)
    {
        if (mode == 9)
        {
            const double unit = juce::jlimit(0.0, 1.0, raw);
            const double hz = unit <= 0.5 ? 1000.0 * std::pow(2.0 * unit, 2.0)
                                          : 1000.0 * std::pow(10.0, 2.0 * unit - 1.0);
            return hz >= 1000.0 ? juce::String(hz / 1000.0, hz < 10000.0 ? 2 : 1) + " kHz"
                                : juce::String(hz, 0) + " Hz";
        }
        double shown = bipolarCharacter ? raw * 100.0 : juce::jmax(0.0, raw) * 100.0;
        if (std::abs(shown) < 0.005) shown = 0.0;
        return juce::String(shown, 0) + "%";
    };
    driveSlider.updateText();
    driveCharacterSlider.updateText();
    driveMixSlider.updateText();
    driveOutputSlider.updateText();

    const bool tape = mode == 5, sine = mode == 9;

    if (resetModeDefaults)
    {
        proc.undoManager.beginNewTransaction("Change drive algorithm");
        const float characterDefault = (tape || mode == 7 || sine) ? 0.5f : 0.0f;
        const float secondaryDefault = tape ? 0.5f : 0.0f;
        for (const auto& change : { std::pair<const char*, float>{ "drive_character", characterDefault },
                                    { "drive_secondary", secondaryDefault } })
            if (auto* parameter = proc.apvts.getParameter(bandId(selectedBand + 1, change.first)))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->convertTo0to1(change.second));
                parameter->endChangeGesture();
            }
    }
    resized();
    repaint();
}

void DefaultEqualizerAudioProcessorEditor::selectBand(int band, bool updateGraphSelection)
{
    selectedBand = juce::jlimit(0, kNumBands - 1, band);
    sidechainAudition.setToggleState(proc.sidechainAuditionBand.load(std::memory_order_acquire) == selectedBand,
                                     juce::dontSendNotification);
    bandSolo.setToggleState(proc.soloBand.load(std::memory_order_acquire) == selectedBand,
                            juce::dontSendNotification);
    if (updateGraphSelection) responseCurve.setSelectedBand(selectedBand);
    rebindBandControls();
    updateHeaderText();
    if ((int)proc.apvts.getRawParameterValue("auto_gain_mode")->load() == 2)
        autoGainBtn.setTooltip(proc.smartAutoGainLocked.load(std::memory_order_acquire) ? "Smart Auto Gain: locked" : "Smart Auto Gain: analysing");
}

void DefaultEqualizerAudioProcessorEditor::setWorkspacePage(WorkspacePage page)
{
    workspacePage = page;
    pageRail.setSelectedPage(static_cast<int>(page), juce::dontSendNotification);

    const bool band = workspaceExpanded && page == WorkspacePage::Band;
    const bool dyn = workspaceExpanded && page == WorkspacePage::Dynamic;
    const bool analyzer = workspaceExpanded && page == WorkspacePage::Analyzer;
    const bool match = workspaceExpanded && page == WorkspacePage::Match;
    const std::array<juce::Component*, 12> bandComponents { &bandOn, &bandSolo,
        &placementModeBtn, &placementSlider, &typeBox, &slopeSlider, &driveOn, &driveAutoGain,
        &driveSlider, &driveCharacterSlider, &driveMixSlider, &driveOutputSlider };
    const std::array<juce::Component*, 9> dynComponents { &dynLookahead, &sidechainAudition, &dynModeBox, &sidechainBox, &dynThreshold,
                                                          &dynRange, &dynRatio, &dynAttack, &dynRelease };
    const std::array<juce::Component*, 9> analyzerComponents { &analyzerVisible, &spectrumFreeze, &analyzerPeakHold,
        &analyzerResolutionBox, &analyzerRange, &analyzerFloor, &analyzerSpeed, &analyzerAveraging, &analyzerTilt };
    const std::array<juce::Component*, 9> matchComponents { &matchCaptureBtn, &matchApplyBtn, &matchCommitBtn, &matchClearBtn,
        &matchAmount, &matchSmoothing, &matchLow, &matchHigh, &matchTime };
    for (auto* component : bandComponents) component->setVisible(band);
    saturationBox.setVisible(band);
    for (auto* component : dynComponents) component->setVisible(dyn);
    for (auto* component : analyzerComponents) component->setVisible(analyzer);
    for (auto* component : { static_cast<juce::Component*>(&phaseModeBox), static_cast<juce::Component*>(&decrampBtn),
                             static_cast<juce::Component*>(&oversamplingSlider), static_cast<juce::Component*>(&outputSlider),
                             static_cast<juce::Component*>(&adaptiveQBtn) })
        component->setVisible(true);
    for (auto* component : matchComponents) component->setVisible(match);
    pageRail.setVisible(workspaceExpanded);
    resized(); repaint();
}

void DefaultEqualizerAudioProcessorEditor::setWorkspaceExpanded(bool shouldExpand, bool resizeWindow)
{
    const int panelHeight = workspaceHeightForWidth(getWidth());
    if (workspaceExpanded && !shouldExpand)
        expandedWindowHeight = getHeight();
    const int currentHeight = getHeight();
    workspaceExpanded = shouldExpand;
    workspaceToggleBtn.setButtonText(workspaceExpanded ? "HIDE ADVANCED" : "ADVANCED");
    workspaceToggleBtn.setToggleState(workspaceExpanded, juce::dontSendNotification);
    if (resizeWindow)
    {
        setResizeLimits(720, workspaceExpanded ? 296 + panelHeight : 296, 1200, 900);
        const int target = workspaceExpanded
            ? juce::jlimit(296 + panelHeight, 900, currentHeight + panelHeight)
            : juce::jlimit(296, 900 - panelHeight, currentHeight - panelHeight);
        setSize(getWidth(), target);
    }
    setWorkspacePage(workspacePage);
}

void DefaultEqualizerAudioProcessorEditor::updateHeaderText()
{
    powerBtn.setButtonText(powerBtn.getToggleState() ? "ON" : "OFF");
}

bool DefaultEqualizerAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() && key.getKeyCode() == 'Z')
    {
        if (mods.isShiftDown()) proc.undoManager.redo();
        else                    proc.undoManager.undo();
        return true;
    }
    return juce::AudioProcessorEditor::keyPressed(key);
}

void DefaultEqualizerAudioProcessorEditor::timerCallback()
{
    // Hosts can attach an already-visible editor to a native window without a
    // JUCE visibilityChanged() callback. Reconcile the cheap atomic analyzer
    // gate here as well, so reopening the editor cannot leave Spectrum visibly
    // enabled while the audio-side producer remains stopped.
    updateAnalyzerLifecycle();

    const bool sharedDark = !default_family::ThemePreferences::loadLight();
    if (sharedDark != darkTheme)
    {
        darkTheme = sharedDark;
        familyLook.setDark(darkTheme); responseCurve.setDarkMode(darkTheme);
        applySliderPalette();
        sendLookAndFeelChange(); repaint();
    }
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    if (!uiPreferences->getBoolValue("reducedMotion", false) && nowMs >= nextBrandGlitchTimeMs)
    {
        static const juce::String original { "default_equalizer" };
        auto animated = original;
        juce::Array<int> positions;
        for (int index = 0; index < original.length(); ++index)
            if (original[index] != '_') positions.add(index);
        for (int index = positions.size() - 1; index > 0; --index)
            positions.swap(index, brandRandom.nextInt(index + 1));
        const int count = 1 + brandRandom.nextInt(positions.size());
        for (int index = 0; index < count; ++index)
            if (brandRandom.nextBool())
                animated = animated.replaceSection(positions[index], 1,
                    juce::String::charToString((juce::juce_wchar)(33 + brandRandom.nextInt(94))));
        themeBtn.setButtonText(animated);
        nextBrandGlitchTimeMs = nowMs + 4000.0;
    }
    const double sampleRate = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    if (proc.preSpectrumFifo.processIfReady())
        responseCurve.pushSpectrumData(proc.preSpectrumFifo.getMagnitudes(), proc.preSpectrumFifo.getNumBins(), sampleRate, true);
    if (proc.spectrumFifo.processIfReady())
        responseCurve.pushSpectrumData(proc.spectrumFifo.getMagnitudes(), proc.spectrumFifo.getNumBins(), sampleRate, false);
    const int curveSelection = responseCurve.getSelectedBand();
    if (curveSelection >= 0 && curveSelection != selectedBand) selectBand(curveSelection, false);
    bandSolo.setToggleState(proc.soloBand.load(std::memory_order_acquire) == selectedBand,
                            juce::dontSendNotification);
    const int autoMode = (int)proc.apvts.getRawParameterValue("auto_gain_mode")->load();
    const bool smartSelected = autoMode == 2;
    const bool smartLocked = proc.smartAutoGainLocked.load(std::memory_order_acquire);
    autoGainBtn.setButtonText(autoMode == 0 ? "AUTO GAIN: OFF" : autoMode == 1 ? "AUTO GAIN"
                                                                    : "SMART AUTO GAIN");
    autoGainBtn.setToggleState(autoMode > 0, juce::dontSendNotification);
    autoGainBtn.setLoadingState(proc.smartAutoGainProgress.load(std::memory_order_relaxed),
        smartSelected && !smartLocked, uiPreferences->getBoolValue("reducedMotion", false));
    if (smartSelected)
        autoGainBtn.setTooltip(smartLocked ? "Smart Auto Gain: locked"
                                           : "Smart Auto Gain: analysing");
    else
        autoGainBtn.setTooltip("Cycle Off / Regular / Smart Auto Gain");
    autoGainBtn.repaint();
    matchCaptureBtn.setButtonText(proc.matchEQ.isCapturing() ? "LEARNING TARGET" : proc.matchEQ.hasCapture() ? "TARGET READY" : "LEARN TARGET");
    matchCaptureBtn.setToggleState(proc.matchEQ.isCapturing() || proc.matchEQ.hasCapture(), juce::dontSendNotification);
    matchCaptureBtn.getProperties().set("progress", proc.matchEQ.isCapturing() ? proc.matchEQ.getCaptureProgress() : 0.0f);
    matchApplyBtn.setEnabled(proc.matchEQ.hasCapture() && !proc.matchEQ.isCapturing());
    matchApplyBtn.setButtonText(proc.matchEQ.isAnalyzing() ? "ANALYSING INPUT" : proc.matchEQ.isMatchActive() ? "PREVIEW READY" : "ANALYZE INPUT");
    matchApplyBtn.setToggleState(proc.matchEQ.isAnalyzing() || proc.matchEQ.isMatchActive(), juce::dontSendNotification);
    matchApplyBtn.getProperties().set("progress", proc.matchEQ.isAnalyzing() ? proc.matchEQ.getAnalysisProgress() : 0.0f);
    matchCommitBtn.setEnabled(proc.matchEQ.isMatchActive());
    const bool linear = proc.apvts.getRawParameterValue("linear_phase")->load() > 0.5f;
    const int quality = (int)proc.apvts.getRawParameterValue("linear_quality")->load();
    const int phaseId = linear ? std::clamp(quality + 2, 2, 4) : 1;
    if (phaseModeBox.getSelectedId() != phaseId)
        phaseModeBox.setSelectedId(phaseId, juce::dontSendNotification);
    const int driveMode = std::clamp((int)proc.apvts.getRawParameterValue(bandId(selectedBand + 1, "sat_mode"))->load(), 0, 9);
    if (driveMode != displayedDriveMode)
    {
        displayedDriveMode = driveMode;
        updateDriveControls(false);
    }
    if (driveFormatPending)
    {
        driveFormatPending = false;
        updateDriveControls(false);
    }
    const bool midSide = proc.apvts.getRawParameterValue(bandId(selectedBand + 1, "placement_mode"))->load() > 0.5f;
    placementModeBtn.setButtonText(midSide ? "M/S" : "L/R");
    placementSlider.getProperties().set("midSide", midSide);
    placementSlider.repaint();
    updateHeaderText();
}

void DefaultEqualizerAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto fg = familyLook.foreground(), bg = familyLook.background();
    const int headerH = juce::jlimit(56, 70, juce::roundToInt(64.0f * getWidth() / 860.0f));
    const int workspaceH = workspaceExpanded ? workspaceHeightForWidth(getWidth()) : 0;
    g.fillAll(fg);
    g.setColour(bg); g.fillRect(0, 0, getWidth(), headerH);
    const int seamX = juce::roundToInt((float)default_family::metrics::wordmarkWidth
                                      * (float)getWidth() / (float)default_family::metrics::designWidth);
    const int seam = juce::jmax(18, headerH * 3 / 8);
    g.setColour(fg); g.fillRect(seamX, 0, seam, seam); g.fillRect(seamX, headerH - seam, seam, seam);
    if (workspaceExpanded)
    {
        g.setColour(bg);
        g.fillRect(0, getHeight() - workspaceH, getWidth(), workspaceH);
    }

    g.setFont(mono(9.0f, true));
    g.setColour(fg.withAlpha(0.55f));
    const std::array<juce::Slider*, 21> sliders { &slopeSlider, &dynThreshold, &dynRange, &dynRatio, &dynAttack,
                                                  &dynRelease, &driveSlider, &driveCharacterSlider,
                                                  &driveMixSlider, &driveOutputSlider, &outputSlider,
                                                  &matchAmount, &matchSmoothing, &matchLow, &matchHigh, &matchTime,
                                                  &analyzerRange, &analyzerFloor, &analyzerSpeed, &analyzerAveraging, &analyzerTilt };
    for (auto* slider : sliders)
        if (slider->isVisible() && slider != &outputSlider)
        {
            g.drawText(slider->getName(), slider->getX(), slider->getY() - 12,
                       slider->getWidth(), 11,
                       juce::Justification::centred);
        }
}

void DefaultEqualizerAudioProcessorEditor::resized()
{
    const int w = getWidth(), h = getHeight();
    familyLook.setUiScale(juce::jlimit(0.90f, 1.25f,
        (float)w / (float)default_family::metrics::designWidth));
    const int headerH = juce::jlimit(56, 70, juce::roundToInt(64.0f * w / 860.0f));
    const int workspaceH = workspaceExpanded ? workspaceHeightForWidth(w) : 0;
    const int wordW = juce::roundToInt((float)default_family::metrics::wordmarkWidth
                                      * (float)w / (float)default_family::metrics::designWidth);
    const int seamW = juce::jmax(18, headerH * 3 / 8);
    themeBtn.setBounds(0, 0, wordW, headerH);

    const int actionH = 40;
    const int actionY = (headerH - actionH) / 2;
    const int powerW = w < 820 ? 68 : 76;
    const int powerX = w - powerW - 8;
    powerBtn.setBounds(powerX, actionY, powerW, actionH);

    const int autoW = w < 820 ? 120 : 152;
    const int outW = w < 820 ? 60 : 68;
    const int sectionGap = 4;
    const int autoX = powerX - 4 - autoW;
    const int outputX = autoX - sectionGap - outW;
    outputSlider.setBounds(outputX, actionY, outW, actionH);
    autoGainBtn.setBounds(autoX, actionY, autoW, actionH);

    const int globalStart = wordW + seamW + 8;
    const int globalEnd = outputX - 8;
    const int globalGap = 4;
    const int availableGlobal = juce::jmax(190, globalEnd - globalStart);
    const int phaseW = availableGlobal < 230 ? 84 : 108;
    const int osW = availableGlobal < 230 ? 68 : 76;
    const int decrampW = availableGlobal < 230 ? 64 : 72;
    const int globalTotal = phaseW + osW + decrampW + globalGap * 2;
    int globalX = globalStart + juce::jmax(0, (availableGlobal - globalTotal) / 2);
    phaseModeBox.setBounds(globalX, actionY, phaseW, actionH); globalX += phaseW + globalGap;
    oversamplingSlider.setBounds(globalX, actionY, osW, actionH); globalX += osW + globalGap;
    decrampBtn.setBounds(globalX, actionY, decrampW, actionH);

    // default_distortion leaves a four-pixel structural frame around its
    // response surface at the 860 px design size.
    const int graphFrame = juce::jmax(3, juce::roundToInt(4.0f * familyLook.getUiScale()));
    const int graphTop = headerH + graphFrame;
    const int toggleH = 28;
    const int toggleY = h - workspaceH - toggleH - 8;
    adaptiveQBtn.setBounds(graphFrame, toggleY, 128, toggleH);
    workspaceToggleBtn.setBounds(w - graphFrame - 176, toggleY, 176, toggleH);
    const int graphBottom = toggleY - graphFrame;
    responseCurve.setBounds(graphFrame, graphTop, w - graphFrame * 2,
                            juce::jmax(160, graphBottom - graphTop));

    if (!workspaceExpanded)
        return;

    const int workspaceY = h - workspaceH;
    const int rowH = w < 820 ? 24 : w < 1080 ? 28 : 32;
    const int gap = 4;
    const int pairH = rowH * 2 + gap;
    // JUCE reserves the exact 16 px text-box height below the rotary area.
    // Therefore a pairH-wide component with pairH+16 height produces a visible
    // square exactly as tall as the neighbouring two-row stack.
    const int knobSide = pairH;
    const int knobComponentH = knobSide + 16;
    const int pairY = workspaceY + 12;
    // Band defines the horizontal grid for every page. At the narrow 720 px
    // limit its text columns contract, while the five knob squares retain the
    // exact shared size and positions used by Dynamic, RTA and Match.
    const int bandOnW = w < 820 ? 40 : w < 1080 ? 48 : 52;
    const int bandSelectorW = w < 820 ? 120 : w < 1080 ? 156 : 172;
    const int bandPlaceW = w < 820 ? 68 : w < 1080 ? 84 : 96;
    const int bandDriveButtonW = w < 820 ? 64 : w < 1080 ? 76 : 84;
    const int bandContentWidth = bandOnW + bandSelectorW + bandPlaceW + bandDriveButtonW
        + knobSide * 5 + gap * 8;
    const int railX = workspaceToggleBtn.getBounds().getCentreX() - knobSide / 2;
    pageRail.setBounds(railX, pairY, knobSide, pairH);

    // Equal whitespace on both sides of the Band row: screen edge -> first
    // button equals final knob -> Page Rail.
    const int bandStartX = juce::jmax(0, (pageRail.getX() - bandContentWidth) / 2);
    const int bandFixedWidth = bandOnW + bandSelectorW + bandPlaceW + bandDriveButtonW;
    const int firstBandKnobX = bandStartX + bandFixedWidth + gap * 4;
    const std::array<int, 5> sharedKnobX { firstBandKnobX,
        firstBandKnobX + (knobSide + gap),
        firstBandKnobX + (knobSide + gap) * 2,
        firstBandKnobX + (knobSide + gap) * 3,
        firstBandKnobX + (knobSide + gap) * 4 };

    const auto stack = [rowH, gap](juce::Component& top, juce::Component& bottom,
                                   int x, int y, int width)
    {
        top.setBounds(x, y, width, rowH);
        bottom.setBounds(x, y + rowH + gap, width, rowH);
    };

    if (workspacePage == WorkspacePage::Band)
    {
        int x = bandStartX;
        stack(bandOn, bandSolo, x, pairY, bandOnW); x += bandOnW + gap;
        stack(typeBox, saturationBox, x, pairY, bandSelectorW); x += bandSelectorW + gap;
        stack(placementModeBtn, placementSlider, x, pairY, bandPlaceW); x += bandPlaceW + gap;
        stack(driveOn, driveAutoGain, x, pairY, bandDriveButtonW); x += bandDriveButtonW + gap;
        const int knobY = pairY;
        const std::array<juce::Slider*, 5> bandKnobs { &slopeSlider, &driveSlider,
            &driveCharacterSlider, &driveMixSlider, &driveOutputSlider };
        for (size_t index = 0; index < bandKnobs.size(); ++index)
            bandKnobs[index]->setBounds(sharedKnobX[index], knobY, knobSide, knobComponentH);
    }
    else
    {
        // Dynamic, RTA and Match deliberately share one immutable template.
        // Switching pages changes only controls/text, never geometry.
        const int firstColumnW = w < 820 ? 80 : w < 1080 ? 92 : 104;
        const int secondColumnW = w < 820 ? 104 : w < 1080 ? 120 : 136;
        const int secondX = sharedKnobX[0] - gap - secondColumnW;
        const int firstX = secondX - gap - firstColumnW;

        if (workspacePage == WorkspacePage::Dynamic)
        {
            stack(dynModeBox, sidechainBox, firstX, pairY, firstColumnW);
            stack(sidechainAudition, dynLookahead, secondX, pairY, secondColumnW);
            int index = 0;
            for (auto* slider : { &dynThreshold, &dynRange, &dynRatio, &dynAttack, &dynRelease })
                slider->setBounds(sharedKnobX[(size_t)index++], pairY, knobSide, knobComponentH);
        }
        else if (workspacePage == WorkspacePage::Analyzer)
        {
            stack(analyzerVisible, analyzerPeakHold, firstX, pairY, firstColumnW);
            stack(spectrumFreeze, analyzerResolutionBox, secondX, pairY, secondColumnW);
            int index = 0;
            for (auto* slider : { &analyzerRange, &analyzerFloor, &analyzerSpeed, &analyzerAveraging, &analyzerTilt })
                slider->setBounds(sharedKnobX[(size_t)index++], pairY, knobSide, knobComponentH);
        }
        else if (workspacePage == WorkspacePage::Match)
        {
            stack(matchCaptureBtn, matchApplyBtn, firstX, pairY, firstColumnW);
            stack(matchCommitBtn, matchClearBtn, secondX, pairY, secondColumnW);
            int index = 0;
            for (auto* slider : { &matchAmount, &matchSmoothing, &matchLow, &matchHigh, &matchTime })
                slider->setBounds(sharedKnobX[(size_t)index++], pairY, knobSide, knobComponentH);
        }
    }
}

void DefaultEqualizerAudioProcessorEditor::updateAnalyzerSettings()
{
    uiPreferences->setValue("analyzerRange", analyzerRange.getValue());
    uiPreferences->setValue("analyzerFloor", analyzerFloor.getValue());
    uiPreferences->setValue("analyzerSpeed", analyzerSpeed.getValue());
    uiPreferences->setValue("analyzerAveraging", analyzerAveraging.getValue());
    uiPreferences->setValue("analyzerTilt", analyzerTilt.getValue());
    uiPreferences->setValue("analyzerResolution", analyzerResolutionBox.getSelectedId());
    uiPreferences->setValue("analyzerPeakHold", analyzerPeakHold.getToggleState());
    proc.preSpectrumFifo.setResolution(analyzerResolutionBox.getSelectedItemIndex());
    proc.spectrumFifo.setResolution(analyzerResolutionBox.getSelectedItemIndex());
    responseCurve.setAnalyzerSettings((float)analyzerFloor.getValue(), (float)analyzerRange.getValue(),
        (float)analyzerSpeed.getValue(), (float)analyzerAveraging.getValue(),
        analyzerResolutionBox.getSelectedItemIndex(), (float)analyzerTilt.getValue(),
        analyzerPeakHold.getToggleState());
}

void DefaultEqualizerAudioProcessorEditor::applyMatchToBands()
{
    if (!proc.matchEQ.isMatchActive()) return;
    const auto* correction = proc.matchEQ.getCorrectionDb();
    const int bins = proc.matchEQ.getNumBins();
    const double sampleRate = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
    const float low = (float) std::min(matchLow.getValue(), matchHigh.getValue() * 0.5);
    const float high = (float) std::max(matchHigh.getValue(), matchLow.getValue() * 2.0);
    const float amount = (float) matchAmount.getValue() * 0.01f;
    const int radius = juce::roundToInt((float) matchSmoothing.getValue() * 0.16f);
    proc.undoManager.beginNewTransaction("Apply Match EQ as editable bands");
    for (int b = 0; b < kNumBands; ++b)
    {
        proc.resetBandToDefaults(b, true);
        const float lo = low * std::pow(high / low, (float)b / (float)kNumBands);
        const float hi = low * std::pow(high / low, (float)(b + 1) / (float)kNumBands);
        const int first = juce::jlimit(1, bins - 1, juce::roundToInt(lo * MatchEQ::fftSize / sampleRate));
        const int last = juce::jlimit(first, bins - 1, juce::roundToInt(hi * MatchEQ::fftSize / sampleRate));
        float best = 0.0f; int bestBin = first;
        for (int bin = first; bin <= last; ++bin)
        {
            float sum = 0.0f; int count = 0;
            for (int k = std::max(1, bin - radius); k <= std::min(bins - 1, bin + radius); ++k)
            { sum += correction[k]; ++count; }
            const float smoothed = count > 0 ? sum / (float)count : correction[bin];
            if (std::abs(smoothed) > std::abs(best)) { best = smoothed; bestBin = bin; }
        }
        const float frequency = (float)(bestBin * sampleRate / MatchEQ::fftSize);
        const float gain = std::clamp(best * amount, -18.0f, 18.0f);
        for (const auto& change : { std::pair<const char*, float>{"on", 1.0f}, {"type", 0.0f}, {"freq", frequency},
                                   {"gain", gain}, {"q", 1.4f}, {"slope", 12.0f} })
            if (auto* parameter = proc.apvts.getParameter(bandId(b + 1, change.first)))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->convertTo0to1(change.second));
                parameter->endChangeGesture();
            }
    }
    proc.matchEQ.setMatchActive(false);
    selectBand(0);
    setWorkspacePage(WorkspacePage::Band);
}
