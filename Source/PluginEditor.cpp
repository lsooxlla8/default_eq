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
}

FamilyLookAndFeel::FamilyLookAndFeel() { setDark(true); }

void FamilyLookAndFeel::setDark(bool shouldBeDark)
{
    dark = shouldBeDark;
    const auto fg = foreground(), bg = background();
    setColour(juce::Label::textColourId, fg);
    setColour(juce::Label::backgroundColourId, bg);
    setColour(juce::TextEditor::textColourId, fg);
    setColour(juce::TextEditor::backgroundColourId, bg);
    setColour(juce::TextEditor::outlineColourId, fg);
    setColour(juce::Slider::textBoxTextColourId, fg);
    setColour(juce::Slider::textBoxBackgroundColourId, bg);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, bg);
    setColour(juce::ComboBox::textColourId, fg);
    setColour(juce::ComboBox::outlineColourId, fg);
    setColour(juce::ComboBox::arrowColourId, fg);
    setColour(juce::PopupMenu::backgroundColourId, bg);
    setColour(juce::PopupMenu::textColourId, fg);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, fg);
    setColour(juce::PopupMenu::highlightedTextColourId, bg);
}

juce::Font FamilyLookAndFeel::getTextButtonFont(juce::TextButton&, int h)
{
    return mono(juce::jlimit(9.0f, 15.0f, h * 0.40f), true);
}

juce::Font FamilyLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return mono(juce::jlimit(9.0f, 13.0f, box.getHeight() * 0.48f));
}

juce::Font FamilyLookAndFeel::getLabelFont(juce::Label& label)
{
    return mono(juce::jlimit(9.0f, 13.0f, label.getHeight() * 0.52f));
}

void FamilyLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                              const juce::Colour&, bool over, bool down)
{
    const bool active = button.getToggleState() || down;
    auto r = button.getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(active ? foreground() : background());
    g.fillRect(r);
    g.setColour(active ? background() : foreground());
    g.drawRect(r, 2.0f);
    if (over && !active) g.drawRect(r.reduced(4.0f), 1.0f);
}

void FamilyLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool down)
{
    const bool active = button.getToggleState() || down;
    g.setColour((active ? background() : foreground()).withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.35f));
    g.setFont(getTextButtonFont(button, button.getHeight()));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(5, 2), juce::Justification::centred, 1);
}

void FamilyLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool over, bool down)
{
    drawButtonBackground(g, button, {}, over, down);
    const bool active = button.getToggleState() || down;
    g.setColour((active ? background() : foreground()).withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.35f));
    g.setFont(mono(juce::jlimit(8.0f, 12.0f, button.getHeight() * 0.42f), true));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(4, 2), juce::Justification::centred, 1);
}

void FamilyLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                         float pos, float, float, juce::Slider& slider)
{
    const auto side = (float) juce::jmin(w, h);
    auto r = juce::Rectangle<float>((float)x + ((float)w - side) * 0.5f,
                                    (float)y + ((float)h - side) * 0.5f, side, side).reduced(2.0f);
    const auto fg = foreground().withMultipliedAlpha(slider.isEnabled() ? 1.0f : 0.35f);
    g.setColour(background()); g.fillRect(r);
    g.setColour(fg); g.drawRect(r, 2.0f);
    auto inner = r.reduced(8.0f);
    g.setColour(fg.withAlpha(0.12f)); g.fillRect(inner);
    for (int i = 1; i < 4; ++i)
    {
        const float p = (float)i / 4.0f;
        g.setColour(fg.withAlpha(0.16f));
        g.drawVerticalLine(juce::roundToInt(inner.getX() + inner.getWidth() * p), inner.getY(), inner.getBottom());
        g.drawHorizontalLine(juce::roundToInt(inner.getY() + inner.getHeight() * p), inner.getX(), inner.getRight());
    }
    const float fillH = inner.getHeight() * pos;
    g.setColour(fg.withAlpha(0.88f));
    g.fillRect(inner.getX(), inner.getBottom() - fillH, inner.getWidth(), fillH);
    const float marker = juce::jmax(4.0f, side * 0.07f);
    g.setColour(fg);
    g.fillRect(inner.getX() + pos * (inner.getWidth() - marker), inner.getY() - marker * 0.5f, marker, marker);
}

void FamilyLookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    const auto fg = foreground().withMultipliedAlpha(box.isEnabled() ? 1.0f : 0.35f);
    g.setColour(background()); g.fillRect(0, 0, w, h);
    g.setColour(fg); g.drawRect(0, 0, w, h, 2);
    juce::Path triangle;
    triangle.addTriangle((float)w - 14.0f, h * 0.40f, (float)w - 5.0f, h * 0.40f, (float)w - 9.5f, h * 0.67f);
    g.fillPath(triangle);
}

DefaultEqualizerAudioProcessorEditor::DefaultEqualizerAudioProcessorEditor(DefaultEqualizerAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), proc(p), responseCurve(p),
      levelMeter(p.meterPeakL, p.meterPeakR, p.meterRmsL, p.meterRmsR)
{
    juce::PropertiesFile::Options options;
    options.applicationName = "default_equalizer";
    options.filenameSuffix = "settings";
    options.folderName = "icanseesounds";
    options.osxLibrarySubFolder = "Application Support";
    uiPreferences = std::make_unique<juce::PropertiesFile>(options);
    darkTheme = uiPreferences->getBoolValue("darkTheme", true);
    familyLook.setDark(darkTheme);
    setLookAndFeel(&familyLook);
    responseCurve.setDarkMode(darkTheme);

    setResizable(true, true);
    setResizeLimits(720, 519, 1200, 900);
    setSize(juce::jlimit(720, 1200, uiPreferences->getIntValue("windowWidth", 860)),
            juce::jlimit(519, 865, uiPreferences->getIntValue("windowHeight", 620)));

    addAndMakeVisible(responseCurve);
    addAndMakeVisible(levelMeter);

    auto addButton = [this](auto& button) { addAndMakeVisible(button); };
    addButton(themeBtn); addButton(previousBandBtn); addButton(bandContextBtn); addButton(nextBandBtn);
    addButton(powerBtn); addButton(abBtn); addButton(copyABBtn);
    addButton(bandPageBtn); addButton(dynamicPageBtn); addButton(drivePageBtn); addButton(analyzerPageBtn); addButton(matchPageBtn); addButton(globalPageBtn);
    addButton(bandOn); addButton(bandSolo); addButton(adaptiveQBtn); addButton(deleteBandBtn);
    addButton(dynOn); addButton(dynLookahead); addButton(sidechainAudition); addButton(driveOn); addButton(analyzerVisible); addButton(spectrumFreeze);
    addButton(analyzerPeakHold); addButton(pianoOverlay);
    addButton(reducedMotionBtn); addButton(linearPhaseBtn); addButton(decrampBtn);
    addButton(matchCaptureBtn); addButton(matchApplyBtn); addButton(matchCommitBtn); addButton(matchClearBtn);
    addButton(undoBtn); addButton(redoBtn);

    auto addCombo = [this](juce::ComboBox& box) { box.setJustificationType(juce::Justification::centred); addAndMakeVisible(box); };
    for (auto* type : { "BELL", "LOW SHELF", "HIGH SHELF", "LOW CUT", "HIGH CUT", "BAND PASS", "NOTCH" }) typeBox.addItem(type, typeBox.getNumItems() + 1);
    for (auto* route : { "STEREO", "LEFT", "RIGHT", "MID", "SIDE" }) channelBox.addItem(route, channelBox.getNumItems() + 1);
    dynModeBox.addItemList({ "DOWN", "UP" }, 1);
    sidechainBox.addItemList({ "INT SC", "EXT SC" }, 1);
    saturationBox.addItemList({ "TANH", "TUBE", "TAPE", "TRANSISTOR", "MORPH SOFT", "HARD CLIP", "RECURSIVE FOLD", "SINE FOLD" }, 1);
    oversamplingBox.addItemList({ "OFF", "2X", "4X", "8X" }, 1);
    analyzerSourceBox.addItemList({ "INPUT", "OUTPUT", "BOTH" }, 1);
    analyzerResolutionBox.addItemList({ "RTA: LOW", "RTA: MED", "RTA: HIGH" }, 1);
    linearQualityBox.addItemList({ "LINEAR: ECON", "LINEAR: STD", "LINEAR: HIGH" }, 1);
    autoGainBox.addItemList({ "AUTO: OFF", "AUTO: REG", "AUTO: SMART" }, 1);
    addCombo(typeBox); addCombo(channelBox); addCombo(dynModeBox); addCombo(sidechainBox);
    addCombo(saturationBox); addCombo(oversamplingBox); addCombo(analyzerSourceBox); addCombo(analyzerResolutionBox); addCombo(linearQualityBox); addCombo(autoGainBox);

    for (auto* slider : { &slopeSlider, &dynThreshold, &dynRange, &dynRatio, &dynAttack,
                          &dynRelease, &driveSlider, &driveMixSlider, &driveOutputSlider, &outputSlider,
                          &matchAmount, &matchSmoothing, &matchLow, &matchHigh, &matchTime,
                          &analyzerRange, &analyzerFloor, &analyzerSpeed, &analyzerAveraging, &analyzerTilt })
        initParameter(*slider, {});
    slopeSlider.setName("SLOPE"); dynThreshold.setName("THRESHOLD"); dynRange.setName("RANGE");
    dynRatio.setName("RATIO"); dynAttack.setName("ATTACK"); dynRelease.setName("RELEASE");
    driveSlider.setName("DRIVE"); driveMixSlider.setName("MIX"); driveOutputSlider.setName("COMP");
    outputSlider.setName("OUTPUT");
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
    driveSlider.textFromValueFunction = driveMixSlider.textFromValueFunction = [](double v) { return juce::String(v, 1) + "%"; };
    driveOutputSlider.textFromValueFunction = outputSlider.textFromValueFunction = [](double v) { return cleanDb(v); };

    themeBtn.setTooltip("Toggle exact paper/ink inversion");
    dynOn.setTooltip("Enable dynamic processing for the selected band");
    dynModeBox.setTooltip("Downward or upward dynamic EQ");
    sidechainBox.setTooltip("Internal frequency-filtered detector or host sidechain");
    dynLookahead.setTooltip("Use 5 ms lookahead and report its latency to the host");
    sidechainAudition.setTooltip("Momentarily listen to this band's filtered detector signal");
    themeBtn.onClick = [this]
    {
        darkTheme = !darkTheme;
        familyLook.setDark(darkTheme); responseCurve.setDarkMode(darkTheme);
        uiPreferences->setValue("darkTheme", darkTheme);
        sendLookAndFeelChange(); repaint();
    };
    previousBandBtn.onClick = [this] { selectBand((selectedBand + kNumBands - 1) % kNumBands); };
    nextBandBtn.onClick = [this] { selectBand((selectedBand + 1) % kNumBands); };
    bandContextBtn.onClick = [this] { setWorkspacePage(WorkspacePage::Band); };
    abBtn.onClick = [this] { toggleAB(); };
    copyABBtn.onClick = [this] { proc.storeSnapshot(proc.isSlotA); proc.copySnapshot(proc.isSlotA); };
    bandPageBtn.onClick = [this] { setWorkspacePage(WorkspacePage::Band); };
    dynamicPageBtn.onClick = [this] { setWorkspacePage(WorkspacePage::Dynamic); };
    drivePageBtn.onClick = [this] { setWorkspacePage(WorkspacePage::Drive); };
    analyzerPageBtn.onClick = [this] { setWorkspacePage(WorkspacePage::Analyzer); };
    matchPageBtn.onClick = [this] { setWorkspacePage(WorkspacePage::Match); };
    globalPageBtn.onClick = [this] { setWorkspacePage(WorkspacePage::Global); };
    deleteBandBtn.onClick = [this]
    {
        proc.undoManager.beginNewTransaction("Delete EQ band");
        if (auto* parameter = proc.apvts.getParameter(bandId(selectedBand + 1, "on")))
        {
            parameter->beginChangeGesture(); parameter->setValueNotifyingHost(0.0f); parameter->endChangeGesture();
        }
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
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    matchCaptureBtn.onClick = [this]
    {
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
    };
    responseCurve.setAnalyzerVisible(analyzerVisible.getToggleState());
    analyzerSourceBox.setSelectedId(uiPreferences->getIntValue("analyzerSource", 3), juce::dontSendNotification);
    analyzerSourceBox.onChange = [this]
    {
        const int id = analyzerSourceBox.getSelectedId();
        uiPreferences->setValue("analyzerSource", id);
        responseCurve.setAnalyzerSources(id == 1 || id == 3, id == 2 || id == 3);
    };
    analyzerSourceBox.onChange();
    spectrumFreeze.setClickingTogglesState(true);
    spectrumFreeze.onClick = [this] { responseCurve.setSpectrumFrozen(spectrumFreeze.getToggleState()); };
    reducedMotionBtn.setClickingTogglesState(true);
    reducedMotionBtn.setToggleState(uiPreferences->getBoolValue("reducedMotion", false), juce::dontSendNotification);
    reducedMotionBtn.onClick = [this] { uiPreferences->setValue("reducedMotion", reducedMotionBtn.getToggleState()); };
    analyzerPeakHold.setClickingTogglesState(true);
    analyzerPeakHold.setToggleState(uiPreferences->getBoolValue("analyzerPeakHold", false), juce::dontSendNotification);
    pianoOverlay.setClickingTogglesState(true);
    pianoOverlay.setToggleState(uiPreferences->getBoolValue("pianoOverlay", false), juce::dontSendNotification);
    analyzerResolutionBox.setSelectedId(uiPreferences->getIntValue("analyzerResolution", 3), juce::dontSendNotification);
    const auto analyzerChanged = [this] { updateAnalyzerSettings(); };
    analyzerPeakHold.onClick = analyzerChanged; pianoOverlay.onClick = analyzerChanged;
    analyzerResolutionBox.onChange = analyzerChanged;
    analyzerRange.onValueChange = analyzerChanged; analyzerFloor.onValueChange = analyzerChanged;
    analyzerSpeed.onValueChange = analyzerChanged; analyzerAveraging.onValueChange = analyzerChanged;
    analyzerTilt.onValueChange = analyzerChanged;
    updateAnalyzerSettings();

    powerAtt = std::make_unique<ButtonAttachment>(proc.apvts, "plugin_enabled", powerBtn);
    autoGainAtt = std::make_unique<ComboAttachment>(proc.apvts, "auto_gain_mode", autoGainBox);
    adaptiveQAtt = std::make_unique<ButtonAttachment>(proc.apvts, "adaptive_q", adaptiveQBtn);
    linearPhaseAtt = std::make_unique<ButtonAttachment>(proc.apvts, "linear_phase", linearPhaseBtn);
    linearQualityAtt = std::make_unique<ComboAttachment>(proc.apvts, "linear_quality", linearQualityBox);
    decrampAtt = std::make_unique<ButtonAttachment>(proc.apvts, "decramp", decrampBtn);
    oversamplingAtt = std::make_unique<ComboAttachment>(proc.apvts, "oversampling", oversamplingBox);
    outputAtt = std::make_unique<SliderAttachment>(proc.apvts, "output_gain", outputSlider);

    proc.storeSnapshot(true);
    selectBand(0);
    setWorkspacePage(WorkspacePage::Band);
    startTimerHz(30);
}

DefaultEqualizerAudioProcessorEditor::~DefaultEqualizerAudioProcessorEditor()
{
    proc.sidechainAuditionBand.store(-1, std::memory_order_release);
    proc.soloBand.store(-1, std::memory_order_release);
    if (uiPreferences)
    {
        uiPreferences->setValue("windowWidth", getWidth());
        uiPreferences->setValue("windowHeight", getHeight());
        uiPreferences->saveIfNeeded();
    }
    setLookAndFeel(nullptr);
}

void DefaultEqualizerAudioProcessorEditor::initParameter(juce::Slider& slider, const juce::String& name)
{
    slider.setName(name);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 17);
    slider.setDoubleClickReturnValue(false, 0.0);
    addAndMakeVisible(slider);
}

void DefaultEqualizerAudioProcessorEditor::rebindBandControls()
{
    bandOnAtt.reset(); typeAtt.reset(); channelAtt.reset(); slopeAtt.reset();
    dynOnAtt.reset(); dynLookaheadAtt.reset(); dynModeAtt.reset(); sidechainAtt.reset(); dynThresholdAtt.reset(); dynRangeAtt.reset();
    dynRatioAtt.reset(); dynAttackAtt.reset(); dynReleaseAtt.reset(); driveOnAtt.reset(); driveAtt.reset();
    driveMixAtt.reset(); driveOutputAtt.reset(); saturationAtt.reset();
    const int idx = selectedBand + 1;
    bandOnAtt = std::make_unique<ButtonAttachment>(proc.apvts, bandId(idx, "on"), bandOn);
    typeAtt = std::make_unique<ComboAttachment>(proc.apvts, bandId(idx, "type"), typeBox);
    channelAtt = std::make_unique<ComboAttachment>(proc.apvts, bandId(idx, "ch"), channelBox);
    slopeAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "slope"), slopeSlider);
    dynOnAtt = std::make_unique<ButtonAttachment>(proc.apvts, bandId(idx, "dyn_on"), dynOn);
    dynLookaheadAtt = std::make_unique<ButtonAttachment>(proc.apvts, bandId(idx, "dyn_lookahead"), dynLookahead);
    dynModeAtt = std::make_unique<ComboAttachment>(proc.apvts, bandId(idx, "dyn_mode"), dynModeBox);
    sidechainAtt = std::make_unique<ComboAttachment>(proc.apvts, bandId(idx, "sc_source"), sidechainBox);
    dynThresholdAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_thresh"), dynThreshold);
    dynRangeAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_range"), dynRange);
    dynRatioAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_ratio"), dynRatio);
    dynAttackAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_attack"), dynAttack);
    dynReleaseAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_release"), dynRelease);
    driveOnAtt = std::make_unique<ButtonAttachment>(proc.apvts, bandId(idx, "drive_on"), driveOn);
    driveAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "drive"), driveSlider);
    driveMixAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "drive_mix"), driveMixSlider);
    driveOutputAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "drive_output"), driveOutputSlider);
    saturationAtt = std::make_unique<ComboAttachment>(proc.apvts, bandId(idx, "sat_mode"), saturationBox);
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
    if (autoGainBox.getSelectedItemIndex() == 2)
        autoGainBox.setTooltip(proc.smartAutoGainLocked.load(std::memory_order_acquire) ? "Smart Auto Gain: locked" : "Smart Auto Gain: analysing");
}

void DefaultEqualizerAudioProcessorEditor::setWorkspacePage(WorkspacePage page)
{
    workspacePage = page;
    bandPageBtn.setToggleState(page == WorkspacePage::Band, juce::dontSendNotification);
    dynamicPageBtn.setToggleState(page == WorkspacePage::Dynamic, juce::dontSendNotification);
    drivePageBtn.setToggleState(page == WorkspacePage::Drive, juce::dontSendNotification);
    analyzerPageBtn.setToggleState(page == WorkspacePage::Analyzer, juce::dontSendNotification);
    matchPageBtn.setToggleState(page == WorkspacePage::Match, juce::dontSendNotification);
    globalPageBtn.setToggleState(page == WorkspacePage::Global, juce::dontSendNotification);

    const bool band = page == WorkspacePage::Band, dyn = page == WorkspacePage::Dynamic;
    const bool drive = page == WorkspacePage::Drive, analyzer = page == WorkspacePage::Analyzer;
    const bool match = page == WorkspacePage::Match;
    const bool global = page == WorkspacePage::Global;
    const std::array<juce::Component*, 7> bandComponents { &bandOn, &bandSolo, &adaptiveQBtn, &deleteBandBtn,
                                                           &typeBox, &channelBox, &slopeSlider };
    const std::array<juce::Component*, 10> dynComponents { &dynOn, &dynLookahead, &sidechainAudition, &dynModeBox, &sidechainBox, &dynThreshold,
                                                          &dynRange, &dynRatio, &dynAttack, &dynRelease };
    const std::array<juce::Component*, 6> driveComponents { &driveOn, &driveSlider, &driveMixSlider,
                                                            &driveOutputSlider, &saturationBox, &oversamplingBox };
    const std::array<juce::Component*, 11> analyzerComponents { &analyzerVisible, &spectrumFreeze, &analyzerPeakHold, &pianoOverlay,
        &analyzerSourceBox, &analyzerResolutionBox, &analyzerRange, &analyzerFloor, &analyzerSpeed, &analyzerAveraging, &analyzerTilt };
    const std::array<juce::Component*, 8> globalComponents { &reducedMotionBtn, &linearPhaseBtn, &linearQualityBox, &decrampBtn,
        &undoBtn, &redoBtn, &outputSlider, &levelMeter };
    const std::array<juce::Component*, 9> matchComponents { &matchCaptureBtn, &matchApplyBtn, &matchCommitBtn, &matchClearBtn,
        &matchAmount, &matchSmoothing, &matchLow, &matchHigh, &matchTime };
    for (auto* component : bandComponents) component->setVisible(band);
    for (auto* component : dynComponents) component->setVisible(dyn);
    for (auto* component : driveComponents) component->setVisible(drive);
    for (auto* component : analyzerComponents) component->setVisible(analyzer);
    for (auto* component : globalComponents) component->setVisible(component == &levelMeter ? true : global);
    for (auto* component : matchComponents) component->setVisible(match);
    resized(); repaint();
}

void DefaultEqualizerAudioProcessorEditor::updateHeaderText()
{
    static const char* types[] = { "BELL", "LOW SHELF", "HIGH SHELF", "LOW CUT", "HIGH CUT", "BAND PASS", "NOTCH" };
    const int idx = selectedBand + 1;
    const int type = std::clamp((int) proc.apvts.getRawParameterValue(bandId(idx, "type"))->load(), 0, 6);
    const float freq = proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load();
    const auto freqText = freq >= 1000.0f ? juce::String(freq / 1000.0f, 2) + "k" : juce::String(freq, 0);
    bandContextBtn.setButtonText("B" + juce::String(idx) + "  " + types[type] + "  " + freqText);
    powerBtn.setButtonText(powerBtn.getToggleState() ? "ON" : "OFF");
}

void DefaultEqualizerAudioProcessorEditor::timerCallback()
{
    const double sampleRate = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    if (proc.preSpectrumFifo.processIfReady())
        responseCurve.pushSpectrumData(proc.preSpectrumFifo.getMagnitudes(), proc.preSpectrumFifo.getNumBins(), sampleRate, true);
    if (proc.spectrumFifo.processIfReady())
        responseCurve.pushSpectrumData(proc.spectrumFifo.getMagnitudes(), proc.spectrumFifo.getNumBins(), sampleRate, false);
    const int curveSelection = responseCurve.getSelectedBand();
    if (curveSelection >= 0 && curveSelection != selectedBand) selectBand(curveSelection, false);
    undoBtn.setEnabled(proc.undoManager.canUndo()); redoBtn.setEnabled(proc.undoManager.canRedo());
    updateHeaderText();
}

void DefaultEqualizerAudioProcessorEditor::toggleAB()
{
    proc.storeSnapshot(proc.isSlotA);
    proc.isSlotA = !proc.isSlotA;
    proc.recallSnapshot(proc.isSlotA);
    abBtn.setButtonText(proc.isSlotA ? "A" : "B");
    copyABBtn.setButtonText(proc.isSlotA ? "A>B" : "B>A");
    rebindBandControls();
}

void DefaultEqualizerAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto fg = familyLook.foreground(), bg = familyLook.background();
    const int headerH = juce::roundToInt(64.0f * getWidth() / 860.0f);
    const int workspaceH = juce::jlimit(142, 206, juce::roundToInt(getHeight() * 0.245f));
    g.fillAll(fg);
    g.setColour(bg); g.fillRect(0, 0, getWidth(), headerH);
    const int seamX = juce::jmin(220, getWidth() / 4);
    const int seam = juce::jmax(18, headerH * 3 / 8);
    g.setColour(fg); g.fillRect(seamX, 0, seam, seam); g.fillRect(seamX, headerH - seam, seam, seam);
    g.setColour(bg); g.fillRect(0, getHeight() - workspaceH, getWidth(), workspaceH);

    g.setFont(mono(9.0f, true));
    g.setColour(fg.withAlpha(0.55f));
    const std::array<juce::Slider*, 20> sliders { &slopeSlider, &dynThreshold, &dynRange, &dynRatio, &dynAttack,
                                                  &dynRelease, &driveSlider, &driveMixSlider, &driveOutputSlider, &outputSlider,
                                                  &matchAmount, &matchSmoothing, &matchLow, &matchHigh, &matchTime,
                                                  &analyzerRange, &analyzerFloor, &analyzerSpeed, &analyzerAveraging, &analyzerTilt };
    for (auto* slider : sliders)
        if (slider->isVisible())
            g.drawText(slider->getName(), slider->getX(), slider->getY() - 12, slider->getWidth(), 11, juce::Justification::centred);
}

void DefaultEqualizerAudioProcessorEditor::resized()
{
    const int w = getWidth(), h = getHeight();
    const int headerH = juce::roundToInt(64.0f * w / 860.0f);
    const int workspaceH = juce::jlimit(142, 206, juce::roundToInt(h * 0.245f));
    const int wordW = juce::jmin(220, w / 4);
    const int seamW = juce::jmax(18, headerH * 3 / 8);
    themeBtn.setBounds(0, 0, wordW, headerH);

    const int actionY = juce::jmax(8, (headerH - 40) / 2), actionH = juce::jmin(40, headerH - 12);
    powerBtn.setBounds(w - 88, actionY, 76, actionH);
    autoGainBox.setBounds(w - 194, actionY, 96, actionH);
    abBtn.setBounds(w - 246, actionY, 42, actionH);
    copyABBtn.setBounds(w - 298, actionY, 42, actionH);
    int navX = wordW + seamW + 8;
    previousBandBtn.setBounds(navX, actionY, 30, actionH); navX += 34;
    const int contextRight = w - 308;
    bandContextBtn.setBounds(navX, actionY, juce::jmax(100, contextRight - navX - 34), actionH);
    nextBandBtn.setBounds(contextRight - 30, actionY, 30, actionH);

    const int graphTop = headerH + 10;
    const int graphBottom = h - workspaceH - 10;
    responseCurve.setBounds(10, graphTop, w - 20, juce::jmax(160, graphBottom - graphTop));
    levelMeter.setBounds(w - 18, graphTop + 3, 6, juce::jmax(30, graphBottom - graphTop - 6));

    const int workspaceY = h - workspaceH;
    const int tabsY = workspaceY + 8, tabH = 28, tabW = juce::jlimit(74, 112, (w - 36) / 7);
    int tx = 10;
    bandPageBtn.setBounds(tx, tabsY, tabW, tabH); tx += tabW + 4;
    dynamicPageBtn.setBounds(tx, tabsY, tabW, tabH); tx += tabW + 4;
    drivePageBtn.setBounds(tx, tabsY, tabW, tabH); tx += tabW + 4;
    analyzerPageBtn.setBounds(tx, tabsY, tabW, tabH);
    tx += tabW + 4;
    matchPageBtn.setBounds(tx, tabsY, tabW, tabH);
    tx += tabW + 4;
    globalPageBtn.setBounds(tx, tabsY, tabW, tabH);

    const int contentY = tabsY + tabH + 18;
    const int contentH = h - contentY - 8;
    const int controlH = juce::jmax(34, contentH - 2);
    const int gap = juce::jmax(4, w / 170);

    if (workspacePage == WorkspacePage::Band)
    {
        int x = 10;
        bandOn.setBounds(x, contentY + 12, 64, 34); x += 64 + gap;
        bandSolo.setBounds(x, contentY + 12, 64, 34); x += 64 + gap;
        typeBox.setBounds(x, contentY + 12, juce::jlimit(112, 156, w / 6), 34); x += typeBox.getWidth() + gap;
        slopeSlider.setBounds(x, contentY, juce::jlimit(78, 100, contentH), controlH); x += slopeSlider.getWidth() + gap;
        channelBox.setBounds(x, contentY + 12, juce::jlimit(92, 126, w / 8), 34); x += channelBox.getWidth() + gap;
        adaptiveQBtn.setBounds(x, contentY + 12, juce::jlimit(96, 126, w / 8), 34); x += adaptiveQBtn.getWidth() + gap;
        deleteBandBtn.setBounds(x, contentY + 12, juce::jmax(70, w - x - 10), 34);
    }
    else if (workspacePage == WorkspacePage::Dynamic)
    {
        int x = 10;
        const int smallW = juce::jlimit(72, 92, w / 10);
        dynOn.setBounds(x, contentY + 12, smallW, 34); x += smallW + gap;
        dynModeBox.setBounds(x, contentY + 12, smallW, 34); x += smallW + gap;
        sidechainBox.setBounds(x, contentY + 12, smallW, 34); x += smallW + gap;
        dynLookahead.setBounds(x, contentY + 12, smallW, 34); x += smallW + gap;
        sidechainAudition.setBounds(x, contentY + 12, smallW, 34); x += smallW + gap;
        const int knobW = juce::jmax(58, (w - x - 10 - gap * 4) / 5);
        for (auto* slider : { &dynThreshold, &dynRange, &dynRatio, &dynAttack, &dynRelease })
        { slider->setBounds(x, contentY, knobW, controlH); x += knobW + gap; }
    }
    else if (workspacePage == WorkspacePage::Drive)
    {
        int x = 10;
        driveOn.setBounds(x, contentY + 12, 92, 34); x += 92 + gap;
        saturationBox.setBounds(x, contentY + 12, 118, 34); x += 118 + gap;
        const int knobW = juce::jlimit(72, 102, contentH);
        for (auto* slider : { &driveSlider, &driveMixSlider, &driveOutputSlider })
        { slider->setBounds(x, contentY, knobW, controlH); x += knobW + gap; }
        oversamplingBox.setBounds(x, contentY + 12, 92, 34);
    }
    else if (workspacePage == WorkspacePage::Analyzer)
    {
        int x = 10;
        const int buttonW = juce::jlimit(70, 92, w / 10);
        analyzerVisible.setBounds(x, contentY + 2, buttonW, 30); analyzerSourceBox.setBounds(x, contentY + 36, buttonW, 30); x += buttonW + gap;
        spectrumFreeze.setBounds(x, contentY + 2, buttonW, 30); analyzerResolutionBox.setBounds(x, contentY + 36, buttonW, 30); x += buttonW + gap;
        analyzerPeakHold.setBounds(x, contentY + 2, buttonW, 30); pianoOverlay.setBounds(x, contentY + 36, buttonW, 30); x += buttonW + gap;
        const int knobW = juce::jmax(54, (w - x - 10 - gap * 4) / 5);
        for (auto* slider : { &analyzerRange, &analyzerFloor, &analyzerSpeed, &analyzerAveraging, &analyzerTilt })
        { slider->setBounds(x, contentY, knobW, controlH); x += knobW + gap; }
    }
    else if (workspacePage == WorkspacePage::Match)
    {
        int x = 10;
        const int buttonW = juce::jlimit(78, 112, w / 8);
        matchCaptureBtn.setBounds(x, contentY + 2, buttonW, 30); matchApplyBtn.setBounds(x, contentY + 36, buttonW, 30); x += buttonW + gap;
        matchCommitBtn.setBounds(x, contentY + 2, buttonW + 18, 30); matchClearBtn.setBounds(x, contentY + 36, buttonW + 18, 30); x += buttonW + 18 + gap;
        const int knobW = juce::jmax(54, (w - x - 10 - gap * 4) / 5);
        for (auto* slider : { &matchAmount, &matchSmoothing, &matchLow, &matchHigh, &matchTime })
        { slider->setBounds(x, contentY, knobW, controlH); x += knobW + gap; }
    }
    else
    {
        int x = 10;
        const int buttonW = juce::jlimit(86, 112, w / 8);
        linearPhaseBtn.setBounds(x, contentY + 2, buttonW, 30); linearQualityBox.setBounds(x, contentY + 36, buttonW, 30); x += buttonW + gap;
        decrampBtn.setBounds(x, contentY + 2, buttonW, 30); reducedMotionBtn.setBounds(x, contentY + 36, buttonW, 30); x += buttonW + gap;
        outputSlider.setBounds(x, contentY, juce::jlimit(72, 96, contentH), controlH); x += outputSlider.getWidth() + gap;
        const int remaining = juce::jmax(116, w - x - 10);
        const int actionW = (remaining - 4) / 2;
        undoBtn.setBounds(x, contentY + 18, actionW, 38);
        redoBtn.setBounds(x + actionW + 4, contentY + 18, actionW, 38);
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
    uiPreferences->setValue("pianoOverlay", pianoOverlay.getToggleState());
    proc.preSpectrumFifo.setResolution(analyzerResolutionBox.getSelectedItemIndex());
    proc.spectrumFifo.setResolution(analyzerResolutionBox.getSelectedItemIndex());
    responseCurve.setAnalyzerSettings((float)analyzerFloor.getValue(), (float)analyzerRange.getValue(),
        (float)analyzerSpeed.getValue(), (float)analyzerAveraging.getValue(),
        analyzerResolutionBox.getSelectedItemIndex(), (float)analyzerTilt.getValue(),
        analyzerPeakHold.getToggleState(), pianoOverlay.getToggleState());
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
