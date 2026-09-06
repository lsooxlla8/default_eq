#pragma once

#include <juce_graphics/juce_graphics.h>

namespace deq::ui
{
inline void paintFilterIcon(juce::Graphics& g, juce::Rectangle<float> r,
                            int type, juce::Colour colour)
{
    juce::Path path;
    switch (juce::jlimit(0, 9, type))
    {
        case 0:
            path.startNewSubPath(1, 9); path.lineTo(9, 9);
            path.cubicTo(12, 9, 13, 2, 16, 3);
            path.cubicTo(20, 4, 23, 14, 27, 17); break;
        case 1:
            path.startNewSubPath(1, 17); path.cubicTo(5, 14, 8, 4, 12, 3);
            path.cubicTo(15, 2, 16, 9, 19, 9); path.lineTo(27, 9); break;
        case 2:
            path.startNewSubPath(1, 9); path.lineTo(11, 9); path.lineTo(14, 17);
            path.lineTo(17, 9); path.lineTo(27, 9); break;
        case 3:
            path.startNewSubPath(1, 14); path.lineTo(27, 4); break;
        case 4:
            path.startNewSubPath(1, 15); path.cubicTo(7, 15, 8, 3, 14, 3);
            path.cubicTo(20, 3, 21, 15, 27, 15); break;
        case 5:
            path.startNewSubPath(1, 9); path.lineTo(8, 9);
            path.addEllipse(8, 5, 12, 8);
            path.startNewSubPath(20, 9); path.lineTo(27, 9); break;
        case 6:
            path.startNewSubPath(1, 4); path.lineTo(9, 4);
            path.cubicTo(14, 4, 14, 9, 19, 9); path.lineTo(27, 9); break;
        case 7:
            path.startNewSubPath(1, 9); path.lineTo(9, 9);
            path.cubicTo(14, 9, 14, 4, 19, 4); path.lineTo(27, 4); break;
        case 8:
            path.startNewSubPath(1, 9); path.lineTo(12, 9);
            path.cubicTo(19, 9, 21, 8, 27, 17); break;
        case 9:
            path.startNewSubPath(1, 17); path.cubicTo(7, 8, 9, 9, 16, 9);
            path.lineTo(27, 9); break;
    }
    const float viewScale = juce::jmin(r.getWidth() / 28.0f, r.getHeight() / 18.0f);
    const float translateX = r.getCentreX() - 14.0f * viewScale;
    const float translateY = r.getCentreY() - 9.0f * viewScale;
    path.applyTransform(juce::AffineTransform(viewScale, 0.0f, translateX,
                                               0.0f, viewScale, translateY));
    const float uiScale = r.getHeight() / 16.0f;
    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(1.6f * uiScale, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
}

inline int filterTypeForDisplayName(const juce::String& text) noexcept
{
    static constexpr const char* names[] {
        "RES LP", "RES HP", "NOTCH", "TILT", "BAND PASS",
        "BELL", "LOW SHELF", "HIGH SHELF", "LOW PASS", "HIGH PASS"
    };
    for (int type = 0; type < 10; ++type)
        if (text == names[type])
            return type;
    return -1;
}
}
