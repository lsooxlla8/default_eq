#include "PluginEditor.h"
#include "UI/DriveCharacterFormatting.h"
#include "DSP/FilterTypes.h"
#include <numeric>

namespace
{
constexpr int minimumEditorWidth = 800;
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

juce::String bandId(int idx, const juce::String& suffix)
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
    return juce::jlimit(1.0f, 1.25f, (float)width / 860.0f);
}

int workspaceHeightForWidth(int width) noexcept
{
    return juce::roundToInt(104.0f * layoutScaleForWidth(width));
}

int wordmarkWidthForWidth(int width) noexcept
{
    return juce::roundToInt(178.0f * layoutScaleForWidth(width));
}
}

void VerticalDragSlider::mouseDown(const juce::MouseEvent& event)
{
    dragStartValue = getValue();
    ResettableSlider::mouseDown(event);
}

void VerticalDragSlider::mouseDrag(const juce::MouseEvent& event)
{
    if (!isEnabled()) return;
    const double range = getMaximum() - getMinimum();
    const double next = dragStartValue
        - (double)event.getDistanceFromDragStartY() * range
            / (double)juce::jmax(1, getMouseDragSensitivity());
    setValue(juce::jlimit(getMinimum(), getMaximum(), next), juce::sendNotificationSync);
}

void TwoAxisDragSlider::mouseDown(const juce::MouseEvent& event)
{
    dragStartValue = getValue();
    ResettableSlider::mouseDown(event);
}

void TwoAxisDragSlider::mouseDrag(const juce::MouseEvent& event)
{
    if (!isEnabled()) return;
    const int dx = event.getDistanceFromDragStartX();
    const int dy = event.getDistanceFromDragStartY();
    // Use the dominant axis so horizontal and vertical drags have identical
    // sensitivity without a diagonal gesture accidentally running twice as fast.
    const int directedDistance = std::abs(dx) >= std::abs(dy) ? dx : -dy;
    const double range = getMaximum() - getMinimum();
    const double next = dragStartValue + (double)directedDistance * range
        / (double)juce::jmax(1, getMouseDragSensitivity());
    setValue(juce::jlimit(getMinimum(), getMaximum(), next), juce::sendNotificationSync);
}

void DriveCharacterSlider::setSaturationMode(int newMode)
{
    saturationMode = juce::jlimit(0, kSaturationModeCount - 1, newMode);
    updateText();
}

juce::String DriveCharacterSlider::getTextFromValue(double raw)
{
    return deq::ui::formatDriveCharacter(saturationMode, raw);
}

ThresholdMeterSlider::ThresholdMeterSlider()
{
    setName("THRES");
    setSliderStyle(juce::Slider::LinearVertical);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setSliderSnapsToMousePosition(false);
    setMouseDragSensitivity(240);
    valueLabel.setJustificationType(juce::Justification::centred);
    valueLabel.setEditable(false, true, false);
    valueLabel.setInterceptsMouseClicks(false, false);
    valueLabel.getProperties().set("rotaryValueLabel", true);
    addAndMakeVisible(valueLabel);
    onValueChange = [this] { updateText(); };
    valueLabel.onTextChange = [this]
    {
        if (updating) return;
        const double parsed = parseUnitValue(valueLabel.getText());
        if (std::isfinite(parsed))
            setValue(getNormalisableRange().snapToLegalValue(
                juce::jlimit(getMinimum(), getMaximum(), parsed)), juce::sendNotificationSync);
        updateText();
    };
    updateText();
}

void ThresholdMeterSlider::setInputLevelsDb(float leftDb, float rightDb)
{
    displayedLevelDbL = std::max(std::clamp(leftDb, -60.0f, 0.0f), displayedLevelDbL - 1.5f);
    displayedLevelDbR = std::max(std::clamp(rightDb, -60.0f, 0.0f), displayedLevelDbR - 1.5f);
    repaint();
}

void ThresholdMeterSlider::paint(juce::Graphics& g)
{
    const auto fg = findColour(default_family::LookAndFeel::foregroundColourId, true)
        .withMultipliedAlpha(isEnabled() ? 1.0f : default_family::metrics::disabledOpacity);
    const auto bg = findColour(default_family::LookAndFeel::backgroundColourId, true);
    auto bounds = getLocalBounds();
    auto valueArea = bounds.removeFromBottom(18);
    auto meter = bounds.reduced(3, 1).toFloat();

    g.setColour(fg); g.fillRect(meter);
    auto inner = meter.reduced(2.0f);
    g.setColour(bg); g.fillRect(inner);
    g.setColour(fg.withAlpha(0.55f));
    const float laneGap = 2.0f;
    const float laneWidth = (inner.getWidth() - laneGap) * 0.5f;
    const auto drawLane = [&](float levelDb, float x)
    {
        const float level = juce::jmap(levelDb, -60.0f, 0.0f, 0.0f, 1.0f);
        g.fillRect(juce::Rectangle<float>(x, inner.getBottom() - inner.getHeight() * level,
                                         laneWidth, inner.getHeight() * level));
    };
    drawLane(displayedLevelDbL, inner.getX());
    drawLane(displayedLevelDbR, inner.getX() + laneWidth + laneGap);

    // Paint the attached parameter directly. Keeping a second display value
    // made the line stale while SliderAttachment was rebinding to a new band.
    const float threshold = (float)valueToProportionOfLength(getValue());
    const float thresholdY = inner.getBottom() - inner.getHeight() * threshold;
    g.setColour(fg);
    g.fillRect(meter.getX(), thresholdY - 1.0f, meter.getWidth(), 2.0f);
    juce::ignoreUnused(valueArea);
}

void ThresholdMeterSlider::updateText()
{
    juce::ScopedValueSetter<bool> guard(updating, true);
    valueLabel.setText(juce::String(getValue(), 1), juce::dontSendNotification);
}

void ThresholdMeterSlider::resized()
{
    valueLabel.setBounds(getLocalBounds().removeFromBottom(18));
}

void ThresholdMeterSlider::mouseDoubleClick(const juce::MouseEvent&)
{
    valueLabel.showEditor();
    if (auto* editor = valueLabel.getCurrentTextEditor()) editor->selectAll();
}

void ThresholdMeterSlider::lookAndFeelChanged()
{
    juce::Slider::lookAndFeelChanged();
    const auto fg = findColour(default_family::LookAndFeel::foregroundColourId, true);
    valueLabel.setColour(juce::Label::textColourId, fg);
    valueLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    valueLabel.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    updateText();
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
    valueLabel.setMinimumHorizontalScale(1.0f);
    valueLabel.getProperties().set("numericValueControl", true);
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
    valueLabel.setText(showValue ? getName() + " " + value : getName(), juce::dontSendNotification);
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
    updateText();
}

DefaultEqualizerAudioProcessorEditor::DefaultEqualizerAudioProcessorEditor(DefaultEqualizerAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), proc(p), responseCurve(p)
{
    juce::PropertiesFile::Options options;
    options.applicationName = "default_eq";
    options.filenameSuffix = "settings";
    options.folderName = "icanseesounds";
    options.osxLibrarySubFolder = "Application Support";
    uiPreferences = std::make_unique<juce::PropertiesFile>(options);
    darkTheme = !default_family::ThemePreferences::loadLight();
    familyLook.setDark(darkTheme);
    setLookAndFeel(&familyLook);
    responseCurve.setDarkMode(darkTheme);

    setResizable(true, true);
    addMouseListener(this, true);
    const int initialWidth = minimumEditorWidth;
    setResizeLimits(minimumEditorWidth, minimumEditorHeight, 1200, 900);
    setSize(initialWidth, 464);

    addAndMakeVisible(responseCurve);

    auto addButton = [this](auto& button) { addAndMakeVisible(button); };
    nextBrandGlitchTimeMs = juce::Time::getMillisecondCounterHiRes() + 4000.0;
    addButton(themeBtn); addButton(powerBtn);
    addButton(bandOn); addButton(bandSolo); addButton(adaptiveQBtn);
    addButton(dynModeBtn); addButton(sidechainBtn);

    auto addCombo = [this](juce::ComboBox& box) { box.setJustificationType(juce::Justification::centred); addAndMakeVisible(box); };
    for (auto* type : { "RES LOW CUT", "RES HIGH CUT", "NOTCH", "TILT", "BAND PASS",
                        "BELL", "LOW SHELF", "HIGH SHELF", "LOW CUT", "HIGH CUT" })
        typeBox.addItem(type, typeBox.getNumItems() + 1);
    saturationBox.addItemList({ "SOFT CLIP", "DIODE", "TRIODE", "TRANSISTOR",
                                "TAPE", "ODD / EVEN", "PHASE DISTORTION", "SINE EROSION" }, 1);
    phaseModeBox.addItemList({ "MIN PHASE", "LINEAR ECO", "LINEAR MED", "LINEAR HIGH" }, 1);
    placementModeBox.addItemList({ "L/R", "M/S", "T/S" }, 1);
    addCombo(typeBox);
    addCombo(placementModeBox); addCombo(saturationBox); addCombo(phaseModeBox);
    addAndMakeVisible(autoGainBtn);

    const std::array<juce::Slider*, 7> rotaryParameters {
        &dynRange, &dynSpeed,
        &driveSlider, &driveCharacterSlider, &amountSlider, &shiftSlider, &outputSlider
    };
    for (auto* slider : rotaryParameters)
        initParameter(*slider, {});
    addAndMakeVisible(dynRatio);
    dynRatio.setRange(1.0, 20.0, 0.1);
    dynRatio.setSkewFactor(0.5);
    dynRatio.setDoubleClickReturnValue(true, 4.0);
    dynRatio.setFormatter([](double v) { return juce::String(v, 2); },
                          [](const juce::String& s) { return parseUnitValue(s); });
    dynRatio.setValueVisible(true);
    dynThreshold.setDoubleClickReturnValue(true, 0.0);
    addAndMakeVisible(dynThreshold);
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
    placementSlider.setSliderSnapsToMousePosition(false);
    placementSlider.setMouseDragSensitivity(240);
    addAndMakeVisible(placementSlider);
    for (auto* field : { &freqField, &gainField, &qField, &slopeField })
        addAndMakeVisible(*field);
    freqField.setRange(20.0, 20000.0, 0.001); freqField.setSkewFactor(0.5);
    gainField.setRange(-36.0, 36.0, 0.01);
    qField.setRange(0.1, 24.0, 0.001); qField.setSkewFactor(0.5);
    slopeField.setRange(3.0, 96.0, 0.1);
    freqField.setFormatter(
        [this](double v)
        {
            const float shift = proc.apvts.getRawParameterValue("shift")->load();
            return juce::String(juce::roundToInt(
                DefaultEqualizerAudioProcessor::shiftedFrequency((float)v, shift)));
        },
        [this](const juce::String& s)
        {
            const float shift = proc.apvts.getRawParameterValue("shift")->load();
            return parseUnitValue(s, true)
                / DefaultEqualizerAudioProcessor::frequencyShiftRatio(shift);
        });
    gainField.setFormatter([](double v) { return cleanDb(v, 1); },
                           [](const juce::String& s) { return parseUnitValue(s); });
    qField.setFormatter([](double v) { return juce::String(v, 2); },
                        [](const juce::String& s) { return parseUnitValue(s); });
    slopeField.setFormatter([this](double v)
                            {
                                if(selectedBand>=0)
                                {
                                    const int type=(int)proc.apvts.getRawParameterValue(bandId(selectedBand+1,"type"))->load();
                                    if(!ResponseCurveComponent::isClassicCutType(type))
                                    {
                                        static constexpr double values[]={6,12,24,36,48,72,96};
                                        const int first=(type==2||type==4||type==5)?1:0; int best=first;
                                        for(int i=first+1;i<7;++i)if(std::abs(v-values[i])<std::abs(v-values[best]))best=i;
                                        v=values[best];
                                    }
                                }
                                return juce::String(v, 1) + " dB";
                            },
                            [](const juce::String& s) { return parseUnitValue(s); });
    slopeField.onDragEnd=[this]
    {
        if(selectedBand<0)return;
        const int type=(int)proc.apvts.getRawParameterValue(bandId(selectedBand+1,"type"))->load();
        if(ResponseCurveComponent::isClassicCutType(type))return;
        static constexpr double values[]={6,12,24,36,48,72,96};
        const int first=(type==2||type==4||type==5)?1:0; int best=first;
        for(int i=first+1;i<7;++i)if(std::abs(slopeField.getValue()-values[i])<std::abs(slopeField.getValue()-values[best]))best=i;
        slopeField.setValue(values[best],juce::sendNotificationSync);
    };
    dynRange.setName("RANGE");
    dynSpeed.setName("SPEED");
    driveSlider.setName("DRIVE"); driveCharacterSlider.setName("CHARACTER");
    outputSlider.setName("OUT");
    outputSlider.setFormatter([](double v) { return cleanDb(v, 1); },
                              [](const juce::String& s) { return parseUnitValue(s); });
    outputSlider.setValueVisible(true);
    outputSlider.setDoubleClickReturnValue(true, 0.0);
    shiftSlider.setName("SHIFT");
    shiftSlider.setFormatter([](double v)
                             {
                                 const double clean = std::abs(v) < 0.005 ? 0.0 : v;
                                 return juce::String(clean > 0.0 ? "+" : "")
                                    + juce::String(clean, 1);
                             },
                             [](const juce::String& s) { return parseUnitValue(s); });
    shiftSlider.setValueVisible(true);
    shiftSlider.setDoubleClickReturnValue(true, 0.0);
    amountSlider.setName("AMOUNT");
    amountSlider.setFormatter([](double v)
                              {
                                  const double percent = std::abs(v) < 0.005 ? 0.0 : v * 100.0;
                                  return juce::String(juce::roundToInt(percent)) + "%";
                              },
                              [](const juce::String& s) { return parseUnitValue(s) * 0.01; });
    amountSlider.setValueVisible(true);
    amountSlider.setDoubleClickReturnValue(true, 1.0);
    // RTA settings are intentionally retained as internal preferences even
    // though their controls are no longer part of the interface.
    if (!uiPreferences->getBoolValue("analyzerFloorDefault80", false))
    {
        if (!uiPreferences->containsKey("analyzerFloor")
            || std::abs(uiPreferences->getDoubleValue("analyzerFloor", -90.0) + 90.0) < 0.01)
            uiPreferences->setValue("analyzerFloor", -80.0);
        uiPreferences->setValue("analyzerFloorDefault80", true);
    }
    if (!uiPreferences->getBoolValue("analyzerAveragingSecondsV1", false))
    {
        uiPreferences->setValue("analyzerAveraging", 0.065);
        uiPreferences->setValue("analyzerAveragingSecondsV1", true);
    }
    if (!uiPreferences->getBoolValue("analyzerTiltDefault45", false))
    {
        uiPreferences->setValue("analyzerTilt", 4.5);
        uiPreferences->setValue("analyzerTiltDefault45", true);
    }
    uiPreferences->removeValue("analyzerRange");
    uiPreferences->removeValue("analyzerSpeed");
    responseCurve.setAnalyzerSettings(
        (float)uiPreferences->getDoubleValue("analyzerFloor", -80.0),
        (float)uiPreferences->getDoubleValue("analyzerAveraging", 0.065),
        (float)uiPreferences->getDoubleValue("analyzerTilt", 4.5));
    dynThreshold.textFromValueFunction = [](double v) { return cleanDb(v, 1); };
    dynRange.textFromValueFunction = [](double v) { return cleanDb(v, 1); };
    dynSpeed.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v)) + "%"; };
    dynSpeed.setTooltip("Linked attack/release speed: Slow 100/1000 ms, Fast 0.1/15 ms; default 75%");
    driveSlider.textFromValueFunction = [](double v) { return cleanDb(v, 1); };
    applySliderPalette();

    themeBtn.setTooltip("Toggle exact paper/ink inversion");
    dynModeBtn.setTooltip("Toggle downward or upward dynamic EQ");
    sidechainBtn.setTooltip("Toggle internal or external sidechain");
    dynModeBtn.onClick = [this]
    {
        const bool upward = proc.apvts.getRawParameterValue(
            bandId(selectedBand + 1, "dyn_mode"))->load(std::memory_order_relaxed) > 0.5f;
        applyAbsoluteToSelectedBands("dyn_mode", upward ? 0.0f : 1.0f);
    };
    sidechainBtn.onClick = [this]
    {
        const bool external = proc.apvts.getRawParameterValue(
            bandId(selectedBand + 1, "sc_source"))->load(std::memory_order_relaxed) > 0.5f;
        applyAbsoluteToSelectedBands("sc_source", external ? 0.0f : 1.0f);
    };
    dynLookahead.setTooltip("Click or drag from 0 to 5 ms; latency is reported to the host");
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
    typeBox.onChange = [this]
    {
        const int type = typeBox.getSelectedItemIndex();
        const bool groupUserChange = typeMouseInteraction;
        typeMouseInteraction = false;
        const bool typeChanged = type != displayedFilterType;
        displayedFilterType = type;
        if (groupUserChange) applyAbsoluteToSelectedBands("type", (float)type);
        if (typeChanged && selectedBand >= 0 && deq::filter_types::isResonantCutIndex(type))
        {
            if (groupUserChange)
                applyAbsoluteToSelectedBands("q", deq::filter_types::resonantCutDefaultQ);
            else if (auto* q = proc.apvts.getParameter(bandId(selectedBand + 1, "q")))
            {
                q->beginChangeGesture();
                q->setValueNotifyingHost(q->convertTo0to1(
                    deq::filter_types::resonantCutDefaultQ));
                q->endChangeGesture();
            }
        }
        slopeField.refreshText();
        if (ResponseCurveComponent::typeDefaultsToMidSide(type))
            if (auto* placementMode = proc.apvts.getParameter(bandId(selectedBand + 1, "placement_mode")))
            {
                placementMode->beginChangeGesture();
                placementMode->setValueNotifyingHost(placementMode->convertTo0to1(1.0f));
                placementMode->endChangeGesture();
            }
    };
    placementModeBox.onChange = [this]
    {
        const bool groupUserChange = placementModeMouseInteraction;
        placementModeMouseInteraction = false;
        if (groupUserChange)
            applyAbsoluteToSelectedBands("placement_mode",
                                         (float)placementModeBox.getSelectedItemIndex());
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
        const int requested = juce::jlimit(0, kSaturationModeCount - 1, saturationBox.getSelectedItemIndex());
        const bool userChangedMode = saturationMouseInteraction
            && displayedDriveMode >= 0 && requested != displayedDriveMode;
        saturationMouseInteraction = false;
        if (userChangedMode) applyAbsoluteToSelectedBands("sat_mode", (float)requested);
        displayedDriveMode = requested;
        updateDriveControls(userChangedMode);
    };
    bandOn.onClick = [this]
    {
        applyAbsoluteToSelectedBands("on", bandOn.getToggleState() ? 1.0f : 0.0f, false);
    };
    bandSolo.setClickingTogglesState(true);
    bandSolo.onClick = [this]
    {
        proc.soloBand.store(bandSolo.getToggleState() ? selectedBand : -1, std::memory_order_release);
    };
    responseCurve.setAnalyzerSources(true, true);
    proc.preSpectrumFifo.setResolution(2);
    proc.spectrumFifo.setResolution(2);
    responseCurve.resetPeakHold();
    for (auto* obsoletePreference : { "analyzerVisible", "analyzerPeakHold", "analyzerResolution",
                                      "analyzerResolutionV2", "analyzerResolutionV3" })
        uiPreferences->removeValue(obsoletePreference);
    powerAtt = std::make_unique<ButtonAttachment>(proc.apvts, "plugin_enabled", powerBtn);
    adaptiveQAtt = std::make_unique<ButtonAttachment>(proc.apvts, "adaptive_q", adaptiveQBtn);
    oversamplingAtt = std::make_unique<SliderAttachment>(proc.apvts, "oversampling", oversamplingSlider);
    amountAtt = std::make_unique<SliderAttachment>(proc.apvts, "scale", amountSlider);
    shiftAtt = std::make_unique<SliderAttachment>(proc.apvts, "shift", shiftSlider);
    outputAtt = std::make_unique<SliderAttachment>(proc.apvts, "output_gain", outputSlider);

    const int initialAutoMode = (int)proc.apvts.getRawParameterValue("auto_gain_mode")->load();
    autoGainBtn.setButtonText(initialAutoMode == 2 ? "SMART GAIN" : "AUTO GAIN");
    autoGainBtn.setToggleState(initialAutoMode > 0, juce::dontSendNotification);

    setWantsKeyboardFocus(true);
    // Prime the disabled band controls with their real parameter defaults,
    // then leave the graph and panel with no selected band.
    selectBand(0);
    selectBand(-1);
    for (auto* slider : { static_cast<juce::Slider*>(&placementSlider),
                          static_cast<juce::Slider*>(&freqField),
                          static_cast<juce::Slider*>(&gainField),
                          static_cast<juce::Slider*>(&qField),
                          static_cast<juce::Slider*>(&slopeField),
                          static_cast<juce::Slider*>(&dynLookahead),
                          static_cast<juce::Slider*>(&dynThreshold),
                          static_cast<juce::Slider*>(&dynRange),
                          static_cast<juce::Slider*>(&dynRatio),
                          static_cast<juce::Slider*>(&dynSpeed),
                          static_cast<juce::Slider*>(&driveSlider),
                          static_cast<juce::Slider*>(&driveCharacterSlider) })
        slider->addListener(this);

    uiPreferences->removeValue("workspaceExpanded");
    applySliderPalette();
    sendLookAndFeelChange();
    repaint();
    startTimerHz(30);
}

DefaultEqualizerAudioProcessorEditor::~DefaultEqualizerAudioProcessorEditor()
{
    for (auto* slider : { static_cast<juce::Slider*>(&placementSlider),
                          static_cast<juce::Slider*>(&freqField),
                          static_cast<juce::Slider*>(&gainField),
                          static_cast<juce::Slider*>(&qField),
                          static_cast<juce::Slider*>(&slopeField),
                          static_cast<juce::Slider*>(&dynLookahead),
                          static_cast<juce::Slider*>(&dynThreshold),
                          static_cast<juce::Slider*>(&dynRange),
                          static_cast<juce::Slider*>(&dynRatio),
                          static_cast<juce::Slider*>(&dynSpeed),
                          static_cast<juce::Slider*>(&driveSlider),
                          static_cast<juce::Slider*>(&driveCharacterSlider) })
        slider->removeListener(this);
    removeMouseListener(this);
    proc.soloBand.store(-1, std::memory_order_release);
    proc.uiMeterBand.store(-1, std::memory_order_release);
    proc.setAnalyzerEnabled(false);
    if (uiPreferences)
    {
        uiPreferences->removeValue("windowWidth");
        uiPreferences->removeValue("windowHeight");
        uiPreferences->saveIfNeeded();
    }
    setLookAndFeel(nullptr);
}

void DefaultEqualizerAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    const auto belongsTo = [&event](const juce::Component& parent)
    { return event.originalComponent == &parent || parent.isParentOf(event.originalComponent); };
    if (belongsTo(typeBox)) typeMouseInteraction = true;
    if (belongsTo(placementModeBox)) placementModeMouseInteraction = true;
    if (belongsTo(saturationBox)) saturationMouseInteraction = true;

    if (event.mods.isPopupMenu())
    {
        auto* component = event.originalComponent;
        auto* slider = dynamic_cast<juce::Slider*>(component);
        if (slider == nullptr && component != nullptr)
            slider = component->findParentComponentOfClass<juce::Slider>();
        if (slider != nullptr && slider->isEnabled())
        {
            const auto suffix = groupSuffixForSlider(slider);
            if (suffix.isNotEmpty() && responseCurve.getSelectionCount() > 1)
            {
                if (auto* primary = proc.apvts.getParameter(bandId(selectedBand + 1, suffix)))
                    applyAbsoluteToSelectedBands(suffix,
                        primary->convertFrom0to1(primary->getDefaultValue()));
            }
            slider->setValue(slider->getDoubleClickReturnValue(), juce::sendNotificationSync);
            return;
        }
    }
    if (!responseCurve.isNumericEditorComponent(event.originalComponent))
        responseCurve.dismissNumericEditor();
}

juce::String DefaultEqualizerAudioProcessorEditor::groupSuffixForSlider(
    const juce::Slider* slider) const
{
    if (slider == &placementSlider) return "placement";
    if (slider == &freqField) return "freq";
    if (slider == &gainField) return "gain";
    if (slider == &qField) return "q";
    if (slider == &slopeField) return "slope";
    if (slider == &dynLookahead) return "dyn_lookahead";
    if (slider == &dynThreshold) return "dyn_thresh";
    if (slider == &dynRange) return "dyn_range";
    if (slider == &dynRatio) return "dyn_ratio";
    if (slider == &dynSpeed) return "dyn_speed";
    if (slider == &driveSlider) return "drive";
    if (slider == &driveCharacterSlider) return "drive_character";
    return {};
}

void DefaultEqualizerAudioProcessorEditor::applyAbsoluteToSelectedBands(
    const juce::String& suffix, float value, bool includePrimary)
{
    if (applyingGroupEdit) return;
    juce::ScopedValueSetter<bool> guard(applyingGroupEdit, true);
    const auto& selected = responseCurve.getSelection();
    for (int band = 0; band < kNumBands; ++band)
        if (selected[(size_t)band] && (includePrimary || band != selectedBand))
            if (auto* parameter = proc.apvts.getParameter(bandId(band + 1, suffix)))
            {
                const auto legal = parameter->getNormalisableRange().snapToLegalValue(value);
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->convertTo0to1(legal));
                parameter->endChangeGesture();
            }
}

void DefaultEqualizerAudioProcessorEditor::beginGroupSliderEdit(juce::Slider& slider)
{
    if (selectedBand < 0 || responseCurve.getSelectionCount() < 2) return;
    const auto suffix = groupSuffixForSlider(&slider);
    if (suffix.isEmpty()) return;
    activeGroupSlider = &slider;
    activeGroupSuffix = suffix;
    groupPrimaryStart = (float)slider.getValue();
    groupParameters.fill(nullptr);
    const auto& selected = responseCurve.getSelection();
    proc.undoManager.beginNewTransaction("Adjust selected band parameters");
    for (int band = 0; band < kNumBands; ++band)
        if (selected[(size_t)band])
        {
            auto* parameter = proc.apvts.getParameter(bandId(band + 1, suffix));
            groupParameters[(size_t)band] = parameter;
            groupParameterStarts[(size_t)band] = parameter != nullptr
                ? proc.apvts.getRawParameterValue(bandId(band + 1, suffix))->load()
                : 0.0f;
            if (parameter != nullptr && band != selectedBand)
                parameter->beginChangeGesture();
        }
}

void DefaultEqualizerAudioProcessorEditor::sliderDragStarted(juce::Slider* slider)
{
    if (slider != nullptr) beginGroupSliderEdit(*slider);
}

void DefaultEqualizerAudioProcessorEditor::sliderValueChanged(juce::Slider* slider)
{
    if (slider == nullptr || slider != activeGroupSlider || applyingGroupEdit) return;
    juce::ScopedValueSetter<bool> guard(applyingGroupEdit, true);
    const float current = (float)slider->getValue();
    const bool multiplicative = activeGroupSuffix == "freq" || activeGroupSuffix == "q";
    const float ratio = std::abs(groupPrimaryStart) > 1.0e-9f
        ? current / groupPrimaryStart : 1.0f;
    const float delta = current - groupPrimaryStart;
    for (int band = 0; band < kNumBands; ++band)
        if (band != selectedBand)
            if (auto* parameter = groupParameters[(size_t)band])
            {
                const float raw = multiplicative
                    ? groupParameterStarts[(size_t)band] * ratio
                    : groupParameterStarts[(size_t)band] + delta;
                const auto legal = parameter->getNormalisableRange().snapToLegalValue(raw);
                parameter->setValueNotifyingHost(parameter->convertTo0to1(legal));
            }
}

void DefaultEqualizerAudioProcessorEditor::endGroupSliderEdit(juce::Slider& slider)
{
    if (&slider != activeGroupSlider) return;
    if (activeGroupSuffix == "slope")
    {
        static constexpr float values[] { 6, 12, 24, 36, 48, 72, 96 };
        for (int band = 0; band < kNumBands; ++band)
            if (auto* parameter = groupParameters[(size_t)band])
            {
                const int type = (int)proc.apvts.getRawParameterValue(
                    bandId(band + 1, "type"))->load();
                if (ResponseCurveComponent::isClassicCutType(type)) continue;
                const int first = (type == 2 || type == 4 || type == 5) ? 1 : 0;
                const float current = proc.apvts.getRawParameterValue(
                    bandId(band + 1, "slope"))->load();
                int best = first;
                for (int value = first + 1; value < 7; ++value)
                    if (std::abs(current - values[value]) < std::abs(current - values[best]))
                        best = value;
                parameter->setValueNotifyingHost(parameter->convertTo0to1(values[best]));
            }
    }
    for (int band = 0; band < kNumBands; ++band)
        if (band != selectedBand)
            if (auto* parameter = groupParameters[(size_t)band])
                parameter->endChangeGesture();
    groupParameters.fill(nullptr);
    activeGroupSlider = nullptr;
    activeGroupSuffix.clear();
}

void DefaultEqualizerAudioProcessorEditor::sliderDragEnded(juce::Slider* slider)
{
    if (slider != nullptr) endGroupSliderEdit(*slider);
}

int DefaultEqualizerAudioProcessorEditor::getControlParameterIndex(juce::Component& control)
{
    const auto parameterIndex = [this](const juce::String& id)
    {
        if (auto* parameter = proc.apvts.getParameter(id))
            return parameter->getParameterIndex();
        return -1;
    };

    for (auto* component = &control; component != nullptr && component != this;
         component = component->getParentComponent())
    {
        if (component == &powerBtn)          return parameterIndex("plugin_enabled");
        if (component == &adaptiveQBtn)      return parameterIndex("adaptive_q");
        if (component == &autoGainBtn)       return parameterIndex("auto_gain_mode");
        if (component == &phaseModeBox)
            return parameterIndex("linear_phase");
        if (component == &oversamplingSlider) return parameterIndex("oversampling");
        if (component == &amountSlider)       return parameterIndex("scale");
        if (component == &shiftSlider)        return parameterIndex("shift");
        if (component == &outputSlider)       return parameterIndex("output_gain");

        if (selectedBand < 0 || selectedBand >= kNumBands)
            continue;

        const int band = selectedBand + 1;
        const auto bandParameter = [&](const char* suffix)
        { return parameterIndex(bandId(band, suffix)); };

        if (component == &responseCurve || component == &freqField)
            return bandParameter("freq");
        if (component == &gainField)          return bandParameter("gain");
        if (component == &qField)             return bandParameter("q");
        if (component == &slopeField)         return bandParameter("slope");
        if (component == &bandOn)             return bandParameter("on");
        if (component == &typeBox)            return bandParameter("type");
        if (component == &placementModeBox)   return bandParameter("placement_mode");
        if (component == &placementSlider)    return bandParameter("placement");
        if (component == &dynModeBtn)         return bandParameter("dyn_mode");
        if (component == &sidechainBtn)       return bandParameter("sc_source");
        if (component == &dynThreshold)       return bandParameter("dyn_thresh");
        if (component == &dynRange)           return bandParameter("dyn_range");
        if (component == &dynRatio)           return bandParameter("dyn_ratio");
        if (component == &dynSpeed)           return bandParameter("dyn_speed");
        if (component == &dynLookahead)       return bandParameter("dyn_lookahead");
        if (component == &driveSlider)        return bandParameter("drive");
        if (component == &driveCharacterSlider) return bandParameter("drive_character");
        if (component == &saturationBox)      return bandParameter("sat_mode");
    }

    return -1;
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
    const std::array<juce::Slider*, 12> sliders {
        &placementSlider, &dynThreshold, &dynRange, &dynRatio, &dynSpeed,
        &dynLookahead, &driveSlider, &driveCharacterSlider,
        &amountSlider, &shiftSlider, &outputSlider,
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
    dynRatioAtt.reset(); dynSpeedAtt.reset(); driveAtt.reset();
    driveCharacterAtt.reset(); saturationAtt.reset();
    if (selectedBand < 0 || selectedBand >= kNumBands)
    {
        displayedFilterType = -1;
        displayedDriveMode = -1;
        driveFormatPending = false;
        return;
    }
    const int idx = selectedBand + 1;
    displayedFilterType = std::clamp((int)proc.apvts.getRawParameterValue(
        bandId(idx, "type"))->load(), 0, deq::filter_types::count - 1);
    bandOnAtt = std::make_unique<ButtonAttachment>(proc.apvts, bandId(idx, "on"), bandOn);
    typeAtt = std::make_unique<ComboAttachment>(proc.apvts, bandId(idx, "type"), typeBox);
    placementModeAtt = std::make_unique<ComboAttachment>(proc.apvts,
        bandId(idx, "placement_mode"), placementModeBox);
    placementAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "placement"), placementSlider);
    freqAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "freq"), freqField);
    gainAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "gain"), gainField);
    qAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "q"), qField);
    slopeAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "slope"), slopeField);
    dynLookaheadAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_lookahead"), dynLookahead);
    dynThresholdAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_thresh"), dynThreshold);
    dynRangeAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_range"), dynRange);
    dynRatioAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_ratio"), dynRatio);
    dynSpeedAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "dyn_speed"), dynSpeed);
    driveAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "drive"), driveSlider);
    driveCharacterAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "drive_character"), driveCharacterSlider);
    displayedDriveMode = std::clamp((int)proc.apvts.getRawParameterValue(bandId(idx, "sat_mode"))->load(),
                                      0, kSaturationModeCount - 1);
    saturationAtt = std::make_unique<ComboAttachment>(proc.apvts, bandId(idx, "sat_mode"), saturationBox);
    updateDriveControls(false);
    driveFormatPending = true;
}

void DefaultEqualizerAudioProcessorEditor::updateBandControlEnablement(bool bandSelected)
{
    for (auto* component : { static_cast<juce::Component*>(&bandOn),
                             static_cast<juce::Component*>(&bandSolo),
                             static_cast<juce::Component*>(&placementModeBox),
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
                             static_cast<juce::Component*>(&dynSpeed),
                             static_cast<juce::Component*>(&dynLookahead),
                             static_cast<juce::Component*>(&freqField),
                             static_cast<juce::Component*>(&gainField),
                             static_cast<juce::Component*>(&qField),
                             static_cast<juce::Component*>(&slopeField) })
        component->setEnabled(bandSelected);

    if (!bandSelected)
    {
        if (proc.soloBand.load(std::memory_order_acquire) == selectedBand)
            proc.soloBand.store(-1, std::memory_order_release);
        bandSolo.setToggleState(false, juce::dontSendNotification);
    }
}

void DefaultEqualizerAudioProcessorEditor::updateDriveControls(bool resetModeDefaults)
{
    static constexpr const char* characterNames[] { "CURVE", "TOPOLOGY", "BIAS", "GATE",
                                                     "HYSTERESIS", "ODD / EVEN", "TONE", "FREQUENCY" };
    const int mode = juce::jlimit(0, kSaturationModeCount - 1, displayedDriveMode);
    const bool bipolarCharacter = saturationModeUsesBipolarCharacter(mode);
    driveCharacterSlider.setName(characterNames[mode]);
    driveCharacterSlider.setSaturationMode(mode);
    driveCharacterSlider.setRange(bipolarCharacter ? -1.0 : 0.0, 1.0, 0.001);
    driveSlider.setPersistentTextFormatter([](double value) { return cleanDb(value, 1); });
    driveSlider.updateText();
    driveCharacterSlider.updateText();

    const bool tape = mode == static_cast<int>(SaturationType::Tape);
    const bool sine = mode == static_cast<int>(SaturationType::SineErosion);

    if (resetModeDefaults)
    {
        proc.undoManager.beginNewTransaction("Change drive algorithm");
        const float characterDefault = (tape
            || mode == static_cast<int>(SaturationType::PhaseDistortion) || sine) ? 0.5f : 0.0f;
        const float secondaryDefault = tape ? 0.5f : 0.0f;
        applyAbsoluteToSelectedBands("drive_character", characterDefault);
        applyAbsoluteToSelectedBands("drive_secondary", secondaryDefault);
    }
    resized();
    repaint();
}

void DefaultEqualizerAudioProcessorEditor::selectBand(int band, bool updateGraphSelection)
{
    selectedBand = band >= 0 && band < kNumBands ? band : -1;
    proc.uiMeterBand.store(selectedBand, std::memory_order_release);
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
        autoGainBtn.setTooltip(proc.smartAutoGainLocked.load(std::memory_order_acquire) ? "Smart Gain: locked" : "Smart Gain: analysing");
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
        static const juce::String original { "default_eq" };
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
    autoGainBtn.setButtonText(autoMode == 2 ? "SMART GAIN" : "AUTO GAIN");
    autoGainBtn.setToggleState(autoMode > 0, juce::dontSendNotification);
    autoGainBtn.setLoadingState(proc.smartAutoGainProgress.load(std::memory_order_relaxed),
        smartSelected && !smartLocked, uiPreferences->getBoolValue("reducedMotion", false));
    if (smartSelected)
        autoGainBtn.setTooltip(smartLocked ? "Smart Gain: locked"
                                           : "Smart Gain: analysing");
    else
        autoGainBtn.setTooltip("Cycle Off / Regular Auto Gain / Smart Gain");
    autoGainBtn.repaint();
    const float shiftSemitones = proc.apvts.getRawParameterValue("shift")->load();
    if (!std::isfinite(displayedShiftSemitones)
        || std::abs(displayedShiftSemitones - shiftSemitones) > 0.0001f)
    {
        displayedShiftSemitones = shiftSemitones;
        freqField.refreshText();
    }
    const bool linear = proc.apvts.getRawParameterValue("linear_phase")->load() > 0.5f;
    const int quality = (int)proc.apvts.getRawParameterValue("linear_quality")->load();
    const int phaseId = linear ? std::clamp(quality + 2, 2, 4) : 1;
    if (phaseModeBox.getSelectedId() != phaseId)
        phaseModeBox.setSelectedId(phaseId, juce::dontSendNotification);
    if (selectedBand >= 0)
    {
        const int driveMode = std::clamp((int)proc.apvts.getRawParameterValue(
            bandId(selectedBand + 1, "sat_mode"))->load(), 0, kSaturationModeCount - 1);
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
        const int placementMode = std::clamp((int)proc.apvts.getRawParameterValue(
            bandId(selectedBand + 1, "placement_mode"))->load(),0,2);
        placementSlider.getProperties().set("midSide", placementMode==1);
        placementSlider.getProperties().set("routeMode", placementMode);
        placementSlider.repaint();
        const bool upward = proc.apvts.getRawParameterValue(
            bandId(selectedBand + 1, "dyn_mode"))->load(std::memory_order_relaxed) > 0.5f;
        dynModeBtn.setButtonText(upward ? "UP" : "DOWN");
        dynModeBtn.setToggleState(upward, juce::dontSendNotification);
        const bool externalSidechain = proc.apvts.getRawParameterValue(
            bandId(selectedBand + 1, "sc_source"))->load(std::memory_order_relaxed) > 0.5f;
        sidechainBtn.setButtonText(externalSidechain ? "EX SC" : "IN SC");
        sidechainBtn.setToggleState(externalSidechain, juce::dontSendNotification);
        const auto detector = proc.getBandDetectorLevelsDb(selectedBand);
        dynThreshold.setInputLevelsDb(detector.first, detector.second);
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
    const int seamX = wordmarkWidthForWidth(getWidth());
    const int seam = juce::jmax(18, headerH * 3 / 8);
    g.setColour(fg); g.fillRect(seamX, 0, seam, seam); g.fillRect(seamX, headerH - seam, seam, seam);
    g.setColour(bg);
    g.fillRect(0, getHeight() - workspaceH, getWidth(), workspaceH);

    const float layoutScale = layoutScaleForWidth(getWidth());
    const float panelScale = layoutScale * 1.15f;
    g.setFont(mono(14.25f * panelScale, true));
    const std::array<juce::Slider*, 8> sliders { &dynThreshold, &dynRange, &dynSpeed,
                                                 &driveSlider, &driveCharacterSlider,
                                                 &amountSlider, &shiftSlider, &outputSlider };
    for (auto* slider : sliders)
        if (slider->isVisible() && slider != &outputSlider
            && slider != &amountSlider && slider != &shiftSlider)
        {
            g.setColour(fg.withAlpha(slider->isEnabled() ? 0.85f : 0.35f));
            const int captionH = juce::roundToInt(13.0f * panelScale);
            const int captionWidth = slider->getWidth();
            g.drawFittedText(slider->getName(), slider->getX(), slider->getY() - captionH,
                             captionWidth, captionH,
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
    const int wordW = wordmarkWidthForWidth(w);
    const int seamW = juce::jmax(18, headerH * 3 / 8);
    themeBtn.setBounds(0, 0, wordW, headerH);

    const int actionH = juce::roundToInt(40.0f * layoutScale);
    const int actionY = (headerH - actionH) / 2;
    const int powerW = juce::roundToInt(60.0f * layoutScale);
    const int outerPad = juce::roundToInt(6.0f * layoutScale);
    const int powerX = w - powerW - outerPad;
    powerBtn.setBounds(powerX, actionY, powerW, actionH);

    const int autoW = juce::roundToInt(100.0f * layoutScale);
    const int amountW = juce::roundToInt(124.0f * layoutScale);
    const int shiftW = juce::roundToInt(124.0f * layoutScale);
    const int sectionGap = juce::roundToInt(2.0f * layoutScale);
    const int autoX = powerX - sectionGap - autoW;
    const int shiftX = autoX - sectionGap - shiftW;
    const int amountX = shiftX - sectionGap - amountW;
    amountSlider.setBounds(amountX, actionY, amountW, actionH);
    shiftSlider.setBounds(shiftX, actionY, shiftW, actionH);
    autoGainBtn.setBounds(autoX, actionY, autoW, actionH);

    const int globalStart = wordW + seamW + outerPad;
    const int globalEnd = amountX - sectionGap;
    const int globalGap = sectionGap;
    const int phaseW = juce::roundToInt(112.0f * layoutScale);
    const int osW = juce::roundToInt(54.0f * layoutScale);
    const int globalTotal = phaseW + osW + globalGap;
    int globalX = juce::jmax(globalStart, globalEnd - globalTotal);
    phaseModeBox.setBounds(globalX, actionY, phaseW, actionH); globalX += phaseW + globalGap;
    oversamplingSlider.setBounds(globalX, actionY, osW, actionH);

    const int graphFrame = juce::jmax(3, juce::roundToInt(4.0f * familyLook.getUiScale()));
    const int graphTop = headerH + graphFrame;
    const int toggleH = juce::roundToInt(28.0f * layoutScale);
    const int toggleY = h - workspaceH - toggleH - outerPad;
    const int adaptiveW = juce::roundToInt(116.0f * layoutScale);
    const int pageW = juce::roundToInt(118.0f * layoutScale);
    adaptiveQBtn.setBounds(graphFrame, toggleY, adaptiveW, toggleH);
    outputSlider.setBounds(w - graphFrame - pageW, toggleY, pageW, toggleH);
    const std::array<int, 4> fieldWidths {
        juce::roundToInt(132.0f * layoutScale), juce::roundToInt(128.0f * layoutScale),
        juce::roundToInt(104.0f * layoutScale), juce::roundToInt(158.0f * layoutScale) };
    const int totalFieldWidth = std::accumulate(fieldWidths.begin(), fieldWidths.end(), 0);
    const int lowerGap = juce::jmax(sectionGap, (w - graphFrame * 2 - adaptiveW
        - pageW - totalFieldWidth) / 5);
    const int fieldsX = adaptiveQBtn.getRight() + lowerGap;
    int fieldX = fieldsX;
    const std::array<NumericValueControl*, 4> fields { &freqField, &gainField, &qField, &slopeField };
    for (size_t i = 0; i < fields.size(); ++i)
    {
        fields[i]->setBounds(fieldX, toggleY, fieldWidths[i], toggleH);
        fieldX += fieldWidths[i] + lowerGap;
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
    for (auto* slider : std::array<juce::Slider*, 4> {
                          &dynRange, &dynSpeed, &driveSlider, &driveCharacterSlider })
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, knobSide, textBoxH);
    const int controlY = workspaceY + juce::roundToInt(20.0f * layoutScale);
    const int pairY = controlY;
    const int bandOnW = juce::roundToInt(38.0f * panelScale);
    const int selectorW = juce::roundToInt(96.0f * panelScale);
    const int placeW = juce::roundToInt(52.0f * panelScale);
    const int modeW = juce::roundToInt(48.0f * panelScale);
    const int listenW = juce::roundToInt(100.0f * panelScale);

    const auto stack = [rowH, gap](juce::Component& top, juce::Component& bottom,
                                   int x, int y, int width)
    {
        top.setBounds(x, y, width, rowH);
        bottom.setBounds(x, y + rowH + gap, width, rowH);
    };

    const int blockGap = juce::roundToInt(14.0f * panelScale);
    const int thresholdMeterW = juce::roundToInt(46.0f * panelScale);
    const int fixedWidth = bandOnW + placeW + selectorW + modeW + listenW
        + knobSide * 4 + thresholdMeterW + blockGap;
    const int panelLeft = adaptiveQBtn.getX() + 2;
    const int panelRight = outputSlider.getRight() - 2;
    const int available = panelRight - panelLeft;
    const int fittedGap = juce::jmax(2, (available - fixedWidth) / 8);
    int x = panelLeft;
    stack(bandOn, bandSolo, x, pairY, bandOnW); x += bandOnW + fittedGap;
    stack(placementModeBox, placementSlider, x, pairY, placeW); x += placeW + fittedGap;
    stack(typeBox, saturationBox, x, pairY, selectorW); x += selectorW + fittedGap;
    driveSlider.setBounds(x, pairY, knobSide, knobComponentH); x += knobSide + fittedGap;
    driveCharacterSlider.setBounds(x, pairY, knobSide, knobComponentH);
    x += knobSide + blockGap;
    stack(dynModeBtn, sidechainBtn, x, pairY, modeW); x += modeW + fittedGap;
    dynThreshold.setBounds(x, controlY - 2, thresholdMeterW, knobComponentH + 2);
    x += thresholdMeterW + fittedGap;
    for (auto* slider : { &dynRange, &dynSpeed })
    {
        slider->setBounds(x, controlY, knobSide, knobComponentH);
        x += knobSide + fittedGap;
    }
    stack(dynRatio, dynLookahead, panelRight - listenW, pairY, listenW);
}
