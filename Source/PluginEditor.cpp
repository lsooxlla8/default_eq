#include "PluginEditor.h"

namespace
{
constexpr int minimumEditorWidth = 960;
constexpr int minimumEditorHeight = 464;

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

float layoutScaleForWidth(int width) noexcept
{
    return juce::jlimit(1.0f, 1.25f, (float)width / 960.0f);
}

int workspaceHeightForWidth(int width) noexcept
{
    return juce::roundToInt(110.0f * layoutScaleForWidth(width));
}
}

NumericValueControl::NumericValueControl(juce::String labelText)
{
    setName(std::move(labelText));
    setSliderStyle(juce::Slider::LinearVertical);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setSliderSnapsToMousePosition(false);
    setMouseDragSensitivity(540);
    valueLabel.setJustificationType(juce::Justification::centred);
    valueLabel.setEditable(false, true, false);
    valueLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(valueLabel);
    onValueChange = [this] { updateText(); };
}

void NumericValueControl::setFormatter(std::function<juce::String(double)> formatter,
                                       std::function<double(const juce::String&)> parser)
{
    formatValue = std::move(formatter);
    parseValue = std::move(parser);
    valueLabel.onTextChange = [this]
    {
        if (updating || !parseValue) return;
        const double parsed = parseValue(valueLabel.getText());
        if (std::isfinite(parsed))
            setValue(getNormalisableRange().snapToLegalValue(
                juce::jlimit(getMinimum(), getMaximum(), parsed)), juce::sendNotificationSync);
        updateText();
    };
    updateText();
}

void NumericValueControl::updateText()
{
    juce::ScopedValueSetter<bool> guard(updating, true);
    const auto value = formatValue ? formatValue(getValue()) : juce::String(getValue(), 2);
    valueLabel.setText(showValue ? getName() + "  " + value : getName(), juce::dontSendNotification);
}

void NumericValueControl::setValueVisible(bool shouldShowValue)
{
    if (showValue == shouldShowValue) return;
    showValue = shouldShowValue;
    updateText();
}

void NumericValueControl::paint(juce::Graphics& g)
{
    const auto fg = findColour(default_family::LookAndFeel::foregroundColourId, true)
        .withMultipliedAlpha(isEnabled() ? 1.0f : default_family::metrics::disabledOpacity);
    const auto bg = findColour(default_family::LookAndFeel::backgroundColourId, true);
    const int border = juce::jmax(1, juce::roundToInt(2.0f * default_family::metrics::designWidth / 880.0f));
    g.setColour(fg); g.fillRect(getLocalBounds());
    g.setColour(bg); g.fillRect(getLocalBounds().reduced(border));
}

void NumericValueControl::resized() { valueLabel.setBounds(getLocalBounds().reduced(3, 2)); }

void NumericValueControl::mouseDoubleClick(const juce::MouseEvent&)
{
    valueLabel.showEditor();
    if (auto* editor = valueLabel.getCurrentTextEditor()) editor->selectAll();
}

void NumericValueControl::lookAndFeelChanged()
{
    juce::Slider::lookAndFeelChanged();
    const auto fg = findColour(default_family::LookAndFeel::foregroundColourId, true);
    valueLabel.setColour(juce::Label::textColourId, fg);
    valueLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    valueLabel.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    valueLabel.setFont(mono(11.0f, true));
    updateText();
}

DefaultEqualizerAudioProcessorEditor::DefaultEqualizerAudioProcessorEditor(DefaultEqualizerAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), proc(p), responseCurve(p)
{
    juce::PropertiesFile::Options options;
    options.applicationName = "default_eq8";
    options.filenameSuffix = "settings";
    options.folderName = "icanseesounds";
    options.osxLibrarySubFolder = "Application Support";
    uiPreferences = std::make_unique<juce::PropertiesFile>(options);
    darkTheme = !default_family::ThemePreferences::loadLight();
    familyLook.setDark(darkTheme);
    setLookAndFeel(&familyLook);
    responseCurve.setDarkMode(darkTheme);

    workspaceExpanded = true;
    expandedWindowHeight = 464;
    setResizable(true, true);
    addMouseListener(this, true);
    const int initialWidth = minimumEditorWidth;
    setResizeLimits(minimumEditorWidth, minimumEditorHeight, 1200, 900);
    setSize(initialWidth, expandedWindowHeight);

    addAndMakeVisible(responseCurve);

    auto addButton = [this](auto& button) { addAndMakeVisible(button); };
    nextBrandGlitchTimeMs = juce::Time::getMillisecondCounterHiRes() + 4000.0;
    addButton(themeBtn); addButton(powerBtn); addButton(workspaceToggleBtn);
    addButton(bandOn); addButton(bandSolo); addButton(adaptiveQBtn); addButton(placementModeBtn);
    addButton(sidechainAudition); addButton(dynModeBtn); addButton(sidechainBtn);
    addButton(matchSidechainBtn); addButton(matchCaptureBtn); addButton(matchApplyBtn); addButton(matchCommitBtn); addButton(matchClearBtn);

    auto addCombo = [this](juce::ComboBox& box) { box.setJustificationType(juce::Justification::centred); addAndMakeVisible(box); };
    for (auto* type : { "BELL", "LOW SHELF", "HIGH SHELF", "LOW CUT", "HIGH CUT", "BAND PASS", "NOTCH", "TILT" }) typeBox.addItem(type, typeBox.getNumItems() + 1);
    saturationBox.addItemList({ "SOFT CLIP", "HARD CLIP", "DIODE CLIPPER", "TRIODE STAGE", "TRANSISTOR / FET",
                                "TAPE HYSTERESIS", "HARMONIC MORPH", "PHASE DISTORTION", "SPECTRAL CLIP", "SINE EROSION" }, 1);
    phaseModeBox.addItemList({ "MIN PHASE", "LINEAR ECO", "LINEAR MED", "LINEAR HIGH" }, 1);
    addCombo(typeBox);
    addCombo(saturationBox); addCombo(phaseModeBox);
    addAndMakeVisible(autoGainBtn);

    const std::array<juce::Slider*, 17> rotaryParameters {
        &dynThreshold, &dynRange, &dynRatio, &dynAttack, &dynRelease,
        &driveSlider, &driveCharacterSlider, &amountSlider, &outputSlider,
        &matchAmount, &matchSmoothing, &matchLow, &matchHigh, &matchTime,
        &analyzerFloor, &analyzerAveraging, &analyzerTilt
    };
    for (auto* slider : rotaryParameters)
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
    for (auto* field : { &freqField, &gainField, &qField, &slopeField })
        addAndMakeVisible(*field);
    freqField.setRange(20.0, 20000.0, 0.001); freqField.setSkewFactor(0.5);
    gainField.setRange(-36.0, 36.0, 0.01);
    qField.setRange(0.1, 24.0, 0.001); qField.setSkewFactor(0.5);
    slopeField.setRange(3.0, 48.0, 0.1);
    freqField.setFormatter(
        [](double v) { return v >= 1000.0 ? juce::String(v / 1000.0, 2) + "k" : juce::String(v, 0); },
        [](const juce::String& s) { return parseUnitValue(s, true); });
    gainField.setFormatter([](double v) { return cleanDb(v, 1); },
                           [](const juce::String& s) { return parseUnitValue(s); });
    qField.setFormatter([](double v) { return juce::String(v, 2); },
                        [](const juce::String& s) { return parseUnitValue(s); });
    slopeField.setFormatter([](double v) { return juce::String(v, 1) + " dB/OCT"; },
                            [](const juce::String& s) { return parseUnitValue(s); });
    dynThreshold.setName("THRESHOLD"); dynRange.setName("RANGE");
    dynRatio.setName("RATIO"); dynAttack.setName("ATTACK"); dynRelease.setName("RELEASE");
    driveSlider.setName("DRIVE"); driveCharacterSlider.setName("CHARACTER");
    outputSlider.setName("OUT");
    outputSlider.setFormatter([](double v) { return cleanDb(v, 1); },
                              [](const juce::String& s) { return parseUnitValue(s); });
    outputSlider.setValueVisible(true);
    amountSlider.setName("AMOUNT");
    amountSlider.setFormatter([](double v)
                              {
                                  const double percent = std::abs(v) < 0.005 ? 0.0 : v * 100.0;
                                  return juce::String(percent, 0) + "%";
                              },
                              [](const juce::String& s) { return parseUnitValue(s) * 0.01; });
    amountSlider.setValueVisible(true);
    amountSlider.setDoubleClickReturnValue(true, 1.0);
    matchAmount.setName("AMOUNT"); matchSmoothing.setName("SMOOTHING");
    matchLow.setName("LOW LIMIT"); matchHigh.setName("HIGH LIMIT");
    matchTime.setName("ANALYSIS");
    analyzerFloor.setName("FLOOR"); analyzerAveraging.setName("AVERAGING"); analyzerTilt.setName("TILT");
    analyzerFloor.setRange(-140.0, -30.0, 1.0);
    analyzerAveraging.setRange(0.0, 1.6, 0.005);
    analyzerTilt.setRange(-6.0, 6.0, 0.1);
    analyzerFloor.setValue(uiPreferences->getDoubleValue("analyzerFloor", -90.0), juce::dontSendNotification);
    if (!uiPreferences->getBoolValue("analyzerAveragingSecondsV1", false))
    {
        uiPreferences->setValue("analyzerAveraging", 0.065);
        uiPreferences->setValue("analyzerAveragingSecondsV1", true);
    }
    analyzerAveraging.setValue(uiPreferences->getDoubleValue("analyzerAveraging", 0.065), juce::dontSendNotification);
    if (!uiPreferences->getBoolValue("analyzerTiltDefault45", false))
    {
        uiPreferences->setValue("analyzerTilt", 4.5);
        uiPreferences->setValue("analyzerTiltDefault45", true);
    }
    analyzerTilt.setValue(uiPreferences->getDoubleValue("analyzerTilt", 4.5), juce::dontSendNotification);
    analyzerFloor.setDoubleClickReturnValue(true, -90.0);
    analyzerAveraging.setDoubleClickReturnValue(true, 0.065);
    analyzerTilt.setDoubleClickReturnValue(true, 4.5);
    analyzerFloor.textFromValueFunction = [](double v) { return cleanDb(v, 0); };
    analyzerAveraging.textFromValueFunction = [](double seconds)
    {
        return seconds < 1.0 ? juce::String(seconds * 1000.0, 0) + " ms"
                             : juce::String(seconds, 2) + " s";
    };
    analyzerAveraging.valueFromTextFunction = [](juce::String text)
    {
        auto source = text.trim().toLowerCase().replaceCharacter(',', '.');
        const double value = source.retainCharacters("0123456789.e+-").getDoubleValue();
        return source.contains("ms") ? value * 0.001 : value;
    };
    analyzerTilt.textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB/oct"; };
    uiPreferences->removeValue("analyzerRange");
    uiPreferences->removeValue("analyzerSpeed");
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

    dynThreshold.textFromValueFunction = [](double v) { return cleanDb(v, 1); };
    dynRange.textFromValueFunction = [](double v) { return cleanDb(v, 1); };
    dynRatio.textFromValueFunction = [](double v) { return juce::String(v, 2) + ":1"; };
    dynAttack.textFromValueFunction = [](double v) { return juce::String(v, 1) + " ms"; };
    dynRelease.textFromValueFunction = [](double v) { return v >= 1000.0 ? juce::String(v / 1000.0, 2) + " s" : juce::String(v, 0) + " ms"; };
    dynAttack.valueFromTextFunction = dynRelease.valueFromTextFunction = [](const juce::String& s) { return parseUnitValue(s, false, true); };
    driveSlider.textFromValueFunction = [](double v) { return cleanDb(v, 1); };
    applySliderPalette();

    themeBtn.setTooltip("Toggle exact paper/ink inversion");
    dynModeBtn.setTooltip("Toggle downward or upward dynamic EQ");
    sidechainBtn.setTooltip("Toggle internal or external sidechain");
    dynModeBtn.onClick = [this]
    {
        if (auto* parameter = proc.apvts.getParameter(bandId(selectedBand + 1, "dyn_mode")))
        {
            const bool upward = proc.apvts.getRawParameterValue(
                bandId(selectedBand + 1, "dyn_mode"))->load(std::memory_order_relaxed) > 0.5f;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(upward ? 0.0f : 1.0f));
            parameter->endChangeGesture();
        }
    };
    sidechainBtn.onClick = [this]
    {
        if (auto* parameter = proc.apvts.getParameter(bandId(selectedBand + 1, "sc_source")))
        {
            const bool external = proc.apvts.getRawParameterValue(
                bandId(selectedBand + 1, "sc_source"))->load(std::memory_order_relaxed) > 0.5f;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(external ? 0.0f : 1.0f));
            parameter->endChangeGesture();
        }
    };
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
    workspaceToggleBtn.onClick = [this]
    {
        setWorkspacePage(workspacePage == WorkspacePage::Band
            ? WorkspacePage::Match : WorkspacePage::Band);
    };
    typeBox.onChange = [this]
    {
        const int type = typeBox.getSelectedItemIndex();
        if (ResponseCurveComponent::typeDefaultsToMidSide(type))
            if (auto* placementMode = proc.apvts.getParameter(bandId(selectedBand + 1, "placement_mode")))
            {
                placementMode->beginChangeGesture();
                placementMode->setValueNotifyingHost(placementMode->convertTo0to1(1.0f));
                placementMode->endChangeGesture();
            }
    };
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
    matchSidechainBtn.setClickingTogglesState(true);
    matchSidechainBtn.setTooltip("Use the external sidechain as the current Match EQ analysis source");
    proc.matchUseSidechain.store(false, std::memory_order_release);
    matchSidechainBtn.onClick = [this]
    {
        proc.matchUseSidechain.store(matchSidechainBtn.getToggleState(), std::memory_order_release);
    };

    responseCurve.setAnalyzerSources(true, true);
    proc.preSpectrumFifo.setResolution(2);
    proc.spectrumFifo.setResolution(2);
    responseCurve.resetPeakHold();
    for (auto* obsoletePreference : { "analyzerVisible", "analyzerPeakHold", "analyzerResolution",
                                      "analyzerResolutionV2", "analyzerResolutionV3" })
        uiPreferences->removeValue(obsoletePreference);
    const auto analyzerChanged = [this] { updateAnalyzerSettings(); };
    analyzerFloor.onValueChange = analyzerChanged;
    analyzerAveraging.onValueChange = analyzerChanged;
    analyzerTilt.onValueChange = analyzerChanged;
    updateAnalyzerSettings();

    powerAtt = std::make_unique<ButtonAttachment>(proc.apvts, "plugin_enabled", powerBtn);
    adaptiveQAtt = std::make_unique<ButtonAttachment>(proc.apvts, "adaptive_q", adaptiveQBtn);
    linearPhaseAtt = std::make_unique<ButtonAttachment>(proc.apvts, "linear_phase", linearPhaseBtn);
    oversamplingAtt = std::make_unique<SliderAttachment>(proc.apvts, "oversampling", oversamplingSlider);
    amountAtt = std::make_unique<SliderAttachment>(proc.apvts, "scale", amountSlider);
    outputAtt = std::make_unique<SliderAttachment>(proc.apvts, "output_gain", outputSlider);

    const int initialAutoMode = (int)proc.apvts.getRawParameterValue("auto_gain_mode")->load();
    autoGainBtn.setButtonText(initialAutoMode == 0 ? "AUTO GAIN: OFF"
                              : initialAutoMode == 1 ? "AUTO GAIN" : "SMART AUTO GAIN");
    autoGainBtn.setToggleState(initialAutoMode > 0, juce::dontSendNotification);

    setWantsKeyboardFocus(true);
    // Prime the disabled band controls with their real parameter defaults,
    // then leave the graph and panel with no selected band.
    selectBand(0);
    selectBand(-1);

    uiPreferences->removeValue("workspacePage");
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
    proc.matchUseSidechain.store(false, std::memory_order_release);
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
    if (event.mods.isPopupMenu())
    {
        auto* component = event.originalComponent;
        auto* slider = dynamic_cast<juce::Slider*>(component);
        if (slider == nullptr && component != nullptr)
            slider = component->findParentComponentOfClass<juce::Slider>();
        if (slider != nullptr && slider->isEnabled())
        {
            slider->setValue(slider->getDoubleClickReturnValue(), juce::sendNotificationSync);
            return;
        }
    }
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
    proc.setAnalyzerEnabled(isShowing());
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
    const std::array<juce::Slider*, 20> sliders {
        &placementSlider, &dynThreshold, &dynRange, &dynRatio, &dynAttack,
        &dynRelease, &dynLookahead, &driveSlider, &driveCharacterSlider,
        &amountSlider, &outputSlider, &matchAmount, &matchSmoothing, &matchLow, &matchHigh,
        &matchTime, &analyzerFloor, &analyzerAveraging, &analyzerTilt,
        &oversamplingSlider
    };
    for (auto* slider : sliders)
    {
        slider->setColour(juce::Slider::textBoxTextColourId, fg);
        slider->setColour(juce::Slider::textBoxBackgroundColourId, bg);
        slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider->updateText();
    }
}

void DefaultEqualizerAudioProcessorEditor::rebindBandControls()
{
    bandOnAtt.reset(); typeAtt.reset(); placementModeAtt.reset(); placementAtt.reset();
    freqAtt.reset(); gainAtt.reset(); qAtt.reset(); slopeAtt.reset();
    dynLookaheadAtt.reset(); dynThresholdAtt.reset(); dynRangeAtt.reset();
    dynRatioAtt.reset(); dynAttackAtt.reset(); dynReleaseAtt.reset(); driveAtt.reset();
    driveCharacterAtt.reset(); saturationAtt.reset();
    if (selectedBand < 0 || selectedBand >= kNumBands)
    {
        displayedDriveMode = -1;
        driveFormatPending = false;
        return;
    }
    const int idx = selectedBand + 1;
    bandOnAtt = std::make_unique<ButtonAttachment>(proc.apvts, bandId(idx, "on"), bandOn);
    typeAtt = std::make_unique<ComboAttachment>(proc.apvts, bandId(idx, "type"), typeBox);
    placementModeAtt = std::make_unique<ButtonAttachment>(proc.apvts, bandId(idx, "placement_mode"), placementModeBtn);
    placementAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "placement"), placementSlider);
    freqAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "freq"), freqField);
    gainAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "gain"), gainField);
    qAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "q"), qField);
    slopeAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "slope"), slopeField);
    dynLookaheadAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_lookahead"), dynLookahead);
    dynThresholdAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_thresh"), dynThreshold);
    dynRangeAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_range"), dynRange);
    dynRatioAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_ratio"), dynRatio);
    dynAttackAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_attack"), dynAttack);
    dynReleaseAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_release"), dynRelease);
    driveAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "drive"), driveSlider);
    driveCharacterAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "drive_character"), driveCharacterSlider);
    displayedDriveMode = std::clamp((int)proc.apvts.getRawParameterValue(bandId(idx, "sat_mode"))->load(), 0, 9);
    saturationAtt = std::make_unique<ComboAttachment>(proc.apvts, bandId(idx, "sat_mode"), saturationBox);
    updateDriveControls(false);
    driveFormatPending = true;
}

void DefaultEqualizerAudioProcessorEditor::updateBandControlEnablement(bool bandSelected)
{
    for (auto* component : { static_cast<juce::Component*>(&bandOn),
                             static_cast<juce::Component*>(&bandSolo),
                             static_cast<juce::Component*>(&placementModeBtn),
                             static_cast<juce::Component*>(&placementSlider),
                             static_cast<juce::Component*>(&typeBox),
                             static_cast<juce::Component*>(&saturationBox),
                             static_cast<juce::Component*>(&driveSlider),
                             static_cast<juce::Component*>(&driveCharacterSlider),
                             static_cast<juce::Component*>(&dynModeBtn),
                             static_cast<juce::Component*>(&sidechainBtn),
                             static_cast<juce::Component*>(&dynThreshold),
                             static_cast<juce::Component*>(&dynRange),
                             static_cast<juce::Component*>(&dynRatio),
                             static_cast<juce::Component*>(&dynAttack),
                             static_cast<juce::Component*>(&dynRelease),
                             static_cast<juce::Component*>(&sidechainAudition),
                             static_cast<juce::Component*>(&dynLookahead),
                             static_cast<juce::Component*>(&freqField),
                             static_cast<juce::Component*>(&gainField),
                             static_cast<juce::Component*>(&qField),
                             static_cast<juce::Component*>(&slopeField) })
        component->setEnabled(bandSelected);

    if (!bandSelected)
    {
        if (proc.sidechainAuditionBand.load(std::memory_order_acquire) == selectedBand)
            proc.sidechainAuditionBand.store(-1, std::memory_order_release);
        if (proc.soloBand.load(std::memory_order_acquire) == selectedBand)
            proc.soloBand.store(-1, std::memory_order_release);
        sidechainAudition.setToggleState(false, juce::dontSendNotification);
        bandSolo.setToggleState(false, juce::dontSendNotification);
    }
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
    selectedBand = band >= 0 && band < kNumBands ? band : -1;
    if (selectedBand < 0)
    {
        if (updateGraphSelection) responseCurve.setSelectedBand(-1);
        rebindBandControls();
        for (auto* field : { &freqField, &gainField, &qField, &slopeField })
            field->setValueVisible(false);
        updateBandControlEnablement(false);
        updateHeaderText();
        return;
    }
    sidechainAudition.setToggleState(proc.sidechainAuditionBand.load(std::memory_order_acquire) == selectedBand,
                                     juce::dontSendNotification);
    bandSolo.setToggleState(selectedBand >= 0
                                && proc.soloBand.load(std::memory_order_acquire) == selectedBand,
                            juce::dontSendNotification);
    if (updateGraphSelection) responseCurve.setSelectedBand(selectedBand);
    rebindBandControls();
    const bool bandPresent = proc.apvts.getRawParameterValue(
        bandId(selectedBand + 1, "present"))->load(std::memory_order_relaxed) > 0.5f;
    for (auto* field : { &freqField, &gainField, &qField, &slopeField })
        field->setValueVisible(bandPresent);
    updateBandControlEnablement(bandPresent);
    updateHeaderText();
    if ((int)proc.apvts.getRawParameterValue("auto_gain_mode")->load() == 2)
        autoGainBtn.setTooltip(proc.smartAutoGainLocked.load(std::memory_order_acquire) ? "Smart Auto Gain: locked" : "Smart Auto Gain: analysing");
}

void DefaultEqualizerAudioProcessorEditor::setWorkspacePage(WorkspacePage page)
{
    workspacePage = page;
    const bool band = page == WorkspacePage::Band;
    const bool match = page == WorkspacePage::Match;
    const std::array<juce::Component*, 8> bandComponents { &bandOn, &bandSolo,
        &placementModeBtn, &placementSlider, &typeBox, &driveSlider, &driveCharacterSlider,
        &saturationBox };
    const std::array<juce::Component*, 9> dynComponents { &dynLookahead, &sidechainAudition, &dynModeBtn, &sidechainBtn, &dynThreshold,
                                                          &dynRange, &dynRatio, &dynAttack, &dynRelease };
    const std::array<juce::Component*, 3> analyzerComponents {
        &analyzerFloor, &analyzerAveraging, &analyzerTilt };
    const std::array<juce::Component*, 10> matchComponents { &matchSidechainBtn, &matchCaptureBtn, &matchApplyBtn, &matchCommitBtn, &matchClearBtn,
        &matchAmount, &matchSmoothing, &matchLow, &matchHigh, &matchTime };
    for (auto* component : bandComponents) component->setVisible(band);
    for (auto* component : dynComponents) component->setVisible(band);
    for (auto* component : analyzerComponents) component->setVisible(match);
    for (auto* component : { static_cast<juce::Component*>(&phaseModeBox),
                             static_cast<juce::Component*>(&oversamplingSlider), static_cast<juce::Component*>(&amountSlider),
                             static_cast<juce::Component*>(&outputSlider),
                             static_cast<juce::Component*>(&adaptiveQBtn) })
        component->setVisible(true);
    for (auto* component : matchComponents) component->setVisible(match);
    workspaceToggleBtn.setButtonText(band ? "MATCH / RTA" : "BAND / DYNAMIC");
    resized(); repaint();
}

void DefaultEqualizerAudioProcessorEditor::setWorkspaceExpanded(bool, bool)
{
    workspaceExpanded = true;
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
    // Some plug-in hosts resize editors with a direct setBounds(), which JUCE
    // explicitly documents as bypassing the normal bounds constrainer.  Clamp
    // again on the message thread so those hosts cannot leave clipped UI.
    if (getWidth() < minimumEditorWidth || getHeight() < minimumEditorHeight)
        setSize(juce::jmax(minimumEditorWidth, getWidth()),
                juce::jmax(minimumEditorHeight, getHeight()));

    // Hosts can attach an already-visible editor to a native window without a
    // JUCE visibilityChanged() callback. Reconcile the cheap atomic analyzer
    // gate here as well, so reopening the editor cannot leave Spectrum visibly
    // enabled while the audio-side producer remains stopped.
    updateAnalyzerLifecycle();

    const auto transportGeneration = proc.transportStartGeneration.load(std::memory_order_acquire);
    if (transportGeneration != lastTransportStartGeneration)
        responseCurve.resetPeakHold();
    lastTransportStartGeneration = transportGeneration;

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
        static const juce::String original { "default_eq8" };
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
    if (curveSelection != selectedBand) selectBand(curveSelection, false);
    const bool bandPresent = selectedBand >= 0 && proc.apvts.getRawParameterValue(
        bandId(selectedBand + 1, "present"))->load(std::memory_order_relaxed) > 0.5f;
    for (auto* field : { &freqField, &gainField, &qField, &slopeField })
        field->setValueVisible(bandPresent);
    updateBandControlEnablement(bandPresent);
    bandSolo.setToggleState(selectedBand >= 0
                                && proc.soloBand.load(std::memory_order_acquire) == selectedBand,
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
    if (selectedBand >= 0)
    {
        const int driveMode = std::clamp((int)proc.apvts.getRawParameterValue(
            bandId(selectedBand + 1, "sat_mode"))->load(), 0, 9);
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
        const bool midSide = proc.apvts.getRawParameterValue(
            bandId(selectedBand + 1, "placement_mode"))->load() > 0.5f;
        placementModeBtn.setButtonText(midSide ? "M/S" : "L/R");
        placementSlider.getProperties().set("midSide", midSide);
        placementSlider.repaint();
        const bool upward = proc.apvts.getRawParameterValue(
            bandId(selectedBand + 1, "dyn_mode"))->load(std::memory_order_relaxed) > 0.5f;
        dynModeBtn.setButtonText(upward ? "UP" : "DOWN");
        dynModeBtn.setToggleState(upward, juce::dontSendNotification);
        const bool externalSidechain = proc.apvts.getRawParameterValue(
            bandId(selectedBand + 1, "sc_source"))->load(std::memory_order_relaxed) > 0.5f;
        sidechainBtn.setButtonText(externalSidechain ? "EXT SC" : "INT SC");
        sidechainBtn.setToggleState(externalSidechain, juce::dontSendNotification);
    }
    updateHeaderText();
}

void DefaultEqualizerAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto fg = familyLook.foreground(), bg = familyLook.background();
    const int headerH = juce::jlimit(56, 70, juce::roundToInt(64.0f * getWidth() / 860.0f));
    const int workspaceH = workspaceHeightForWidth(getWidth());
    g.fillAll(fg);
    g.setColour(bg); g.fillRect(0, 0, getWidth(), headerH);
    const int seamX = juce::roundToInt((float)default_family::metrics::wordmarkWidth
                                      * (float)getWidth() / (float)default_family::metrics::designWidth);
    const int seam = juce::jmax(18, headerH * 3 / 8);
    g.setColour(fg); g.fillRect(seamX, 0, seam, seam); g.fillRect(seamX, headerH - seam, seam, seam);
    g.setColour(bg);
    g.fillRect(0, getHeight() - workspaceH, getWidth(), workspaceH);

    const float layoutScale = layoutScaleForWidth(getWidth());
    const float panelScale = layoutScale * 1.15f;
    g.setFont(mono(9.5f * panelScale, true));
    const std::array<juce::Slider*, 17> sliders { &dynThreshold, &dynRange, &dynRatio, &dynAttack,
                                                  &dynRelease, &driveSlider, &driveCharacterSlider,
                                                  &amountSlider, &outputSlider,
                                                  &matchAmount, &matchSmoothing, &matchLow, &matchHigh, &matchTime,
                                                  &analyzerFloor, &analyzerAveraging, &analyzerTilt };
    for (auto* slider : sliders)
        if (slider->isVisible() && slider != &outputSlider && slider != &amountSlider)
        {
            g.setColour(fg.withAlpha(slider->isEnabled() ? 0.85f : 0.35f));
            const int captionH = juce::roundToInt(13.0f * panelScale);
            g.drawFittedText(slider->getName(), slider->getX(), slider->getY() - captionH,
                             slider->getWidth(), captionH,
                             juce::Justification::centred, 1);
        }
}

void DefaultEqualizerAudioProcessorEditor::resized()
{
    const int w = getWidth(), h = getHeight();
    if (w < minimumEditorWidth || h < minimumEditorHeight)
    {
        setSize(juce::jmax(minimumEditorWidth, w), juce::jmax(minimumEditorHeight, h));
        return;
    }
    const float layoutScale = layoutScaleForWidth(w);
    familyLook.setUiScale(layoutScale);
    const int headerH = juce::jlimit(56, 70, juce::roundToInt(64.0f * w / 860.0f));
    const int workspaceH = workspaceHeightForWidth(w);
    const int wordW = juce::roundToInt((float)default_family::metrics::wordmarkWidth
                                      * (float)w / (float)default_family::metrics::designWidth);
    const int seamW = juce::jmax(18, headerH * 3 / 8);
    themeBtn.setBounds(0, 0, wordW, headerH);

    const int actionH = juce::roundToInt(40.0f * layoutScale);
    const int actionY = (headerH - actionH) / 2;
    const int powerW = juce::roundToInt(76.0f * layoutScale);
    const int outerPad = juce::roundToInt(8.0f * layoutScale);
    const int powerX = w - powerW - outerPad;
    powerBtn.setBounds(powerX, actionY, powerW, actionH);

    const int autoW = juce::roundToInt(152.0f * layoutScale);
    const int outW = juce::roundToInt(104.0f * layoutScale);
    const int sectionGap = juce::roundToInt(4.0f * layoutScale);
    const int autoX = powerX - sectionGap - autoW;
    const int outputX = autoX - sectionGap - outW;
    const int amountX = outputX - sectionGap - outW;
    amountSlider.setBounds(amountX, actionY, outW, actionH);
    outputSlider.setBounds(outputX, actionY, outW, actionH);
    autoGainBtn.setBounds(autoX, actionY, autoW, actionH);

    const int globalStart = wordW + seamW + outerPad;
    const int globalEnd = amountX - outerPad;
    const int globalGap = sectionGap;
    const int availableGlobal = juce::jmax(190, globalEnd - globalStart);
    const bool compactGlobals = availableGlobal < juce::roundToInt(310.0f * layoutScale);
    const int phaseW = juce::roundToInt((compactGlobals ? 80.0f : 108.0f) * layoutScale);
    const int osW = juce::roundToInt((compactGlobals ? 64.0f : 76.0f) * layoutScale);
    const int globalTotal = phaseW + osW + globalGap;
    int globalX = globalStart + juce::jmax(0, (availableGlobal - globalTotal) / 2);
    phaseModeBox.setBounds(globalX, actionY, phaseW, actionH); globalX += phaseW + globalGap;
    oversamplingSlider.setBounds(globalX, actionY, osW, actionH);

    const int graphFrame = juce::jmax(3, juce::roundToInt(4.0f * familyLook.getUiScale()));
    const int graphTop = headerH + graphFrame;
    const int toggleH = juce::roundToInt(28.0f * layoutScale);
    const int toggleY = h - workspaceH - toggleH - outerPad;
    const int lowerGap = sectionGap;
    const int adaptiveW = juce::roundToInt(112.0f * layoutScale);
    const int pageW = juce::roundToInt(176.0f * layoutScale);
    adaptiveQBtn.setBounds(graphFrame, toggleY, adaptiveW, toggleH);
    workspaceToggleBtn.setBounds(w - graphFrame - pageW, toggleY, pageW, toggleH);
    const int fieldsX = adaptiveQBtn.getRight() + lowerGap;
    const int fieldsRight = workspaceToggleBtn.getX() - lowerGap;
    const int fieldW = juce::jmax(58, (fieldsRight - fieldsX - lowerGap * 3) / 4);
    int fieldX = fieldsX;
    for (auto* field : { &freqField, &gainField, &qField, &slopeField })
    {
        field->setBounds(fieldX, toggleY, fieldW, toggleH);
        fieldX += fieldW + lowerGap;
    }
    const int graphBottom = toggleY - graphFrame;
    responseCurve.setBounds(graphFrame, graphTop, w - graphFrame * 2,
                            juce::jmax(160, graphBottom - graphTop));

    const int workspaceY = h - workspaceH;
    const float panelScale = layoutScale * 1.15f;
    const int rowH = juce::roundToInt(25.0f * panelScale);
    const int gap = juce::roundToInt(4.0f * panelScale);
    const int pairH = rowH * 2 + gap;
    const int knobSide = pairH;
    const int textBoxH = juce::roundToInt(18.0f * panelScale);
    const int knobComponentH = knobSide + textBoxH;
    for (auto* slider : { &dynThreshold, &dynRange, &dynRatio, &dynAttack, &dynRelease,
                          &driveSlider, &driveCharacterSlider, &matchAmount, &matchSmoothing,
                          &matchLow, &matchHigh, &matchTime, &analyzerFloor,
                          &analyzerAveraging, &analyzerTilt })
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobSide, textBoxH);
    const int controlY = workspaceY + juce::roundToInt(17.0f * layoutScale);
    const int pairY = controlY;
    const int bandOnW = juce::roundToInt(38.0f * panelScale);
    const int selectorW = juce::roundToInt(88.0f * panelScale);
    const int placeW = juce::roundToInt(52.0f * panelScale);
    const int modeW = juce::roundToInt(48.0f * panelScale);
    const int listenW = juce::roundToInt(68.0f * panelScale);

    const auto stack = [rowH, gap](juce::Component& top, juce::Component& bottom,
                                   int x, int y, int width)
    {
        top.setBounds(x, y, width, rowH);
        bottom.setBounds(x, y + rowH + gap, width, rowH);
    };

    if (workspacePage == WorkspacePage::Band)
    {
        const int blockGap = juce::roundToInt(14.0f * panelScale);
        const int rowWidth = bandOnW + placeW + selectorW + modeW + listenW
            + knobSide * 7 + gap * 10 + blockGap;
        int x = juce::jmax(graphFrame, (w - rowWidth) / 2);
        stack(bandOn, bandSolo, x, pairY, bandOnW); x += bandOnW + gap;
        stack(placementModeBtn, placementSlider, x, pairY, placeW); x += placeW + gap;
        stack(typeBox, saturationBox, x, pairY, selectorW); x += selectorW + gap;
        driveSlider.setBounds(x, pairY, knobSide, knobComponentH); x += knobSide + gap;
        driveCharacterSlider.setBounds(x, pairY, knobSide, knobComponentH);
        x += knobSide + blockGap;
        stack(dynModeBtn, sidechainBtn, x, pairY, modeW); x += modeW + gap;
        for (auto* slider : { &dynThreshold, &dynRange, &dynRatio, &dynAttack, &dynRelease })
        {
            slider->setBounds(x, controlY, knobSide, knobComponentH);
            x += knobSide + gap;
        }
        stack(sidechainAudition, dynLookahead, x, pairY, listenW);
    }
    else
    {
        const int firstColumnW = juce::roundToInt(64.0f * panelScale);
        const int secondColumnW = juce::roundToInt(72.0f * panelScale);
        const int blockGap = juce::roundToInt(14.0f * panelScale);
        const int rowWidth = knobSide + firstColumnW + secondColumnW + knobSide * 8
            + gap * 9 + blockGap;
        int sourceX = juce::jmax(graphFrame, (w - rowWidth) / 2);
        int firstX = sourceX + knobSide + gap;
        int secondX = firstX + firstColumnW + gap;
        int knobX = secondX + secondColumnW + gap;
        matchSidechainBtn.setBounds(sourceX, pairY, knobSide, knobSide);
        stack(matchCaptureBtn, matchApplyBtn, firstX, pairY, firstColumnW);
        stack(matchCommitBtn, matchClearBtn, secondX, pairY, secondColumnW);
        for (auto* slider : { &matchAmount, &matchSmoothing, &matchLow, &matchHigh, &matchTime })
        {
            slider->setBounds(knobX, controlY, knobSide, knobComponentH);
            knobX += knobSide + gap;
        }
        knobX += blockGap - gap;
        for (auto* slider : { &analyzerFloor, &analyzerAveraging, &analyzerTilt })
        {
            slider->setBounds(knobX, controlY, knobSide, knobComponentH);
            knobX += knobSide + gap;
        }
    }
}

void DefaultEqualizerAudioProcessorEditor::updateAnalyzerSettings()
{
    uiPreferences->setValue("analyzerFloor", analyzerFloor.getValue());
    uiPreferences->setValue("analyzerAveraging", analyzerAveraging.getValue());
    uiPreferences->setValue("analyzerTilt", analyzerTilt.getValue());
    responseCurve.setAnalyzerSettings((float)analyzerFloor.getValue(),
        (float)analyzerAveraging.getValue(), (float)analyzerTilt.getValue());
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
