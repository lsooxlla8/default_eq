#include "PluginEditor.h"
#include "DSP/FilterTypes.h"

namespace
{
juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}

juce::String bandId(int idx, const juce::String& suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}

juce::String cleanDb(double value, int digits = 2)
{
    if (std::abs(value) < std::pow(10.0, -digits) * 0.5) value = 0.0;
    return juce::String(value, digits) + " dB";
}
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
            const auto resetValueForBand = [this, &suffix](int band)
            {
                auto* parameter = proc.apvts.getParameter(bandId(band + 1, suffix));
                if (parameter == nullptr) return 0.0f;
                if (suffix == "q")
                {
                    const int type = (int)proc.apvts.getRawParameterValue(
                        bandId(band + 1, "type"))->load();
                    return ResponseCurveComponent::qResetValueForType(type);
                }
                return parameter->convertFrom0to1(parameter->getDefaultValue());
            };
            if (suffix.isNotEmpty() && responseCurve.getSelectionCount() > 1)
            {
                proc.undoManager.beginNewTransaction("Reset selected band parameters");
                const auto& selected = responseCurve.getSelection();
                for (int band = 0; band < kNumBands; ++band)
                    if (selected[(size_t)band])
                        if (auto* parameter = proc.apvts.getParameter(bandId(band + 1, suffix)))
                        {
                            parameter->beginChangeGesture();
                            parameter->setValueNotifyingHost(parameter->convertTo0to1(
                                resetValueForBand(band)));
                            parameter->endChangeGesture();
                        }
            }
            if (suffix.isNotEmpty())
                slider->setDoubleClickReturnValue(true, resetValueForBand(selectedBand));
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
    qField.setDoubleClickReturnValue(true,
        ResponseCurveComponent::qResetValueForType(displayedFilterType));
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
