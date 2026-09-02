#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace deq::ui::context_menu
{
constexpr int standardItemHeight = 23;
constexpr int separatorHeight = 5;
constexpr int choiceRowHeight = 34;
constexpr int saturationItemCount = 8;
constexpr int mainMenuWidth = 280;
constexpr int saturationMenuWidth = 190;
constexpr int saturationMenuHeight = saturationItemCount * standardItemHeight;

inline juce::Rectangle<int> mainMenuTarget(juce::Point<int> cursor) noexcept
{
    // Keep this rectangle empty so JUCE chooses the horizontal side exactly as
    // it does for a normal mouse-position popup. withItemThatMustBeVisible(101)
    // then overlays the placement row on this Y position.
    return juce::Rectangle<int>().withPosition(
        cursor.x, cursor.y - choiceRowHeight / 2);
}

struct SubmenuPlacement
{
    juce::Rectangle<int> anchor;
    bool opensLeft = false;
};

inline SubmenuPlacement saturationSubmenuPlacement(juce::Rectangle<int> rowOnScreen,
                                                    juce::Rectangle<int> displayBounds,
                                                    juce::Point<int> originalPointer) noexcept
{
    const int spaceLeft = rowOnScreen.getX() - displayBounds.getX();
    const int spaceRight = displayBounds.getRight() - rowOnScreen.getRight();
    const int distanceToLeftEdge = std::abs(originalPointer.x - rowOnScreen.getX());
    const int distanceToRightEdge = std::abs(originalPointer.x - rowOnScreen.getRight());
    bool opensLeft = distanceToLeftEdge <= distanceToRightEdge;

    // Prefer the edge of the main menu nearest the original click. Only flip
    // away from it when that side cannot contain the submenu.
    if (opensLeft && spaceLeft < saturationMenuWidth && spaceRight > spaceLeft)
        opensLeft = false;
    else if (!opensLeft && spaceRight < saturationMenuWidth && spaceLeft > spaceRight)
        opensLeft = true;

    const int desiredX = opensLeft ? rowOnScreen.getX() - saturationMenuWidth
                                   : rowOnScreen.getRight();
    const int x = juce::jlimit(displayBounds.getX() + 4,
                               displayBounds.getRight() - saturationMenuWidth - 4,
                               desiredX);
    const int y = juce::jlimit(displayBounds.getY() + 4,
                               displayBounds.getBottom() - saturationMenuHeight - 32,
                               rowOnScreen.getY());
    return { { x, y - 1, 1, 1 }, opensLeft };
}
}
