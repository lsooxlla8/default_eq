#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace deq::ui::editor_layout
{
constexpr int designWidth = 752;
constexpr int designHeight = 454;
constexpr int minimumWidth = 640;
constexpr int minimumHeight = 386;
constexpr int defaultWidth = designWidth;
constexpr int defaultHeight = designHeight;
constexpr int maximumWidth = 2400;
constexpr int maximumHeight = 1449;
constexpr double aspectRatio = (double)designWidth / (double)designHeight;

struct Metrics
{
    float scale = 1.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float geometryScale = 1.0f;
    int editorHeight = designHeight;

    int x(float designX) const noexcept { return juce::roundToInt(designX * geometryScale); }
    int y(float designY) const noexcept { return juce::roundToInt(designY * geometryScale); }
    int width(float designWidthValue) const noexcept
    {
        return juce::roundToInt(designWidthValue * geometryScale);
    }
    int height(float designHeightValue) const noexcept
    {
        return juce::roundToInt(designHeightValue * geometryScale);
    }
    juce::Rectangle<int> bounds(float designX, float designY,
                                float designWidthValue, float designHeightValue) const noexcept
    {
        const int left = x(designX);
        const int top = y(designY);
        return { left, top,
                 x(designX + designWidthValue) - left,
                 y(designY + designHeightValue) - top };
    }
};

inline juce::Point<int> constrainedSize(int width, int height) noexcept
{
    const float requestedScale = juce::jmin((float)width / (float)designWidth,
                                            (float)height / (float)designHeight);
    const float minimumScale = juce::jmax((float)minimumWidth / (float)designWidth,
                                          (float)minimumHeight / (float)designHeight);
    const float maximumScale = juce::jmin((float)maximumWidth / (float)designWidth,
                                          (float)maximumHeight / (float)designHeight);
    const float scale = juce::jlimit(minimumScale, maximumScale, requestedScale);
    return { juce::roundToInt((float)designWidth * scale),
             juce::roundToInt((float)designHeight * scale) };
}

inline float scaleForSize(int width, int height) noexcept
{
    const float widthScale = (float)width / (float)defaultWidth;
    const float heightScale = (float)height / (float)defaultHeight;
    return juce::jmin(widthScale, heightScale);
}

inline Metrics metricsForSize(int width, int height) noexcept
{
    const float scale = scaleForSize(width, height);
    return { scale, scale, scale, scale, height };
}
}
