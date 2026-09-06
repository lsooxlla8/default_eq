#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace default_family
{
namespace metrics
{
constexpr int designWidth = 752;
constexpr int designHeight = 454;
constexpr float frame = 4.0f;
constexpr float thinLine = 1.0f;
constexpr float minimumHitTarget = 24.0f;
constexpr float headerFontHeight = 20.0f;
constexpr float ordinaryFontHeight = 9.0f;
constexpr float disabledOpacity = 0.32f;
}

juce::Font mono(float height, bool bold = false);

enum class PrototypeTextAlign { left, centre, right };

float prototypeTextWidth(const juce::String& text, float fontSize, bool extraBold,
                         float letterSpacingEm, float scale);
void drawPrototypeText(juce::Graphics&, const juce::String&, juce::Rectangle<float> lineBox,
                       float fontSize, bool extraBold, float letterSpacingEm,
                       juce::Colour, PrototypeTextAlign, float scale);
void drawPrototypeBaselineText(juce::Graphics&, const juce::String&,
                               juce::Point<float> baseline, float fontSize,
                               bool extraBold, float letterSpacingEm, juce::Colour,
                               PrototypeTextAlign, float scaleX, float scaleY);

class ThemePreferences final
{
public:
    enum Mode { automatic = 0, white = 1, black = 2 };
    static int loadMode();
    static void saveMode(int mode);
    static bool isDarkForHour(int mode, int localHour) noexcept;
    static bool loadLight();
    static void saveLight(bool light);
};

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    enum ColourIds
    {
        foregroundColourId = 0x2300100,
        backgroundColourId,
        mutedColourId,
        surfaceColourId
    };

    LookAndFeel();
    void setDark(bool shouldBeDark);
    void setThemeColours(juce::Colour lightBackground, juce::Colour lightForeground,
                         juce::Colour darkBackground, juce::Colour darkForeground);
    void setUiScale(float newScale) noexcept;
    bool isDark() const noexcept { return dark; }
    float getUiScale() const noexcept { return uiScale; }

    juce::Colour paper() const noexcept { return lightBackgroundColour; }
    juce::Colour ink() const noexcept { return lightForegroundColour; }
    juce::Colour foreground() const noexcept { return findColour(foregroundColourId); }
    juce::Colour background() const noexcept { return findColour(backgroundColourId); }

    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    int getPopupMenuBorderSize() override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    void getIdealPopupMenuItemSize(const juce::String&, bool, int, int&, int&) override;
    void drawPopupMenuBackground(juce::Graphics&, int, int) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>&, bool, bool,
                           bool, bool, bool, const juce::String&, const juce::String&,
                           const juce::Drawable*, const juce::Colour*) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;

private:
    void applyPalette();
    bool dark = false;
    float uiScale = 1.0f;
    juce::Colour lightBackgroundColour { 0xfff6f6f6 };
    juce::Colour lightForegroundColour { 0xff050505 };
    juce::Colour darkBackgroundColour { 0xff050505 };
    juce::Colour darkForegroundColour { 0xfff6f6f6 };
};

class WordmarkButton final : public juce::TextButton
{
public:
    explicit WordmarkButton(juce::String text);
    void paintButton(juce::Graphics&, bool, bool) override;
};

class SmartGainButton final : public juce::TextButton
{
public:
    explicit SmartGainButton(juce::String text = "AUTO GAIN");
    void setLoadingState(float progress, bool loading, bool reducedMotion);
    void paintButton(juce::Graphics&, bool, bool) override;

private:
    float loadingProgress = 0.0f;
    bool loading = false;
    bool reducedMotion = false;
};

}
