#include "PluginEditor.h"
#include "UI/DriveCharacterFormatting.h"

namespace
{
double parseUnitValue(juce::String text)
{
    auto source = text.trim().toLowerCase().replaceCharacter(',', '.');
    const auto numeric = source.retainCharacters("-+0123456789.e");
    if (numeric.isEmpty()) return std::numeric_limits<double>::quiet_NaN();
    const double value = numeric.getDoubleValue();
    return std::isfinite(value) ? value : std::numeric_limits<double>::quiet_NaN();
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
    const float nextLeft = std::max(std::clamp(leftDb, -60.0f, 0.0f), displayedLevelDbL - 1.5f);
    const float nextRight = std::max(std::clamp(rightDb, -60.0f, 0.0f), displayedLevelDbR - 1.5f);
    if (std::abs(nextLeft - displayedLevelDbL) > 0.001f
        || std::abs(nextRight - displayedLevelDbR) > 0.001f)
    {
        displayedLevelDbL = nextLeft;
        displayedLevelDbR = nextRight;
        repaint();
    }
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
