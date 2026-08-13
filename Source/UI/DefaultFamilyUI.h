#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace default_family
{
namespace metrics
{
constexpr int designWidth = 860;
constexpr int headerHeight = 64;
constexpr int wordmarkWidth = 220;
constexpr int structuralGap = 10;
constexpr int controlGap = 4;
constexpr int buttonHeight = 28;
constexpr int controlBorder = 2;
constexpr float headerFontHeight = 16.0f;
constexpr float disabledOpacity = 0.35f;
}

juce::Font mono(float height, bool bold = false);

class ThemePreferences final
{
public:
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
    void setUiScale(float newScale) noexcept;
    bool isDark() const noexcept { return dark; }
    float getUiScale() const noexcept { return uiScale; }

    juce::Colour paper() const noexcept { return juce::Colour(0xfff6f6f6); }
    juce::Colour ink() const noexcept { return juce::Colour(0xff050505); }
    juce::Colour foreground() const noexcept { return findColour(foregroundColourId); }
    juce::Colour background() const noexcept { return findColour(backgroundColourId); }

    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getPopupMenuFont() override;
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

// A compact four-page navigator. It occupies the same square as one rotary
// control and uses shape/inversion rather than colour to encode selection.
class PageRail final : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    PageRail();

    void setSelectedPage(int page, juce::NotificationType notification);
    int getSelectedPage() const noexcept { return selectedPage; }
    std::function<void(int)> onPageChange;

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    int pageAt(juce::Point<int>) const noexcept;
    void choosePage(int page);

    int selectedPage = 0;
    int hoveredPage = -1;
    int pressedPage = -1;
};
}
