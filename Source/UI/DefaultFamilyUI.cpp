#include "DefaultFamilyUI.h"
#include "FilterIcon.h"
#include <DefaultEQFonts.h>

#include <cmath>

namespace default_family
{
namespace
{
juce::PropertiesFile& themeProperties()
{
    static juce::PropertiesFile properties([]
    {
        juce::PropertiesFile::Options options;
        // Shared with default_distortion so every default_* instance sees the
        // same family preference without putting it in project/audio state.
        options.applicationName = "default_distortion-ui";
        options.filenameSuffix = "settings";
        options.folderName = "icanseesounds";
        options.osxLibrarySubFolder = "Application Support";
        options.millisecondsBeforeSaving = 0;
        return options;
    }());
    return properties;
}

float scaleOf(const juce::Component& component) noexcept
{
    if (const auto* look = dynamic_cast<const LookAndFeel*>(&component.getLookAndFeel()))
        return look->getUiScale();
    return 1.0f;
}

juce::Colour foregroundOf(const juce::Component& component)
{
    return component.findColour(LookAndFeel::foregroundColourId);
}

juce::Colour backgroundOf(const juce::Component& component)
{
    return component.findColour(LookAndFeel::backgroundColourId);
}

float controlFontHeight(float controlHeight) noexcept
{
    juce::ignoreUnused(controlHeight);
    return metrics::ordinaryFontHeight;
}

int controlTextPadding(float scale) noexcept
{
    return juce::jmax(2, juce::roundToInt(3.0f * scale));
}

const juce::Typeface::Ptr& familyTypeface(bool bold)
{
    static const auto medium = juce::Typeface::createSystemTypefaceFor(
        DefaultEQFonts::JetBrainsMonoMedium_ttf,
        DefaultEQFonts::JetBrainsMonoMedium_ttfSize);
    static const auto extraBold = juce::Typeface::createSystemTypefaceFor(
        DefaultEQFonts::JetBrainsMonoExtraBold_ttf,
        DefaultEQFonts::JetBrainsMonoExtraBold_ttfSize);
    return bold ? extraBold : medium;
}
}

juce::Font mono(float height, bool bold)
{
    return juce::Font(juce::FontOptions(familyTypeface(bold)).withPointHeight(height)
                                                        .withFallbackEnabled(false));
}

namespace
{
// CSS font-size addresses the em square. JUCE's point-height mode does the
// same, so both renderers can use the same nominal size without scaling
// already-rasterised glyphs.
constexpr float cssFontToJuceHeight = 1.0f;
juce::Font trackedMono(float fontSize, bool extraBold,
                       float letterSpacingEm, float scale)
{
    auto font = mono(fontSize * cssFontToJuceHeight * scale, extraBold);
    if (letterSpacingEm == 0.0f || font.getHeight() <= 0.0f)
        return font;

    const float cssTracking = letterSpacingEm * fontSize * scale;
    return font.withExtraKerningFactor(cssTracking / font.getHeight());
}

juce::GlyphArrangement makePrototypeGlyphs(const juce::String& text, float fontSize,
                                            bool extraBold, float letterSpacingEm,
                                            float scale)
{
    juce::GlyphArrangement glyphs;
    const auto font = trackedMono(fontSize, extraBold, letterSpacingEm, scale);
    glyphs.addLineOfText(font, text, 0.0f, font.getAscent());
    return glyphs;
}
}

float prototypeTextWidth(const juce::String& text, float fontSize, bool extraBold,
                         float letterSpacingEm, float scale)
{
    const auto font = trackedMono(fontSize, extraBold, letterSpacingEm, scale);
    // CSS letter-spacing contributes after every character, including the
    // final one, and flex layout uses advances rather than the ink bounds.
    return juce::GlyphArrangement::getStringWidth(font, text)
        + letterSpacingEm * fontSize * scale;
}

void drawPrototypeText(juce::Graphics& g, const juce::String& text,
                       juce::Rectangle<float> lineBox, float fontSize, bool extraBold,
                       float letterSpacingEm, juce::Colour colour,
                       PrototypeTextAlign align, float scale)
{
    auto glyphs = makePrototypeGlyphs(text, fontSize, extraBold,
                                      letterSpacingEm, scale);
    const auto bounds = glyphs.getBoundingBox(0, glyphs.getNumGlyphs(), true);
    float targetX = lineBox.getX();
    if (align == PrototypeTextAlign::centre)
        targetX = lineBox.getCentreX() - bounds.getWidth() * 0.5f;
    else if (align == PrototypeTextAlign::right)
        targetX = lineBox.getRight() - bounds.getWidth();
    glyphs.moveRangeOfGlyphs(0, glyphs.getNumGlyphs(),
                            targetX - bounds.getX(),
                            lineBox.getCentreY() - bounds.getCentreY());
    g.setColour(colour);
    glyphs.draw(g);
}

void drawPrototypeBaselineText(juce::Graphics& g, const juce::String& text,
                               juce::Point<float> baseline, float fontSize,
                               bool extraBold, float letterSpacingEm, juce::Colour colour,
                               PrototypeTextAlign align, float scaleX, float scaleY)
{
    auto glyphs = makePrototypeGlyphs(text, fontSize, extraBold,
                                      letterSpacingEm, juce::jmin(scaleX, scaleY));
    const auto bounds = glyphs.getBoundingBox(0, glyphs.getNumGlyphs(), true);
    float targetX = baseline.x;
    if (align == PrototypeTextAlign::centre)
        targetX -= bounds.getWidth() * 0.5f;
    else if (align == PrototypeTextAlign::right)
        targetX -= bounds.getWidth();
    const auto baselineY = glyphs.getNumGlyphs() > 0
        ? glyphs.getGlyph(0).getBaselineY() : 0.0f;
    glyphs.moveRangeOfGlyphs(0, glyphs.getNumGlyphs(),
                            targetX - bounds.getX(), baseline.y - baselineY);
    g.setColour(colour);
    glyphs.draw(g);
}

int ThemePreferences::loadMode()
{
    auto& properties = themeProperties();
    if (properties.containsKey("themeMode"))
        return juce::jlimit((int)automatic, (int)black,
                            properties.getIntValue("themeMode", (int)automatic));
    // The old lightTheme key was written even when the user had never chosen a
    // theme. Treat an installation without the new three-state key as AUTO.
    return automatic;
}

void ThemePreferences::saveMode(int mode)
{
    auto& properties = themeProperties();
    mode = juce::jlimit((int)automatic, (int)black, mode);
    properties.setValue("themeMode", mode);
    properties.setValue("lightTheme", mode != black);
    properties.saveIfNeeded();
}

bool ThemePreferences::isDarkForHour(int mode, int localHour) noexcept
{
    mode = juce::jlimit((int)automatic, (int)black, mode);
    if (mode == white) return false;
    if (mode == black) return true;
    localHour = juce::jlimit(0, 23, localHour);
    return localHour < 8 || localHour >= 20;
}

bool ThemePreferences::loadLight()
{
    return !isDarkForHour(loadMode(), juce::Time::getCurrentTime().getHours());
}
void ThemePreferences::saveLight(bool light)
{
    saveMode(light ? white : black);
}

LookAndFeel::LookAndFeel() { applyPalette(); }

void LookAndFeel::setDark(bool shouldBeDark)
{
    if (dark == shouldBeDark) return;
    dark = shouldBeDark;
    applyPalette();
}

void LookAndFeel::setThemeColours(juce::Colour lightBackground,
                                  juce::Colour lightForeground,
                                  juce::Colour darkBackground,
                                  juce::Colour darkForeground)
{
    if (lightBackgroundColour == lightBackground
        && lightForegroundColour == lightForeground
        && darkBackgroundColour == darkBackground
        && darkForegroundColour == darkForeground)
        return;
    lightBackgroundColour = lightBackground.withAlpha(1.0f);
    lightForegroundColour = lightForeground.withAlpha(1.0f);
    darkBackgroundColour = darkBackground.withAlpha(1.0f);
    darkForegroundColour = darkForeground.withAlpha(1.0f);
    applyPalette();
}

void LookAndFeel::setUiScale(float newScale) noexcept
{
    uiScale = juce::jlimit(0.5f, 4.0f, newScale);
}

void LookAndFeel::applyPalette()
{
    const auto fg = dark ? darkForegroundColour : lightForegroundColour;
    const auto bg = dark ? darkBackgroundColour : lightBackgroundColour;
    const auto muted = fg.interpolatedWith(bg, 0.28f);
    const auto surface = bg;
    setColour(foregroundColourId, fg);
    setColour(backgroundColourId, bg);
    setColour(mutedColourId, muted);
    setColour(surfaceColourId, surface);
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
    setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::arrowColourId, fg);
    setColour(juce::PopupMenu::backgroundColourId, fg);
    setColour(juce::PopupMenu::textColourId, bg);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, bg);
    setColour(juce::PopupMenu::highlightedTextColourId, fg);
}

juce::Font LookAndFeel::getTextButtonFont(juce::TextButton& button, int)
{
    return mono(controlFontHeight((float)button.getHeight()) * uiScale, true);
}
juce::Font LookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return mono(controlFontHeight((float)box.getHeight()) * uiScale, true);
}
juce::Font LookAndFeel::getLabelFont(juce::Label& label)
{
    if ((bool)label.getProperties().getWithDefault("rotaryValueLabel", false))
        return mono(metrics::ordinaryFontHeight * uiScale, false);
    if ((bool)label.getProperties().getWithDefault("numericValueControl", false))
    {
        const auto* parent = label.getParentComponent();
        return mono(controlFontHeight((float)(parent != nullptr ? parent->getHeight() : label.getHeight()))
                        * uiScale, true);
    }
    if (const auto* slider = dynamic_cast<const juce::Slider*>(label.getParentComponent()))
        if (slider->getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag)
            return mono(metrics::ordinaryFontHeight * uiScale, false);
    return mono(metrics::ordinaryFontHeight * uiScale, false);
}
juce::Font LookAndFeel::getPopupMenuFont()
{
    return mono(9.0f * cssFontToJuceHeight * uiScale, true);
}

int LookAndFeel::getPopupMenuBorderSize()
{
    return juce::roundToInt(7.0f * uiScale);
}

void LookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setVisible(false);
    label.setBounds({});
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
}

void LookAndFeel::getIdealPopupMenuItemSize(const juce::String& text, bool separator, int,
                                             int& width, int& height)
{
    height = separator ? juce::roundToInt(11.0f * uiScale) : juce::roundToInt(30.0f * uiScale);
    width = separator ? 48 : juce::jlimit(88, 260,
        juce::roundToInt((float)text.length() * 7.5f * uiScale + 34.0f * uiScale));
}

void LookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
    g.setColour(findColour(juce::PopupMenu::textColourId));
    g.drawRect(0, 0, width, height, juce::jmax(1, juce::roundToInt(uiScale)));
}

void LookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                    bool separator, bool active, bool highlighted, bool ticked,
                                    bool hasSubMenu, const juce::String& text,
                                    const juce::String& shortcut, const juce::Drawable*,
                                    const juce::Colour*)
{
    if (separator)
    {
        g.setColour(findColour(juce::PopupMenu::textColourId).withAlpha(0.38f));
        g.fillRect(area.reduced(juce::roundToInt(5.0f * uiScale),
                                area.getHeight() / 2).withHeight(
                                    juce::jmax(1, juce::roundToInt(uiScale))));
        return;
    }
    const bool inverse = highlighted || ticked;
    const auto normalBackground = findColour(juce::PopupMenu::backgroundColourId);
    const auto normalText = findColour(juce::PopupMenu::textColourId);
    const auto fg = inverse ? normalBackground : normalText;
    const auto bg = inverse ? normalText : normalBackground;
    g.setColour(bg); g.fillRect(area);
    g.setColour(fg.withMultipliedAlpha(active ? 1.0f : metrics::disabledOpacity));
    auto content = area.reduced(juce::roundToInt(9.0f * uiScale),
                                juce::roundToInt(5.0f * uiScale));
    if (const int filterType = deq::ui::filterTypeForDisplayName(text); filterType >= 0)
    {
        const int iconWidth = juce::roundToInt(26.0f * uiScale);
        auto iconArea = content.removeFromLeft(iconWidth).toFloat();
        deq::ui::paintFilterIcon(g, iconArea.withSizeKeepingCentre(
            (float)iconWidth, juce::roundToInt(16.0f * uiScale)), filterType,
            fg.withMultipliedAlpha(active ? 1.0f : metrics::disabledOpacity));
        content.removeFromLeft(juce::roundToInt(7.0f * uiScale));
    }
    if (hasSubMenu) g.fillRect(content.getRight() - 5, content.getCentreY() - 2, 4, 4);
    if (shortcut.isNotEmpty())
        g.drawText(shortcut, content.removeFromRight(70), juce::Justification::centredRight);
    drawPrototypeText(g, text, content.toFloat(), 9.0f, true, 0.0f,
                      fg.withMultipliedAlpha(active ? 1.0f : metrics::disabledOpacity),
                      PrototypeTextAlign::left, uiScale);
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                       const juce::Colour&, bool highlighted, bool down)
{
    juce::ignoreUnused(highlighted, down);
    const auto bounds = button.getLocalBounds();
    const int border = juce::jmax(1, juce::roundToInt(metrics::thinLine * uiScale));
    const bool active = button.getToggleState();
    const auto fg = foreground();
    const auto bg = background();
    const auto caption = button.getProperties().getWithDefault("headerLabel", {}).toString();
    const bool valueStrip = (bool)button.getProperties().getWithDefault("valueStripCell", false);
    const bool workspaceButton = (bool)button.getProperties().getWithDefault("workspaceButton", false);
    const bool header = caption.isNotEmpty();
    const bool inverse = header ? (caption == "POWER" && active) : active;

    if (header || valueStrip)
    {
        g.setColour(inverse ? fg : bg); g.fillRect(bounds);
        if ((bool)button.getProperties().getWithDefault("leftDivider", false))
        {
            g.setColour(fg);
            g.fillRect(bounds.getX(), bounds.getY(), border, bounds.getHeight());
        }
        if ((bool)button.getProperties().getWithDefault("rightDivider", false))
        {
            g.setColour(fg);
            g.fillRect(bounds.getRight() - border, bounds.getY(), border, bounds.getHeight());
        }
        if (button.hasKeyboardFocus(false) && !button.getMouseClickGrabsKeyboardFocus())
        {
            g.setColour(inverse ? bg : fg);
            g.drawRect(bounds.reduced(juce::roundToInt(3.0f * uiScale)),
                       juce::jmax(1, juce::roundToInt(2.0f * uiScale)));
        }
        return;
    }

    if (workspaceButton)
    {
        g.setColour(active ? fg : bg); g.fillRect(bounds);
        g.setColour(fg); g.drawRect(bounds, border);
        if (button.hasKeyboardFocus(false) && !button.getMouseClickGrabsKeyboardFocus())
        {
            g.setColour(active ? bg : fg);
            g.drawRect(bounds.reduced(juce::roundToInt(3.0f * uiScale)),
                       juce::jmax(1, juce::roundToInt(2.0f * uiScale)));
        }
        return;
    }

    g.setColour(active ? fg : bg); g.fillRect(bounds);
    g.setColour(fg); g.drawRect(bounds, border);
}

void LookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool down)
{
    juce::ignoreUnused(down);
    const auto caption = button.getProperties().getWithDefault("headerLabel", {}).toString();
    const bool inverse = caption.isNotEmpty()
        ? (caption == "POWER" && button.getToggleState())
        : button.getToggleState();
    const auto colour = inverse ? background() : foreground();
    if (caption.isNotEmpty())
    {
        const float labelHeight = 9.0f * uiScale;
        const float valueHeight = 13.0f * uiScale;
        const float gap = 9.0f * uiScale;
        const float top = ((float)button.getHeight() - labelHeight - gap - valueHeight) * 0.5f
            - uiScale;
        const auto content = button.getLocalBounds().toFloat().reduced(13.0f * uiScale, 0.0f);
        const bool power = caption == "POWER";
        const bool autoGain = caption == "AUTO GAIN";
        const auto align = power ? PrototypeTextAlign::centre : PrototypeTextAlign::left;
        drawPrototypeText(g, caption,
                          { content.getX(), top, content.getWidth(), labelHeight },
                          9.0f, true, autoGain ? 0.0f : 0.13f,
                          colour.withAlpha(0.72f), align, uiScale);
        const auto valueColour = colour.withAlpha(
            (power || autoGain) && !button.getToggleState() ? 0.42f : 1.0f);
        drawPrototypeText(g, button.getButtonText(),
                          { content.getX(), top + labelHeight + gap,
                            content.getWidth(), valueHeight },
                          13.0f, true, autoGain ? -0.075f : 0.0f,
                          valueColour, align, uiScale);
        return;
    }
    auto textBounds = button.getLocalBounds().toFloat();
    drawPrototypeText(g, button.getButtonText(), textBounds,
                      9.0f, true, 0.0f, colour,
                      PrototypeTextAlign::centre, uiScale);
}

void LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool over, bool down)
{
    drawButtonBackground(g, button, {}, over, down);
    const auto caption = button.getProperties().getWithDefault("headerLabel", {}).toString();
    const bool inverse = caption.isNotEmpty()
        ? (caption == "POWER" && button.getToggleState())
        : button.getToggleState();
    const auto colour = inverse ? background() : foreground();
    if (caption.isNotEmpty())
    {
        const float labelHeight = 9.0f * uiScale;
        const float valueHeight = 13.0f * uiScale;
        const float gap = 9.0f * uiScale;
        const float top = ((float)button.getHeight() - labelHeight - gap - valueHeight) * 0.5f
            - uiScale;
        const auto content = button.getLocalBounds().toFloat().reduced(13.0f * uiScale, 0.0f);
        const bool power = caption == "POWER";
        const bool autoGain = caption == "AUTO GAIN";
        const auto align = power ? PrototypeTextAlign::centre : PrototypeTextAlign::left;
        drawPrototypeText(g, caption,
                          { content.getX(), top, content.getWidth(), labelHeight },
                          9.0f, true, autoGain ? 0.0f : 0.13f,
                          colour.withAlpha(0.72f), align, uiScale);
        drawPrototypeText(g, button.getButtonText(),
                          { content.getX(), top + labelHeight + gap,
                            content.getWidth(), valueHeight },
                          13.0f, true, autoGain ? -0.075f : 0.0f,
                          colour.withAlpha((power || autoGain) && !button.getToggleState()
                                               ? 0.42f : 1.0f),
                          align, uiScale);
        return;
    }
    auto textBounds = button.getLocalBounds().toFloat();
    drawPrototypeText(g, button.getButtonText(), textBounds,
                      9.0f, true, 0.0f, colour,
                      PrototypeTextAlign::centre, uiScale);
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float position, float, float, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height);
    const auto fg = foreground();
    const auto bg = background();
    g.setColour(bg); g.fillRect(bounds);
    const float contentOffsetX = slider.getName() == "DRIVE" ? -uiScale : 0.0f;
    const float labelOffsetX = contentOffsetX
        + (slider.getName() == "RANGE" ? -uiScale : 0.0f);
    drawPrototypeText(g, slider.getName(),
                      { 5.0f * uiScale + labelOffsetX, 6.0f * uiScale,
                        slider.getWidth() - 10.0f * uiScale, 9.0f * uiScale },
                      9.0f, true, 0.06f, fg.withAlpha(0.72f),
                      PrototypeTextAlign::centre, uiScale);
    const float trackCentre = slider.getLocalBounds().toFloat().getCentreX() + contentOffsetX;
    auto track = juce::Rectangle<float>(
        std::floor(trackCentre - 12.0f * uiScale), 22.0f * uiScale,
        24.0f * uiScale, slider.getHeight() - 43.0f * uiScale);
    g.setColour(bg); g.fillRect(track);
    g.setColour(fg); g.drawRect(track, 1.0f * uiScale);
    const float progress = juce::jlimit(0.0f, 1.0f, position);
    const float maximumFillHeight = slider.getHeight() - 44.0f * uiScale;
    const float fillHeight = maximumFillHeight * progress;
    g.setColour(fg.withAlpha(slider.isEnabled() ? 1.0f : metrics::disabledOpacity));
    const int fillLeft = juce::roundToInt(trackCentre - 9.0f * uiScale);
    const int fillRight = juce::roundToInt(trackCentre + 9.0f * uiScale);
    const int fillBottom = juce::roundToInt(slider.getHeight() - 22.0f * uiScale);
    const int fillTop = juce::roundToInt((float)fillBottom - fillHeight);
    g.fillRect(fillLeft, fillTop, fillRight - fillLeft, fillBottom - fillTop);
    drawPrototypeText(g, slider.getTextFromValue(slider.getValue()),
                      { 5.0f * uiScale + contentOffsetX,
                        slider.getHeight() - 16.0f * uiScale,
                        slider.getWidth() - 10.0f * uiScale, 9.0f * uiScale },
                      9.0f, true, 0.0f, fg,
                      PrototypeTextAlign::centre, uiScale);
    if ((bool)slider.getProperties().getWithDefault("rightDivider", false))
        g.fillRect(slider.getWidth() - juce::jmax(1, juce::roundToInt(uiScale)), 0,
                   juce::jmax(1, juce::roundToInt(uiScale)), slider.getHeight());
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                               int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int>(0, 0, width, height);
    const auto fg = foreground();
    const auto valueFg = fg.withAlpha(box.isEnabled() ? 1.0f : 0.70f);
    g.setColour(background()); g.fillRect(bounds);
    const int stateInset = juce::jmax(1, juce::roundToInt(metrics::thinLine * uiScale));
    const bool pickerOpen = isButtonDown || box.isPopupActive()
        || (bool)box.getProperties().getWithDefault("pickerOpen", false);
    g.setColour(fg);
    if (pickerOpen)
        g.drawRect(bounds, stateInset);
    else if (box.hasKeyboardFocus(false))
    {
        g.drawRect(bounds.reduced(juce::roundToInt(3.0f * uiScale)),
                   juce::jmax(1, juce::roundToInt(2.0f * uiScale)));
    }
    const bool headerCell = (bool)box.getProperties().getWithDefault("headerCell", false);
    const bool workspaceCell = (bool)box.getProperties().getWithDefault("workspaceCell", false);
    const bool valueStripCell = (bool)box.getProperties().getWithDefault("valueStripCell", false);
    const bool mixedValue = (bool)box.getProperties().getWithDefault("mixedValue", false);
    const auto shownValue = mixedValue ? juce::String("MULTI") : box.getText();

    if (headerCell)
    {
        const float labelHeight = 9.0f * uiScale;
        const float valueLineHeight = 14.4f * uiScale;
        const float gap = 9.0f * uiScale;
        const float top = ((float)height - labelHeight - gap - valueLineHeight) * 0.5f;
        const auto content = bounds.toFloat().reduced(13.0f * uiScale, 0.0f);
        drawPrototypeText(g, box.getName(),
                          { content.getX(), top, content.getWidth(), labelHeight },
                          9.0f, true, 0.13f, fg.withAlpha(0.72f),
                          PrototypeTextAlign::left, uiScale);
        drawPrototypeText(g, shownValue,
                          { content.getX(), top + labelHeight + gap - uiScale,
                            content.getWidth(), valueLineHeight },
                          12.0f, true, 0.0f, valueFg,
                          PrototypeTextAlign::left, uiScale);
    }
    else if (workspaceCell)
    {
        const float horizontal = (box.getName() == "ROUTE" ? 6.0f : 10.0f) * uiScale;
        const float labelHeight = 9.0f * uiScale;
        const float valueLineHeight = 10.8f * uiScale;
        const float gap = 6.0f * uiScale;
        const float top = ((float)height - labelHeight - gap - valueLineHeight) * 0.5f
            - (box.getName() == "ROUTE" ? uiScale : 0.0f);
        const auto content = bounds.toFloat().reduced(horizontal, 0.0f);
        drawPrototypeText(g, box.getName(),
                          { content.getX(), top, content.getWidth(), labelHeight },
                          9.0f, true, 0.06f, fg.withAlpha(0.72f),
                          PrototypeTextAlign::left, uiScale);
        drawPrototypeText(g, shownValue,
                          { content.getX(), top + labelHeight + gap,
                            content.getWidth(), valueLineHeight },
                          9.0f, true, 0.0f, valueFg,
                          PrototypeTextAlign::left, uiScale);
    }
    else if (valueStripCell)
    {
        auto area = bounds.toFloat().reduced(10.0f * uiScale, 3.0f * uiScale);
        if (!mixedValue)
        {
            const float iconWidth = 26.0f * uiScale;
            auto iconArea = area.removeFromLeft(iconWidth);
            const int type = (int)box.getProperties().getWithDefault("filterType", 5);
            deq::ui::paintFilterIcon(g, iconArea.withSizeKeepingCentre(
                iconWidth, 16.0f * uiScale), type, fg);
            area.removeFromLeft(7.0f * uiScale);
        }
        drawPrototypeText(g, shownValue, area,
                          9.0f, true, 0.0f, valueFg,
                          PrototypeTextAlign::left, uiScale);
    }
    else
    {
        g.setColour(valueFg); g.setFont(getComboBoxFont(box));
        g.drawFittedText(shownValue, bounds.reduced(8, 3), juce::Justification::centred, 1);
    }
    if ((bool)box.getProperties().getWithDefault("rightDivider", false))
    {
        g.setColour(fg); g.fillRect(width - stateInset, 0, stateInset, height);
    }
    if ((bool)box.getProperties().getWithDefault("bottomDivider", false))
    {
        g.setColour(fg); g.fillRect(0, height - stateInset, width, stateInset);
    }
    if ((bool)box.getProperties().getWithDefault("wordmarkSeam", false))
    {
        g.setColour(fg);
        g.fillRect(0, juce::roundToInt(9.0f * uiScale), stateInset,
                   height - juce::roundToInt(18.0f * uiScale));
    }
}

void LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float position, float min, float max,
                                   juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (slider.getName() != "LOOKAHEAD" && slider.getName() != "OUTPUT_HDR"
        && slider.getName() != "PLACEMENT" && slider.getName() != "OVERSAMPLING")
        return LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, position, min, max, style, slider);
    auto r = slider.getLocalBounds().toFloat();
    const auto fg = foreground();
    const auto bg = background();
    const float proportion = (float)slider.valueToProportionOfLength(slider.getValue());
    g.setColour(bg); g.fillRect(r);
    const auto drawRightDivider = [&]
    {
        if (!(bool)slider.getProperties().getWithDefault("rightDivider", false)) return;
        g.setColour(fg);
        g.fillRect(slider.getWidth() - juce::jmax(1, juce::roundToInt(uiScale)), 0,
                   juce::jmax(1, juce::roundToInt(uiScale)), slider.getHeight());
    };
    if (slider.getName() == "PLACEMENT")
    {
        const int routeMode = (int)slider.getProperties().getWithDefault("routeMode", 0);
        const double placement = slider.getValue();
        const bool nameMixed = (bool)slider.getProperties().getWithDefault(
            "nameMixed", (bool)slider.getProperties().getWithDefault("mixedValue", false));
        const bool valueMixed = (bool)slider.getProperties().getWithDefault(
            "valueMixed", (bool)slider.getProperties().getWithDefault("mixedValue", false));
        const auto name = nameMixed ? juce::String("MULTI") : std::abs(placement) < 1.0
            ? juce::String(routeMode == 2 ? "SUM" : "CENTER")
            : routeMode == 0 ? juce::String(placement < 0.0 ? "LEFT" : "RIGHT")
            : routeMode == 1 ? juce::String(placement < 0.0 ? "MID" : "SIDE")
                             : juce::String(placement < 0.0 ? "TRNSNT" : "SUSTAIN");
        const auto value = valueMixed ? juce::String(juce::CharPointer_UTF8("\xe2\x80\x94"))
                                      : juce::String(juce::roundToInt(placement)) + "%";
        const float lineHeight = 9.0f * uiScale;
        const float gap = 6.0f * uiScale;
        const float top = ((float)height - lineHeight * 2.0f - gap) * 0.5f;
        drawPrototypeText(g, name,
                          { 6.0f * uiScale, top,
                            width - 12.0f * uiScale, lineHeight },
                          9.0f, true, 0.0f, fg.withAlpha(0.72f),
                          PrototypeTextAlign::left, uiScale);
        drawPrototypeText(g, value,
                          { 6.0f * uiScale, top + lineHeight + gap,
                            width - 12.0f * uiScale, lineHeight },
                          9.0f, true, 0.0f, fg,
                          PrototypeTextAlign::left, uiScale);
        drawRightDivider();
        return;
    }

    auto track = juce::Rectangle<float>(11.0f * uiScale, 32.0f * uiScale,
                                        r.getWidth() - 22.0f * uiScale, 16.0f * uiScale);
    g.setColour(bg); g.fillRect(track);
    g.setColour(fg); g.drawRect(track, 1.0f * uiScale);
    auto fill = juce::Rectangle<float>(12.0f * uiScale, 33.0f * uiScale,
                                       r.getWidth() - 24.0f * uiScale, 14.0f * uiScale);
    g.setColour(fg.withAlpha(slider.isEnabled() ? 1.0f : metrics::disabledOpacity));
    g.fillRect(fill.withWidth(fill.getWidth() * proportion));
    const auto value = (bool)slider.getProperties().getWithDefault("mixedValue", false)
        ? juce::String("MULTI") : slider.getValue() <= 0.001 ? juce::String("OFF")
        : juce::String(slider.getValue(), 2) + " ms";
    drawPrototypeText(g, "LOOKAHEAD",
                      { 10.0f * uiScale, 57.0f * uiScale,
                        width - 20.0f * uiScale, 9.0f * uiScale },
                      9.0f, true, 0.06f, fg.withAlpha(0.72f),
                      PrototypeTextAlign::left, uiScale);
    drawPrototypeText(g, value,
                      { 10.0f * uiScale, 72.0f * uiScale,
                        width - 20.0f * uiScale, 9.0f * uiScale },
                      9.0f, true, 0.0f, fg,
                      PrototypeTextAlign::left, uiScale);
    drawRightDivider();
}

WordmarkButton::WordmarkButton(juce::String text) : juce::TextButton(std::move(text))
{
    setWantsKeyboardFocus(false);
}

void WordmarkButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    juce::ignoreUnused(highlighted, down);
    const auto ink = foregroundOf(*this);
    const auto paper = backgroundOf(*this);
    g.fillAll(paper);
    const float scale = scaleOf(*this);
    drawPrototypeText(g, getButtonText(),
                      getLocalBounds().toFloat().reduced(18.0f * scale, 0.0f),
                      20.0f, true, -0.065f, ink,
                      PrototypeTextAlign::centre, scale);
}

SmartGainButton::SmartGainButton(juce::String text) : juce::TextButton(std::move(text)) {}

void SmartGainButton::setLoadingState(float progress, bool shouldLoad, bool reduceMotion)
{
    const float nextProgress = juce::jlimit(0.0f, 1.0f, progress);
    if (std::abs(loadingProgress - nextProgress) < 0.0001f
        && loading == shouldLoad && reducedMotion == reduceMotion)
        return;
    loadingProgress = nextProgress;
    loading = shouldLoad;
    reducedMotion = reduceMotion;
    repaint();
}

void SmartGainButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    getLookAndFeel().drawButtonBackground(g, *this, findColour(buttonColourId), highlighted, down);
    getLookAndFeel().drawButtonText(g, *this, highlighted, down);
}

}
