#include "../Source/PluginEditor.h"
#include "../Source/UI/ContextMenuLayout.h"
#include "../Source/UI/EditorLayout.h"

#include <array>
#include <cstdint>
#include <cstdio>

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL: %s\n", m); ++failures; } } while (0)

std::uint64_t hashValue(std::uint64_t hash, std::uint64_t value) noexcept
{
    hash ^= value;
    return hash * 1099511628211ull;
}

std::uint64_t layoutSignature(const juce::Component& editor)
{
    std::uint64_t hash = 1469598103934665603ull;
    hash = hashValue(hash, (std::uint64_t)editor.getWidth());
    hash = hashValue(hash, (std::uint64_t)editor.getHeight());
    for (int index = 0; index < editor.getNumChildComponents(); ++index)
    {
        const auto* child = editor.getChildComponent(index);
        const auto bounds = child->getBounds();
        hash = hashValue(hash, (std::uint64_t)index);
        hash = hashValue(hash, (std::uint64_t)(std::uint32_t)bounds.getX());
        hash = hashValue(hash, (std::uint64_t)(std::uint32_t)bounds.getY());
        hash = hashValue(hash, (std::uint64_t)(std::uint32_t)bounds.getWidth());
        hash = hashValue(hash, (std::uint64_t)(std::uint32_t)bounds.getHeight());
        hash = hashValue(hash, child->isVisible() ? 1u : 0u);
    }
    return hash;
}

bool renderHasStructure(juce::Component& editor, int dpiScale)
{
    juce::Image image(juce::Image::ARGB, editor.getWidth() * dpiScale,
                      editor.getHeight() * dpiScale, true);
    juce::Graphics graphics(image);
    graphics.addTransform(juce::AffineTransform::scale((float)dpiScale));
    editor.paintEntireComponent(graphics, true);

    std::uint32_t first = 0;
    bool haveFirst = false;
    bool differs = false;
    const int step = juce::jmax(1, 8 * dpiScale);
    for (int y = 0; y < image.getHeight(); y += step)
        for (int x = 0; x < image.getWidth(); x += step)
        {
            const auto colour = image.getPixelAt(x, y).getARGB();
            if (!haveFirst) { first = colour; haveFirst = true; }
            differs = differs || colour != first;
        }
    return haveFirst && differs;
}

bool directChildrenFit(const juce::Component& editor)
{
    const auto bounds = editor.getLocalBounds();
    for (int index = 0; index < editor.getNumChildComponents(); ++index)
    {
        const auto* child = editor.getChildComponent(index);
        if (child->isVisible() && (!bounds.contains(child->getBounds()) || child->getBounds().isEmpty()))
            return false;
    }
    return true;
}

bool directControlsDoNotOverlap(const juce::Component& editor)
{
    for (int first = 0; first < editor.getNumChildComponents(); ++first)
    {
        const auto* a = editor.getChildComponent(first);
        if (!a->isVisible() || dynamic_cast<const juce::ResizableCornerComponent*>(a) != nullptr)
            continue;
        for (int second = first + 1; second < editor.getNumChildComponents(); ++second)
        {
            const auto* b = editor.getChildComponent(second);
            if (!b->isVisible() || dynamic_cast<const juce::ResizableCornerComponent*>(b) != nullptr)
                continue;
            if (!a->getBounds().getIntersection(b->getBounds()).isEmpty())
                return false;
        }
    }
    return true;
}
}

int runEditorLayoutRegression()
{
    using namespace deq::ui;
    const auto minimum = editor_layout::constrainedSize(10, 10);
    const auto maximum = editor_layout::constrainedSize(9999, 9999);
    CHECK(minimum == juce::Point<int>(640, 400), "editor minimum is 640x400");
    CHECK(maximum == juce::Point<int>(2400, 1600), "editor maximum is 2400x1600");
    const auto defaultMetrics = editor_layout::metricsForSize(800, 464);
    CHECK(std::abs(defaultMetrics.scale - 1.0f) < 0.0001f
              && defaultMetrics.headerHeight == 64
              && defaultMetrics.workspaceHeight == 104
              && defaultMetrics.wordmarkWidth == 178,
          "default editor metrics preserve the existing layout");
    CHECK(editor_layout::scaleForSize(1600, 400) < 0.87f
              && editor_layout::scaleForSize(640, 1200) < 0.81f,
          "wide and tall aspect ratios scale from their limiting dimension");

    const juce::Point<int> cursor { 500, 300 };
    const juce::Rectangle<int> display { 0, 0, 1000, 800 };
    const auto mainTarget = context_menu::mainMenuTarget(cursor);
    CHECK(mainTarget.isEmpty()
              && mainTarget.getX() == cursor.x
              && mainTarget.getY() + context_menu::choiceRowHeight / 2 == cursor.y,
          "main menu target places the placement row centre under the cursor");
    const auto opensLeft = context_menu::saturationSubmenuPlacement(
        { 300, 100, 280, 23 }, display, { 300, 100 });
    const auto opensRight = context_menu::saturationSubmenuPlacement(
        { 300, 100, 280, 23 }, display, { 580, 100 });
    const auto edgeFlip = context_menu::saturationSubmenuPlacement(
        { 4, 100, 280, 23 }, display, { 4, 100 });
    const auto bottomClamp = context_menu::saturationSubmenuPlacement(
        { 400, 770, 190, 23 }, display, { 590, 770 });
    CHECK(opensLeft.opensLeft && opensLeft.anchor.getX() == 110,
          "hover saturation submenu opens from the edge nearest a left-side pointer");
    CHECK(!opensRight.opensLeft && opensRight.anchor.getX() == 580,
          "hover saturation submenu opens from the edge nearest a right-side pointer");
    CHECK(!edgeFlip.opensLeft && edgeFlip.anchor.getX() == 284,
          "hover saturation submenu flips away from the screen edge");
    CHECK(bottomClamp.anchor.getBottom() + context_menu::saturationMenuHeight
              <= display.getBottom() - 4,
          "hover saturation submenu stays inside the vertical screen edge");

    juce::PropertiesFile::Options preferenceOptions;
    preferenceOptions.applicationName = "default_eq";
    preferenceOptions.filenameSuffix = "settings";
    preferenceOptions.folderName = "icanseesounds";
    preferenceOptions.osxLibrarySubFolder = "Application Support";
    juce::PropertiesFile originalPreferences(preferenceOptions);
    const bool hadStoredWidth = originalPreferences.containsKey("windowWidth");
    const bool hadStoredHeight = originalPreferences.containsKey("windowHeight");
    const int storedWidth = originalPreferences.getIntValue(
        "windowWidth", editor_layout::defaultWidth);
    const int storedHeight = originalPreferences.getIntValue(
        "windowHeight", editor_layout::defaultHeight);
    const auto originalSize = editor_layout::constrainedSize(storedWidth, storedHeight);
    struct Scenario { int width, height; const char* name; std::uint64_t expectedSignature; };
    constexpr std::array<Scenario, 5> scenarios {{
        { 640, 400, "minimum", 7534618247937379978ull },
        { 800, 464, "default", 7984807521604059066ull },
        { 2400, 1600, "maximum", 12416417462713966329ull },
        { 1600, 400, "wide", 872529952135879279ull },
        { 640, 1200, "tall", 9749822556288630602ull }
    }};
    {
        DefaultEqualizerAudioProcessor processor;
        DefaultEqualizerAudioProcessorEditor editor(processor);
        CHECK(editor.getWidth() == originalSize.x && editor.getHeight() == originalSize.y,
              "editor restores its saved width and height");
        for (const auto& scenario : scenarios)
        {
            editor.setSize(scenario.width, scenario.height);
            CHECK(editor.getWidth() == scenario.width && editor.getHeight() == scenario.height,
                  "editor accepts independent width and height");
            CHECK(directChildrenFit(editor), "all visible editor children remain inside the window");
            CHECK(directControlsDoNotOverlap(editor),
                  "visible editor controls do not overlap at the tested aspect ratio");
            ResponseCurveComponent* graph = nullptr;
            for (int child = 0; child < editor.getNumChildComponents(); ++child)
                if (auto* response = dynamic_cast<ResponseCurveComponent*>(editor.getChildComponent(child)))
                    graph = response;
            CHECK(graph != nullptr && graph->getWidth() >= 600 && graph->getHeight() >= 120,
                  "response graph remains usable at every tested aspect ratio");
            if (graph != nullptr)
            {
                graph->refreshForTimer(false);
                CHECK(!graph->refreshForTimer(false),
                      "unchanged response graph skips duplicate timer repaints");
                CHECK(graph->refreshForTimer(true),
                      "a new spectrum frame requests a response-graph repaint");
            }
#if JUCE_WINDOWS
            // GitHub's headless Windows runner returns a uniform bitmap when a
            // peerless AudioProcessorEditor is painted into a software image.
            // Bounds/signatures remain deterministic here; the following
            // pluginval step exercises the real Windows editor and renderer.
#else
            CHECK(renderHasStructure(editor, 1) && renderHasStructure(editor, 2),
                  "editor renders structured 1x and 2x screenshots");
#endif
            const auto signature = layoutSignature(editor);
            CHECK(signature == scenario.expectedSignature,
                  "editor child bounds match the approved layout snapshot");
            std::printf("layout_snapshot_%s=%llu\n", scenario.name,
                        (unsigned long long)signature);
        }
        editor.setSize(913, 577);
    }
    {
        DefaultEqualizerAudioProcessor processor;
        DefaultEqualizerAudioProcessorEditor restored(processor);
        CHECK(restored.getWidth() == 913 && restored.getHeight() == 577,
              "a newly opened editor restores the previous editor size");
        restored.setSize(originalSize.x, originalSize.y);
    }
    juce::PropertiesFile restoredPreferences(preferenceOptions);
    if (hadStoredWidth) restoredPreferences.setValue("windowWidth", storedWidth);
    else                restoredPreferences.removeValue("windowWidth");
    if (hadStoredHeight) restoredPreferences.setValue("windowHeight", storedHeight);
    else                 restoredPreferences.removeValue("windowHeight");
    restoredPreferences.saveIfNeeded();

    std::printf(failures == 0 ? "EDITOR LAYOUT REGRESSION PASSED\n"
                              : "%d EDITOR LAYOUT REGRESSION FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
