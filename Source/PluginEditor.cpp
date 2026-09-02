#include "PluginEditor.h"
#include "UI/DriveCharacterFormatting.h"
#include "UI/EditorLayout.h"
#include "DSP/FilterTypes.h"
#include <numeric>

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
    setResizeLimits(deq::ui::editor_layout::minimumWidth,
                    deq::ui::editor_layout::minimumHeight,
                    deq::ui::editor_layout::maximumWidth,
                    deq::ui::editor_layout::maximumHeight);
    const auto initialSize = deq::ui::editor_layout::constrainedSize(
        uiPreferences->getIntValue("windowWidth", deq::ui::editor_layout::defaultWidth),
        uiPreferences->getIntValue("windowHeight", deq::ui::editor_layout::defaultHeight));
    setSize(initialSize.x, initialSize.y);

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
        qField.setDoubleClickReturnValue(true,
            ResponseCurveComponent::qResetValueForType(type));
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
        const auto savedSize = deq::ui::editor_layout::constrainedSize(getWidth(), getHeight());
        uiPreferences->setValue("windowWidth", savedSize.x);
        uiPreferences->setValue("windowHeight", savedSize.y);
        uiPreferences->saveIfNeeded();
    }
    setLookAndFeel(nullptr);
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
    const auto* focused = juce::Component::getCurrentlyFocusedComponent();
    const bool editingText = dynamic_cast<const juce::TextEditor*>(focused) != nullptr
        || (focused != nullptr && focused->findParentComponentOfClass<juce::TextEditor>() != nullptr);
    if (!editingText && (key.getKeyCode() == juce::KeyPress::deleteKey
                         || key.getKeyCode() == juce::KeyPress::backspaceKey))
    {
        if (responseCurve.deleteSelectedBands())
        {
            selectBand(-1, false);
            return true;
        }
    }
    return juce::AudioProcessorEditor::keyPressed(key);
}

void DefaultEqualizerAudioProcessorEditor::timerCallback()
{
    // Some plug-in hosts resize editors with a direct setBounds(), which JUCE
    // explicitly documents as bypassing the normal bounds constrainer.  Clamp
    // again on the message thread so those hosts cannot leave clipped UI.
    const auto constrained = deq::ui::editor_layout::constrainedSize(getWidth(), getHeight());
    if (getWidth() != constrained.x || getHeight() != constrained.y)
        setSize(constrained.x, constrained.y);

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
    bool spectrumFrameArrived = false;
    if (proc.preSpectrumFifo.processIfReady())
    {
        responseCurve.pushSpectrumData(proc.preSpectrumFifo.getMagnitudes(), proc.preSpectrumFifo.getNumBins(), sampleRate, true);
        spectrumFrameArrived = true;
    }
    if (proc.spectrumFifo.processIfReady())
    {
        responseCurve.pushSpectrumData(proc.spectrumFifo.getMagnitudes(), proc.spectrumFifo.getNumBins(), sampleRate, false);
        spectrumFrameArrived = true;
    }
    responseCurve.refreshForTimer(spectrumFrameArrived);
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
        const bool midSide = placementMode == 1;
        if ((bool)placementSlider.getProperties().getWithDefault("midSide", false) != midSide
            || (int)placementSlider.getProperties().getWithDefault("routeMode", 0) != placementMode)
        {
            placementSlider.getProperties().set("midSide", midSide);
            placementSlider.getProperties().set("routeMode", placementMode);
            placementSlider.repaint();
        }
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
    const auto layout = deq::ui::editor_layout::metricsForSize(getWidth(), getHeight());
    const int headerH = layout.headerHeight;
    const int workspaceH = layout.workspaceHeight;
    g.fillAll(fg);
    g.setColour(bg); g.fillRect(0, 0, getWidth(), headerH);
    const int seamX = layout.wordmarkWidth;
    const int seam = juce::jmax(18, headerH * 3 / 8);
    g.setColour(fg); g.fillRect(seamX, 0, seam, seam); g.fillRect(seamX, headerH - seam, seam, seam);
    g.setColour(bg);
    g.fillRect(0, getHeight() - workspaceH, getWidth(), workspaceH);

    const float layoutScale = layout.scale;
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
    const auto constrained = deq::ui::editor_layout::constrainedSize(w, h);
    if (w != constrained.x || h != constrained.y)
    {
        setSize(constrained.x, constrained.y);
        return;
    }
    const auto layout = deq::ui::editor_layout::metricsForSize(w, h);
    const float layoutScale = layout.scale;
    familyLook.setUiScale(layoutScale);
    const int headerH = layout.headerHeight;
    const int workspaceH = layout.workspaceHeight;
    const int wordW = layout.wordmarkWidth;
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
