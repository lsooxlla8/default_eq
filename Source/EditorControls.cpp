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
    return hasMixedValue() ? juce::String("MULTI")
                           : deq::ui::formatDriveCharacter(saturationMode, raw);
}

ThresholdMeterSlider::ThresholdMeterSlider()
{
    setName("THRESHOLD");
    setSliderStyle(juce::Slider::LinearVertical);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setSliderSnapsToMousePosition(false);
    setMouseDragSensitivity(240);
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    valueLabel.setJustificationType(juce::Justification::centred);
    valueLabel.setEditable(false, true, false);
    valueLabel.setInterceptsMouseClicks(false, false);
    valueLabel.getProperties().set("rotaryValueLabel", true);
    addAndMakeVisible(valueLabel);
    valueLabel.setVisible(false);
    onValueChange = [this] { updateText(); };
    valueLabel.onEditorHide = [this]
    {
        valueLabel.setVisible(false);
        repaint();
    };
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

void ThresholdMeterSlider::setMixedValue(bool shouldBeMixed)
{
    if (mixedValue == shouldBeMixed) return;
    mixedValue = shouldBeMixed;
    updateText();
    repaint();
}

void ThresholdMeterSlider::paint(juce::Graphics& g)
{
    const auto fg = findColour(default_family::LookAndFeel::foregroundColourId, true);
    const auto bg = findColour(default_family::LookAndFeel::backgroundColourId, true);
    const float scale = dynamic_cast<const default_family::LookAndFeel*>(&getLookAndFeel()) != nullptr
        ? dynamic_cast<const default_family::LookAndFeel*>(&getLookAndFeel())->getUiScale() : 1.0f;
    default_family::drawPrototypeText(g, getName(),
        { 0.0f, 6.0f * scale, (float)getWidth(), 9.0f * scale },
        9.0f, true, 0.0f, fg.withAlpha(0.72f),
        default_family::PrototypeTextAlign::centre, scale);
    const auto inner = juce::Rectangle<int>(
        juce::roundToInt(getWidth() * 0.5f - 17.0f * scale),
        juce::roundToInt(22.0f * scale),
        juce::roundToInt(34.0f * scale),
        getHeight() - juce::roundToInt(43.0f * scale));
    const int laneGap = juce::roundToInt(3.0f * scale);
    const int availableLaneWidth = inner.getWidth() - laneGap;
    const int leftLaneWidth = availableLaneWidth / 2;
    const int rightLaneWidth = availableLaneWidth - leftLaneWidth;
    const auto drawLane = [&](float levelDb, int x, int width)
    {
        const float level = juce::jmap(levelDb, -60.0f, 0.0f, 0.0f, 1.0f);
        g.setColour(fg.withAlpha(0.07f));
        g.fillRect(x, inner.getY(), width, inner.getHeight());
        g.setColour(fg.withAlpha(isEnabled() ? 1.0f
                                              : default_family::metrics::disabledOpacity));
        const int fillTop = juce::roundToInt(
            (float)inner.getBottom() - (float)inner.getHeight() * level);
        g.fillRect(x, fillTop, width, inner.getBottom() - fillTop);
    };
    drawLane(displayedLevelDbL, inner.getX(), leftLaneWidth);
    drawLane(displayedLevelDbR, inner.getX() + leftLaneWidth + laneGap, rightLaneWidth);

    // Paint the attached parameter directly. Keeping a second display value
    // made the line stale while SliderAttachment was rebinding to a new band.
    const float threshold = (float)valueToProportionOfLength(getValue());
    const float thresholdY = (float)inner.getBottom() - (float)inner.getHeight() * threshold;
    const int lineThickness = juce::jmax(1, juce::roundToInt(scale));
    const int lineY = juce::jlimit(inner.getY() + lineThickness,
        inner.getBottom() - lineThickness * 2,
        juce::roundToInt(thresholdY - (float)lineThickness * 0.5f));
    g.setColour(bg.withAlpha(isEnabled() ? 1.0f
                                         : default_family::metrics::disabledOpacity));
    g.fillRect(inner.getX(), lineY - lineThickness, inner.getWidth(), lineThickness);
    g.fillRect(inner.getX(), lineY + lineThickness, inner.getWidth(), lineThickness);
    g.setColour(fg.withAlpha(isEnabled() ? 1.0f
                                         : default_family::metrics::disabledOpacity));
    g.fillRect(inner.getX(), lineY, inner.getWidth(), lineThickness);
    if ((bool)getProperties().getWithDefault("rightDivider", false))
    {
        g.setColour(fg);
        g.fillRect(getWidth() - juce::jmax(1, juce::roundToInt(scale)), 0,
                   juce::jmax(1, juce::roundToInt(scale)), getHeight());
    }
    default_family::drawPrototypeText(g, getDisplayedText(),
        { 0.0f, getHeight() - 16.0f * scale, (float)getWidth(), 9.0f * scale },
        9.0f, true, 0.0f, fg,
        default_family::PrototypeTextAlign::centre, scale);
}

void ThresholdMeterSlider::updateText()
{
    juce::ScopedValueSetter<bool> guard(updating, true);
    valueLabel.setText(mixedValue ? juce::String("MULTI")
                                 : juce::String(getValue(), 1) + " dB",
                       juce::dontSendNotification);
}

void ThresholdMeterSlider::resized()
{
    const float scale = dynamic_cast<const default_family::LookAndFeel*>(&getLookAndFeel()) != nullptr
        ? dynamic_cast<const default_family::LookAndFeel*>(&getLookAndFeel())->getUiScale() : 1.0f;
    valueLabel.setBounds(getLocalBounds().removeFromBottom(juce::roundToInt(18.0f * scale)));
}

void ThresholdMeterSlider::mouseDoubleClick(const juce::MouseEvent&)
{
    valueLabel.setVisible(true);
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
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    valueLabel.setJustificationType(juce::Justification::centred);
    valueLabel.setEditable(false, true, false);
    valueLabel.setMinimumHorizontalScale(1.0f);
    valueLabel.getProperties().set("numericValueControl", true);
    valueLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(valueLabel);
    valueLabel.setVisible(false);
    onValueChange = [this] { updateText(); };
    valueLabel.onEditorHide = [this]
    {
        valueLabel.setVisible(false);
        repaint();
    };
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
    const auto value = mixedValue ? juce::String("MULTI")
                                  : formatValue ? formatValue(getValue())
                                                : juce::String(getValue(), 2);
    valueLabel.setText(verticalMini ? value
                                   : showValue ? getName() + "\n" + value : getName(),
                       juce::dontSendNotification);
    repaint();
}

void NumericValueControl::setMixedValue(bool shouldBeMixed)
{
    if (mixedValue == shouldBeMixed) return;
    mixedValue = shouldBeMixed;
    updateText();
    repaint();
}

void NumericValueControl::setVerticalMini(bool shouldUseVerticalMini)
{
    verticalMini = shouldUseVerticalMini;
    valueLabel.setVisible(false);
    resized();
    updateText();
}

void NumericValueControl::setValueVisible(bool shouldShowValue)
{
    if (showValue == shouldShowValue) return;
    showValue = shouldShowValue;
    updateText();
}

void NumericValueControl::paint(juce::Graphics& g)
{
    const auto rawFg = findColour(default_family::LookAndFeel::foregroundColourId, true);
    const auto fg = rawFg;
    const auto bg = findColour(default_family::LookAndFeel::backgroundColourId, true);
    g.setColour(bg); g.fillRect(getLocalBounds());
    const float scale = dynamic_cast<const default_family::LookAndFeel*>(&getLookAndFeel()) != nullptr
        ? dynamic_cast<const default_family::LookAndFeel*>(&getLookAndFeel())->getUiScale() : 1.0f;
    const bool rightDivider = (bool)getProperties().getWithDefault("rightDivider", false);
    const auto drawRightDivider = [&]
    {
        if (!rightDivider) return;
        g.setColour(verticalMini ? rawFg : fg);
        g.fillRect(getWidth() - juce::jmax(1, juce::roundToInt(scale)), 0,
                   juce::jmax(1, juce::roundToInt(scale)), getHeight());
    };

    const auto value = mixedValue ? juce::String("MULTI") : showValue
        ? (formatValue ? formatValue(getValue()) : juce::String(getValue(), 2)) : juce::String();
    if ((bool)getProperties().getWithDefault("headerCell", false))
    {
        const float labelHeight = 9.0f * scale;
        const float valueHeight = 13.0f * scale;
        const float gap = 9.0f * scale;
        const float top = ((float)getHeight() - labelHeight - gap - valueHeight) * 0.5f
            - scale;
        const auto content = getLocalBounds().toFloat().reduced(13.0f * scale, 0.0f);
        default_family::drawPrototypeText(g, getName(),
            { content.getX(), top, content.getWidth(), labelHeight },
            9.0f, true, 0.13f, fg.withAlpha(0.72f),
            default_family::PrototypeTextAlign::left, scale);
        default_family::drawPrototypeText(g, value,
            { content.getX(), top + labelHeight + gap, content.getWidth(), valueHeight },
            13.0f, true, 0.0f, fg,
            default_family::PrototypeTextAlign::left, scale);
        drawRightDivider();
        return;
    }

    if ((bool)getProperties().getWithDefault("valueStripCell", false))
    {
        auto area = getLocalBounds().toFloat().reduced(8.0f * scale, 3.0f * scale);
        const float labelWidth = default_family::prototypeTextWidth(
            getName(), 9.0f, true, 0.07f, scale);
        auto labelArea = area.removeFromLeft(labelWidth);
        default_family::drawPrototypeText(g, getName(), labelArea,
            9.0f, true, 0.07f, fg.withAlpha(0.72f),
            default_family::PrototypeTextAlign::left, scale);
        area.removeFromLeft(7.0f * scale);
        if (value.isNotEmpty())
        {
            juce::String numeric = value;
            juce::String unit;
            if (getName() == "FREQUENCY") unit = "Hz";
            else if (getName() == "GAIN" || getName() == "OUT") unit = "dB";
            else if (getName() == "SLOPE") unit = "dB/OCT";
            if (!mixedValue)
                numeric = numeric.upToFirstOccurrenceOf(" dB", false, false);
            auto unitArea = area;
            if (unit.isNotEmpty())
            {
                const float unitWidth = default_family::prototypeTextWidth(
                    unit, 9.0f, true, 0.07f, scale);
                unitArea = area.removeFromRight(unitWidth);
                area.removeFromRight(4.0f * scale);
            }
            default_family::drawPrototypeText(g, numeric, area,
                10.0f, true, 0.0f,
                mixedValue ? juce::Colour(0xff757575) : fg,
                default_family::PrototypeTextAlign::left, scale);
            if (unit.isNotEmpty())
                default_family::drawPrototypeText(g, unit,
                    unitArea.translated(-scale, 0.0f),
                    9.0f, true, 0.07f, fg.withAlpha(0.72f),
                    default_family::PrototypeTextAlign::left, scale);
        }
        drawRightDivider();
        return;
    }

    if (!verticalMini) { drawRightDivider(); return; }

    const auto full = getLocalBounds().toFloat();
    const float labelOffsetX = (getName() == "RANGE" || getName() == "RATIO")
        ? -scale : 0.0f;
    default_family::drawPrototypeText(g, getName(),
        { full.getX() + 5.0f * scale + labelOffsetX, 6.0f * scale,
          full.getWidth() - 10.0f * scale, 9.0f * scale },
        9.0f, true, 0.06f, rawFg.withAlpha(0.72f),
        default_family::PrototypeTextAlign::centre, scale);
    const float trackCentre = getLocalBounds().toFloat().getCentreX();
    auto track = juce::Rectangle<float>(
        std::floor(trackCentre - 12.0f * scale), 22.0f * scale,
        24.0f * scale, getHeight() - 43.0f * scale);
    g.setColour(bg); g.fillRect(track);
    g.setColour(rawFg); g.drawRect(track, 1.0f * scale);
    const float proportion = getName() == "RATIO"
        ? (float)juce::jmap(getValue(), getMinimum(), getMaximum(), 0.0, 1.0)
        : (float)valueToProportionOfLength(getValue());
    const float maximumFillHeight = getHeight() - 44.0f * scale;
    const float fillHeight = maximumFillHeight * proportion
        + (getName() == "RATIO" ? 0.5f * scale : 0.0f);
    g.setColour(rawFg.withAlpha(isEnabled() ? 1.0f : default_family::metrics::disabledOpacity));
    const int fillLeft = juce::roundToInt(trackCentre - 9.0f * scale);
    const int fillRight = juce::roundToInt(trackCentre + 9.0f * scale);
    const int fillBottom = juce::roundToInt(getHeight() - 22.0f * scale);
    const int fillTop = juce::roundToInt((float)fillBottom - fillHeight);
    g.fillRect(fillLeft, fillTop, fillRight - fillLeft, fillBottom - fillTop);
    default_family::drawPrototypeText(g, value,
        { full.getX() + 5.0f * scale, getHeight() - 16.0f * scale,
          full.getWidth() - 10.0f * scale, 9.0f * scale },
        9.0f, true, 0.0f, rawFg,
        default_family::PrototypeTextAlign::centre, scale);
    drawRightDivider();
}

void NumericValueControl::resized()
{
    const float scale = dynamic_cast<const default_family::LookAndFeel*>(&getLookAndFeel()) != nullptr
        ? dynamic_cast<const default_family::LookAndFeel*>(&getLookAndFeel())->getUiScale() : 1.0f;
    valueLabel.setBounds(verticalMini ? getLocalBounds().removeFromBottom(
                                           juce::roundToInt(20.0f * scale))
                                      : getLocalBounds().reduced(3, 2));
}

void NumericValueControl::mouseDoubleClick(const juce::MouseEvent&)
{
    valueLabel.setVisible(true);
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
