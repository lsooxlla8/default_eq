#include "PluginEditor.h"
#include "UI/DriveCharacterFormatting.h"
#include "UI/EditorLayout.h"
#include "UI/FilterIcon.h"
#include "DSP/FilterTypes.h"
#include <numeric>

namespace
{
juce::Font mono(float size, bool bold = false)
{
    return default_family::mono(size, bold);
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

juce::String compactNumber(double value, int digits)
{
    if (std::abs(value) < std::pow(10.0, -digits) * 0.5) value = 0.0;
    auto text = juce::String(value, digits);
    while (text.containsChar('.') && text.endsWithChar('0'))
        text = text.dropLastCharacters(1);
    if (text.endsWithChar('.')) text = text.dropLastCharacters(1);
    return text;
}

}

SettingsOverlay::SettingsOverlay()
{
    setOpaque(true);
}

SettingsOverlay::~SettingsOverlay()
{
    if (colourSelector)
        colourSelector->removeChangeListener(this);
}

void SettingsOverlay::setState(State next)
{
    state = next;
    repaint();
}

void SettingsOverlay::setStatistics(Statistics next)
{
    statistics = next;
    if (isVisible()) repaint();
}

void SettingsOverlay::dismissColourEditor()
{
    if (!colourSelector) return;
    colourSelector->removeChangeListener(this);
    colourSelector.reset();
    editedColour = Cell::none;
    repaint();
}

void SettingsOverlay::resetNavigation()
{
    dismissColourEditor();
    shortcutPageVisible = false;
    repaint();
}

SettingsOverlay::Cell SettingsOverlay::cellAt(juce::Point<int> point) const noexcept
{
    if (shortcutPageVisible)
        return Cell::shortcuts;
    const float x = (float)point.x * 744.0f / (float)juce::jmax(1, getWidth());
    const float y = (float)point.y * 254.0f / (float)juce::jmax(1, getHeight());
    if (y < 42.0f)
    {
        if (x < 186.0f) return Cell::theme;
        if (x < 372.0f) return Cell::gainRange;
        if (x < 558.0f) return Cell::fft;
        return Cell::hover;
    }
    if (y < 84.0f)
    {
        if (x < 248.0f) return Cell::floor;
        if (x < 496.0f) return Cell::average;
        return Cell::slope;
    }
    if (y < 126.0f)
    {
        if (x < 186.0f) return Cell::lightBackground;
        if (x < 372.0f) return Cell::lightForeground;
        if (x < 558.0f) return Cell::darkBackground;
        return Cell::darkForeground;
    }
    if (y < 182.0f)
        return Cell::shortcuts;
    return Cell::none;
}

void SettingsOverlay::resized()
{
    if (colourSelector)
        colourSelector->setBounds(0, juce::roundToInt(getHeight() * 126.0f / 254.0f),
                                  getWidth(), getHeight() - juce::roundToInt(getHeight() * 126.0f / 254.0f));
}

void SettingsOverlay::notifyChanged()
{
    repaint();
    if (onStateChange) onStateChange(state);
}

void SettingsOverlay::nudge(Cell cell, float amount)
{
    if (cell == Cell::floor)
        state.rtaFloorDb = juce::jlimit(-140.0f, -30.0f, state.rtaFloorDb + amount * 2.0f);
    else if (cell == Cell::average)
        state.rtaAverageSeconds = juce::jlimit(0.0f, 1.0f,
                                               state.rtaAverageSeconds + amount * 0.01f);
    else if (cell == Cell::slope)
        state.rtaSlopeDbPerOct = juce::jlimit(-6.0f, 6.0f,
                                              state.rtaSlopeDbPerOct + amount * 0.25f);
    else return;
    notifyChanged();
}

void SettingsOverlay::mouseDown(const juce::MouseEvent& event)
{
    draggedCell = cellAt(event.getPosition());
    if (draggedCell == Cell::shortcuts)
    {
        dismissColourEditor();
        shortcutPageVisible = !shortcutPageVisible;
        draggedCell = Cell::none;
        repaint();
        return;
    }
    dragStartY = event.y;
    dragStartState = state;
    if (event.mods.isPopupMenu() || event.mods.isRightButtonDown())
    {
        if (draggedCell == Cell::floor) state.rtaFloorDb = -80.0f;
        else if (draggedCell == Cell::average) state.rtaAverageSeconds = 0.065f;
        else if (draggedCell == Cell::slope) state.rtaSlopeDbPerOct = 4.5f;
        else if (draggedCell == Cell::lightBackground) state.lightBackground = juce::Colour(0xfff6f6f6);
        else if (draggedCell == Cell::lightForeground) state.lightForeground = juce::Colour(0xff050505);
        else if (draggedCell == Cell::darkBackground) state.darkBackground = juce::Colour(0xff050505);
        else if (draggedCell == Cell::darkForeground) state.darkForeground = juce::Colour(0xfff6f6f6);
        else return;
        dismissColourEditor();
        notifyChanged();
        draggedCell = Cell::none;
        return;
    }
    if (draggedCell == Cell::lightBackground || draggedCell == Cell::lightForeground
        || draggedCell == Cell::darkBackground || draggedCell == Cell::darkForeground)
    {
        showColourEditor(draggedCell);
        draggedCell = Cell::none;
        return;
    }
    if (colourSelector)
        dismissColourEditor();
    if (draggedCell == Cell::theme) state.themeMode = (state.themeMode + 1) % 3;
    else if (draggedCell == Cell::gainRange) state.gainRangeMode = (state.gainRangeMode + 1) % 5;
    else if (draggedCell == Cell::fft) state.fftSizeMode = (state.fftSizeMode + 1) % 3;
    else if (draggedCell == Cell::hover) state.showHoverTooltip = !state.showHoverTooltip;
    else return;
    draggedCell = Cell::none;
    notifyChanged();
}

void SettingsOverlay::showColourEditor(Cell cell)
{
    if (colourSelector && editedColour == cell)
    {
        dismissColourEditor();
        return;
    }
    if (colourSelector)
        colourSelector->removeChangeListener(this);
    editedColour = cell;
    colourSelector = std::make_unique<juce::ColourSelector>(
        juce::ColourSelector::showColourspace, 3, 3);
    const auto selected = cell == Cell::lightBackground ? state.lightBackground
        : cell == Cell::lightForeground ? state.lightForeground
        : cell == Cell::darkBackground ? state.darkBackground : state.darkForeground;
    colourSelector->setCurrentColour(selected, juce::dontSendNotification);
    colourSelector->setColour(juce::ColourSelector::backgroundColourId,
                              findColour(default_family::LookAndFeel::backgroundColourId, true));
    colourSelector->setColour(juce::ColourSelector::labelTextColourId,
                              findColour(default_family::LookAndFeel::foregroundColourId, true));
    colourSelector->addChangeListener(this);
    addAndMakeVisible(*colourSelector);
    resized();
    colourSelector->toFront(false);
    repaint();
}

void SettingsOverlay::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source != colourSelector.get() || editedColour == Cell::none) return;
    const auto colour = colourSelector->getCurrentColour().withAlpha(1.0f);
    if (editedColour == Cell::lightBackground) state.lightBackground = colour;
    else if (editedColour == Cell::lightForeground) state.lightForeground = colour;
    else if (editedColour == Cell::darkBackground) state.darkBackground = colour;
    else if (editedColour == Cell::darkForeground) state.darkForeground = colour;
    notifyChanged();
}

void SettingsOverlay::mouseDrag(const juce::MouseEvent& event)
{
    const float upward = (float)(dragStartY - event.y);
    if (draggedCell == Cell::floor)
        state.rtaFloorDb = juce::jlimit(-140.0f, -30.0f,
                                        dragStartState.rtaFloorDb + upward * 0.25f);
    else if (draggedCell == Cell::average)
        state.rtaAverageSeconds = juce::jlimit(0.0f, 1.0f,
            dragStartState.rtaAverageSeconds + upward * 0.0025f);
    else if (draggedCell == Cell::slope)
        state.rtaSlopeDbPerOct = juce::jlimit(-6.0f, 6.0f,
            dragStartState.rtaSlopeDbPerOct + upward * 0.03f);
    else return;
    notifyChanged();
}

void SettingsOverlay::mouseUp(const juce::MouseEvent&)
{
    draggedCell = Cell::none;
}

void SettingsOverlay::mouseWheelMove(const juce::MouseEvent& event,
                                     const juce::MouseWheelDetails& wheel)
{
    if (std::abs(wheel.deltaY) < 0.0001f)
        return;
    nudge(cellAt(event.getPosition()), wheel.deltaY > 0.0f ? 1.0f : -1.0f);
}

juce::String SettingsOverlay::gainRangeText(int mode)
{
    const auto plusMinus = juce::String::fromUTF8("\xc2\xb1");
    const std::array<juce::String, 5> ranges {
        "AUTO", plusMinus + "6 dB", plusMinus + "12 dB",
        plusMinus + "24 dB", plusMinus + "36 dB"
    };
    return ranges[(size_t)juce::jlimit(0, 4, mode)];
}

void SettingsOverlay::paint(juce::Graphics& g)
{
    const auto fg = findColour(default_family::LookAndFeel::foregroundColourId, true);
    const auto bg = findColour(default_family::LookAndFeel::backgroundColourId, true);
    const float sx = (float)getWidth() / 744.0f;
    const float sy = (float)getHeight() / 254.0f;
    const float scale = juce::jmin(sx, sy);
    const int line = juce::jmax(1, juce::roundToInt(scale));
    const auto rect = [sx, sy](float x, float y, float w, float h)
    {
        return juce::Rectangle<int>(juce::roundToInt(x * sx), juce::roundToInt(y * sy),
                                    juce::roundToInt(w * sx), juce::roundToInt(h * sy));
    };
    const auto text = [&g, fg, scale](juce::String value, juce::Rectangle<int> area,
                                      float size, bool bold, float alpha,
                                      default_family::PrototypeTextAlign alignment)
    {
        default_family::drawPrototypeText(g, value, area.toFloat(), size, bold, 0.0f,
                                           fg.withAlpha(alpha), alignment, scale);
    };
    const auto setting = [&](juce::String label, juce::String value, juce::Rectangle<int> area)
    {
        text(label, area.reduced(8 * line, 4 * line).withHeight(11 * line),
             9.0f, true, 0.72f, default_family::PrototypeTextAlign::left);
        text(value, area.reduced(8 * line, 4 * line).withTrimmedTop(12 * line),
             9.5f, true, 1.0f, default_family::PrototypeTextAlign::left);
    };
    const auto colourSetting = [&](juce::String label, juce::Colour colour,
                                   juce::Rectangle<int> area, bool selected)
    {
        text(label, area.reduced(8 * line, 4 * line).withHeight(11 * line),
             9.0f, true, 0.72f, default_family::PrototypeTextAlign::left);
        auto swatch = area.reduced(8 * line, 4 * line).withTrimmedTop(14 * line);
        swatch = swatch.removeFromLeft(22 * line).withHeight(15 * line);
        g.setColour(colour);
        g.fillRect(swatch);
        g.setColour(fg);
        g.drawRect(swatch, selected ? 2 * line : line);
        text("#" + colour.toDisplayString(false).toUpperCase(),
             area.reduced(8 * line, 4 * line).withTrimmedLeft(29 * line).withTrimmedTop(12 * line),
             9.0f, true, 0.9f, default_family::PrototypeTextAlign::left);
    };
#if JUCE_MAC
    const auto command = juce::String("CMD");
#else
    const auto command = juce::String("CTRL");
#endif
    struct ShortcutRow { juce::String key, action; };
    struct ShortcutGroup { juce::String title; std::array<ShortcutRow, 5> rows; };
    const std::array<ShortcutGroup, 6> shortcutGroups {{
        { "NODE", {{ { "DRAG", "FREQ / GAIN" }, { command + "+DRAG", "DRIVE" },
                      { "SHIFT+DRAG", "THRESHOLD" }, { "ALT+CLICK", "MOMENTARY SOLO" },
                      { "DRAG RANGE", "DYN RANGE" } }} },
        { "WHEEL", {{ { "WHEEL", "Q / CUT SLOPE" }, { command + "+WHEEL", "SLOPE" },
                       { "SHIFT+WHEEL", "PLACEMENT" }, { "ALT+WHEEL", "CHARACTER" },
                       { "UPWARD", "INCREASE VALUE" } }} },
        { "SELECTION", {{ { "SHIFT+CLICK", "TOGGLE BAND" }, { "SHIFT+DRAG", "MARQUEE" },
                           { "RIGHT+DRAG", "MARQUEE" }, { command + "+CLICK", "BYPASS" },
                           { "CLICK NODE", "SELECT" } }} },
        { "RESET", {{ { "SHIFT+" + command + "+CLICK", "CENTER" }, { "SHIFT+RIGHT", "THRESHOLD" },
                       { "ALT+RIGHT", "SLOPE" }, { command + "+RIGHT", "DRIVE / CHARACTER" },
                       { "RIGHT VALUE", "RESET" } }} },
        { "KEYBOARD", {{ { "DEL / BKSP", "DELETE" }, { command + "+Z", "UNDO" },
                          { command + "+SHIFT+Z", "REDO" }, { "ESC", "CLOSE / CANCEL" },
                          { "RETURN", "COMMIT VALUE" } }} },
        { "DIRECT", {{ { "CLICK EMPTY", "CREATE BAND" }, { "SHIFT+EMPTY", "ALTERNATE TYPE" },
                        { "DOUBLE NODE", "DELETE" }, { "RIGHT NODE", "MENU" },
                        { "DOUBLE VALUE", "TYPE" } }} }
    }};

    g.fillAll(bg);
    g.setColour(fg);
    g.drawRect(getLocalBounds(), line);
    if (shortcutPageVisible)
    {
        g.fillRect(rect(0, 32, 744, 1));
        for (float x : { 248.0f, 496.0f }) g.fillRect(rect(x, 32, 1, 222));
        g.fillRect(rect(0, 143, 744, 1));
        text("SHORTCUTS / COMPLETE INTERACTION MAP", rect(10, 8, 430, 14),
             10.0f, true, 1.0f, default_family::PrototypeTextAlign::left);
        text("CLICK ANYWHERE TO RETURN", rect(450, 8, 284, 14),
             9.0f, true, 0.72f, default_family::PrototypeTextAlign::right);
        for (int group = 0; group < 6; ++group)
        {
            const int column = group % 3;
            const int row = group / 3;
            const float x = column * 248.0f;
            const float y = 32.0f + row * 111.0f;
            text(shortcutGroups[(size_t)group].title, rect(x + 10, y + 8, 228, 12),
                 9.0f, true, 0.72f, default_family::PrototypeTextAlign::left);
            for (int item = 0; item < 5; ++item)
            {
                const auto& shortcut = shortcutGroups[(size_t)group].rows[(size_t)item];
                text(shortcut.key, rect(x + 10, y + 27.0f + item * 14.8f, 106, 12),
                     9.0f, true, 1.0f, default_family::PrototypeTextAlign::left);
                text(shortcut.action, rect(x + 118, y + 27.0f + item * 14.8f, 120, 12),
                     9.0f, false, 0.72f, default_family::PrototypeTextAlign::left);
            }
        }
        return;
    }
    for (float y : { 42.0f, 84.0f, 126.0f, 182.0f }) g.fillRect(rect(0, y, 744, 1));
    for (float x : { 186.0f, 372.0f, 558.0f })
    {
        g.fillRect(rect(x, 0, 1, 42));
        g.fillRect(rect(x, 84, 1, 42));
    }
    for (float x : { 248.0f, 496.0f }) g.fillRect(rect(x, 42, 1, 42));
    for (int i = 1; i < 5; ++i) g.fillRect(rect(148.8f * i, 182, 1, 72));

    static const char* themes[] { "AUTO", "WHITE", "BLACK" };
    static const char* fft[] { "4096", "8192", "16384" };
    setting("THEME", themes[juce::jlimit(0, 2, state.themeMode)], rect(0, 0, 186, 42));
    setting("GAIN RANGE", gainRangeText(state.gainRangeMode), rect(186, 0, 186, 42));
    setting("RTA FFT SIZE", fft[juce::jlimit(0, 2, state.fftSizeMode)], rect(372, 0, 186, 42));
    setting("SHOW HOVER TOOLTIP", state.showHoverTooltip ? "ON" : "OFF", rect(558, 0, 186, 42));
    setting("RTA FLOOR", juce::String(state.rtaFloorDb, 0) + " dB", rect(0, 42, 248, 42));
    setting("RTA AVERAGE", juce::String(juce::roundToInt(state.rtaAverageSeconds * 1000.0f)) + " ms",
            rect(248, 42, 248, 42));
    setting("RTA SLOPE", juce::String(state.rtaSlopeDbPerOct, 1) + " dB/oct", rect(496, 42, 248, 42));
    colourSetting("WHITE BACKGROUND", state.lightBackground, rect(0, 84, 186, 42),
                  editedColour == Cell::lightBackground);
    colourSetting("WHITE INK", state.lightForeground, rect(186, 84, 186, 42),
                  editedColour == Cell::lightForeground);
    colourSetting("BLACK BACKGROUND", state.darkBackground, rect(372, 84, 186, 42),
                  editedColour == Cell::darkBackground);
    colourSetting("BLACK INK", state.darkForeground, rect(558, 84, 186, 42),
                  editedColour == Cell::darkForeground);

    const std::array<std::pair<juce::String, juce::String>, 8> shortcuts {{
        { "DRAG", "FREQ / GAIN" }, { command + "+DRAG", "DRIVE" },
        { "SHIFT+DRAG", "THRESHOLD" }, { "WHEEL", "Q / CUT SLOPE" },
        { command + "+WHEEL", "SLOPE" }, { "SHIFT+WHEEL", "PLACEMENT" },
        { "ALT+WHEEL", "CHARACTER" }, { "ALL SHORTCUTS", "OPEN >" }
    }};
    g.setColour(fg.withAlpha(0.13f));
    for (int column = 1; column < 4; ++column) g.fillRect(rect(186.0f * column, 126, 1, 56));
    g.fillRect(rect(0, 154, 744, 1));
    for (int item = 0; item < 8; ++item)
    {
        const int column = item % 4;
        const int row = item / 4;
        const auto area = rect(column * 186.0f, 126.0f + row * 28.0f, 186, 28);
        text(shortcuts[(size_t)item].first,
             area.reduced(8 * line, 3 * line).withHeight(11 * line),
             9.0f, true, 1.0f, default_family::PrototypeTextAlign::left);
        text(shortcuts[(size_t)item].second,
             area.reduced(8 * line, 3 * line).withTrimmedLeft(82 * line).withHeight(11 * line),
             9.0f, false, 0.72f, default_family::PrototypeTextAlign::left);
    }

    const auto statCell = [&](int index, juce::String label, juce::String value)
    {
        const auto area = rect(148.8f * index, 182, 148.8f, 72);
        text(label, area.reduced(8 * line, 4 * line).withHeight(11 * line),
             9.0f, true, 0.72f, default_family::PrototypeTextAlign::left);
        text(value, area.reduced(8 * line, 4 * line).withTrimmedTop(13 * line).withHeight(13 * line),
             9.5f, true, 1.0f, default_family::PrototypeTextAlign::left);
    };
    const bool valid = statistics.spectrum.valid;
    const float centroid = statistics.spectrum.centroidHz;
    const auto centroidText = !valid ? "--" : centroid >= 1000.0f
        ? juce::String(centroid / 1000.0f, 2) + " kHz"
        : juce::String(juce::roundToInt(centroid)) + " Hz";
    statCell(0, "CENTROID", centroidText);
    statCell(1, "CREST FACTOR", valid ? juce::String(statistics.crestDb, 1) + " dB" : "--");
    statCell(2, "SPECTRAL TILT", valid ? juce::String(statistics.spectrum.averageTiltDbPerOct, 1) + " dB/oct" : "--");
    statCell(3, "L/R CORRELATION", valid ? juce::String(statistics.correlation, 2) : "--");
    const auto visual = [&](int index)
    { return rect(148.8f * index + 8.0f, 226, 132.8f, 20); };
    for (int i = 0; i < 4; ++i)
    {
        const auto area = visual(i);
        g.setColour(fg.withAlpha(0.18f));
        g.fillRect(area.getX(), area.getCentreY(), area.getWidth(), line);
    }
    if (valid)
    {
        const float centroidNorm = juce::jlimit(0.0f, 1.0f,
            std::log10(juce::jmax(20.0f, centroid) / 20.0f) / 3.0f);
        auto area = visual(0);
        g.setColour(fg);
        g.fillRect(area.getX() + juce::roundToInt(centroidNorm * (area.getWidth() - 3 * line)),
                   area.getY(), 3 * line, area.getHeight());
        area = visual(1);
        g.fillRect(area.withWidth(juce::roundToInt(area.getWidth()
            * juce::jlimit(0.0f, 1.0f, statistics.crestDb / 30.0f))));
        area = visual(2);
        g.fillRect(area.getCentreX(), area.getY(), line, area.getHeight());
        const float tiltNorm = juce::jmap(juce::jlimit(-12.0f, 12.0f,
            statistics.spectrum.averageTiltDbPerOct), -12.0f, 12.0f, 0.0f, 1.0f);
        g.fillRect(area.getX() + juce::roundToInt(tiltNorm * (area.getWidth() - 3 * line)),
                   area.getY(), 3 * line, area.getHeight());
        area = visual(3);
        g.fillRect(area.getCentreX(), area.getY(), line, area.getHeight());
        const float correlationNorm = juce::jmap(juce::jlimit(-1.0f, 1.0f,
            statistics.correlation), -1.0f, 1.0f, 0.0f, 1.0f);
        g.fillRect(area.getX() + juce::roundToInt(correlationNorm * (area.getWidth() - 3 * line)),
                   area.getY(), 3 * line, area.getHeight());
    }
    const auto tonalArea = rect(148.8f * 4, 182, 148.8f, 72);
    text("TONAL BALANCE", tonalArea.reduced(8 * line, 4 * line).withHeight(11 * line),
         9.0f, true, 0.72f, default_family::PrototypeTextAlign::left);
    const auto bars = visual(4);
    const int gap = juce::jmax(1, line);
    const int barWidth = juce::jmax(1, (bars.getWidth() - gap * 9) / 10);
    float largestBand = 1.0f;
    for (float amount : statistics.spectrum.tonalPercent) largestBand = std::max(largestBand, amount);
    for (int i = 0; i < 10; ++i)
    {
        const int height = valid ? juce::jlimit(line, bars.getHeight(),
            juce::roundToInt(bars.getHeight() * statistics.spectrum.tonalPercent[(size_t)i] / largestBand)) : line;
        g.setColour(fg.withAlpha(valid ? 0.85f : 0.2f));
        g.fillRect(bars.getX() + i * (barWidth + gap), bars.getBottom() - height, barWidth, height);
    }
}

bool PrototypeSelectMenu::isShowingFor(const PrototypeComboBox& box) const noexcept
{
    return isVisible() && activeBox == &box;
}

void PrototypeSelectMenu::showFor(PrototypeComboBox& box, juce::Component& shell, float scale)
{
    if (activeBox != nullptr)
    {
        activeBox->getProperties().set("pickerOpen", false);
        activeBox->repaint();
    }
    activeBox = &box;
    hoverRow = -1;
    uiScale = scale;
    activeBox->getProperties().set("pickerOpen", true);
    activeBox->repaint();

    const int shellInset = juce::roundToInt(4.0f * scale);
    const int border = juce::jmax(1, juce::roundToInt(scale));
    const int padding = juce::roundToInt(3.0f * scale);
    const int rowHeight = juce::roundToInt(22.0f * scale);
    const int menuHeight = juce::jmin(juce::roundToInt(260.0f * scale),
                                      box.getNumItems() * rowHeight
                                          + padding * 2 + border * 2);
    const int menuWidth = box.getWidth();
    const auto field = box.getBounds();
    const int left = juce::jlimit(shellInset,
        juce::jmax(shellInset, shell.getWidth() - menuWidth - shellInset), field.getX());
    int top = field.getBottom();
    if (top + menuHeight > shell.getHeight() - shellInset)
        top = field.getY() - menuHeight;
    top = juce::jmax(shellInset, top);
    setBounds(left, top, menuWidth, menuHeight);
    setVisible(true);
    toFront(false);
    repaint();
}

void PrototypeSelectMenu::hide()
{
    if (activeBox != nullptr)
    {
        activeBox->getProperties().set("pickerOpen", false);
        activeBox->repaint();
    }
    activeBox = nullptr;
    hoverRow = -1;
    setVisible(false);
    setBounds({});
}

int PrototypeSelectMenu::rowAt(juce::Point<int> point) const noexcept
{
    if (activeBox == nullptr) return -1;
    const int border = juce::jmax(1, juce::roundToInt(uiScale));
    const int padding = juce::roundToInt(3.0f * uiScale);
    const int inset = border + padding;
    const int rowHeight = juce::roundToInt(22.0f * uiScale);
    const int row = (point.y - inset) / juce::jmax(1, rowHeight);
    return point.x >= inset && point.x < getWidth() - inset
        && point.y >= inset && row >= 0 && row < activeBox->getNumItems() ? row : -1;
}

void PrototypeSelectMenu::paint(juce::Graphics& g)
{
    if (activeBox == nullptr) return;
    const auto fg = findColour(default_family::LookAndFeel::foregroundColourId, true);
    const auto bg = findColour(default_family::LookAndFeel::backgroundColourId, true);
    g.fillAll(bg);
    g.setColour(fg);
    g.drawRect(getLocalBounds(), juce::jmax(1, juce::roundToInt(uiScale)));

    const int border = juce::jmax(1, juce::roundToInt(uiScale));
    const int padding = juce::roundToInt(3.0f * uiScale);
    const int inset = border + padding;
    const int rowHeight = juce::roundToInt(22.0f * uiScale);
    const bool filterMenu = (bool)activeBox->getProperties().getWithDefault(
        "filterSelector", false);
    for (int row = 0; row < activeBox->getNumItems(); ++row)
    {
        auto area = juce::Rectangle<int>(inset, inset + row * rowHeight,
                                         getWidth() - inset * 2, rowHeight);
        const bool active = row == activeBox->getSelectedItemIndex() || row == hoverRow;
        g.setColour(active ? fg : bg);
        g.fillRect(area);
        auto content = area.toFloat().reduced(6.0f * uiScale, 3.0f * uiScale);
        const auto textColour = active ? bg : fg;
        if (filterMenu)
        {
            auto icon = content.removeFromLeft(26.0f * uiScale);
            deq::ui::paintFilterIcon(g, icon.withSizeKeepingCentre(
                26.0f * uiScale, 16.0f * uiScale), row, textColour);
            content.removeFromLeft(7.0f * uiScale);
        }
        default_family::drawPrototypeText(
            g, activeBox->getItemText(row), content, 9.0f, true, 0.0f,
            textColour, default_family::PrototypeTextAlign::left, uiScale);
    }
}

void PrototypeSelectMenu::mouseMove(const juce::MouseEvent& event)
{
    const int next = rowAt(event.getPosition());
    if (next == hoverRow) return;
    hoverRow = next;
    repaint();
}

void PrototypeSelectMenu::mouseExit(const juce::MouseEvent&)
{
    if (hoverRow == -1) return;
    hoverRow = -1;
    repaint();
}

void PrototypeSelectMenu::mouseDown(const juce::MouseEvent& event)
{
    if (!event.mods.isLeftButtonDown() || activeBox == nullptr) return;
    const int row = rowAt(event.getPosition());
    if (row < 0) return;
    auto* box = activeBox;
    if (onChoose) onChoose(*box, row);
    hide();
}

juce::Rectangle<int> PrototypeContextMenu::scaledRect(
    int x, int y, int width, int height, juce::Point<int> origin) const noexcept
{
    return { origin.x + juce::roundToInt((float)x * uiScale),
             origin.y + juce::roundToInt((float)y * uiScale),
             juce::roundToInt((float)width * uiScale),
             juce::roundToInt((float)height * uiScale) };
}

void PrototypeContextMenu::showAt(juce::Point<int> pointer, juce::Component& shell,
                                  float scale, PrototypeContextMenuModel newModel)
{
    model = std::move(newModel);
    uiScale = scale;
    hovered = {};
    submenuVisible = false;
    setBounds(shell.getLocalBounds());

    const int inset = juce::roundToInt(4.0f * scale);
    const int width = juce::roundToInt(288.0f * scale);
    const int height = juce::roundToInt((model.selectedCount > 1 ? 257.0f : 216.0f) * scale);
    const int routeCentre = juce::roundToInt(110.0f * scale);
    const int left = juce::jlimit(inset, shell.getWidth() - width - inset, pointer.x);
    const int top = juce::jlimit(inset, shell.getHeight() - height - inset,
                                 pointer.y - routeCentre);
    mainBounds = { left, top, width, height };

    bool opensLeft = std::abs(pointer.x - left)
        <= std::abs(pointer.x - mainBounds.getRight());
    const int submenuReach = juce::roundToInt(185.0f * scale);
    if (opensLeft && left < submenuReach) opensLeft = false;
    if (!opensLeft && mainBounds.getRight() + submenuReach > shell.getWidth()) opensLeft = true;
    const int submenuWidth = juce::roundToInt(178.0f * scale);
    const int submenuHeight = juce::roundToInt(254.0f * scale);
    // The submenu is positioned from the 274 px content row: its CSS 7 px
    // offset exactly consumes the main menu's border + padding.
    const int submenuX = opensLeft ? left - submenuWidth : mainBounds.getRight();
    submenuBounds = { submenuX,
                      top + juce::roundToInt(132.0f * scale),
                      submenuWidth, submenuHeight };
    setVisible(true);
    toFront(false);
    repaint();
}

void PrototypeContextMenu::hide()
{
    model = {};
    mainBounds = {};
    submenuBounds = {};
    hovered = {};
    submenuVisible = false;
    setVisible(false);
}

bool PrototypeContextMenu::hitTest(int x, int y)
{
    const auto point = juce::Point<int>(x, y);
    return mainBounds.contains(point) || (submenuVisible && submenuBounds.contains(point));
}

PrototypeContextMenu::Hit PrototypeContextMenu::itemAt(juce::Point<int> point) const noexcept
{
    const auto mainOrigin = mainBounds.getPosition();
    if (scaledRect(7, 7, 274, 30, mainOrigin).contains(point))
        return { HitKind::toggle, 0 };
    const auto filter = scaledRect(7, 48, 274, 34, mainOrigin);
    if (filter.contains(point))
        return { HitKind::filter, juce::jlimit(0, 9,
            (point.x - filter.getX()) * 10 / juce::jmax(1, filter.getWidth())) };
    const auto route = scaledRect(7, 93, 274, 34, mainOrigin);
    if (route.contains(point))
        return { HitKind::route, juce::jlimit(0, 6,
            (point.x - route.getX()) * 7 / juce::jmax(1, route.getWidth())) };
    if (scaledRect(7, 138, 274, 30, mainOrigin).contains(point))
        return { HitKind::saturation, 0 };
    if (scaledRect(7, 179, 274, 30, mainOrigin).contains(point))
        return { HitKind::reset, 0 };
    if (model.selectedCount > 1
        && scaledRect(7, 220, 274, 30, mainOrigin).contains(point))
        return { HitKind::bypass, 0 };
    if (submenuVisible && submenuBounds.contains(point))
    {
        const auto content = submenuBounds.reduced(juce::roundToInt(7.0f * uiScale));
        const int rowHeight = juce::roundToInt(30.0f * uiScale);
        const int row = (point.y - content.getY()) / juce::jmax(1, rowHeight);
        if (point.x >= content.getX() && point.x < content.getRight()
            && row >= 0 && row < 8)
            return { HitKind::saturationChoice, row };
    }
    return {};
}

void PrototypeContextMenu::paint(juce::Graphics& g)
{
    if (mainBounds.isEmpty()) return;
    const auto paper = findColour(default_family::LookAndFeel::backgroundColourId, true);
    const auto ink = findColour(default_family::LookAndFeel::foregroundColourId, true);
    const auto border = juce::jmax(1, juce::roundToInt(uiScale));
    const auto mainOrigin = mainBounds.getPosition();
    const auto isHovered = [this](HitKind kind, int index = -1)
    {
        return hovered.kind == kind && (index < 0 || hovered.index == index);
    };
    const auto paintButton = [&](juce::Rectangle<int> area, const juce::String& text,
                                 HitKind kind, bool active = false)
    {
        const bool reverse = active || isHovered(kind);
        g.setColour(reverse ? paper : ink);
        g.fillRect(area);
        default_family::drawPrototypeText(g, text.toUpperCase(),
            area.toFloat().reduced(9.0f * uiScale, 5.0f * uiScale),
            9.0f, true, 0.0f, reverse ? ink : paper,
            default_family::PrototypeTextAlign::left, uiScale);
    };

    g.setColour(juce::Colour(0x33050505));
    g.fillRect(mainBounds.translated(juce::roundToInt(7.0f * uiScale),
                                    juce::roundToInt(7.0f * uiScale)));
    g.setColour(ink);
    g.fillRect(mainBounds);
    g.setColour(paper);
    g.drawRect(mainBounds, border);

    paintButton(scaledRect(7, 7, 274, 30, mainOrigin),
                "ENABLE/DISABLE BAND " + juce::String(model.bandNumber), HitKind::toggle);

    const auto filter = scaledRect(7, 48, 274, 34, mainOrigin);
    const float filterWidth = (float)filter.getWidth() / 10.0f;
    for (int index = 0; index < 10; ++index)
    {
        auto cell = juce::Rectangle<float>(filter.getX() + filterWidth * (float)index,
            (float)filter.getY(), filterWidth, (float)filter.getHeight());
        const bool reverse = index == model.selectedType
            || (hovered.kind == HitKind::filter && hovered.index == index);
        g.setColour(reverse ? paper : ink);
        g.fillRect(cell);
        if (!reverse)
        {
            g.setColour(paper.withAlpha(0.08f));
            g.drawRect(cell, uiScale);
        }
        deq::ui::paintFilterIcon(g,
            cell.reduced(1.0f * uiScale).withSizeKeepingCentre(
                26.0f * uiScale, 16.0f * uiScale),
            index, reverse ? ink : paper);
    }

    static constexpr const char* routeLabels[] { "L", "C", "R", "M", "S", "T", "S" };
    const auto route = scaledRect(7, 93, 274, 34, mainOrigin);
    const float routeWidth = (float)route.getWidth() / 7.0f;
    for (int index = 0; index < 7; ++index)
    {
        auto cell = juce::Rectangle<float>(route.getX() + routeWidth * (float)index,
            (float)route.getY(), routeWidth, (float)route.getHeight());
        const bool reverse = index == model.selectedRoute
            || (hovered.kind == HitKind::route && hovered.index == index);
        g.setColour(reverse ? paper : ink);
        g.fillRect(cell);
        if (!reverse)
        {
            g.setColour(paper.withAlpha(0.08f));
            g.drawRect(cell, uiScale);
        }
        default_family::drawPrototypeText(g, routeLabels[index], cell,
            11.0f, true, 0.0f, reverse ? ink : paper,
            default_family::PrototypeTextAlign::centre, uiScale);
    }

    auto saturation = scaledRect(7, 138, 274, 30, mainOrigin);
    const bool saturationHover = isHovered(HitKind::saturation) || submenuVisible;
    g.setColour(saturationHover ? paper : ink);
    g.fillRect(saturation);
    default_family::drawPrototypeText(g, "SATURATION",
        saturation.toFloat().reduced(9.0f * uiScale, 5.0f * uiScale),
        9.0f, true, 0.0f, saturationHover ? ink : paper,
        default_family::PrototypeTextAlign::left, uiScale);
    g.setColour(saturationHover ? ink : paper);
    g.fillRect(saturation.removeFromRight(juce::roundToInt(14.0f * uiScale))
        .withSizeKeepingCentre(juce::roundToInt(5.0f * uiScale),
                               juce::roundToInt(5.0f * uiScale)));

    paintButton(scaledRect(7, 179, 274, 30, mainOrigin),
                "RESET EQUALIZER", HitKind::reset);
    if (model.selectedCount > 1)
        paintButton(scaledRect(7, 220, 274, 30, mainOrigin),
            "BYPASS SELECTED (" + juce::String(model.selectedCount) + ")", HitKind::bypass);

    for (const int y : { 42, 87, 132, 173, 214 })
    {
        if (y == 214 && model.selectedCount <= 1) continue;
        g.setColour(paper.withAlpha(0.38f));
        g.fillRect(scaledRect(7, y, 274, 1, mainOrigin));
    }

    if (!submenuVisible) return;
    g.setColour(ink);
    g.fillRect(submenuBounds);
    g.setColour(paper);
    g.drawRect(submenuBounds, border);
    const auto submenuOrigin = submenuBounds.getPosition();
    static constexpr const char* saturationNames[] {
        "SOFT CLIP", "DIODE", "TRIODE", "TRANSISTOR",
        "TAPE", "ODD / EVEN", "PHASE DISTORTION", "SINE EROSION"
    };
    for (int mode = 0; mode < 8; ++mode)
    {
        auto row = scaledRect(7, 7 + mode * 30, 164, 30, submenuOrigin);
        const bool reverse = mode == model.selectedSaturation
            || (hovered.kind == HitKind::saturationChoice && hovered.index == mode);
        g.setColour(reverse ? paper : ink);
        g.fillRect(row);
        default_family::drawPrototypeText(g, saturationNames[mode],
            row.toFloat().reduced(9.0f * uiScale, 5.0f * uiScale),
            9.0f, true, 0.0f, reverse ? ink : paper,
            default_family::PrototypeTextAlign::left, uiScale);
    }
}

void PrototypeContextMenu::mouseMove(const juce::MouseEvent& event)
{
    const auto next = itemAt(event.getPosition());
    if (next.kind == HitKind::saturation) submenuVisible = true;
    if (next.kind == hovered.kind && next.index == hovered.index) return;
    hovered = next;
    repaint();
}

void PrototypeContextMenu::mouseExit(const juce::MouseEvent&)
{
    hovered = {};
    repaint();
}

void PrototypeContextMenu::mouseDown(const juce::MouseEvent& event)
{
    if (!event.mods.isLeftButtonDown()) return;
    const auto hit = itemAt(event.getPosition());
    switch (hit.kind)
    {
        case HitKind::toggle: if (model.toggleBand) model.toggleBand(); break;
        case HitKind::filter: if (model.chooseFilter) model.chooseFilter(hit.index); break;
        case HitKind::route: if (model.chooseRoute) model.chooseRoute(hit.index); break;
        case HitKind::saturation:
            submenuVisible = true; repaint(); return;
        case HitKind::saturationChoice:
            if (model.chooseSaturation) model.chooseSaturation(hit.index); break;
        case HitKind::reset: if (model.resetEqualizer) model.resetEqualizer(); break;
        case HitKind::bypass: if (model.bypassSelected) model.bypassSelected(); break;
        case HitKind::none: return;
    }
    hide();
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
    themeMode = default_family::ThemePreferences::loadMode();
    darkTheme = default_family::ThemePreferences::isDarkForHour(
        themeMode, juce::Time::getCurrentTime().getHours());
    familyLook.setDark(darkTheme);
    setLookAndFeel(&familyLook);
    responseCurve.setDarkMode(darkTheme);

    setResizable(true, false);
    addMouseListener(this, true);
    setResizeLimits(deq::ui::editor_layout::minimumWidth,
                    deq::ui::editor_layout::minimumHeight,
                    deq::ui::editor_layout::maximumWidth,
                    deq::ui::editor_layout::maximumHeight);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(deq::ui::editor_layout::aspectRatio);
    const auto initialSize = deq::ui::editor_layout::constrainedSize(
        uiPreferences->getIntValue("windowWidth", deq::ui::editor_layout::defaultWidth),
        uiPreferences->getIntValue("windowHeight", deq::ui::editor_layout::defaultHeight));
    setSize(initialSize.x, initialSize.y);

    addAndMakeVisible(responseCurve);
    addChildComponent(settingsOverlay);

    auto addButton = [this](auto& button)
    {
        button.setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(button);
    };
    addButton(themeBtn); addButton(powerBtn);
    addButton(bandOn); addButton(bandSolo); addButton(adaptiveQBtn);
    addButton(dynModeBtn); addButton(sidechainBtn);

    auto addCombo = [this](juce::ComboBox& box) { box.setJustificationType(juce::Justification::centred); addAndMakeVisible(box); };
    for (auto* type : { "RES LP", "RES HP", "NOTCH", "TILT", "BAND PASS",
                        "BELL", "LOW SHELF", "HIGH SHELF", "LOW PASS", "HIGH PASS" })
        typeBox.addItem(type, typeBox.getNumItems() + 1);
    saturationBox.addItemList({ "SOFT CLIP", "DIODE", "TRIODE", "TRANSISTOR",
                                "TAPE", "ODD / EVEN", "PHASE DISTORTION", "SINE EROSION" }, 1);
    const auto multiplicationSign = juce::String::charToString(0x00d7);
    oversamplingBox.addItemList(juce::StringArray {
        "OFF", "2" + multiplicationSign, "4" + multiplicationSign,
        "8" + multiplicationSign }, 1);
    phaseModeBox.addItemList({ "MINIMUM", "LINEAR ECO", "LINEAR MED", "LINEAR MAX" }, 1);
    placementModeBox.addItemList({ "L/R", "M/S", "T/S" }, 1);
    oversamplingBox.setName("OS");
    phaseModeBox.setName("PHASE");
    placementModeBox.setName("ROUTE");
    saturationBox.setName("DIST TYPE");
    typeBox.setName({});
    typeBox.getProperties().set("filterSelector", true);
    typeBox.getProperties().set("filterType", 5);
    oversamplingBox.getProperties().set("headerCell", true);
    oversamplingBox.getProperties().set("rightDivider", true);
    phaseModeBox.getProperties().set("headerCell", true);
    phaseModeBox.getProperties().set("rightDivider", true);
    typeBox.getProperties().set("valueStripCell", true);
    typeBox.getProperties().set("rightDivider", true);
    placementModeBox.getProperties().set("workspaceCell", true);
    placementModeBox.getProperties().set("rightDivider", true);
    placementModeBox.getProperties().set("bottomDivider", true);
    saturationBox.getProperties().set("workspaceCell", true);
    saturationBox.getProperties().set("rightDivider", true);
    addCombo(typeBox);
    addCombo(placementModeBox); addCombo(saturationBox);
    addCombo(oversamplingBox); addCombo(phaseModeBox);
    autoGainBtn.setMouseClickGrabsKeyboardFocus(false);
    addAndMakeVisible(autoGainBtn);
    addChildComponent(selectMenu);
    addChildComponent(contextMenu);
    responseCurve.onContextMenuRequest = [this](juce::Point<int> pointer,
                                                 PrototypeContextMenuModel model)
    {
        hideSelectMenu();
        contextMenu.showAt(pointer, *this, familyLook.getUiScale(), std::move(model));
    };
    responseCurve.onContextMenuDismissRequest = [this] { hideContextMenu(); };
    selectMenu.onChoose = [this](PrototypeComboBox& box, int row)
    {
        if (&box == &typeBox) typeMouseInteraction = true;
        if (&box == &placementModeBox) placementModeMouseInteraction = true;
        if (&box == &saturationBox) saturationMouseInteraction = true;
        box.setSelectedItemIndex(row, juce::sendNotificationSync);
    };
    for (auto* box : { &typeBox, &placementModeBox, &saturationBox,
                       &oversamplingBox, &phaseModeBox })
        box->onPickerRequest = [this](PrototypeComboBox& requested)
        { toggleSelectMenu(requested); };

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
    dynRatio.setVerticalMini(true);
    dynRatio.setValueVisible(true);
    dynThreshold.setDoubleClickReturnValue(true, 0.0);
    addAndMakeVisible(dynThreshold);
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
    placementSlider.setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
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
    qField.setFormatter([](double v)
                        {
                            auto text = juce::String(v, 2);
                            while (text.containsChar('.') && text.endsWithChar('0'))
                                text = text.dropLastCharacters(1);
                            if (text.endsWithChar('.')) text = text.dropLastCharacters(1);
                            return text;
                        },
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
                                return compactNumber(v, 1) + " dB";
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
    for (auto* field : { &freqField, &gainField, &qField, &slopeField, &outputSlider })
        field->getProperties().set("valueStripCell", true);
    for (auto* field : { &freqField, &gainField, &qField, &slopeField })
        field->getProperties().set("rightDivider", true);
    outputSlider.setFormatter([](double v) { return compactNumber(v, 1) + " dB"; },
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
    amountSlider.getProperties().set("headerCell", true);
    amountSlider.getProperties().set("rightDivider", true);
    shiftSlider.getProperties().set("headerCell", true);
    shiftSlider.getProperties().set("rightDivider", true);
    amountSlider.setFormatter([](double v)
                              {
                                  const double percent = std::abs(v) < 0.005 ? 0.0 : v * 100.0;
                                  return juce::String(juce::roundToInt(percent)) + "%";
                              },
                              [](const juce::String& s) { return parseUnitValue(s) * 0.01; });
    amountSlider.setValueVisible(true);
    amountSlider.setDoubleClickReturnValue(true, 1.0);
    autoGainBtn.getProperties().set("headerLabel", "AUTO GAIN");
    powerBtn.getProperties().set("headerLabel", "POWER");
    powerBtn.getProperties().set("leftDivider", true);
    adaptiveQBtn.getProperties().set("valueStripCell", true);
    adaptiveQBtn.getProperties().set("rightDivider", true);
    for (auto* button : { static_cast<juce::Button*>(&bandOn),
                          static_cast<juce::Button*>(&bandSolo),
                          static_cast<juce::Button*>(&dynModeBtn),
                          static_cast<juce::Button*>(&sidechainBtn) })
        button->getProperties().set("workspaceButton", true);
    placementSlider.getProperties().set("rightDivider", true);
    for (auto* slider : { static_cast<juce::Slider*>(&driveSlider),
                          static_cast<juce::Slider*>(&dynThreshold),
                          static_cast<juce::Slider*>(&dynRange),
                          static_cast<juce::Slider*>(&dynRatio),
                          static_cast<juce::Slider*>(&dynSpeed) })
        slider->getProperties().set("rightDivider", true);
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

    themeBtn.setTooltip("Open settings and diagnostics");
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
        toggleSettingsOverlay();
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
        typeBox.getProperties().set("filterType", type);
        typeBox.repaint();
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
        {
            if (groupUserChange)
                applyAbsoluteToSelectedBands("placement_mode", 1.0f);
            else if (auto* placementMode = proc.apvts.getParameter(
                         bandId(selectedBand + 1, "placement_mode")))
            {
                placementMode->beginChangeGesture();
                placementMode->setValueNotifyingHost(placementMode->convertTo0to1(1.0f));
                placementMode->endChangeGesture();
            }
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
    SettingsOverlay::State settings;
    settings.themeMode = themeMode;
    settings.gainRangeMode = juce::jlimit(0, 4,
        uiPreferences->getIntValue("gainRangeMode", 0));
    settings.fftSizeMode = juce::jlimit(0, 2,
        uiPreferences->getIntValue("analyzerFftSizeMode", 1));
    settings.rtaFloorDb = (float)uiPreferences->getDoubleValue("analyzerFloor", -80.0);
    settings.rtaAverageSeconds = (float)uiPreferences->getDoubleValue("analyzerAveraging", 0.065);
    settings.rtaSlopeDbPerOct = (float)uiPreferences->getDoubleValue("analyzerTilt", 4.5);
    settings.showHoverTooltip = uiPreferences->getBoolValue("showHoverTooltip", true);
    settings.lightBackground = juce::Colour::fromString(uiPreferences->getValue(
        "lightBackgroundColour", settings.lightBackground.toString()));
    settings.lightForeground = juce::Colour::fromString(uiPreferences->getValue(
        "lightForegroundColour", settings.lightForeground.toString()));
    settings.darkBackground = juce::Colour::fromString(uiPreferences->getValue(
        "darkBackgroundColour", settings.darkBackground.toString()));
    settings.darkForeground = juce::Colour::fromString(uiPreferences->getValue(
        "darkForegroundColour", settings.darkForeground.toString()));
    settingsOverlay.setState(settings);
    settingsOverlay.onStateChange = [this](const SettingsOverlay::State& next)
    {
        applySettings(next, true);
    };
    applySettings(settings, false);
    responseCurve.resetAutoRtaRangeForOpen();
    responseCurve.resetPeakHold();
    for (auto* obsoletePreference : { "analyzerVisible", "analyzerPeakHold", "analyzerResolution",
                                      "analyzerResolutionV2", "analyzerResolutionV3" })
        uiPreferences->removeValue(obsoletePreference);
    powerAtt = std::make_unique<ButtonAttachment>(proc.apvts, "plugin_enabled", powerBtn);
    adaptiveQAtt = std::make_unique<ButtonAttachment>(proc.apvts, "adaptive_q", adaptiveQBtn);
    oversamplingAtt = std::make_unique<ComboAttachment>(proc.apvts, "oversampling", oversamplingBox);
    amountAtt = std::make_unique<SliderAttachment>(proc.apvts, "scale", amountSlider);
    shiftAtt = std::make_unique<SliderAttachment>(proc.apvts, "shift", shiftSlider);
    outputAtt = std::make_unique<SliderAttachment>(proc.apvts, "output_gain", outputSlider);

    const bool initialLinear = proc.apvts.getRawParameterValue("linear_phase")->load() > 0.5f;
    const int initialLinearQuality = (int)proc.apvts.getRawParameterValue("linear_quality")->load();
    phaseModeBox.setSelectedId(initialLinear
        ? std::clamp(initialLinearQuality + 2, 2, 4) : 1, juce::dontSendNotification);

    const int initialAutoMode = (int)proc.apvts.getRawParameterValue("auto_gain_mode")->load();
    autoGainBtn.setButtonText(initialAutoMode == 2 ? "SMART"
                              : initialAutoMode == 1 ? "REGULAR" : "OFF");
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

void DefaultEqualizerAudioProcessorEditor::toggleSelectMenu(PrototypeComboBox& box)
{
    hideContextMenu();
    if (selectMenu.isShowingFor(box))
    {
        hideSelectMenu();
        return;
    }
    selectMenu.showFor(box, *this, familyLook.getUiScale());
}

void DefaultEqualizerAudioProcessorEditor::hideSelectMenu()
{
    selectMenu.hide();
}

void DefaultEqualizerAudioProcessorEditor::hideContextMenu()
{
    contextMenu.hide();
}

void DefaultEqualizerAudioProcessorEditor::toggleSettingsOverlay()
{
    if (settingsOverlay.isVisible())
    {
        hideSettingsOverlay();
        return;
    }
    hideSelectMenu();
    hideContextMenu();
    responseCurve.dismissNumericEditor();
    settingsOverlay.setVisible(true);
    settingsOverlay.toFront(false);
    settingsOverlay.repaint();
}

void DefaultEqualizerAudioProcessorEditor::hideSettingsOverlay()
{
    const bool wasVisible = settingsOverlay.isVisible();
    settingsOverlay.resetNavigation();
    settingsOverlay.setVisible(false);
    // Closing the settings panel is the user's explicit boundary for applying
    // a smaller AUTO RTA gain range, just like closing and reopening the editor.
    if (wasVisible)
        responseCurve.resetAutoRtaRangeForOpen();
}

void DefaultEqualizerAudioProcessorEditor::applyThemeMode(int mode, bool persist)
{
    themeMode = juce::jlimit((int)default_family::ThemePreferences::automatic,
                             (int)default_family::ThemePreferences::black, mode);
    const bool nextDark = default_family::ThemePreferences::isDarkForHour(
        themeMode, juce::Time::getCurrentTime().getHours());
    if (persist)
        default_family::ThemePreferences::saveMode(themeMode);
    if (nextDark == darkTheme) return;
    darkTheme = nextDark;
    familyLook.setDark(darkTheme);
    responseCurve.setDarkMode(darkTheme);
    applySliderPalette();
    sendLookAndFeelChange();
    repaint();
}

void DefaultEqualizerAudioProcessorEditor::applySettings(const SettingsOverlay::State& state,
                                                          bool persist)
{
    familyLook.setThemeColours(state.lightBackground, state.lightForeground,
                               state.darkBackground, state.darkForeground);
    responseCurve.setThemeColours(state.lightBackground, state.lightForeground,
                                  state.darkBackground, state.darkForeground);
    applyThemeMode(state.themeMode, persist);
    responseCurve.setGainRangeMode(state.gainRangeMode);
    responseCurve.setHoverTooltipEnabled(state.showHoverTooltip);
    responseCurve.setAnalyzerSettings(state.rtaFloorDb, state.rtaAverageSeconds,
                                      state.rtaSlopeDbPerOct);
    if (appliedFftSizeMode != state.fftSizeMode)
    {
        appliedFftSizeMode = state.fftSizeMode;
        proc.preSpectrumFifo.setResolution(state.fftSizeMode);
        proc.spectrumFifo.setResolution(state.fftSizeMode);
        responseCurve.resetPeakHold();
    }
    applySliderPalette();
    sendLookAndFeelChange();
    repaint();
    if (!persist || uiPreferences == nullptr) return;
    uiPreferences->setValue("gainRangeMode", state.gainRangeMode);
    uiPreferences->setValue("analyzerFftSizeMode", state.fftSizeMode);
    uiPreferences->setValue("analyzerFloor", state.rtaFloorDb);
    uiPreferences->setValue("analyzerAveraging", state.rtaAverageSeconds);
    uiPreferences->setValue("analyzerTilt", state.rtaSlopeDbPerOct);
    uiPreferences->setValue("showHoverTooltip", state.showHoverTooltip);
    uiPreferences->setValue("lightBackgroundColour", state.lightBackground.toString());
    uiPreferences->setValue("lightForegroundColour", state.lightForeground.toString());
    uiPreferences->setValue("darkBackgroundColour", state.darkBackground.toString());
    uiPreferences->setValue("darkForegroundColour", state.darkForeground.toString());
}


bool DefaultEqualizerAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey
        && (selectMenu.isVisible() || contextMenu.isVisible() || settingsOverlay.isVisible()))
    {
        hideSelectMenu();
        hideContextMenu();
        hideSettingsOverlay();
        return true;
    }
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

    const int sharedMode = default_family::ThemePreferences::loadMode();
    const bool sharedDark = default_family::ThemePreferences::isDarkForHour(
        sharedMode, juce::Time::getCurrentTime().getHours());
    if (sharedMode != themeMode || sharedDark != darkTheme)
    {
        themeMode = sharedMode;
        darkTheme = sharedDark;
        familyLook.setDark(darkTheme); responseCurve.setDarkMode(darkTheme);
        applySliderPalette();
        sendLookAndFeelChange(); repaint();
        auto state = settingsOverlay.getState();
        state.themeMode = themeMode;
        settingsOverlay.setState(state);
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
    settingsOverlay.setStatistics({ responseCurve.calculateSpectralStatistics(),
        proc.uiOutputCrestDb.load(std::memory_order_relaxed),
        proc.uiOutputCorrelation.load(std::memory_order_relaxed) });
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
    autoGainBtn.setButtonText(autoMode == 2 ? "SMART"
                              : autoMode == 1 ? "REGULAR" : "OFF");
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
    updateMixedSelectionDisplays();
    updateHeaderText();
}

void DefaultEqualizerAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto fg = familyLook.foreground(), bg = familyLook.background();
    const auto layout = deq::ui::editor_layout::metricsForSize(getWidth(), getHeight());
    g.fillAll(fg);
    g.setColour(bg);
    g.fillRect(layout.bounds(4, 4, 744, 60));
    g.fillRect(layout.bounds(4, 68, 744, 254));
    g.fillRect(layout.bounds(4, 326, 744, 28));
    g.fillRect(layout.bounds(4, 358, 744, 92));
    g.setColour(fg);
    g.fillRect(layout.bounds(53, 358, 1, 92));
    g.fillRect(layout.bounds(350, 358, 1, 92));
    g.fillRect(layout.bounds(400, 358, 1, 92));
}

void DefaultEqualizerAudioProcessorEditor::paintOverChildren(juce::Graphics& g)
{
    const auto layout = deq::ui::editor_layout::metricsForSize(getWidth(), getHeight());
    g.setColour(familyLook.foreground());
    // Literal port of .wordmark::before/::after and
    // .wordmark + .header-cell::before. The 9 px squares extend 4 px beyond
    // the wordmark component, so they must be painted by the common shell.
    g.fillRect(layout.bounds(173, 4, 9, 9));
    g.fillRect(layout.bounds(173, 55, 9, 9));
    g.fillRect(layout.bounds(177, 13, 1, 42));
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
    const auto currentSize = juce::Point<int>(w, h);
    if (lastLaidOutSize != currentSize)
    {
        hideSelectMenu();
        hideContextMenu();
        hideSettingsOverlay();
        lastLaidOutSize = currentSize;
    }
    const auto layout = deq::ui::editor_layout::metricsForSize(w, h);
    familyLook.setUiScale(layout.scale);

    themeBtn.setBounds(layout.bounds(4, 4, 174, 60));
    oversamplingBox.setBounds(layout.bounds(178, 4, 74, 60));
    phaseModeBox.setBounds(layout.bounds(252, 4, 99, 60));
    amountSlider.setBounds(layout.bounds(351, 4, 112, 60));
    shiftSlider.setBounds(layout.bounds(463, 4, 136, 60));
    autoGainBtn.setBounds(layout.bounds(599, 4, 75, 60));
    powerBtn.setBounds(layout.bounds(674, 4, 74, 60));

    responseCurve.setBounds(layout.bounds(4, 68, 744, 254));
    settingsOverlay.setBounds(responseCurve.getBounds());
    adaptiveQBtn.setBounds(layout.bounds(4, 326, 99, 28));
    typeBox.setBounds(layout.bounds(103, 326, 112, 28));
    freqField.setBounds(layout.bounds(215, 326, 136, 28));
    gainField.setBounds(layout.bounds(351, 326, 112, 28));
    qField.setBounds(layout.bounds(463, 326, 68, 28));
    slopeField.setBounds(layout.bounds(531, 326, 143, 28));
    outputSlider.setBounds(layout.bounds(674, 326, 74, 28));

    bandOn.setBounds(layout.bounds(9, 366, 39, 35));
    bandSolo.setBounds(layout.bounds(9, 407, 39, 35));
    placementModeBox.setBounds(layout.bounds(54, 358, 49, 46));
    placementSlider.setBounds(layout.bounds(54, 404, 49, 46));
    saturationBox.setBounds(layout.bounds(103, 358, 112, 92));
    driveSlider.setBounds(layout.bounds(215, 358, 68, 92));
    driveCharacterSlider.setBounds(layout.bounds(283, 358, 67, 92));
    dynModeBtn.setBounds(layout.bounds(356, 366, 39, 35));
    sidechainBtn.setBounds(layout.bounds(356, 407, 39, 35));
    dynThreshold.setBounds(layout.bounds(401, 358, 62, 92));
    dynRange.setBounds(layout.bounds(463, 358, 68, 92));
    dynRatio.setBounds(layout.bounds(531, 358, 68, 92));
    dynSpeed.setBounds(layout.bounds(599, 358, 75, 92));
    dynLookahead.setBounds(layout.bounds(674, 358, 74, 92));

    for (auto* slider : std::array<juce::Slider*, 4> {
                          &dynRange, &dynSpeed, &driveSlider, &driveCharacterSlider })
        slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
}
