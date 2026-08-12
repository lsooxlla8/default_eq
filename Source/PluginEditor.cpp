#include "PluginEditor.h"

namespace
{
juce::Font mono(float size, bool bold = false)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
                                        size,
                                        bold ? juce::Font::bold : juce::Font::plain));
}

double parseNumber(juce::String text, double multiplierForK = 1.0,
                   bool secondsToMilliseconds = false)
{
    auto lower = text.trim().toLowerCase().replaceCharacter(',', '.');
    double multiplier = 1.0;
    if (lower.contains("khz") || lower.endsWithChar('k')) multiplier = multiplierForK;
    if (secondsToMilliseconds && lower.endsWithChar('s') && !lower.endsWith("ms"))
        multiplier = 1000.0;
    lower = lower.retainCharacters("-+0123456789.e");
    return std::isfinite(lower.getDoubleValue()) ? lower.getDoubleValue() * multiplier : 0.0;
}
}

FamilyLookAndFeel::FamilyLookAndFeel()
{
    setDark(true);
}

void FamilyLookAndFeel::setDark(bool shouldBeDark)
{
    dark = shouldBeDark;
    const auto fg = foreground();
    const auto bg = background();
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
    return mono(juce::jlimit(9.0f, 15.0f, h * 0.44f), true);
}

juce::Font FamilyLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return mono(juce::jlimit(9.0f, 13.0f, box.getHeight() * 0.52f));
}

juce::Font FamilyLookAndFeel::getLabelFont(juce::Label& label)
{
    return mono(juce::jlimit(9.0f, 13.0f, label.getHeight() * 0.55f));
}

void FamilyLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                              const juce::Colour&, bool highlighted, bool down)
{
    const bool active = button.getToggleState() || down;
    auto r = button.getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(active ? foreground() : background());
    g.fillRect(r);
    g.setColour(active ? background() : foreground());
    g.drawRect(r, 2.0f);
    if (highlighted && !active)
        g.drawRect(r.reduced(4.0f), 1.0f);
}

void FamilyLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                        bool, bool down)
{
    const bool active = button.getToggleState() || down;
    g.setColour((active ? background() : foreground())
                    .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.35f));
    g.setFont(getTextButtonFont(button, button.getHeight()));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(5, 2),
                     juce::Justification::centred, 1);
}

void FamilyLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                          bool highlighted, bool down)
{
    drawButtonBackground(g, button, {}, highlighted, down);
    const bool active = button.getToggleState() || down;
    g.setColour((active ? background() : foreground())
                    .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.35f));
    g.setFont(mono(juce::jlimit(8.0f, 12.0f, button.getHeight() * 0.44f), true));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(5, 2),
                     juce::Justification::centred, 1);
}

void FamilyLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                          float pos, float, float, juce::Slider& slider)
{
    const auto side = (float)juce::jmin(w, h);
    auto r = juce::Rectangle<float>((float)x + ((float)w - side) * 0.5f,
                                    (float)y + ((float)h - side) * 0.5f,
                                    side, side).reduced(2.0f);
    const auto fg = foreground().withMultipliedAlpha(slider.isEnabled() ? 1.0f : 0.35f);
    const auto bg = background();
    g.setColour(bg);
    g.fillRect(r);
    g.setColour(fg);
    g.drawRect(r, 2.0f);
    auto inner = r.reduced(8.0f);
    g.setColour(fg.withAlpha(0.12f));
    g.fillRect(inner);
    for (int i = 1; i < 4; ++i)
    {
        const float p = (float)i / 4.0f;
        g.setColour(fg.withAlpha(0.16f));
        g.drawVerticalLine(juce::roundToInt(inner.getX() + inner.getWidth() * p),
                           inner.getY(), inner.getBottom());
        g.drawHorizontalLine(juce::roundToInt(inner.getY() + inner.getHeight() * p),
                             inner.getX(), inner.getRight());
    }
    const float fillH = inner.getHeight() * pos;
    g.setColour(fg.withAlpha(0.88f));
    g.fillRect(inner.getX(), inner.getBottom() - fillH, inner.getWidth(), fillH);
    const float marker = juce::jmax(4.0f, side * 0.07f);
    g.setColour(fg);
    g.fillRect(inner.getX() + pos * (inner.getWidth() - marker),
               inner.getY() - marker * 0.5f, marker, marker);
}

void FamilyLookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool,
                                      int, int, int, int, juce::ComboBox& box)
{
    const auto fg = foreground().withMultipliedAlpha(box.isEnabled() ? 1.0f : 0.35f);
    g.setColour(background());
    g.fillRect(0, 0, w, h);
    g.setColour(fg);
    g.drawRect(0, 0, w, h, 2);
    juce::Path triangle;
    triangle.addTriangle((float)w - 13.0f, h * 0.42f,
                         (float)w - 5.0f, h * 0.42f,
                         (float)w - 9.0f, h * 0.66f);
    g.fillPath(triangle);
}

static juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}

// ── Knob initializer ───────────────────────────────────────────────
void DefaultEqualizerAudioProcessorEditor::initKnob(juce::Slider& s, juce::Colour c, bool large)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, large ? 70 : 52, 14);
    s.setColour(juce::Slider::rotarySliderFillColourId, c);
    addAndMakeVisible(s);
}

// ── Dark-style combo box helper ────────────────────────────────────
static void styleCombo(juce::ComboBox& cb)
{
    cb.setJustificationType(juce::Justification::centred);
}

// ── Constructor ────────────────────────────────────────────────────
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

    const int savedW = uiPreferences->getIntValue("windowWidth", 860);
    const int savedH = uiPreferences->getIntValue("windowHeight", 620);
    setSize(savedW, savedH);
    setResizable(true, true);
    setResizeLimits(720, 519, 1200, 865);
    getConstrainer()->setFixedAspectRatio(860.0 / 620.0);

    addAndMakeVisible(responseCurve);

    themeBtn.setTooltip("Toggle exact paper/ink inversion");
    themeBtn.onClick = [this]
    {
        darkTheme = !darkTheme;
        familyLook.setDark(darkTheme);
        responseCurve.setDarkMode(darkTheme);
        uiPreferences->setValue("darkTheme", darkTheme);
        sendLookAndFeelChange();
        repaint();
    };
    addAndMakeVisible(themeBtn);

    reducedMotionBtn.setToggleState(
        uiPreferences->getBoolValue("reducedMotion", false), juce::dontSendNotification);
    reducedMotionBtn.onClick = [this]
    {
        uiPreferences->setValue("reducedMotion", reducedMotionBtn.getToggleState());
    };
    addAndMakeVisible(reducedMotionBtn);

    powerBtn.setClickingTogglesState(true);
    powerBtn.setTooltip("Click-free global bypass");
    addAndMakeVisible(powerBtn);
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, "plugin_enabled", powerBtn);

    // ── Band selector buttons ──
    for (int i = 0; i < kNumBands; ++i)
    {
        auto& btn = bandBtns[(size_t)i];
        btn.setButtonText(juce::String(i + 1));
        btn.setClickingTogglesState(false);
        btn.setColour(juce::TextButton::buttonColourId,
                      ResponseCurveComponent::getBandColour(i).withAlpha(0.25f));
        btn.setColour(juce::TextButton::buttonOnColourId,
                      ResponseCurveComponent::getBandColour(i));
        btn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        btn.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.7f));
        btn.onClick = [this, i] { selectBand(i); };
        addAndMakeVisible(btn);
    }

    // ── Selected band controls ──
    bandOn.setButtonText("On");
    bandOn.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF8BC34A));
    addAndMakeVisible(bandOn);

    bandSolo.setButtonText("Solo");
    bandSolo.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFFFD54F));
    addAndMakeVisible(bandSolo);

    typeBox.addItemList({ "Bell", "LowShelf", "HighShelf", "HighPass", "LowPass", "Bandpass", "Notch" }, 1);
    styleCombo(typeBox);
    addAndMakeVisible(typeBox);

    slopeBox.addItemList({ "12 dB", "24 dB", "48 dB" }, 1);
    styleCombo(slopeBox);
    addAndMakeVisible(slopeBox);

    channelBox.addItemList({ "Both", "L / Mid", "R / Side" }, 1);
    styleCombo(channelBox);
    addAndMakeVisible(channelBox);

    linkBox.addItemList({ "--", "A", "B" }, 1);
    styleCombo(linkBox);
    addAndMakeVisible(linkBox);

    auto bandCol = ResponseCurveComponent::getBandColour(0);
    initKnob(freqKnob,  bandCol, true);
    initKnob(gainKnob,  bandCol, true);
    initKnob(qKnob,     bandCol, true);
    initKnob(driveKnob, bandCol, false);

    freqKnob.textFromValueFunction = [](double v)
    {
        return v >= 1000.0 ? juce::String(v / 1000.0, 2) + " kHz"
                           : juce::String(v, 1) + " Hz";
    };
    freqKnob.valueFromTextFunction = [](const juce::String& s) { return parseNumber(s, 1000.0); };
    const auto dbText = [](double v)
    {
        const double clean = std::abs(v) < 0.005 ? 0.0 : v;
        return juce::String(clean, 2) + " dB";
    };
    gainKnob.textFromValueFunction = dbText;
    gainKnob.valueFromTextFunction = [](const juce::String& s) { return parseNumber(s); };
    qKnob.textFromValueFunction = [](double v) { return juce::String(v, 3); };
    qKnob.valueFromTextFunction = [](const juce::String& s) { return parseNumber(s); };
    driveKnob.textFromValueFunction = [](double v) { return juce::String(v, 1) + "%"; };
    driveKnob.valueFromTextFunction = [](const juce::String& s) { return parseNumber(s); };

#if PROEQ8
    satModeBox.addItemList({ "Tanh", "Tube", "Tape", "Transistor" }, 1);
    styleCombo(satModeBox);
    satModeBox.setTooltip("Saturation mode (Tanh/Tube/Tape/Transistor)");
    addAndMakeVisible(satModeBox);
#endif

    // Dynamic EQ
    dynOn.setButtonText("Dyn");
    dynOn.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFFF5722));
    addAndMakeVisible(dynOn);

    auto dynCol = juce::Colour(0xFFFF5722);
    initKnob(dynThreshKnob,  dynCol, false);
    initKnob(dynRatioKnob,   dynCol, false);
    initKnob(dynAttackKnob,  dynCol, false);
    initKnob(dynReleaseKnob, dynCol, false);

    // ── Global controls ──
    auto globalCol = juce::Colour(0xFF90CAF9);
    initKnob(outputGainSlider, globalCol, true);
    initKnob(scaleSlider,      globalCol, false);
    outputGainSlider.textFromValueFunction = dbText;
    outputGainSlider.valueFromTextFunction = [](const juce::String& s) { return parseNumber(s); };

    adaptiveQBtn.setButtonText("Adaptive Q");
    addAndMakeVisible(adaptiveQBtn);
    linPhaseBtn.setButtonText("Lin Phase");
    addAndMakeVisible(linPhaseBtn);
    autoGainBtn.setButtonText("Auto Gain");
    autoGainBtn.setTooltip("Loudness-compensated bypass (match output RMS to input)");
    addAndMakeVisible(autoGainBtn);

    oversamplingBox.addItemList({ "1x", "2x", "4x", "8x" }, 1);
    styleCombo(oversamplingBox);
    addAndMakeVisible(oversamplingBox);
    procModeBox.addItemList({ "Stereo", "Mid-Side" }, 1);
    styleCombo(procModeBox);
    addAndMakeVisible(procModeBox);

    // Global attachments (permanent)
    outputGainAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "output_gain", outputGainSlider);
    scaleAtt       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "scale", scaleSlider);
    adaptiveQAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "adaptive_q", adaptiveQBtn);
    linPhaseAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "linear_phase", linPhaseBtn);
    autoGainAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "auto_gain", autoGainBtn);
    oversamplingAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "oversampling", oversamplingBox);
    procModeAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "proc_mode", procModeBox);

    // ── Toolbar buttons ──
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    addAndMakeVisible(undoBtn);
    addAndMakeVisible(redoBtn);

    matchCapBtn.onClick = [this] {
        if (proc.matchEQ.isCapturing()) proc.matchEQ.stopCapture();
        else proc.matchEQ.startCapture();
    };
    matchAppBtn.onClick = [this] { proc.matchEQ.setMatchActive(!proc.matchEQ.isMatchActive()); };
    matchClrBtn.onClick = [this] { proc.matchEQ.clear(); };
    addAndMakeVisible(matchCapBtn);
    addAndMakeVisible(matchAppBtn);
    addAndMakeVisible(matchClrBtn);

    addAndMakeVisible(levelMeter);

    // ── Presets ──
    styleCombo(presetBox);
    addAndMakeVisible(presetBox);
    presetBox.setTextWhenNothingSelected("-- Presets --");
    presetBox.onChange = [this] { onPresetSelected(); };
    saveBtn.onClick = [this] { onSaveClicked(); };
    delBtn.onClick  = [this] { onDeleteClicked(); };
    addAndMakeVisible(saveBtn);
    addAndMakeVisible(delBtn);
    refreshPresetList();

    // ── Spectrum toggle ──
    postEqToggle.setButtonText("Post EQ");
    postEqToggle.setToggleState(true, juce::dontSendNotification);
    postEqToggle.onClick = [this] { showPostSpectrum = postEqToggle.getToggleState(); };
    addAndMakeVisible(postEqToggle);

    // ── Tooltips ──
    freqKnob.setTooltip("Band frequency (20 Hz - 20 kHz)");
    gainKnob.setTooltip("Boost/cut (-24 dB to +24 dB)");
    qKnob.setTooltip("Bandwidth (0.1 = wide, 24 = narrow)");
    driveKnob.setTooltip("Per-band saturation (tanh waveshaper)");
    typeBox.setTooltip("Filter type");
    slopeBox.setTooltip("Filter slope (12/24/48 dB/oct)");
    channelBox.setTooltip("Per-band channel routing");
    linkBox.setTooltip("Band link group (A or B)");
    bandOn.setTooltip("Enable/disable this band");
    bandSolo.setTooltip("Solo — audition this band only");
    dynOn.setTooltip("Enable dynamic EQ for this band");
    dynThreshKnob.setTooltip("Dynamic EQ threshold (-60 to 0 dB)");
    dynRatioKnob.setTooltip("Dynamic EQ ratio (1:1 to 20:1)");
    dynAttackKnob.setTooltip("Dynamic EQ attack (0.1 - 100 ms)");
    dynReleaseKnob.setTooltip("Dynamic EQ release (1 - 1000 ms)");
    outputGainSlider.setTooltip("Master output gain (-24 to +24 dB)");
    scaleSlider.setTooltip("Scale all band gains (0.1x to 2x)");
    adaptiveQBtn.setTooltip("Auto-widen Q with increasing gain");
    linPhaseBtn.setTooltip("Linear phase mode (adds 2048 samples latency)");
    oversamplingBox.setTooltip("Oversampling factor (higher = cleaner, more CPU)");
    procModeBox.setTooltip("Stereo or Mid-Side processing");
    matchCapBtn.setTooltip("Capture reference spectrum");
    matchAppBtn.setTooltip("Apply match EQ correction");
    matchClrBtn.setTooltip("Clear match EQ data");
    undoBtn.setTooltip("Undo last parameter change");
    redoBtn.setTooltip("Redo last undone change");
    postEqToggle.setTooltip("Toggle pre/post EQ spectrum display");

    dynThreshKnob.textFromValueFunction = dbText;
    dynThreshKnob.valueFromTextFunction = [](const juce::String& s) { return parseNumber(s); };
    dynRatioKnob.textFromValueFunction = [](double v) { return juce::String(v, 2) + ":1"; };
    dynRatioKnob.valueFromTextFunction = [](const juce::String& s) { return parseNumber(s); };
    dynAttackKnob.textFromValueFunction = [](double v) { return juce::String(v, 1) + " ms"; };
    dynAttackKnob.valueFromTextFunction = [](const juce::String& s) { return parseNumber(s, 1.0, true); };
    dynReleaseKnob.textFromValueFunction = [](double v)
    {
        return v >= 1000.0 ? juce::String(v / 1000.0, 2) + " s"
                           : juce::String(v, 0) + " ms";
    };
    dynReleaseKnob.valueFromTextFunction = [](const juce::String& s) { return parseNumber(s, 1.0, true); };

    // ── A/B comparison buttons ──
    abBtn.onClick = [this] { toggleAB(); };
    abBtn.setTooltip("Switch between A/B settings");
    addAndMakeVisible(abBtn);

    copyABBtn.onClick = [this] {
        if (proc.isSlotA)
            proc.copySnapshot(true);   // A → B
        else
            proc.copySnapshot(false);  // B → A
    };
    copyABBtn.setTooltip("Copy current slot to the other");
    addAndMakeVisible(copyABBtn);

    // Store initial state into slot A
    proc.storeSnapshot(true);


    // ── Initial band selection ──
    rebindBandControls(0);
    selectBand(0);
    startTimerHz(30);
}

DefaultEqualizerAudioProcessorEditor::~DefaultEqualizerAudioProcessorEditor()
{
    if (uiPreferences)
    {
        uiPreferences->setValue("windowWidth", getWidth());
        uiPreferences->setValue("windowHeight", getHeight());
        uiPreferences->saveIfNeeded();
    }
    setLookAndFeel(nullptr);
}

// ── Rebind controls to a specific band ─────────────────────────────
void DefaultEqualizerAudioProcessorEditor::rebindBandControls(int band)
{
    const int idx = band + 1;

    // Destroy old attachments first
    bandOnAtt.reset(); bandSoloAtt.reset(); dynOnAtt.reset();
    typeAtt.reset(); slopeAtt.reset(); channelAtt.reset(); linkAtt.reset();
    freqAtt.reset(); gainAtt.reset(); qAtt.reset(); driveAtt.reset();
    dynThreshAtt.reset(); dynRatioAtt.reset(); dynAttackAtt.reset(); dynReleaseAtt.reset();

    // Create new attachments
    bandOnAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, bandId(idx, "on"),   bandOn);
    bandSoloAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, bandId(idx, "solo"), bandSolo);
    typeAtt      = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, bandId(idx, "type"),  typeBox);
    slopeAtt     = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, bandId(idx, "slope"), slopeBox);
    channelAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, bandId(idx, "ch"),   channelBox);
    linkAtt      = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, bandId(idx, "link"), linkBox);
    freqAtt      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(idx, "freq"),  freqKnob);
    gainAtt      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(idx, "gain"),  gainKnob);
    qAtt         = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(idx, "q"),     qKnob);
    driveAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(idx, "drive"), driveKnob);
#if PROEQ8
    satModeAtt.reset();
    satModeAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, bandId(idx, "sat_mode"), satModeBox);
#endif
    dynOnAtt     = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, bandId(idx, "dyn_on"),      dynOn);
    dynThreshAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(idx, "dyn_thresh"),  dynThreshKnob);
    dynRatioAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(idx, "dyn_ratio"),   dynRatioKnob);
    dynAttackAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(idx, "dyn_attack"),  dynAttackKnob);
    dynReleaseAtt= std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, bandId(idx, "dyn_release"), dynReleaseKnob);

    // Update knob colors to match band
    auto col = ResponseCurveComponent::getBandColour(band);
    freqKnob.setColour(juce::Slider::rotarySliderFillColourId, col);
    gainKnob.setColour(juce::Slider::rotarySliderFillColourId, col);
    qKnob.setColour(juce::Slider::rotarySliderFillColourId, col);
    driveKnob.setColour(juce::Slider::rotarySliderFillColourId, col);
}

// ── Select band ────────────────────────────────────────────────────
void DefaultEqualizerAudioProcessorEditor::selectBand(int band)
{
    selectedBand = band;
    rebindBandControls(band);
    responseCurve.setSelectedBand(band);

    for (int i = 0; i < kNumBands; ++i)
    {
        bool sel = (i == band);
        bandBtns[(size_t)i].setColour(juce::TextButton::buttonColourId,
            ResponseCurveComponent::getBandColour(i).withAlpha(sel ? 0.85f : 0.2f));
    }

    powerBtn.setButtonText(powerBtn.getToggleState() ? "ON" : "OFF");
    repaint();
}

// ── Timer ──────────────────────────────────────────────────────────
void DefaultEqualizerAudioProcessorEditor::timerCallback()
{
    auto& fifo = showPostSpectrum ? proc.spectrumFifo : proc.preSpectrumFifo;
    if (fifo.processIfReady())
    {
        responseCurve.pushSpectrumData(
            fifo.getMagnitudes(), fifo.getNumBins(),
            proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0);
    }

    // Sync band selection from response curve (when user clicks nodes)
    int curveSel = responseCurve.getSelectedBand();
    if (curveSel >= 0 && curveSel != selectedBand)
        selectBand(curveSel);

    // Update band button on/off appearance
    for (int i = 0; i < kNumBands; ++i)
    {
        bool on = proc.apvts.getRawParameterValue(bandId(i + 1, "on"))->load() > 0.5f;
        bandBtns[(size_t)i].setAlpha(on ? 1.0f : 0.4f);
    }

    // ── Linear phase incompatibility: grey out unsupported controls ──
    // When linear phase is on, drive, dynamic EQ, M/S, and oversampling
    // are silently ignored by the DSP path. Disable the controls to
    // make this visible to the user (fixes #12).
    const bool linPhaseOn = proc.apvts.getRawParameterValue("linear_phase")->load() > 0.5f;
    const float linAlpha = linPhaseOn ? 0.35f : 1.0f;
    const bool linEnabled = !linPhaseOn;

    driveKnob.setEnabled(linEnabled);
    driveKnob.setAlpha(linAlpha);
    dynOn.setEnabled(linEnabled);
    dynThreshKnob.setEnabled(linEnabled);
    dynThreshKnob.setAlpha(linAlpha);
    dynRatioKnob.setEnabled(linEnabled);
    dynRatioKnob.setAlpha(linAlpha);
    dynAttackKnob.setEnabled(linEnabled);
    dynAttackKnob.setAlpha(linAlpha);
    dynReleaseKnob.setEnabled(linEnabled);
    dynReleaseKnob.setAlpha(linAlpha);
    procModeBox.setEnabled(linEnabled);
    procModeBox.setAlpha(linAlpha);
    oversamplingBox.setEnabled(linEnabled);
    oversamplingBox.setAlpha(linAlpha);
#if PROEQ8
    satModeBox.setEnabled(linEnabled);
    satModeBox.setAlpha(linAlpha);
#endif
}

// ── Paint ──────────────────────────────────────────────────────────
void DefaultEqualizerAudioProcessorEditor::paint(juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();
    const int titleH = 64;
    const int curveH = std::max(180, (int)(h * 0.52f));
    const int stripH = 28;
    const int sidebarW = 155;
    const int meterW = 36;
    const int controlsTop = titleH + curveH + stripH;

    const auto paper = familyLook.paper();
    const auto ink = familyLook.ink();
    const auto fg = darkTheme ? paper : ink;
    const auto bg = darkTheme ? ink : paper;
    g.fillAll(fg);

    // Title bar
    g.setColour(bg);
    g.fillRect(0, 0, w, titleH);
    g.setColour(fg);
    g.fillRect(220, 0, 24, 24);
    g.fillRect(220, 40, 24, 24);

    // Band strip background
    g.setColour(bg);
    g.fillRect(0, titleH + curveH, w, stripH);

    // Controls panel background
    g.setColour(bg);
    g.fillRect(0, controlsTop, w, h - controlsTop);

    // Separator lines
    g.setColour(fg.withAlpha(0.13f));
    g.drawHorizontalLine(controlsTop, 0.0f, (float)w);

    // Vertical separator between band panel and global sidebar
    const int sepX = w - sidebarW - meterW;
    g.setColour(fg.withAlpha(0.20f));
    g.drawVerticalLine(sepX, (float)controlsTop, (float)h);

    // ── Row 1 labels — computed from the SAME layout constants as resized() ──
    g.setFont(mono(10.0f, true));
    g.setColour(fg.withAlpha(0.55f));
    const int ly = controlsTop + 4;  // label y (12px above controls at controlsTop+16)

    // Mirror the x-offsets from resized() Row 1:
    const int knobL = 80, knobS = 56;
    int lx = 6;                              // On/Solo column
    lx += 48;                                // past On/Solo → typeBox start
    g.drawText("Type", lx, ly, 86, 12, juce::Justification::centred);
    lx += 92;                                // past typeBox → freqKnob start
    g.drawText("Freq", lx, ly, knobL, 12, juce::Justification::centred);
    lx += knobL + 2;
    g.drawText("Gain", lx, ly, knobL, 12, juce::Justification::centred);
    lx += knobL + 2;
    g.drawText("Q",    lx, ly, knobL, 12, juce::Justification::centred);
    lx += knobL + 2;
    g.drawText("Slope", lx, ly, 78, 12, juce::Justification::centred);
    lx += 84;
    g.drawText("Drive", lx, ly, knobS, 12, juce::Justification::centred);

    // ── Row 2 labels — match resized() Row 2 positions ──
    const int r2ly = controlsTop + 4 + 90; // label row 2
    g.drawText("Channel", 6,  r2ly, 72, 12, juce::Justification::centred);
    g.drawText("Link",    82, r2ly, 58, 12, juce::Justification::centred);

    // Dynamic EQ section
    int dlx = 155;
    g.drawText("Dyn", dlx, r2ly, 50, 12, juce::Justification::centred);
    dlx += 55;
    g.drawText("Thr",   dlx, r2ly, knobS, 12, juce::Justification::centred);
    dlx += knobS + 4;
    g.drawText("Ratio", dlx, r2ly, knobS, 12, juce::Justification::centred);
    dlx += knobS + 4;
    g.drawText("Atk",   dlx, r2ly, knobS, 12, juce::Justification::centred);
    dlx += knobS + 4;
    g.drawText("Rel",   dlx, r2ly, knobS, 12, juce::Justification::centred);

    // Sidebar labels
    const int sx = w - sidebarW - meterW + 4;
    g.drawText("Output",  sx, controlsTop + 4,  60, 12, juce::Justification::centred);
    g.drawText("Scale",   sx + 65, controlsTop + 4,  50, 12, juce::Justification::centred);

    // Band indicator on selected band
    g.setColour(fg);
    g.fillRect(2, controlsTop + 1, 3, 14);

}

// ── Resized ────────────────────────────────────────────────────────
void DefaultEqualizerAudioProcessorEditor::resized()
{
    const int w = getWidth();
    const int h = getHeight();
    const int titleH = 64;
    const int curveH = std::max(180, (int)(h * 0.52f));
    const int stripH = 28;
    const int sidebarW = 155;
    const int meterW = 36;
    const int controlsTop = titleH + curveH + stripH;
    const int controlsH = h - controlsTop;
    const int panelW = w - sidebarW - meterW;

    // Response curve
    responseCurve.setBounds(0, titleH, w, curveH);

    // Canonical 64 px family header.
    themeBtn.setBounds(0, 0, juce::jmin(220, w / 4), titleH);
    presetBox.setBounds(252, 12, juce::jmax(100, w - 650), 40);
    saveBtn.setBounds(w - 292, 12, 58, 40);
    delBtn.setBounds(w - 230, 12, 48, 40);
    postEqToggle.setBounds(w - 178, 12, 78, 40);
    powerBtn.setBounds(w - 90, 12, 78, 40);

    // Band selector strip
    {
        const int stripY = titleH + curveH;
#if PROEQ8
        // ProEQ8: 24 bands in 2 rows of 12
        constexpr int bandsPerRow = 12;
        const int btnW = std::min(72, (w - 20) / bandsPerRow);
        const int rowH = (stripH - 2) / 2;  // half height per row
        const int totalW = btnW * bandsPerRow;
        const int startX = (w - totalW) / 2;
        for (int i = 0; i < kNumBands; ++i)
        {
            const int row = i / bandsPerRow;
            const int col = i % bandsPerRow;
            bandBtns[(size_t)i].setBounds(startX + col * btnW, stripY + 1 + row * rowH, btnW - 2, rowH - 2);
        }
#else
        // FreeEQ8: 8 bands in single row
        const int btnW = std::min(60, (w - 20) / kNumBands);
        const int totalW = btnW * kNumBands;
        const int startX = (w - totalW) / 2;
        for (int i = 0; i < kNumBands; ++i)
            bandBtns[(size_t)i].setBounds(startX + i * btnW, stripY + 2, btnW - 2, stripH - 4);
#endif
    }

    // ── Selected band controls ──
    const int cy = controlsTop + 16;
    const int knobL = 80;  // large knob
    const int knobS = 56;  // small knob
    int x = 6;

    // Row 1: On/Solo | Type | Freq | Gain | Q | Slope | Drive
    bandOn.setBounds(x, cy, 44, 20);
    bandSolo.setBounds(x, cy + 22, 44, 20);
    x += 48;

    typeBox.setBounds(x, cy + 6, 86, 22);
    x += 92;

    freqKnob.setBounds(x, cy - 4, knobL, knobL);
    x += knobL + 2;
    gainKnob.setBounds(x, cy - 4, knobL, knobL);
    x += knobL + 2;
    qKnob.setBounds(x, cy - 4, knobL, knobL);
    x += knobL + 2;

    slopeBox.setBounds(x, cy + 6, 78, 22);
    x += 84;

    driveKnob.setBounds(x, cy, knobS, knobS);
#if PROEQ8
    x += knobS + 4;
    satModeBox.setBounds(x, cy + 6, 82, 22);
#endif

    // Row 2: Channel | Link | Dynamic EQ
    const int r2y = cy + 90 + 12;
    channelBox.setBounds(6, r2y, 72, 20);
    linkBox.setBounds(82, r2y, 58, 20);

    int dynX = 155;
    dynOn.setBounds(dynX, r2y, 50, 20);
    dynX += 55;
    dynThreshKnob.setBounds(dynX, r2y - 8, knobS, knobS);
    dynX += knobS + 4;
    dynRatioKnob.setBounds(dynX, r2y - 8, knobS, knobS);
    dynX += knobS + 4;
    dynAttackKnob.setBounds(dynX, r2y - 8, knobS, knobS);
    dynX += knobS + 4;
    dynReleaseKnob.setBounds(dynX, r2y - 8, knobS, knobS);

    // ── Global sidebar ──
    const int sx = w - sidebarW - meterW + 4;
    const int sw = sidebarW - 8;
    int sy = controlsTop + 16;

    outputGainSlider.setBounds(sx, sy, 60, 70);
    scaleSlider.setBounds(sx + 65, sy, 55, 55);
    sy += 74;

    const int halfW = (sw - 4) / 2;
    adaptiveQBtn.setBounds(sx, sy, sw, 18);
    sy += 20;
    linPhaseBtn.setBounds(sx, sy, sw, 18);
    sy += 20;
    autoGainBtn.setBounds(sx, sy, sw, 18);
    sy += 22;
    oversamplingBox.setBounds(sx, sy, halfW, 18);
    procModeBox.setBounds(sx + halfW + 4, sy, halfW, 18);
    sy += 22;

    // Match EQ
    matchCapBtn.setBounds(sx, sy, sw / 3 - 1, 18);
    matchAppBtn.setBounds(sx + sw / 3 + 1, sy, sw / 3 - 1, 18);
    matchClrBtn.setBounds(sx + 2 * (sw / 3) + 2, sy, sw / 3 - 1, 18);
    sy += 22;

    // Undo/Redo
    undoBtn.setBounds(sx, sy, halfW, 18);
    redoBtn.setBounds(sx + halfW + 4, sy, halfW, 18);
    sy += 22;

    // A/B comparison
    abBtn.setBounds(sx, sy, halfW, 18);
    copyABBtn.setBounds(sx + halfW + 4, sy, halfW, 18);
    sy += 22;
    reducedMotionBtn.setBounds(sx, sy, sw, 18);

    // Level meter
    levelMeter.setBounds(w - meterW, controlsTop, meterW - 4, controlsH);
}

// ── Preset management ──────────────────────────────────────────────
void DefaultEqualizerAudioProcessorEditor::refreshPresetList()
{
    presetBox.clear(juce::dontSendNotification);
    if (proc.presetManager)
    {
        auto presets = proc.presetManager->getPresetList();
        for (int i = 0; i < presets.size(); ++i)
            presetBox.addItem(presets[i], i + 1);

        auto current = proc.presetManager->getCurrentPreset();
        if (current.isNotEmpty())
        {
            int idx = presets.indexOf(current);
            if (idx >= 0)
                presetBox.setSelectedId(idx + 1, juce::dontSendNotification);
        }
    }
}

void DefaultEqualizerAudioProcessorEditor::onPresetSelected()
{
    const auto name = presetBox.getText();
    if (name.isNotEmpty() && proc.presetManager)
        proc.presetManager->loadPreset(name);
}

void DefaultEqualizerAudioProcessorEditor::onSaveClicked()
{
    // A2: own the AlertWindow via unique_ptr. No more `new ... + delete dlg`
    //     inside a `deleteWhenDismissed=true` callback (previously a latent
    //     double-free).
    if (activeDialog) return;   // one modal dialog at a time
    auto name = proc.presetManager ? proc.presetManager->getCurrentPreset() : juce::String();
    activeDialog = std::make_unique<juce::AlertWindow>("Save Preset", "Enter preset name:",
                                                        juce::AlertWindow::NoIcon);
    activeDialog->addTextEditor("name", name, "Preset Name:");
    activeDialog->addButton("Save", 1);
    activeDialog->addButton("Cancel", 0);

    activeDialog->enterModalState(
        true,
        juce::ModalCallbackFunction::create(
            [weak = juce::WeakReference<DefaultEqualizerAudioProcessorEditor>(this)](int result)
            {
                auto* editor = weak.get();
                if (editor == nullptr) return;
                if (result == 1 && editor->activeDialog)
                {
                    auto presetName = editor->activeDialog->getTextEditorContents("name");
                    if (presetName.isNotEmpty() && editor->proc.presetManager)
                    {
                        editor->proc.presetManager->savePreset(presetName);
                        editor->refreshPresetList();
                    }
                }
                editor->activeDialog.reset();
            }),
        /* deleteWhenDismissed */ false);
}

void DefaultEqualizerAudioProcessorEditor::onDeleteClicked()
{
    const auto name = presetBox.getText();
    if (name.isNotEmpty() && proc.presetManager)
    {
        proc.presetManager->deletePreset(name);
        refreshPresetList();
    }
}

void DefaultEqualizerAudioProcessorEditor::toggleAB()
{
    // Store current into the active slot, then switch
    proc.storeSnapshot(proc.isSlotA);
    proc.isSlotA = !proc.isSlotA;
    proc.recallSnapshot(proc.isSlotA);

    abBtn.setButtonText(proc.isSlotA ? "A" : "B");
    copyABBtn.setButtonText(proc.isSlotA ? "A\xe2\x86\x92" "B" : "B\xe2\x86\x92" "A");
    rebindBandControls(selectedBand);
    repaint();
}
