#include "PluginEditor.h"

namespace
{
std::atomic<int> sharedDarkTheme { -1 };
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

FamilyLookAndFeel::FamilyLookAndFeel() { setDark(false); }

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
    return mono(juce::jlimit(9.0f, 13.0f, box.getHeight() * 0.42f), true);
}

juce::Font FamilyLookAndFeel::getLabelFont(juce::Label& label)
{
    return mono(juce::jlimit(9.0f, 13.0f, label.getHeight() * 0.48f), true);
}

juce::Font FamilyLookAndFeel::getPopupMenuFont() { return mono(12.0f, true); }

void FamilyLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(8, 1, juce::jmax(1, box.getWidth() - 34), box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
}

void FamilyLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                              const juce::Colour&, bool over, bool down)
{
    if (button.getName() == "wordmark")
    {
        g.setColour(background());
        g.fillAll();
        return;
    }
    const bool active = button.getToggleState();
    const auto bounds = button.getLocalBounds();
    const int border = 2;
    const auto content = bounds.reduced(border);
    g.setColour(foreground()); g.fillRect(bounds);
    g.setColour(background()); g.fillRect(content);
    if (active || down)
    {
        g.setColour(foreground());
        g.fillRect(content.reduced(2));
    }
    const float progress = (float)button.getProperties().getWithDefault("progress", 0.0f);
    if (progress > 0.001f && progress < 1.0f)
    {
        g.setColour(active || down ? background() : foreground());
        g.fillRect((float)content.getX() + 3.0f, (float)content.getBottom() - 5.0f,
                   ((float)content.getWidth() - 6.0f) * std::clamp(progress, 0.0f, 1.0f), 2.0f);
    }
    if (over)
    {
        const auto highlight = content.reduced(4);
        if (!highlight.isEmpty())
        {
            g.setColour(active || down ? background() : foreground());
            g.drawRect(highlight, 1);
        }
    }
}

void FamilyLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool down)
{
    if (button.getName() == "wordmark")
    {
        g.setColour(foreground());
        g.setFont(mono(juce::jlimit(12.0f, 18.0f, button.getHeight() * 0.31f), true));
        g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(12, 2),
                         juce::Justification::centredLeft, 1);
        return;
    }
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

void FamilyLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
                                         float pos, float min, float max,
                                         juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (slider.getName() != "LOOKAHEAD" && slider.getName() != "OUTPUT_HDR"
        && slider.getName() != "PLACEMENT")
        return juce::LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, pos, min, max, style, slider);
    juce::ignoreUnused(min, max, style);
    auto r = juce::Rectangle<float>((float)x + 1.0f, (float)y + 1.0f,
                                     (float)w - 2.0f, (float)h - 2.0f);
    g.setColour(background()); g.fillRect(r);
    g.setColour(foreground()); g.drawRect(r, 2.0f);
    const float proportion = (float)slider.valueToProportionOfLength(slider.getValue());
    juce::Rectangle<float> fillRect;
    if (slider.getName() == "OUTPUT_HDR")
    {
        auto inner = r.reduced(4.0f);
        fillRect = inner.withTop(inner.getBottom() - inner.getHeight() * proportion);
        if (proportion > 0.001f) { g.setColour(foreground()); g.fillRect(fillRect); }
    }
    else if (slider.getName() == "PLACEMENT")
    {
        auto inner = r.reduced(4.0f);
        const float centre = inner.getCentreX();
        const float marker = inner.getX() + inner.getWidth() * proportion;
        fillRect = marker < centre ? juce::Rectangle<float>(marker, inner.getY(), centre - marker, inner.getHeight())
                                   : juce::Rectangle<float>(centre, inner.getY(), marker - centre, inner.getHeight());
        if (fillRect.getWidth() > 0.5f) { g.setColour(foreground()); g.fillRect(fillRect); }
        g.setColour(foreground().withAlpha(0.35f)); g.drawVerticalLine(juce::roundToInt(centre), inner.getY(), inner.getBottom());
    }
    else if (proportion > 0.001f)
    {
        fillRect = r.reduced(4.0f).withWidth((r.getWidth() - 8.0f) * proportion);
        g.setColour(foreground()); g.fillRect(fillRect);
    }
    g.setFont(mono(10.0f, true));
    const bool midSidePlacement = (bool)slider.getProperties().getWithDefault("midSide", false);
    const auto value = slider.getName() == "OUTPUT_HDR"
        ? "OUT " + juce::String(std::abs(slider.getValue()) < 0.005 ? 0.0 : slider.getValue(), 1)
        : slider.getName() == "PLACEMENT"
        ? (std::abs(slider.getValue()) < 0.05 ? "CENTER"
           : slider.getValue() < 0.0 ? juce::String(midSidePlacement ? "M " : "L ") + juce::String(std::abs(slider.getValue()), 0)
                                     : juce::String(midSidePlacement ? "S " : "R ") + juce::String(slider.getValue(), 0))
        : slider.getValue() <= 0.001 ? "LOOK OFF"
        : "LOOK " + juce::String(slider.getValue(), 2) + "ms";
    const auto textBounds = r.toNearestInt().reduced(4, 1);
    g.setColour(foreground());
    g.drawFittedText(value, textBounds, juce::Justification::centred, 1);
    if (!fillRect.isEmpty())
    {
        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(fillRect.toNearestInt());
        g.setColour(background());
        g.drawFittedText(value, textBounds, juce::Justification::centred, 1);
    }
}

void FamilyLookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    const auto fg = foreground().withMultipliedAlpha(box.isEnabled() ? 1.0f : 0.35f);
    g.setColour(background()); g.fillRect(0, 0, w, h);
    g.setColour(fg); g.drawRect(0, 0, w, h, 2);
    const int marker = juce::jmax(7, h / 5);
    g.fillRect(w - marker - 8, (h - marker) / 2, marker, marker);
}

VerticalTextSlider::VerticalTextSlider()
{
    setSliderStyle(juce::Slider::LinearVertical);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setMouseDragSensitivity(140);
}

void VerticalTextSlider::setDescriptor(juce::String text)
{
    descriptor = std::move(text);
    setName(descriptor);
    repaint();
}

void VerticalTextSlider::paint(juce::Graphics& g)
{
    auto* family = dynamic_cast<FamilyLookAndFeel*>(&getLookAndFeel());
    const auto fg = family != nullptr ? family->foreground() : findColour(juce::Slider::textBoxTextColourId);
    const auto bg = family != nullptr ? family->background() : findColour(juce::Slider::textBoxBackgroundColourId);
    auto bounds = getLocalBounds().toFloat();
    const float connector = 5.0f;
    bounds.removeFromLeft(connector);
    g.setColour(fg); g.fillRect(0.0f, bounds.getCentreY() - 1.0f, bounds.getX() + 2.0f, 2.0f);
    g.setColour(bg); g.fillRect(bounds);
    g.setColour(fg); g.drawRect(bounds, 2.0f);
    const float progress = (float)getNormalisableRange().convertTo0to1(getValue());
    const auto inner = bounds.reduced(3.0f);
    const auto fill = inner.withTop(inner.getBottom() - inner.getHeight() * progress);
    g.setColour(fg); g.fillRect(fill);
    const auto drawText = [&](juce::Colour colour, juce::Rectangle<int> clip)
    {
        juce::Graphics::ScopedSaveState clipped(g); g.reduceClipRegion(clip); g.setColour(colour);
        g.setFont(mono(9.0f, true));
        juce::Graphics::ScopedSaveState rotated(g);
        g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                                        bounds.getCentreX(), bounds.getCentreY()));
        const auto textBounds = juce::Rectangle<float>(bounds.getCentreX() - bounds.getHeight() * 0.5f,
            bounds.getCentreY() - bounds.getWidth() * 0.5f, bounds.getHeight(), bounds.getWidth());
        g.drawFittedText(descriptor, textBounds.reduced(4.0f, 1.0f).toNearestInt(), juce::Justification::centred, 1);
    };
    drawText(fg, inner.toNearestInt()); drawText(bg, fill.toNearestInt());
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
    const int storedTheme = uiPreferences->getBoolValue("darkTheme", false) ? 1 : 0;
    int unset = -1;
    sharedDarkTheme.compare_exchange_strong(unset, storedTheme);
    darkTheme = sharedDarkTheme.load(std::memory_order_acquire) != 0;
    familyLook.setDark(darkTheme);
    setLookAndFeel(&familyLook);
    responseCurve.setDarkMode(darkTheme);

    setResizable(true, true);
    addMouseListener(this, true);
    setResizeLimits(720, 519, 1200, 900);
    setSize(juce::jlimit(720, 1200, uiPreferences->getIntValue("windowWidth", 860)),
            juce::jlimit(519, 865, uiPreferences->getIntValue("windowHeight", 620)));

    addAndMakeVisible(responseCurve);
    addAndMakeVisible(levelMeter);

    auto addButton = [this](auto& button) { addAndMakeVisible(button); };
    themeBtn.setName("wordmark");
    nextBrandGlitchTimeMs = juce::Time::getMillisecondCounterHiRes() + 4000.0;
    addButton(themeBtn); addButton(powerBtn);
    addButton(bandPageBtn); addButton(dynamicPageBtn); addButton(analyzerPageBtn); addButton(matchPageBtn);
    addButton(bandOn); addButton(bandSolo); addButton(adaptiveQBtn); addButton(placementModeBtn);
    addButton(dynOn); addButton(sidechainAudition); addButton(driveOn); addButton(driveAutoGain);
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
    oversamplingBox.addItemList({ "OFF", "2X", "4X", "8X" }, 1);
    analyzerResolutionBox.addItemList({ "RTA: LOW", "RTA: MEDIUM", "RTA: HIGH" }, 1);
    phaseModeBox.addItemList({ "MIN PHASE", "LINEAR ECO", "LINEAR STD", "LINEAR HIGH" }, 1);
    addCombo(typeBox); addCombo(dynModeBox); addCombo(sidechainBox);
    addCombo(saturationBox); addCombo(oversamplingBox); addCombo(analyzerResolutionBox); addCombo(phaseModeBox);
    addAndMakeVisible(autoGainBtn);

    for (auto* slider : { &slopeSlider, &dynThreshold, &dynRange, &dynRatio, &dynAttack,
                          &dynRelease, &driveSlider, &driveCharacterSlider,
                          &driveMixSlider, &driveOutputSlider, &outputSlider,
                          &matchAmount, &matchSmoothing, &matchLow, &matchHigh, &matchTime,
                          &analyzerRange, &analyzerFloor, &analyzerSpeed, &analyzerAveraging, &analyzerTilt })
        initParameter(*slider, {});
    addAndMakeVisible(driveSecondarySlider);
    dynLookahead.setName("LOOKAHEAD");
    dynLookahead.setSliderStyle(juce::Slider::LinearHorizontal);
    dynLookahead.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(dynLookahead);
    placementSlider.setName("PLACEMENT");
    placementSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    placementSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(placementSlider);
    slopeSlider.setName("SLOPE"); dynThreshold.setName("THRESHOLD"); dynRange.setName("RANGE");
    dynRatio.setName("RATIO"); dynAttack.setName("ATTACK"); dynRelease.setName("RELEASE");
    driveSlider.setName("DRIVE"); driveCharacterSlider.setName("CHARACTER");
    driveSecondarySlider.setDescriptor("SECONDARY");
    driveMixSlider.setName("MIX"); driveOutputSlider.setName("COMP");
    outputSlider.setName("OUTPUT");
    outputSlider.setName("OUTPUT_HDR");
    outputSlider.setSliderStyle(juce::Slider::LinearBarVertical);
    outputSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
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
    driveSlider.textFromValueFunction = [](double v) { return cleanDb(v, 1); };
    driveMixSlider.textFromValueFunction = [](double v) { return juce::String(v, 0) + "%"; };
    driveOutputSlider.textFromValueFunction = outputSlider.textFromValueFunction = [](double v) { return cleanDb(v); };
    applySliderPalette();

    themeBtn.setTooltip("Toggle exact paper/ink inversion");
    dynOn.setTooltip("Enable dynamic processing for the selected band");
    dynModeBox.setTooltip("Downward or upward dynamic EQ");
    sidechainBox.setTooltip("Internal frequency-filtered detector or host sidechain");
    dynLookahead.setTooltip("Click or drag from 0 to 5 ms; latency is reported to the host");
    sidechainAudition.setTooltip("Momentarily listen to this band's filtered detector signal");
    themeBtn.onClick = [this]
    {
        darkTheme = !darkTheme;
        sharedDarkTheme.store(darkTheme ? 1 : 0, std::memory_order_release);
        familyLook.setDark(darkTheme); responseCurve.setDarkMode(darkTheme);
        applySliderPalette();
        uiPreferences->setValue("darkTheme", darkTheme);
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
    bandPageBtn.onClick = [this] { setWorkspacePage(WorkspacePage::Band); };
    dynamicPageBtn.onClick = [this] { setWorkspacePage(WorkspacePage::Dynamic); };
    analyzerPageBtn.onClick = [this] { setWorkspacePage(WorkspacePage::Analyzer); };
    matchPageBtn.onClick = [this] { setWorkspacePage(WorkspacePage::Match); };
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
    };
    responseCurve.setAnalyzerVisible(analyzerVisible.getToggleState());
    responseCurve.setAnalyzerSources(true, true);
    spectrumFreeze.setClickingTogglesState(true);
    spectrumFreeze.onClick = [this] { responseCurve.setSpectrumFrozen(spectrumFreeze.getToggleState()); };
    analyzerPeakHold.setClickingTogglesState(true);
    analyzerPeakHold.setToggleState(uiPreferences->getBoolValue("analyzerPeakHold", false), juce::dontSendNotification);
    if (!uiPreferences->getBoolValue("analyzerResolutionV2", false))
    {
        const int legacy = uiPreferences->getIntValue("analyzerResolution", 3);
        uiPreferences->setValue("analyzerResolution", legacy == 3 ? 2 : std::max(1, legacy - 1));
        uiPreferences->setValue("analyzerResolutionV2", true);
    }
    analyzerResolutionBox.setSelectedId(uiPreferences->getIntValue("analyzerResolution", 2), juce::dontSendNotification);
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
    oversamplingAtt = std::make_unique<ComboAttachment>(proc.apvts, "oversampling", oversamplingBox);
    outputAtt = std::make_unique<SliderAttachment>(proc.apvts, "output_gain", outputSlider);

    setWantsKeyboardFocus(true);
    selectBand(0);
    setWorkspacePage(WorkspacePage::Band);
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
    if (uiPreferences)
    {
        uiPreferences->setValue("windowWidth", getWidth());
        uiPreferences->setValue("windowHeight", getHeight());
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

void DefaultEqualizerAudioProcessorEditor::initParameter(juce::Slider& slider, const juce::String& name)
{
    slider.setName(name);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 17);
    slider.setDoubleClickReturnValue(false, 0.0);
    addAndMakeVisible(slider);
}

void DefaultEqualizerAudioProcessorEditor::applySliderPalette()
{
    const auto fg = familyLook.foreground();
    const auto bg = familyLook.background();
    for (auto* slider : { &slopeSlider, &placementSlider, &dynThreshold, &dynRange, &dynRatio, &dynAttack,
                          &dynRelease, &dynLookahead, &driveSlider, &driveCharacterSlider,
                          static_cast<juce::Slider*>(&driveSecondarySlider), &driveMixSlider,
                          &driveOutputSlider, &outputSlider, &matchAmount, &matchSmoothing,
                          &matchLow, &matchHigh, &matchTime, &analyzerRange, &analyzerFloor,
                          &analyzerSpeed, &analyzerAveraging, &analyzerTilt })
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
    dynOnAtt.reset(); dynLookaheadAtt.reset(); dynModeAtt.reset(); sidechainAtt.reset(); dynThresholdAtt.reset(); dynRangeAtt.reset();
    dynRatioAtt.reset(); dynAttackAtt.reset(); dynReleaseAtt.reset(); driveOnAtt.reset(); driveAtt.reset();
    driveCharacterAtt.reset(); driveSecondaryAtt.reset(); driveMixAtt.reset();
    driveOutputAtt.reset(); driveAutoGainAtt.reset(); saturationAtt.reset();
    const int idx = selectedBand + 1;
    bandOnAtt = std::make_unique<ButtonAttachment>(proc.apvts, bandId(idx, "on"), bandOn);
    typeAtt = std::make_unique<ComboAttachment>(proc.apvts, bandId(idx, "type"), typeBox);
    placementModeAtt = std::make_unique<ButtonAttachment>(proc.apvts, bandId(idx, "placement_mode"), placementModeBtn);
    placementAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "placement"), placementSlider);
    slopeAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "slope"), slopeSlider);
    dynOnAtt = std::make_unique<ButtonAttachment>(proc.apvts, bandId(idx, "dyn_on"), dynOn);
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
    driveSecondaryAtt = std::make_unique<SliderAttachment>(proc.apvts, bandId(idx, "drive_secondary"), driveSecondarySlider);
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
    driveCharacterSlider.setName(characterNames[mode]);
    driveSlider.textFromValueFunction = [](double value) { return cleanDb(value, 1); };
    driveMixSlider.textFromValueFunction = [](double value) { return juce::String(value, 0) + "%"; };
    driveOutputSlider.textFromValueFunction = [](double value) { return cleanDb(value); };
    driveCharacterSlider.textFromValueFunction = [mode](double raw)
    {
        if (mode == 9)
        {
            const double unit = juce::jlimit(0.0, 1.0, raw);
            const double hz = unit <= 0.5 ? 1000.0 * std::pow(2.0 * unit, 2.0)
                                          : 1000.0 * std::pow(10.0, 2.0 * unit - 1.0);
            return hz >= 1000.0 ? juce::String(hz / 1000.0, hz < 10000.0 ? 2 : 1) + " kHz"
                                : juce::String(hz, 0) + " Hz";
        }
        double shown = (mode == 3 || mode == 4 || mode == 6) ? raw * 100.0 : juce::jmax(0.0, raw) * 100.0;
        if (std::abs(shown) < 0.005) shown = 0.0;
        return juce::String(shown, 0) + "%";
    };
    driveSlider.updateText();
    driveCharacterSlider.updateText();
    driveMixSlider.updateText();
    driveOutputSlider.updateText();

    const bool tape = mode == 5, sine = mode == 9;
    driveSecondarySlider.setDescriptor(tape ? "BIAS" : sine ? "NOISE" : "SECONDARY");
    driveSecondarySlider.setVisible(workspacePage == WorkspacePage::Band && (tape || sine));
    driveSecondarySlider.setEnabled(tape || sine);

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
    bandPageBtn.setToggleState(page == WorkspacePage::Band, juce::dontSendNotification);
    dynamicPageBtn.setToggleState(page == WorkspacePage::Dynamic, juce::dontSendNotification);
    analyzerPageBtn.setToggleState(page == WorkspacePage::Analyzer, juce::dontSendNotification);
    matchPageBtn.setToggleState(page == WorkspacePage::Match, juce::dontSendNotification);

    const bool band = page == WorkspacePage::Band, dyn = page == WorkspacePage::Dynamic;
    const bool analyzer = page == WorkspacePage::Analyzer;
    const bool match = page == WorkspacePage::Match;
    const std::array<juce::Component*, 13> bandComponents { &bandOn, &bandSolo, &adaptiveQBtn,
        &placementModeBtn, &placementSlider, &typeBox, &slopeSlider, &driveOn, &driveAutoGain,
        &driveSlider, &driveCharacterSlider, &driveMixSlider, &driveOutputSlider };
    const std::array<juce::Component*, 10> dynComponents { &dynOn, &dynLookahead, &sidechainAudition, &dynModeBox, &sidechainBox, &dynThreshold,
                                                          &dynRange, &dynRatio, &dynAttack, &dynRelease };
    const std::array<juce::Component*, 9> analyzerComponents { &analyzerVisible, &spectrumFreeze, &analyzerPeakHold,
        &analyzerResolutionBox, &analyzerRange, &analyzerFloor, &analyzerSpeed, &analyzerAveraging, &analyzerTilt };
    const std::array<juce::Component*, 9> matchComponents { &matchCaptureBtn, &matchApplyBtn, &matchCommitBtn, &matchClearBtn,
        &matchAmount, &matchSmoothing, &matchLow, &matchHigh, &matchTime };
    for (auto* component : bandComponents) component->setVisible(band);
    saturationBox.setVisible(band);
    driveSecondarySlider.setVisible(band && (displayedDriveMode == 5 || displayedDriveMode == 9));
    for (auto* component : dynComponents) component->setVisible(dyn);
    for (auto* component : analyzerComponents) component->setVisible(analyzer);
    for (auto* component : { static_cast<juce::Component*>(&phaseModeBox), static_cast<juce::Component*>(&decrampBtn),
                             static_cast<juce::Component*>(&oversamplingBox), static_cast<juce::Component*>(&outputSlider),
                             static_cast<juce::Component*>(&levelMeter) })
        component->setVisible(true);
    for (auto* component : matchComponents) component->setVisible(match);
    resized(); repaint();
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
    const bool sharedDark = sharedDarkTheme.load(std::memory_order_acquire) != 0;
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
    const int autoMode = (int)proc.apvts.getRawParameterValue("auto_gain_mode")->load();
    const bool smartSelected = autoMode == 2;
    const bool smartLocked = proc.smartAutoGainLocked.load(std::memory_order_acquire);
    autoGainBtn.setButtonText(autoMode == 0 ? "AUTO OFF" : autoMode == 1 ? "AUTO REG"
                              : smartLocked ? "SMART LOCKED" : "SMART ANALYSING");
    autoGainBtn.setToggleState(autoMode > 0, juce::dontSendNotification);
    autoGainBtn.getProperties().set("progress", smartSelected && !smartLocked
        ? proc.smartAutoGainProgress.load(std::memory_order_relaxed) : 0.0f);
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
    const int workspaceH = juce::jlimit(170, 196, juce::roundToInt(getHeight() * 0.275f));
    g.fillAll(fg);
    g.setColour(bg); g.fillRect(0, 0, getWidth(), headerH);
    const int seamX = juce::jmin(220, getWidth() / 4);
    const int seam = juce::jmax(18, headerH * 3 / 8);
    g.setColour(fg); g.fillRect(seamX, 0, seam, seam); g.fillRect(seamX, headerH - seam, seam, seam);
    g.setColour(bg); g.fillRect(0, getHeight() - workspaceH, getWidth(), workspaceH);

    g.setFont(mono(9.0f, true));
    g.setColour(fg.withAlpha(0.55f));
    const std::array<juce::Slider*, 21> sliders { &slopeSlider, &dynThreshold, &dynRange, &dynRatio, &dynAttack,
                                                  &dynRelease, &driveSlider, &driveCharacterSlider,
                                                  &driveMixSlider, &driveOutputSlider, &outputSlider,
                                                  &matchAmount, &matchSmoothing, &matchLow, &matchHigh, &matchTime,
                                                  &analyzerRange, &analyzerFloor, &analyzerSpeed, &analyzerAveraging, &analyzerTilt };
    for (auto* slider : sliders)
        if (slider->isVisible() && slider != &outputSlider)
            g.drawText(slider->getName(), slider->getX(), slider->getY() - 12, slider->getWidth(), 11, juce::Justification::centred);
}

void DefaultEqualizerAudioProcessorEditor::resized()
{
    const int w = getWidth(), h = getHeight();
    const int headerH = juce::jlimit(56, 70, juce::roundToInt(64.0f * w / 860.0f));
    const int workspaceH = juce::jlimit(170, 196, juce::roundToInt(h * 0.275f));
    const int wordW = juce::jmin(220, w / 4);
    const int seamW = juce::jmax(18, headerH * 3 / 8);
    themeBtn.setBounds(0, 0, wordW, headerH);

    const int actionY = juce::jmax(8, (headerH - 40) / 2), actionH = juce::jmin(40, headerH - 12);
    powerBtn.setBounds(w - 88, actionY, 76, actionH);
    autoGainBtn.setBounds(w - 214, actionY, 116, actionH);
    int globalX = wordW + seamW + 8;
    const int globalGap = 4;
    const int availableGlobal = juce::jmax(284, w - 222 - globalX);
    const int phaseW = availableGlobal < 340 ? 88 : 106;
    const int osW = availableGlobal < 340 ? 50 : 58;
    const int decrampW = availableGlobal < 340 ? 62 : 72;
    const int outW = actionH * 2;
    phaseModeBox.setBounds(globalX, actionY, phaseW, actionH); globalX += phaseW + globalGap;
    oversamplingBox.setBounds(globalX, actionY, osW, actionH); globalX += osW + globalGap;
    decrampBtn.setBounds(globalX, actionY, decrampW, actionH); globalX += decrampW + globalGap;
    outputSlider.setBounds(globalX, actionY, outW, actionH);

    const int graphTop = headerH + 10;
    const int graphBottom = h - workspaceH - 10;
    responseCurve.setBounds(10, graphTop, w - 20, juce::jmax(160, graphBottom - graphTop));
    levelMeter.setBounds(w - 18, graphTop + 3, 6, juce::jmax(30, graphBottom - graphTop - 6));

    const int workspaceY = h - workspaceH;
    const int tabsY = workspaceY + 8, tabH = 28;
    int tx = 10;
    bandPageBtn.setBounds(tx, tabsY, 72, tabH); tx += 76;
    dynamicPageBtn.setBounds(tx, tabsY, 88, tabH); tx += 92;
    analyzerPageBtn.setBounds(tx, tabsY, 64, tabH); tx += 68;
    matchPageBtn.setBounds(tx, tabsY, 72, tabH);

    const int contentY = tabsY + tabH + 16;
    const int contentH = h - contentY - 8;
    const int gap = juce::jlimit(4, 7, w / 170);
    const int rowH = 30;
    const int knobSide = juce::jlimit(62, 86, (contentH - 8));
    const int knobY = contentY + juce::jmax(4, (contentH - knobSide) / 2);

    if (workspacePage == WorkspacePage::Band)
    {
        const int satW = w >= 900 ? 118 : 96;
        const bool showSecondary = displayedDriveMode == 5 || displayedDriveMode == 9;
        const int topTotal = 42 + 48 + 88 + 76 + 42 + 80 + 58 + 76 + satW
                           + gap * 8 + (showSecondary ? 32 + gap : 0);
        int x = juce::jmax(10, (w - topTotal) / 2);
        const int topY = contentY;
        auto place = [&](juce::Component& component, int width)
        { component.setBounds(x, topY, width, rowH); x += width + gap; };
        place(bandOn, 42); place(bandSolo, 48); place(typeBox, 88); place(adaptiveQBtn, 76);
        place(placementModeBtn, 42); place(placementSlider, 80); place(driveOn, 58); place(driveAutoGain, 76);
        place(saturationBox, satW);
        driveSecondarySlider.setBounds(x, contentY, 32, contentH);

        const int lowerY = contentY + rowH + 10;
        const int lowerSide = juce::jlimit(56, 80, contentH - rowH - 10);
        const int total = lowerSide * 5 + gap * 4;
        x = juce::jmax(10, (w - total) / 2);
        for (auto* slider : { &slopeSlider, &driveSlider, &driveCharacterSlider, &driveMixSlider, &driveOutputSlider })
        { slider->setBounds(x, lowerY, lowerSide, lowerSide); x += lowerSide + gap; }
    }
    else if (workspacePage == WorkspacePage::Dynamic)
    {
        const int pairW = 78;
        const int dynKnob = juce::jlimit(56, knobSide, (w - pairW - 88 - 112 - 10 - gap * 7) / 5);
        const int dynTotal = pairW + 88 + 112 + dynKnob * 5 + gap * 7;
        int x = juce::jmax(10, (w - dynTotal) / 2);
        dynOn.setBounds(x, contentY, pairW, rowH); dynModeBox.setBounds(x, contentY + rowH + 4, pairW, rowH); x += pairW + gap;
        sidechainBox.setBounds(x, contentY, 88, rowH); sidechainAudition.setBounds(x, contentY + rowH + 4, 88, rowH); x += 88 + gap;
        dynLookahead.setBounds(x, contentY + 10, 112, 42); x += 112 + gap;
        const int dynKnobY = contentY + juce::jmax(0, (contentH - dynKnob) / 2);
        for (auto* slider : { &dynThreshold, &dynRange, &dynRatio, &dynAttack, &dynRelease })
        { slider->setBounds(x, dynKnobY, dynKnob, dynKnob); x += dynKnob + gap; }
    }
    else if (workspacePage == WorkspacePage::Analyzer)
    {
        const int buttonW = 90;
        const int rtaKnob = juce::jlimit(56, knobSide, (w - buttonW * 2 - 10 - gap * 6) / 5);
        const int rtaTotal = buttonW * 2 + rtaKnob * 5 + gap * 6;
        int x = juce::jmax(10, (w - rtaTotal) / 2);
        analyzerVisible.setBounds(x, contentY, buttonW, rowH); analyzerPeakHold.setBounds(x, contentY + rowH + 4, buttonW, rowH); x += buttonW + gap;
        spectrumFreeze.setBounds(x, contentY, buttonW, rowH); analyzerResolutionBox.setBounds(x, contentY + rowH + 4, buttonW, rowH); x += buttonW + gap;
        const int rtaY = contentY + juce::jmax(0, (contentH - rtaKnob) / 2);
        for (auto* slider : { &analyzerRange, &analyzerFloor, &analyzerSpeed, &analyzerAveraging, &analyzerTilt })
        { slider->setBounds(x, rtaY, rtaKnob, rtaKnob); x += rtaKnob + gap; }
    }
    else if (workspacePage == WorkspacePage::Match)
    {
        const int buttonW = 108;
        const int matchKnob = juce::jlimit(56, knobSide, (w - buttonW * 2 - 10 - gap * 6) / 5);
        const int matchTotal = buttonW * 2 + matchKnob * 5 + gap * 6;
        int x = juce::jmax(10, (w - matchTotal) / 2);
        matchCaptureBtn.setBounds(x, contentY, buttonW, rowH); matchApplyBtn.setBounds(x, contentY + rowH + 4, buttonW, rowH); x += buttonW + gap;
        matchCommitBtn.setBounds(x, contentY, buttonW, rowH); matchClearBtn.setBounds(x, contentY + rowH + 4, buttonW, rowH); x += buttonW + gap;
        const int matchY = contentY + juce::jmax(0, (contentH - matchKnob) / 2);
        for (auto* slider : { &matchAmount, &matchSmoothing, &matchLow, &matchHigh, &matchTime })
        { slider->setBounds(x, matchY, matchKnob, matchKnob); x += matchKnob + gap; }
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
