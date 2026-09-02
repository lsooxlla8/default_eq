#include "DefaultFamilyUI.h"

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
    return juce::jlimit(15.0f, 16.5f, controlHeight * 0.645f);
}

int controlTextPadding(float scale) noexcept
{
    return juce::jmax(2, juce::roundToInt(3.0f * scale));
}
}

juce::Font mono(float height, bool bold)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), height,
        bold ? juce::Font::bold : juce::Font::plain));
}

bool ThemePreferences::loadLight() { return themeProperties().getBoolValue("lightTheme", true); }
void ThemePreferences::saveLight(bool light)
{
    auto& properties = themeProperties();
    properties.setValue("lightTheme", light);
    properties.saveIfNeeded();
}

LookAndFeel::LookAndFeel() { applyPalette(); }

void LookAndFeel::setDark(bool shouldBeDark)
{
    if (dark == shouldBeDark) return;
    dark = shouldBeDark;
    applyPalette();
}

void LookAndFeel::setUiScale(float newScale) noexcept
{
    uiScale = juce::jlimit(0.5f, 2.0f, newScale);
}

void LookAndFeel::applyPalette()
{
    const auto fg = dark ? paper() : ink();
    const auto bg = dark ? ink() : paper();
    const auto muted = fg.interpolatedWith(bg, 0.28f);
    const auto surface = bg.interpolatedWith(fg, 0.12f);
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
    setColour(juce::PopupMenu::backgroundColourId, surface);
    setColour(juce::PopupMenu::textColourId, fg);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, fg);
    setColour(juce::PopupMenu::highlightedTextColourId, bg);
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
        return mono(17.25f * uiScale, true);
    if ((bool)label.getProperties().getWithDefault("numericValueControl", false))
    {
        const auto* parent = label.getParentComponent();
        return mono(controlFontHeight((float)(parent != nullptr ? parent->getHeight() : label.getHeight()))
                        * uiScale, true);
    }
    if (const auto* slider = dynamic_cast<const juce::Slider*>(label.getParentComponent()))
        if (slider->getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag)
            return mono(17.25f * uiScale, true);
    return mono(15.0f * uiScale, true);
}
juce::Font LookAndFeel::getPopupMenuFont() { return mono(15.0f * uiScale, true); }

void LookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    const int left = controlTextPadding(uiScale);
    const int top = juce::jmax(1, juce::roundToInt(uiScale));
    label.setBounds(left, top, juce::jmax(1, box.getWidth() - left * 2 - juce::roundToInt(14.0f * uiScale)),
                    box.getHeight() - 2 * top);
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centred);
}

void LookAndFeel::getIdealPopupMenuItemSize(const juce::String& text, bool separator, int,
                                             int& width, int& height)
{
    height = separator ? juce::roundToInt(5.0f * uiScale) : juce::roundToInt(23.0f * uiScale);
    width = separator ? 48 : juce::jlimit(88, 260,
        juce::roundToInt((float)text.length() * 7.5f * uiScale + 34.0f * uiScale));
}

void LookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    juce::ignoreUnused(width, height);
    g.fillAll(findColour(surfaceColourId));
}

void LookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                    bool separator, bool active, bool highlighted, bool ticked,
                                    bool hasSubMenu, const juce::String& text,
                                    const juce::String& shortcut, const juce::Drawable*,
                                    const juce::Colour*)
{
    if (separator)
    {
        g.setColour(foreground().withAlpha(0.35f));
        g.fillRect(area.reduced(5, area.getHeight() / 2).withHeight(1));
        return;
    }
    const auto fg = highlighted ? background() : foreground();
    const auto bg = highlighted ? foreground() : findColour(surfaceColourId);
    g.setColour(bg); g.fillRect(area);
    g.setColour(fg.withMultipliedAlpha(active ? 1.0f : metrics::disabledOpacity));
    auto content = area.reduced(8, 1);
    if (ticked)
    {
        const int marker = juce::jmax(4, juce::roundToInt(4.0f * uiScale));
        g.fillRect(content.getX(), content.getCentreY() - marker / 2, marker, marker);
    }
    content.removeFromLeft(12);
    if (hasSubMenu) g.fillRect(content.getRight() - 5, content.getCentreY() - 2, 4, 4);
    if (shortcut.isNotEmpty())
        g.drawText(shortcut, content.removeFromRight(70), juce::Justification::centredRight);
    g.setFont(getPopupMenuFont());
    g.drawFittedText(text, content, juce::Justification::centredLeft, 1);
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                       const juce::Colour&, bool highlighted, bool down)
{
    const auto bounds = button.getLocalBounds();
    const int border = juce::jmax(1, juce::roundToInt(2.0f * uiScale));
    const auto content = bounds.reduced(border);
    const bool active = button.getToggleState();
    auto fg = foreground().withMultipliedAlpha(button.isEnabled() ? 1.0f : metrics::disabledOpacity);
    auto bg = background();
    g.setColour(fg); g.fillRect(bounds);
    g.setColour(bg); g.fillRect(content);
    if (active || down)
    {
        g.setColour(fg);
        g.fillRect(content.reduced(juce::jmax(1, juce::roundToInt(2.0f * uiScale))));
    }
    if (highlighted)
    {
        const auto hover = content.reduced(juce::jmax(2, juce::roundToInt(4.0f * uiScale)));
        if (!hover.isEmpty())
        {
            g.setColour(active || down ? bg : fg);
            g.drawRect(hover, juce::jmax(1, juce::roundToInt(uiScale)));
        }
    }
}

void LookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool down)
{
    const bool inverse = button.getToggleState() || down;
    auto colour = inverse ? background() : foreground();
    colour = colour.withMultipliedAlpha(button.isEnabled() ? 1.0f : metrics::disabledOpacity);
    g.setColour(colour);
    g.setFont(getTextButtonFont(button, button.getHeight()));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(
        controlTextPadding(uiScale), juce::roundToInt(2.0f * uiScale)),
        juce::Justification::centred, 1);
}

void LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool over, bool down)
{
    drawButtonBackground(g, button, {}, over, down);
    const bool inverse = button.getToggleState() || down;
    g.setColour((inverse ? background() : foreground()).withMultipliedAlpha(
        button.isEnabled() ? 1.0f : metrics::disabledOpacity));
    g.setFont(mono(controlFontHeight((float)button.getHeight()) * uiScale, true));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(
        controlTextPadding(uiScale), juce::roundToInt(2.0f * uiScale)),
                     juce::Justification::centred, 1);
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float position, float, float, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height);
    const float side = juce::jmin(bounds.getWidth(), bounds.getHeight());
    auto square = bounds.withSizeKeepingCentre(side, side);
    const auto fg = foreground().withMultipliedAlpha(slider.isEnabled() ? 1.0f : metrics::disabledOpacity);
    const auto bg = background();
    g.setColour(bg); g.fillRect(square);
    g.setColour(fg); g.drawRect(square, 2.0f * uiScale);
    auto inner = square.reduced(9.0f * uiScale);
    g.setColour(bg.interpolatedWith(fg, 0.12f)); g.fillRect(inner);
    const float progress = juce::jlimit(0.0f, 1.0f, position);
    auto progressArea = inner.reduced(5.0f * uiScale);
    g.setColour(fg); g.fillRect(progressArea.withTop(progressArea.getBottom() - progressArea.getHeight() * progress));
    const auto grid = inner.reduced(3.0f * uiScale);
    g.setColour(bg.withAlpha(0.22f));
    for (int i = 1; i < 4; ++i)
    {
        const float px = grid.getX() + grid.getWidth() * (float)i / 4.0f;
        const float py = grid.getY() + grid.getHeight() * (float)i / 4.0f;
        g.drawVerticalLine(juce::roundToInt(px), grid.getY(), grid.getBottom());
        g.drawHorizontalLine(juce::roundToInt(py), grid.getX(), grid.getRight());
    }
    const float marker = juce::jmax(6.0f * uiScale, side * 0.08f);
    const float markerX = inner.getX() + progress * juce::jmax(0.0f, inner.getWidth() - marker);
    g.setColour(bg.interpolatedWith(fg, 0.72f));
    g.fillRect(markerX, inner.getY() + 3.0f * uiScale, marker, marker);
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                               int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int>(0, 0, width, height);
    const auto fg = foreground().withMultipliedAlpha(box.isEnabled() ? 1.0f : metrics::disabledOpacity);
    g.setColour(findColour(surfaceColourId)); g.fillRect(bounds);
    g.setColour(fg);
    g.drawRect(bounds, juce::jmax(1, juce::roundToInt(2.0f * uiScale)));
    g.setColour(fg);
    const int marker = juce::jmax(juce::roundToInt(8.0f * uiScale), height / 5);
    g.fillRect(width - marker - juce::roundToInt(10.0f * uiScale), (height - marker) / 2, marker, marker);
}

void LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float position, float min, float max,
                                   juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (slider.getName() != "LOOKAHEAD" && slider.getName() != "OUTPUT_HDR"
        && slider.getName() != "PLACEMENT" && slider.getName() != "OVERSAMPLING")
        return LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, position, min, max, style, slider);
    auto r = slider.getLocalBounds().toFloat();
    const auto fg = foreground().withMultipliedAlpha(slider.isEnabled() ? 1.0f : metrics::disabledOpacity);
    const auto bg = background();
    g.setColour(bg); g.fillRect(r); g.setColour(fg); g.drawRect(r, 2.0f * uiScale);
    const float proportion = (float)slider.valueToProportionOfLength(slider.getValue());
    juce::Rectangle<float> fill;
    auto inner = r.reduced(4.0f * uiScale);
    if (slider.getName() == "OUTPUT_HDR" || slider.getName() == "PLACEMENT")
    {
        const float centre = inner.getCentreX();
        const float marker = inner.getX() + inner.getWidth() * proportion;
        fill = marker < centre ? juce::Rectangle<float>(marker, inner.getY(), centre - marker, inner.getHeight())
                               : juce::Rectangle<float>(centre, inner.getY(), marker - centre, inner.getHeight());
        g.setColour(fg.withAlpha(0.35f));
        g.drawVerticalLine(juce::roundToInt(centre), inner.getY(), inner.getBottom());
    }
    else fill = inner.withWidth(inner.getWidth() * proportion);
    if (!fill.isEmpty()) { g.setColour(fg); g.fillRect(fill); }
    const int routeMode=(int)slider.getProperties().getWithDefault("routeMode",0);
    const auto value = slider.getName() == "PLACEMENT"
        ? (std::abs(slider.getValue()) < 0.05 ? "CENTER"
           : slider.getValue() < 0.0 ? juce::String(routeMode==0?"L ":routeMode==1?"M ":"T ") + juce::String(std::abs(slider.getValue()), 0)
                                     : juce::String(routeMode==0?"R ":"S ") + juce::String(slider.getValue(), 0))
        : slider.getName() == "OVERSAMPLING"
        ? juce::String("OS ") + std::array<const char*, 4>{ "OFF", "2X", "4X", "8X" }[(size_t)juce::jlimit(0, 3, juce::roundToInt(slider.getValue()))]
        : slider.getName() == "OUTPUT_HDR" ? juce::String("OUT ")
            + juce::String(std::abs(slider.getValue()) < 0.005 ? 0.0 : slider.getValue(), 1)
        : slider.getValue() <= 0.001 ? "LOOK OFF" : juce::String("LOOK ") + juce::String(slider.getValue(), 2);
    const auto textBounds = r.toNearestInt().reduced(4, 1);
    const auto draw = [&](juce::Colour colour)
    {
        g.setColour(colour);
        g.setFont(mono(controlFontHeight((float)slider.getHeight()) * uiScale, true));
        g.drawFittedText(value, textBounds, juce::Justification::centred, 1);
    };
    draw(fg);
    if (!fill.isEmpty())
    {
        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(fill.toNearestInt()); draw(bg);
    }
}

WordmarkButton::WordmarkButton(juce::String text) : juce::TextButton(std::move(text))
{
    setWantsKeyboardFocus(false);
}

void WordmarkButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    auto colour = foregroundOf(*this);
    if (down) colour = findColour(LookAndFeel::mutedColourId);
    else if (highlighted) colour = colour.brighter(0.08f);
    g.setColour(colour);
    const float scale = scaleOf(*this);
    g.setFont(mono(metrics::headerFontHeight * scale, true));
    g.drawFittedText(getButtonText(), getLocalBounds().reduced(juce::roundToInt(10.0f * scale),
        juce::roundToInt(2.0f * scale)), juce::Justification::centred, 1);
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
    if (!loading) return;
    const float scale = scaleOf(*this);
    auto track = getLocalBounds().toFloat().reduced(5.0f * scale).removeFromBottom(5.0f * scale);
    const auto bg = backgroundOf(*this);
    g.setColour(bg.withAlpha(0.22f)); g.fillRect(track);
    g.setColour(bg); g.fillRect(track.withWidth(track.getWidth() * loadingProgress));
    if (!reducedMotion)
    {
        const float scan = (float)std::fmod(juce::Time::getMillisecondCounterHiRes() * 0.0014, 1.0);
        const float scanner = juce::jmax(3.0f * scale, track.getWidth() * 0.045f);
        g.fillRect(track.getX() + scan * juce::jmax(0.0f, track.getWidth() - scanner),
                   track.getY(), scanner, track.getHeight());
    }
}

}
