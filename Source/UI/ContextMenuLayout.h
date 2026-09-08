#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace deq::ui::context_menu
{
constexpr int standardItemHeight = 30;
constexpr int separatorHeight = 5;
constexpr int choiceRowHeight = 34;
constexpr int saturationItemCount = 8;
constexpr int mainMenuWidth = 288;
constexpr int mainMenuHeight = 216;
constexpr int mainMenuExtendedHeight = 257;
constexpr int routeCentreY = 110;
constexpr int saturationMenuWidth = 118;
constexpr int saturationMenuHeight = saturationItemCount * standardItemHeight;

inline juce::Rectangle<int> mainMenuPlacement(juce::Point<int> cursor,
                                              juce::Rectangle<int> displayBounds,
                                              float scale,
                                              bool hasSelectedBypass) noexcept
{
    const int inset = juce::roundToInt(4.0f * scale);
    const int width = juce::roundToInt((float)mainMenuWidth * scale);
    const int height = juce::roundToInt((float)(hasSelectedBypass
        ? mainMenuExtendedHeight : mainMenuHeight) * scale);
    const int rightmostX = juce::jmax(displayBounds.getX() + inset,
                                      displayBounds.getRight() - width - inset);
    const int bottommostY = juce::jmax(displayBounds.getY() + inset,
                                       displayBounds.getBottom() - height - inset);
    return {
        juce::jlimit(displayBounds.getX() + inset, rightmostX, cursor.x),
        juce::jlimit(displayBounds.getY() + inset, bottommostY,
                     cursor.y - juce::roundToInt((float)routeCentreY * scale)),
        width, height
    };
}

struct SubmenuPlacement
{
    juce::Rectangle<int> anchor;
    bool opensLeft = false;
};

inline bool keepsSaturationSubmenuOpen(juce::Rectangle<int> saturationRowOnScreen,
                                      juce::Rectangle<int> submenuOnScreen,
                                      juce::Point<int> pointerOnScreen) noexcept
{
    return saturationRowOnScreen.contains(pointerOnScreen)
        || submenuOnScreen.contains(pointerOnScreen);
}

inline SubmenuPlacement saturationSubmenuPlacement(juce::Rectangle<int> rowOnScreen,
                                                    juce::Rectangle<int> displayBounds,
                                                    juce::Point<int> hoverPointer,
                                                    int menuWidth = saturationMenuWidth,
                                                    int menuHeight = saturationMenuHeight) noexcept
{
    const int distanceToLeftEdge = std::abs(hoverPointer.x - rowOnScreen.getX());
    const int distanceToRightEdge = std::abs(hoverPointer.x - rowOnScreen.getRight());
    const bool opensLeft = distanceToLeftEdge <= distanceToRightEdge;

    const int desiredX = opensLeft ? rowOnScreen.getX() - menuWidth
                                   : rowOnScreen.getRight();
    const int x = juce::jlimit(displayBounds.getX() + 4,
                               displayBounds.getRight() - menuWidth - 4,
                               desiredX);
    const int y = juce::jlimit(displayBounds.getY() + 4,
                               displayBounds.getBottom() - menuHeight - 32,
                               rowOnScreen.getY());
    return { { x, y - 1, 1, 1 }, opensLeft };
}
}
