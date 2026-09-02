#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace deq::ui::editor_layout
{
constexpr int minimumWidth = 640;
constexpr int minimumHeight = 400;
constexpr int defaultWidth = 800;
constexpr int defaultHeight = 464;
constexpr int maximumWidth = 2400;
constexpr int maximumHeight = 1600;

struct Metrics
{
    float scale = 1.0f;
    int headerHeight = 64;
    int workspaceHeight = 104;
    int wordmarkWidth = 178;
};

inline juce::Point<int> constrainedSize(int width, int height) noexcept
{
    return { juce::jlimit(minimumWidth, maximumWidth, width),
             juce::jlimit(minimumHeight, maximumHeight, height) };
}

inline float scaleForSize(int width, int height) noexcept
{
    const float widthScale = (float)width / (float)defaultWidth;
    const float heightScale = (float)height / (float)defaultHeight;
    return juce::jlimit(0.74f, 1.5f, juce::jmin(widthScale, heightScale));
}

inline Metrics metricsForSize(int width, int height) noexcept
{
    const float scale = scaleForSize(width, height);
    return { scale,
             juce::roundToInt(64.0f * scale),
             juce::roundToInt(104.0f * scale),
             juce::roundToInt(178.0f * scale) };
}
}
