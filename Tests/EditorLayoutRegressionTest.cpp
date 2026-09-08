#include "../Source/PluginEditor.h"
#include "../Source/UI/ContextMenuLayout.h"
#include "../Source/UI/EditorLayout.h"

#include <array>
#include <cmath>
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

bool componentRenderHasStructure(juce::Component& component)
{
    if (component.getWidth() <= 0 || component.getHeight() <= 0)
        return false;
    juce::Image image(juce::Image::ARGB, component.getWidth() * 2,
                      component.getHeight() * 2, true);
    juce::Graphics graphics(image);
    graphics.addTransform(juce::AffineTransform::scale(2.0f));
    component.paintEntireComponent(graphics, true);
    const auto first = image.getPixelAt(0, 0).getARGB();
    for (int y = 0; y < image.getHeight(); y += 2)
        for (int x = 0; x < image.getWidth(); x += 2)
            if (image.getPixelAt(x, y).getARGB() != first)
                return true;
    return false;
}

std::uint64_t comboRenderHash(default_family::LookAndFeel& look,
                              juce::ComboBox& combo, bool open)
{
    juce::Image image(juce::Image::ARGB, juce::jmax(1, combo.getWidth()),
                      juce::jmax(1, combo.getHeight()), true);
    juce::Graphics graphics(image);
    look.drawComboBox(graphics, image.getWidth(), image.getHeight(), open,
                      0, 0, 0, 0, combo);
    std::uint64_t hash = 1469598103934665603ull;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
            hash = hashValue(hash, image.getPixelAt(x, y).getARGB());
    return hash;
}

bool regionsContainPaperAndInk(juce::Component& editor,
                               std::initializer_list<juce::Rectangle<int>> regions)
{
    constexpr int scale = 2;
    juce::Image image(juce::Image::ARGB, editor.getWidth() * scale,
                      editor.getHeight() * scale, true);
    juce::Graphics graphics(image);
    graphics.addTransform(juce::AffineTransform::scale((float)scale));
    editor.paintEntireComponent(graphics, true);
    for (auto region : regions)
    {
        region *= scale;
        int paperPixels = 0, inkPixels = 0;
        for (int y = region.getY(); y < region.getBottom(); ++y)
            for (int x = region.getX(); x < region.getRight(); ++x)
            {
                const float brightness = image.getPixelAt(x, y).getPerceivedBrightness();
                paperPixels += brightness > 0.82f ? 1 : 0;
                inkPixels += brightness < 0.12f ? 1 : 0;
            }
        if (paperPixels < 12 || inkPixels < 12)
            return false;
    }
    return true;
}

bool writeRender(juce::Component& editor, int dpiScale, const juce::File& destination)
{
    juce::Image image(juce::Image::ARGB, editor.getWidth() * dpiScale,
                      editor.getHeight() * dpiScale, true);
    juce::Graphics graphics(image);
    graphics.addTransform(juce::AffineTransform::scale((float)dpiScale));
    editor.paintEntireComponent(graphics, true);
    if (auto stream = destination.createOutputStream())
    {
        if (!stream->setPosition(0) || stream->truncate().failed())
            return false;
        return juce::PNGImageFormat().writeImageToStream(image, *stream);
    }
    return false;
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

juce::ComboBox* findDirectCombo(juce::Component& editor, const juce::String& name)
{
    for (int index = 0; index < editor.getNumChildComponents(); ++index)
        if (auto* combo = dynamic_cast<juce::ComboBox*>(editor.getChildComponent(index)))
            if (combo->getName() == name)
                return combo;
    return nullptr;
}
}

int runEditorLayoutRegression()
{
    using namespace deq::ui;
    const juce::Rectangle<int> display { 0, 0, 1000, 800 };
    const auto opensLeft = context_menu::saturationSubmenuPlacement(
        { 300, 100, 280, 23 }, display, { 300, 100 });
    const auto opensRight = context_menu::saturationSubmenuPlacement(
        { 300, 100, 280, 23 }, display, { 580, 100 });
    const auto bottomClamp = context_menu::saturationSubmenuPlacement(
        { 400, 770, 190, 23 }, display, { 590, 770 });
    CHECK(opensLeft.opensLeft && opensLeft.anchor.getX() == 182,
          "saturation popup opens from the edge nearest a left-side hover pointer");
    CHECK(!opensRight.opensLeft && opensRight.anchor.getX() == 580,
          "saturation popup opens from the edge nearest a right-side hover pointer");
    const auto leftBandHover = context_menu::saturationSubmenuPlacement(
        { 80, 100, 280, 23 }, display, { 90, 100 });
    CHECK(leftBandHover.opensLeft,
          "a left-band saturation popup does not override the live hover side");
    CHECK(bottomClamp.anchor.getBottom() + context_menu::saturationMenuHeight
              <= display.getBottom() - 4,
          "saturation popup remains fully visible at the bottom screen edge");
    const auto mainBounds = context_menu::mainMenuPlacement(
        { 500, 300 }, display, 1.0f, true);
    CHECK(mainBounds.getX() == 500
              && mainBounds.getY() + context_menu::routeCentreY == 300,
          "main popup anchors the L route cell immediately to the right of the pointer");
    const juce::Rectangle<int> saturationRow { 300, 100, 288, 30 };
    const juce::Rectangle<int> saturationMenu { 182, 100, 118, 240 };
    CHECK(context_menu::keepsSaturationSubmenuOpen(
              saturationRow, saturationMenu, { 400, 115 })
              && context_menu::keepsSaturationSubmenuOpen(
                  saturationRow, saturationMenu, { 200, 115 })
              && !context_menu::keepsSaturationSubmenuOpen(
                  saturationRow, saturationMenu, { 700, 115 }),
          "saturation popup remains only over its trigger row or its own window");
    CHECK(context_menu::saturationMenuWidth == 118,
          "saturation popup keeps the compact prototype width");
    const auto minimum = editor_layout::constrainedSize(10, 10);
    const auto maximum = editor_layout::constrainedSize(9999, 9999);
    CHECK(minimum == juce::Point<int>(640, 386), "editor minimum preserves 752:454");
    CHECK(maximum == juce::Point<int>(2400, 1449), "editor maximum preserves 752:454");
    const auto defaultMetrics = editor_layout::metricsForSize(752, 454);
    CHECK(std::abs(defaultMetrics.scale - 1.0f) < 0.0001f
              && std::abs(defaultMetrics.scaleX - 1.0f) < 0.0001f
              && std::abs(defaultMetrics.scaleY - 1.0f) < 0.0001f
              && defaultMetrics.bounds(4, 68, 744, 254)
                    == juce::Rectangle<int>(4, 68, 744, 254),
          "default editor metrics preserve the approved 752x454 design space");
    CHECK(std::abs((double)minimum.x / (double)minimum.y - editor_layout::aspectRatio) < 0.003
              && std::abs((double)maximum.x / (double)maximum.y
                          - editor_layout::aspectRatio) < 0.003,
          "all constrained editor sizes retain the prototype aspect ratio");

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
    constexpr std::array<Scenario, 4> scenarios {{
        { 640, 386, "minimum", 14945770963110066184ull },
        { 752, 454, "default", 16590461077126606349ull },
        { 1504, 908, "large_2x", 16673281797170569122ull },
        { 2256, 1362, "large_3x", 7151623727713594423ull }
    }};
    CHECK(default_family::ThemePreferences::isDarkForHour(
              default_family::ThemePreferences::automatic, 7)
              && !default_family::ThemePreferences::isDarkForHour(
                  default_family::ThemePreferences::automatic, 8)
              && !default_family::ThemePreferences::isDarkForHour(
                  default_family::ThemePreferences::automatic, 19)
              && default_family::ThemePreferences::isDarkForHour(
                  default_family::ThemePreferences::automatic, 20),
          "AUTO theme switches exactly at 08:00 and 20:00 local time");
    CHECK(SettingsOverlay::State{}.themeMode
              == default_family::ThemePreferences::automatic,
          "the settings panel factory theme is AUTO");
    CHECK(SettingsOverlay::gainRangeText(1)
              == juce::String::fromUTF8("\xc2\xb1") + "6 dB"
              && !SettingsOverlay::gainRangeText(1).contains(
                  juce::String::fromUTF8("\xc3\x82")),
          "gain range labels preserve the UTF-8 plus-minus sign");
    {
        SettingsOverlay overlay;
        overlay.setSize(744, 254);
        CHECK(overlay.cellAt({ 90, 60 }) == SettingsOverlay::Cell::ceiling
                  && overlay.cellAt({ 270, 60 }) == SettingsOverlay::Cell::floor
                  && overlay.cellAt({ 450, 60 }) == SettingsOverlay::Cell::average
                  && overlay.cellAt({ 650, 60 }) == SettingsOverlay::Cell::slope,
              "RTA ceiling precedes floor, average and slope in the settings row");
        overlay.nudge(SettingsOverlay::Cell::ceiling, -6.0f);
        CHECK(overlay.getState().rtaCeilingDb == -12.0f, "ceiling supports negative values");
        overlay.nudge(SettingsOverlay::Cell::ceiling, -1000.0f);
        CHECK(overlay.getState().rtaCeilingDb == -24.0f
                  && overlay.getState().rtaCeilingDb > -30.0f,
              "ceiling minimum cannot cross the floor maximum");
        overlay.nudge(SettingsOverlay::Cell::ceiling, 1000.0f);
        CHECK(overlay.getState().rtaCeilingDb == 24.0f, "ceiling has a bounded positive range");
        const auto now = juce::Time::getCurrentTime();
        juce::MouseEvent reset(juce::Desktop::getInstance().getMainMouseSource(), { 90, 60 },
            juce::ModifierKeys(juce::ModifierKeys::rightButtonModifier),
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &overlay, &overlay, now, { 90, 60 }, now, 1, false);
        overlay.mouseDown(reset);
        CHECK(overlay.getState().rtaCeilingDb == 0.0f, "right-click resets ceiling to zero");
    }
    {
        DefaultEqualizerAudioProcessor processor;
        ResponseCurveComponent curve(processor);
        curve.setSize(744, 254);
        curve.setGainRangeMode(2);
        const auto mouse = [&curve](juce::Point<float> point, juce::Point<float> start,
                                    int modifiers, bool dragged)
        {
            const auto now = juce::Time::getCurrentTime();
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point,
                juce::ModifierKeys(modifiers), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                &curve, &curve, now, start, now, 1, dragged);
        };
        const int shiftLeft = juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier;
        for (const float frequency : { 1000.0f, 40.0f, 12000.0f })
        {
            for (int band = 0; band < kNumBands; ++band)
                processor.resetBandToDefaults(band, false);
            curve.setSelectedBand(-1);
            processor.apvts.copyState();
            processor.undoManager.clearUndoHistory();
            const auto start = juce::Point<float>(curve.freqToX(frequency), curve.dbToY(-3.0f));
            curve.mouseDown(mouse(start, start, shiftLeft, false));
            const int band = curve.getSelectedBand();
            CHECK(band >= 0 && curve.dragging && !curve.marqueePending,
                  "Shift creates and grabs the alternate filter on mouse-down");
            if (band < 0) continue;
            const auto prefix = "b" + juce::String(band + 1) + "_";
            const int type = (int)processor.apvts.getRawParameterValue(prefix + "type")->load();
            CHECK(type == ResponseCurveComponent::shiftClickTypeForNewBand(frequency, -3.0f),
                  "Shift creates Tilt and both resonant Cut types in the correct regions");
            const float startFrequency = processor.apvts.getRawParameterValue(prefix + "freq")->load();
            const float startGain = processor.apvts.getRawParameterValue(prefix + "gain")->load();
            const float startQ = processor.apvts.getRawParameterValue(prefix + "q")->load();
            const float threshold = processor.apvts.getRawParameterValue(prefix + "dyn_thresh")->load();
            const auto end = start + juce::Point<float>(18.0f, -20.0f);
            curve.mouseDrag(mouse(end, start, shiftLeft, true));
            CHECK(processor.apvts.getRawParameterValue(prefix + "freq")->load() > startFrequency,
                  "new Shift filter moves horizontally without releasing the mouse");
            CHECK(ResponseCurveComponent::usesQVerticalDrag(type)
                      ? processor.apvts.getRawParameterValue(prefix + "q")->load() > startQ
                        && processor.apvts.getRawParameterValue(prefix + "gain")->load() == startGain
                      : processor.apvts.getRawParameterValue(prefix + "gain")->load() > startGain,
                  "new Tilt drags gain while resonant Cuts drag Q");
            CHECK(processor.apvts.getRawParameterValue(prefix + "dyn_thresh")->load() == threshold,
                  "held Shift during creation does not switch into threshold editing");
            curve.mouseUp(mouse(end, start, 0, true));
            CHECK(!curve.dragging && curve.dragFreqParams[(size_t)band] == nullptr
                      && curve.dragGainParams[(size_t)band] == nullptr
                      && curve.dragQParams[(size_t)band] == nullptr,
                  "release ends the new filter parameter gestures");
            processor.apvts.copyState();
            processor.undoManager.beginNewTransaction();
            CHECK(processor.undoManager.undo()
                      && processor.apvts.getRawParameterValue(prefix + "present")->load() < 0.5f,
                  "one Undo removes both Shift creation and its immediate drag");
        }
        const auto start = juce::Point<float>(200, 30);
        curve.mouseDown(mouse(start, start, juce::ModifierKeys::rightButtonModifier, false));
        CHECK(curve.marqueePending, "right-drag still starts marquee selection");
        curve.mouseDrag(mouse({ 500, 200 }, start, juce::ModifierKeys::rightButtonModifier, true));
        CHECK(curve.marqueeDragging, "right-drag still expands marquee selection");
        curve.mouseUp(mouse({ 500, 200 }, start, 0, true));
    }
    {
        DefaultEqualizerAudioProcessor processor;
        ResponseCurveComponent curve(processor);
        constexpr int bins = 2048;
        std::array<float, bins> spectrum;
        spectrum.fill(-40.0f);
        curve.setAnalyzerSettings(-80.0f, 0.0f, 0.0f, -12.0f);
        curve.pushSpectrumData(spectrum.data(), bins, 48000.0, false);
        curve.refreshForTimer(true);
        const auto flat = curve.calculateSpectralStatistics();
        CHECK(flat.valid && flat.tonalPercent[9] > flat.tonalPercent[0],
              "Tonal Balance retains the original relative power distribution");
        curve.setAnalyzerSettings(-100.0f, 0.0f, 3.0f, 12.0f);
        curve.refreshForTimer(true);
        const auto tilted = curve.calculateSpectralStatistics();
        CHECK(tilted.tonalPercent == flat.tonalPercent,
              "RTA slope, floor and ceiling do not change Tonal Balance");
        CHECK(std::abs(tilted.centroidHz - flat.centroidHz) < 0.01f
                  && std::abs(tilted.averageTiltDbPerOct - flat.averageTiltDbPerOct) < 0.001f,
              "display slope does not alter measured centroid or spectral tilt");
        CHECK(curve.analyzerCeilingDb == 12.0f && curve.analyzerFloorDb == -100.0f
                  && ResponseCurveComponent::analyzerLevelToY(12.0f, -100.0f, 112.0f, 200.0f) == 0.0f
                  && ResponseCurveComponent::analyzerLevelToY(-100.0f, -100.0f, 112.0f, 200.0f) == 200.0f,
              "shared analyzer mapping places ceiling at top and floor at bottom");
        curve.setAnalyzerSettings(-100.0f, 1.0f, 3.0f);
        for (int bin = 1; bin < bins; ++bin)
            spectrum[(size_t)bin] = -60.0f - 3.0f * std::log2(bin * (24000.0f / bins) / 1000.0f);
        curve.pushSpectrumData(spectrum.data(), bins, 48000.0, false);
        const auto target = curve.calculateRawSpectralStatistics();
        curve.refreshForTimer(true);
        const auto averaged = curve.calculateSpectralStatistics();
        const auto between = [](float value, float first, float last)
        { return value > std::min(first, last) && value < std::max(first, last); };
        CHECK(between(averaged.centroidHz, tilted.centroidHz, target.centroidHz)
                  && between(averaged.averageTiltDbPerOct, tilted.averageTiltDbPerOct, target.averageTiltDbPerOct)
                  && between(averaged.tonalPercent[9], tilted.tonalPercent[9], target.tonalPercent[9]),
              "RTA average smooths centroid, spectral tilt and Tonal Balance");
        curve.setAnalyzerSettings(-100.0f, 0.0f, 3.0f);
        curve.refreshForTimer(true);
        const auto immediate = curve.calculateSpectralStatistics();
        CHECK(std::abs(immediate.centroidHz - target.centroidHz) < 0.01f
                  && std::abs(immediate.averageTiltDbPerOct - target.averageTiltDbPerOct) < 0.001f
                  && std::abs(immediate.tonalPercent[9] - target.tonalPercent[9]) < 0.001f,
              "zero RTA average removes spectral statistics smoothing");
        spectrum.fill(-110.0f);
        curve.pushSpectrumData(spectrum.data(), bins, 48000.0, false);
        curve.refreshForTimer(true);
        CHECK(!curve.calculateSpectralStatistics().valid,
              "spectral statistics preserve their original silence threshold");
        spectrum.fill(-140.0f);
        curve.pushSpectrumData(spectrum.data(), bins, 48000.0, false);
        curve.refreshForTimer(true);
        CHECK(!curve.calculateSpectralStatistics().valid, "silence clears spectral statistics");
    }
    {
        DefaultEqualizerAudioProcessor processor;
        processor.prepareToPlay(48000.0, 256);
        processor.setAnalyzerEnabled(true);
        processor.uiStatisticsAveragingSeconds.store(1.0f);
        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;
        const auto impulse = [&]
        {
            buffer.clear();
            buffer.setSample(0, 0, 0.5f);
            buffer.setSample(1, 0, 0.5f);
            processor.processBlock(buffer, midi);
        };
        impulse();
        const float slowCrest = processor.uiOutputCrestDb.load();
        const float slowCorrelation = processor.uiOutputCorrelation.load();
        CHECK(slowCrest > 0.0f && slowCrest < 1.0f
                  && slowCorrelation > 0.0f && slowCorrelation < 0.01f,
              "RTA average slows both crest factor and correlation measurements");
        processor.uiStatisticsAveragingSeconds.store(0.0f);
        impulse();
        CHECK(processor.uiOutputCrestDb.load() > 20.0f
                  && std::abs(processor.uiOutputCorrelation.load() - 1.0f) < 0.001f,
              "zero RTA average makes crest factor and correlation immediate");
        processor.releaseResources();
    }
    {
        DefaultEqualizerAudioProcessor menuProcessor;
        DefaultEqualizerAudioProcessorEditor menuEditor(menuProcessor);
        menuEditor.setSize(editor_layout::designWidth, editor_layout::designHeight);
        menuEditor.timerCallback();
        menuEditor.darkTheme = false;
        menuEditor.familyLook.setDark(false);
        menuEditor.responseCurve.setDarkMode(false);
        menuEditor.applySliderPalette();
        menuEditor.sendLookAndFeelChange();
        menuEditor.toggleSelectMenu(menuEditor.typeBox);
        CHECK(menuEditor.selectMenu.isVisible()
                  && menuEditor.selectMenu.getBounds()
                      == juce::Rectangle<int>(103, 98, 112, 228),
              "filter selector includes the prototype 1px border, 3px padding and 22px rows");
        const auto menuSnapshotDirectory = juce::SystemStats::getEnvironmentVariable(
            "DEFAULT_EQ_MENU_SNAPSHOT_DIR", {});
        if (menuSnapshotDirectory.isNotEmpty())
            CHECK(writeRender(menuEditor, 2, juce::File(menuSnapshotDirectory)
                .getChildFile("filter-menu.png")),
                "optional filter-menu snapshot is written");
        menuEditor.toggleSelectMenu(menuEditor.typeBox);
        menuEditor.toggleSelectMenu(menuEditor.oversamplingBox);
        CHECK(menuEditor.selectMenu.isVisible()
                  && menuEditor.selectMenu.getBounds()
                      == juce::Rectangle<int>(178, 64, 74, 96),
              "header selector opens below the field with prototype geometry");
        if (menuSnapshotDirectory.isNotEmpty())
            CHECK(writeRender(menuEditor, 2, juce::File(menuSnapshotDirectory)
                .getChildFile("header-menu.png")),
                "optional header-menu snapshot is written");
        menuEditor.hideSelectMenu();
        CHECK(menuEditor.familyLook.getPopupMenuBorderSize() == 0
                  && menuEditor.familyLook.getMenuWindowFlags() == 0,
              "context popup has neither an inset frame nor a native window shadow");
        menuEditor.setSize(originalSize.x, originalSize.y);
    }
    {
        DefaultEqualizerAudioProcessor contextProcessor;
        DefaultEqualizerAudioProcessorEditor contextEditor(contextProcessor);
        const auto setContextParameter = [&contextProcessor](const juce::String& id, float value)
        {
            if (auto* parameter = contextProcessor.apvts.getParameter(id))
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        };
        setContextParameter("b1_present", 1.0f);
        setContextParameter("b1_on", 1.0f);
        setContextParameter("b1_freq", 1000.0f);
        setContextParameter("b1_gain", 0.0f);
        contextEditor.responseCurve.refreshForTimer(false);
        bool contextRequested = false;
        contextEditor.responseCurve.onContextMenuRequest =
            [&contextRequested](juce::Point<int>, PrototypeContextMenuModel model)
            {
                contextRequested = model.bandNumber == 1
                    && model.selectedCount == 1
                    && (bool)model.toggleBand
                    && (bool)model.chooseFilter
                    && (bool)model.chooseRoute
                    && (bool)model.chooseSaturation
                    && (bool)model.resetEqualizer;
            };
        const auto position = juce::Point<float>(
            contextEditor.responseCurve.freqToX(1000.0f),
            contextEditor.responseCurve.dbToY(0.0f));
        const auto now = juce::Time::getCurrentTime();
        juce::MouseEvent rightClick(
            juce::Desktop::getInstance().getMainMouseSource(), position,
            juce::ModifierKeys(juce::ModifierKeys::rightButtonModifier),
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            &contextEditor.responseCurve, &contextEditor.responseCurve,
            now, position, now, 1, false);
        contextEditor.responseCurve.mouseDown(rightClick);
        CHECK(contextRequested,
              "right-clicking the centre of a present band requests the full context menu");
        contextEditor.setSize(originalSize.x, originalSize.y);
    }
    {
        DefaultEqualizerAudioProcessor processor;
        DefaultEqualizerAudioProcessorEditor editor(processor);
        auto* osSelector = findDirectCombo(editor, "OS");
        auto* phaseSelector = findDirectCombo(editor, "PHASE");
        CHECK(osSelector != nullptr && osSelector->getText() == "OFF",
              "oversampling selector is initialised from the host parameter");
        CHECK(osSelector != nullptr
                  && osSelector->getItemText(1) == juce::String::fromUTF8("2\xc3\x97")
                  && osSelector->getItemText(2) == juce::String::fromUTF8("4\xc3\x97")
                  && osSelector->getItemText(3) == juce::String::fromUTF8("8\xc3\x97"),
              "oversampling selector stores literal UTF-8 multiplication signs");
        CHECK(osSelector != nullptr && !osSelector->getMouseClickGrabsKeyboardFocus(),
              "pointer-opened selectors do not retain a prototype-incompatible focus ring");
        CHECK(phaseSelector != nullptr && phaseSelector->getText() == "MINIMUM",
              "phase selector is initialised before the first timer tick");
        CHECK(editor.responseCurve.getSelectionCount() == 0
                  && !editor.freqField.isEnabled() && !editor.dynThreshold.isEnabled(),
              "zero-selection state disables band-specific controls");
        CHECK(!editor.settingsOverlay.isVisible()
                  && editor.settingsOverlay.getBounds() == editor.responseCurve.getBounds(),
              "settings overlay starts hidden and exactly covers the RTA rectangle");
        auto setRangeParameter = [&processor](const juce::String& id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter(id))
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        };
        setRangeParameter("b1_present", 1.0f);
        setRangeParameter("b1_gain", 13.0f);
        editor.responseCurve.resetAutoRtaRangeForOpen();
        const auto originalAnalyzerSettings = editor.settingsOverlay.getState();
        auto analyzerSettings = originalAnalyzerSettings;
        analyzerSettings.rtaCeilingDb = -12.0f;
        analyzerSettings.rtaFloorDb = -100.0f;
        analyzerSettings.rtaAverageSeconds = 0.5f;
        analyzerSettings.rtaSlopeDbPerOct = 3.0f;
        editor.applySettings(analyzerSettings, false);
        CHECK(editor.responseCurve.analyzerCeilingDb == -12.0f
                  && editor.responseCurve.analyzerFloorDb == -100.0f
                  && editor.responseCurve.analyzerAveragingSeconds == 0.5f
                  && editor.responseCurve.analyzerTiltDbPerOct == 3.0f
                  && processor.uiStatisticsAveragingSeconds.load() == 0.5f,
              "one settings change reaches both spectrum and audio statistics averaging");
        editor.applySettings(originalAnalyzerSettings, false);
        auto paletteState = editor.settingsOverlay.getState();
        paletteState.themeMode = default_family::ThemePreferences::white;
        paletteState.lightBackground = juce::Colour(0xffdcf3d2);
        paletteState.lightForeground = juce::Colour(0xff59131c);
        editor.applySettings(paletteState, false);
        CHECK(editor.familyLook.background() == paletteState.lightBackground
                  && editor.familyLook.foreground() == paletteState.lightForeground
                  && editor.responseCurve.backgroundColour() == paletteState.lightBackground
                  && editor.responseCurve.foregroundColour() == paletteState.lightForeground,
              "custom theme colours apply consistently to controls and the RTA");
        paletteState.lightBackground = juce::Colour(0xfff6f6f6);
        paletteState.lightForeground = juce::Colour(0xff050505);
        editor.applySettings(paletteState, false);
        CHECK(editor.responseCurve.getDisplayMaxDb() == 24.0f,
              "AUTO range opens wide enough for restored band gains");
        setRangeParameter("b1_gain", 3.0f);
        editor.responseCurve.updateResponseCurve();
        CHECK(editor.responseCurve.getDisplayMaxDb() == 24.0f,
              "AUTO range does not shrink while the editor stays open");
        editor.responseCurve.resetAutoRtaRangeForOpen();
        CHECK(editor.responseCurve.getDisplayMaxDb() == 6.0f,
              "a new editor session recomputes the smallest AUTO range");
        CHECK(editor.responseCurve.expandAutoRtaRangeNearEdge(5)
                  && editor.responseCurve.getDisplayMaxDb() == 12.0f,
              "dragging near an RTA edge expands AUTO range before leaving the window");
        CHECK(!editor.responseCurve.expandAutoRtaRangeNearEdge(5)
                  && editor.responseCurve.getDisplayMaxDb() == 12.0f,
              "remaining at the edge cannot skip an AUTO range level");
        CHECK(!editor.responseCurve.expandAutoRtaRangeNearEdge(editor.responseCurve.getHeight() / 2),
              "moving away from the edge rearms AUTO range expansion");
        CHECK(editor.responseCurve.expandAutoRtaRangeNearEdge(editor.responseCurve.getHeight() - 5)
                  && editor.responseCurve.getDisplayMaxDb() == 24.0f,
              "the same drag expands again after leaving and re-entering the edge zone");
        CHECK(!editor.responseCurve.expandAutoRtaRangeNearEdge(editor.responseCurve.getHeight() / 2)
                  && editor.responseCurve.expandAutoRtaRangeNearEdge(5)
                  && editor.responseCurve.getDisplayMaxDb() == 36.0f,
              "AUTO range can reach the final level without releasing the band");
        editor.responseCurve.setGainRangeMode(1);
        CHECK(editor.responseCurve.getDisplayMaxDb() == 6.0f
                  && !editor.responseCurve.expandAutoRtaRangeNearEdge(0),
              "manual gain ranges stay fixed at their requested value");
        editor.responseCurve.setGainRangeMode(0);
        CHECK(editor.responseCurve.expandAutoRtaRangeNearEdge(5)
                  && editor.responseCurve.getDisplayMaxDb() == 12.0f,
              "AUTO RTA range can expand before opening settings");
        editor.settingsOverlay.setVisible(true);
        editor.hideSettingsOverlay();
        CHECK(!editor.settingsOverlay.isVisible()
                  && editor.responseCurve.getDisplayMaxDb() == 6.0f,
              "closing settings resets AUTO RTA range like reopening the editor");
        CHECK(editor.responseCurve.expandAutoRtaRangeNearEdge(5)
                  && editor.responseCurve.getDisplayMaxDb() == 12.0f,
              "AUTO RTA range remains expandable after closing settings");
        editor.hideSettingsOverlay();
        CHECK(editor.responseCurve.getDisplayMaxDb() == 12.0f,
              "closing an unrelated overlay cannot reset AUTO RTA range");
        editor.responseCurve.resetAutoRtaRangeForOpen();
        default_family::WordmarkButton* wordmark = nullptr;
        juce::Button* autoGain = nullptr;
        juce::Button* power = nullptr;
        NumericValueControl* amount = nullptr;
        for (int child = 0; child < editor.getNumChildComponents(); ++child)
        {
            auto* component = editor.getChildComponent(child);
            if (auto* candidate = dynamic_cast<default_family::WordmarkButton*>(component))
                wordmark = candidate;
            if (auto* candidate = dynamic_cast<juce::Button*>(component))
            {
                const auto headerLabel = candidate->getProperties()
                    .getWithDefault("headerLabel", {}).toString();
                if (headerLabel == "AUTO GAIN") autoGain = candidate;
                if (headerLabel == "POWER") power = candidate;
            }
            if (auto* candidate = dynamic_cast<NumericValueControl*>(component))
                if (candidate->getName() == "AMOUNT") amount = candidate;
        }
        CHECK(editor.getWidth() == originalSize.x && editor.getHeight() == originalSize.y,
              "editor restores its saved width and height");
        for (const auto& scenario : scenarios)
        {
            editor.setSize(scenario.width, scenario.height);
            CHECK(editor.getWidth() == scenario.width && editor.getHeight() == scenario.height,
                  "editor accepts a proportional prototype-sized window");
            CHECK(directChildrenFit(editor), "all visible editor children remain inside the window");
            CHECK(directControlsDoNotOverlap(editor),
                  "visible editor controls do not overlap at the tested aspect ratio");
            const auto scenarioMetrics = editor_layout::metricsForSize(
                scenario.width, scenario.height);
            CHECK(wordmark != nullptr && wordmark->getHeight()
                      == scenarioMetrics.bounds(4, 4, 174, 60).getHeight(),
                  "header controls retain uniform geometry at every aspect ratio");
#if !JUCE_WINDOWS
            if (scenario.width == editor_layout::designWidth
                && scenario.height == editor_layout::designHeight)
            {
                CHECK(wordmark != nullptr && componentRenderHasStructure(*wordmark),
                      "wordmark renders visible structure");
                CHECK(autoGain != nullptr && componentRenderHasStructure(*autoGain),
                      "auto gain action renders visible structure");
                CHECK(power != nullptr && componentRenderHasStructure(*power),
                      "power action renders visible structure");
                CHECK(amount != nullptr && componentRenderHasStructure(*amount),
                      "amount value cell renders visible structure");
                CHECK(editor.typeBox.getWidth() > 0
                          && comboRenderHash(editor.familyLook, editor.typeBox, false)
                              != comboRenderHash(editor.familyLook, editor.typeBox, true),
                      "selector open state is visually distinct from its closed state");
                CHECK(regionsContainPaperAndInk(editor, {
                          { 4, 4, 174, 60 }, { 351, 4, 112, 60 },
                          { 599, 4, 75, 60 }, { 674, 4, 74, 60 } }),
                      "header cells retain paper/ink contrast in the combined render");
            }
#endif
            ResponseCurveComponent* graph = nullptr;
            for (int child = 0; child < editor.getNumChildComponents(); ++child)
                if (auto* response = dynamic_cast<ResponseCurveComponent*>(editor.getChildComponent(child)))
                    graph = response;
            CHECK(graph != nullptr && graph->getWidth() >= 600 && graph->getHeight() >= 120,
                  "response graph remains usable at every tested aspect ratio");
            if (graph != nullptr)
            {
                if (scenario.width == editor_layout::designWidth
                    && scenario.height == editor_layout::designHeight)
                {
                    CHECK(graph->getBounds() == juce::Rectangle<int>(4, 68, 744, 254),
                          "default response graph matches the approved design rectangle");
                    CHECK(editor.placementModeBox.getBounds()
                              == juce::Rectangle<int>(54, 358, 49, 46)
                              && editor.placementSlider.getBounds()
                              == juce::Rectangle<int>(54, 404, 49, 46),
                          "routing selector and position split the prototype cell 46/46");
                    CHECK(editor.bandOn.getBounds()
                              == juce::Rectangle<int>(9, 366, 39, 35)
                              && editor.bandSolo.getBounds()
                              == juce::Rectangle<int>(9, 407, 39, 35)
                              && editor.dynModeBtn.getBounds()
                              == juce::Rectangle<int>(356, 366, 39, 35)
                              && editor.sidechainBtn.getBounds()
                              == juce::Rectangle<int>(356, 407, 39, 35),
                          "workspace action buttons match the prototype's visible 39 px boxes");
                    CHECK(editor.driveSlider.getBounds()
                              == juce::Rectangle<int>(215, 358, 68, 92)
                              && editor.driveCharacterSlider.getBounds()
                              == juce::Rectangle<int>(283, 358, 67, 92),
                          "drive and character split the prototype cell at x=283");
                }
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
            const auto snapshotPath = juce::SystemStats::getEnvironmentVariable(
                "DEFAULT_EQ_LAYOUT_SNAPSHOT", {});
            if (snapshotPath.isNotEmpty() && scenario.width == editor_layout::designWidth
                && scenario.height == editor_layout::designHeight)
                CHECK(writeRender(editor, 2, juce::File(snapshotPath)),
                      "optional default layout snapshot is written");
            const auto layoutSnapshotDirectory = juce::SystemStats::getEnvironmentVariable(
                "DEFAULT_EQ_LAYOUT_SNAPSHOT_DIR", {});
            if (layoutSnapshotDirectory.isNotEmpty())
            {
                const juce::File directory(layoutSnapshotDirectory);
                CHECK(directory.createDirectory().wasOk(),
                      "optional layout snapshot directory is available");
                for (const int dpiScale : { 1, 2 })
                    CHECK(writeRender(editor, dpiScale, directory.getChildFile(
                              "layout-" + juce::String(scenario.name) + "-"
                                  + juce::String(dpiScale) + "x.png")),
                          "optional sized layout snapshot is written");
            }
        }

        const auto setParameter = [&processor](const juce::String& id, float value)
        {
            if (auto* parameter = processor.apvts.getParameter(id))
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        };
        setParameter("b1_present", 1.0f);
        setParameter("b2_present", 1.0f);
        setParameter("b1_freq", 1000.0f);
        setParameter("b2_freq", 2200.0f);
        setParameter("b1_gain", 0.0f);
        setParameter("b2_gain", 0.0f);
        setParameter("b1_type", 5.0f);
        setParameter("b2_type", 7.0f);
        setParameter("b1_placement_mode", 0.0f);
        setParameter("b2_placement_mode", 1.0f);
        setParameter("b1_dyn_thresh", -18.0f);
        setParameter("b2_dyn_thresh", -30.0f);
        setParameter("b1_on", 1.0f);
        setParameter("b2_on", 0.0f);

        editor.responseCurve.selection.fill(false);
        editor.responseCurve.selection[0] = true;
        editor.responseCurve.selectedBand = 0;
        editor.selectBand(0, false);
        editor.timerCallback();
        CHECK(editor.responseCurve.getSelectionCount() == 1
                  && editor.freqField.isEnabled() && !editor.freqField.hasMixedValue(),
              "single-selection state exposes the primary band value");
        CHECK(editor.dynThreshold.getDisplayedText().endsWith(" dB"),
              "single threshold values retain the dB suffix");

        editor.responseCurve.selection[1] = true;
        editor.responseCurve.selectedBand = 0;
        editor.timerCallback();
        CHECK(editor.responseCurve.getSelectionCount() == 2,
              "multi-selection state retains both selected bands");
        CHECK(editor.freqField.hasMixedValue()
                  && editor.freqField.getDisplayedText().contains("MULTI")
                  && !editor.gainField.hasMixedValue(),
              "mixed numeric fields show MULTI while shared values stay concrete");
        CHECK(!(bool)editor.typeBox.getProperties().getWithDefault("mixedValue", false)
                  && !(bool)editor.placementModeBox.getProperties()
                      .getWithDefault("mixedValue", false),
              "mixed selectors retain the primary band's concrete prototype value");
        CHECK(editor.dynThreshold.hasMixedValue()
                  && editor.dynThreshold.getDisplayedText() == "MULTI",
              "mixed threshold omits the dB suffix");
        CHECK(editor.bandOn.getButtonText() == "MIXED",
              "mixed binary band state is visible instead of copying the primary band");

        editor.setSize(editor_layout::designWidth, editor_layout::designHeight);
        const auto snapshotDirectory = juce::SystemStats::getEnvironmentVariable(
            "DEFAULT_EQ_SELECTION_SNAPSHOT_DIR", {});
        const auto renderSelectionState = [&](const char* state)
        {
            for (const bool dark : { true, false })
            {
                editor.darkTheme = dark;
                editor.familyLook.setDark(dark);
                editor.responseCurve.setDarkMode(dark);
                editor.applySliderPalette();
                editor.sendLookAndFeelChange();
#if !JUCE_WINDOWS
                CHECK(renderHasStructure(editor, 1) && renderHasStructure(editor, 2),
                      "selection state renders in both paper/ink themes at 1x and 2x");
#endif
                if (snapshotDirectory.isNotEmpty())
                {
                    const juce::File directory(snapshotDirectory);
                    CHECK(directory.createDirectory().wasOk(),
                          "optional selection snapshot directory is available");
                    CHECK(writeRender(editor, 2, directory.getChildFile(
                              juce::String(state) + (dark ? "-dark.png" : "-light.png"))),
                          "optional selection-state snapshot is written");
                }
            }
        };
        renderSelectionState("multi");

        editor.freqField.setValue(1000.0, juce::dontSendNotification);
        editor.beginGroupSliderEdit(editor.freqField);
        editor.freqField.setValue(1500.0, juce::sendNotificationSync);
        editor.endGroupSliderEdit(editor.freqField);
        CHECK(std::abs(processor.apvts.getRawParameterValue("b1_freq")->load() - 1500.0f) < 0.1f
                  && std::abs(processor.apvts.getRawParameterValue("b2_freq")->load() - 3300.0f) < 0.1f,
              "group frequency dragging preserves the selected bands' frequency ratio");

        setParameter("b1_gain", 1.0f);
        setParameter("b2_gain", -2.0f);
        editor.gainField.setValue(1.0, juce::dontSendNotification);
        editor.beginGroupSliderEdit(editor.gainField);
        editor.gainField.setValue(3.0, juce::sendNotificationSync);
        editor.endGroupSliderEdit(editor.gainField);
        CHECK(std::abs(processor.apvts.getRawParameterValue("b1_gain")->load() - 3.0f) < 0.01f
                  && std::abs(processor.apvts.getRawParameterValue("b2_gain")->load()) < 0.01f,
              "group gain dragging applies the same delta to every selected band");

        editor.applyAbsoluteToSelectedBands("type", 5.0f);
        editor.applyAbsoluteToSelectedBands("on", 1.0f);
        editor.timerCallback();
        CHECK(std::abs(processor.apvts.getRawParameterValue("b1_type")->load() - 5.0f) < 0.01f
                  && std::abs(processor.apvts.getRawParameterValue("b2_type")->load() - 5.0f) < 0.01f
                  && !(bool)editor.typeBox.getProperties().getWithDefault("mixedValue", false),
              "absolute selector edits align all selected bands and clear MULTI");
        CHECK(processor.apvts.getRawParameterValue("b1_on")->load() > 0.5f
                  && processor.apvts.getRawParameterValue("b2_on")->load() > 0.5f
                  && editor.bandOn.getButtonText() == "ON",
              "binary group edits align all selected bands and clear MULTI");

        editor.responseCurve.selection[1] = false;
        editor.timerCallback();
        renderSelectionState("single");
        editor.responseCurve.selection.fill(false);
        editor.responseCurve.selectedBand = -1;
        editor.selectBand(-1, false);
        editor.timerCallback();
        renderSelectionState("zero");

        const bool sharedDark = !default_family::ThemePreferences::loadLight();
        editor.darkTheme = sharedDark;
        editor.familyLook.setDark(sharedDark);
        editor.responseCurve.setDarkMode(sharedDark);
        editor.applySliderPalette();
        editor.sendLookAndFeelChange();
        const auto persistedSize = editor_layout::constrainedSize(913, 577);
        editor.setSize(persistedSize.x, persistedSize.y);
    }
    {
        DefaultEqualizerAudioProcessor processor;
        DefaultEqualizerAudioProcessorEditor restored(processor);
        const auto persistedSize = editor_layout::constrainedSize(913, 577);
        CHECK(restored.getWidth() == persistedSize.x && restored.getHeight() == persistedSize.y,
              "a newly opened editor restores the previous proportional editor size");
        restored.setSize(originalSize.x, originalSize.y);
    }
    {
        DefaultEqualizerAudioProcessor prototypeProcessor;
        DefaultEqualizerAudioProcessorEditor prototypeEditor(prototypeProcessor);
        const auto setPrototypeParameter = [&prototypeProcessor](const juce::String& id, float value)
        {
            if (auto* parameter = prototypeProcessor.apvts.getParameter(id))
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        };
        struct PrototypeBand
        {
            int type;
            float frequency;
            float gain;
            float q;
            int placementMode;
            float placement;
            int saturationMode;
            float drive;
            float character;
            float secondary;
            float threshold;
            float range;
        };
        constexpr std::array<PrototypeBand, 6> prototypeBands {{
            { 1, 38.0f, 0.0f, 0.75f, 0, -100.0f, 0, 0.0f, 0.0f, 0.0f, 0.0f, 6.0f },
            { 6, 118.0f, 2.4f, 0.82f, 0, 0.0f, 0, 0.0f, 0.0f, 0.0f, 0.0f, 6.0f },
            { 5, 520.0f, -3.2f, 1.45f, 0, 0.0f, 0, 2.2f, 0.0f, 0.0f, -24.0f, 4.5f },
            { 5, 2100.0f, 1.8f, 1.1f, 2, -100.0f, 4, 5.5f, 0.55f, 0.5f, -18.0f, 3.5f },
            { 7, 7800.0f, 1.2f, 0.9f, 1, 100.0f, 0, 0.0f, 0.0f, 0.0f, 0.0f, 6.0f },
            { 0, 16500.0f, 0.0f, 0.75f, 0, 0.0f, 0, 0.0f, 0.0f, 0.0f, 0.0f, 6.0f }
        }};
        for (int index = 0; index < (int)prototypeBands.size(); ++index)
        {
            const auto prefix = "b" + juce::String(index + 1) + "_";
            const auto& band = prototypeBands[(size_t)index];
            setPrototypeParameter(prefix + "present", 1.0f);
            setPrototypeParameter(prefix + "on", 1.0f);
            setPrototypeParameter(prefix + "type", (float)band.type);
            setPrototypeParameter(prefix + "freq", band.frequency);
            setPrototypeParameter(prefix + "gain", band.gain);
            setPrototypeParameter(prefix + "q", band.q);
            setPrototypeParameter(prefix + "placement_mode", (float)band.placementMode);
            setPrototypeParameter(prefix + "placement", band.placement);
            setPrototypeParameter(prefix + "sat_mode", (float)band.saturationMode);
            setPrototypeParameter(prefix + "drive", band.drive);
            setPrototypeParameter(prefix + "drive_character", band.character);
            setPrototypeParameter(prefix + "drive_secondary", band.secondary);
            setPrototypeParameter(prefix + "dyn_thresh", band.threshold);
            setPrototypeParameter(prefix + "dyn_range", band.range);
        }
        prototypeEditor.responseCurve.selection.fill(false);
        prototypeEditor.responseCurve.selection[2] = true;
        prototypeEditor.responseCurve.selection[3] = true;
        prototypeEditor.responseCurve.selectedBand = 3;
        prototypeEditor.selectBand(3, false);
        prototypeEditor.setSize(editor_layout::designWidth, editor_layout::designHeight);
        prototypeEditor.timerCallback();
        prototypeEditor.darkTheme = false;
        prototypeEditor.familyLook.setDark(false);
        prototypeEditor.responseCurve.setDarkMode(false);
        prototypeEditor.applySliderPalette();
        prototypeEditor.sendLookAndFeelChange();
        prototypeEditor.responseCurve.updateResponseCurve();
        const auto fullAmountRangeHandle = prototypeEditor.responseCurve.dynamicRangeHandleBounds();
        setPrototypeParameter("scale", 0.5f);
        prototypeEditor.responseCurve.updateResponseCurve();
        const auto halfAmountRangeHandle = prototypeEditor.responseCurve.dynamicRangeHandleBounds();
        CHECK(std::abs(fullAmountRangeHandle.getCentreY()
                       - halfAmountRangeHandle.getCentreY()) > 0.5f,
              "dynamic range handle follows the Amount-scaled target curve");
        CHECK(std::abs(halfAmountRangeHandle.getCentreY()
                       - prototypeEditor.responseCurve.dbToY(
                           prototypeEditor.responseCurve.dynamicRangeTargetDb(3.5f))) < 0.01f,
              "dynamic range handle and dashed guide share one response calculation");
        setPrototypeParameter("scale", 1.0f);
        prototypeEditor.responseCurve.updateResponseCurve();
        const float staticGuideBeforeReduction =
            prototypeEditor.responseCurve.dynamicRangeTargetDb(3.5f);
        const float staticResponseBeforeReduction =
            prototypeEditor.responseCurve.staticMagnitudes[256];
        const float liveResponseBeforeReduction =
            prototypeEditor.responseCurve.magnitudes[256];
        prototypeProcessor.bandDynamicGainDb[3].store(-2.0f, std::memory_order_relaxed);
        prototypeEditor.responseCurve.updateResponseCurve();
        CHECK(std::abs(prototypeEditor.responseCurve.dynamicRangeTargetDb(3.5f)
                       - staticGuideBeforeReduction) < 0.0001f
                  && std::abs(prototypeEditor.responseCurve.staticMagnitudes[256]
                              - staticResponseBeforeReduction) < 0.0001f,
              "live dynamic gain does not move the configured range guide");
        CHECK(std::abs(prototypeEditor.responseCurve.magnitudes[256]
                       - liveResponseBeforeReduction) > 0.01f,
              "live dynamic gain still moves the audible response curve");
        prototypeProcessor.bandDynamicGainDb[3].store(0.0f, std::memory_order_relaxed);
        prototypeEditor.responseCurve.updateResponseCurve();
        for (int decayStep = 0; decayStep < 20; ++decayStep)
            prototypeEditor.dynThreshold.setInputLevelsDb(-16.8f, -25.2f);
        constexpr int spectrumBins = 2048;
        constexpr double spectrumSampleRate = 48000.0;
        std::array<float, spectrumBins> inputSpectrum {};
        std::array<float, spectrumBins> outputSpectrum {};
        const auto makePrototypeSpectrum = [](float frequency, bool input)
        {
            const float t = juce::jlimit(0.0f, 1.0f,
                std::log10(juce::jmax(20.0f, frequency) / 20.0f) / 3.0f);
            const float base = input ? 0.66f : 0.61f;
            const float shape = std::sin(t * 41.0f + (input ? 0.7f : 0.0f)) * 0.055f
                + std::sin(t * 93.0f) * 0.024f
                + std::exp(-std::pow((t - 0.24f) / 0.17f, 2.0f)) * 0.12f
                + std::exp(-std::pow((t - 0.61f) / 0.10f, 2.0f)) * 0.09f;
            const float normalizedY = juce::jlimit(0.2f, 0.92f,
                base - shape + t * 0.13f);
            const float tiltedDb = -90.0f + (1.0f - normalizedY) * 90.0f;
            return tiltedDb - 4.5f * std::log2(frequency / 1000.0f);
        };
        for (int bin = 1; bin < spectrumBins; ++bin)
        {
            const float frequency = (float)((double)bin * spectrumSampleRate
                                             / (2.0 * spectrumBins));
            inputSpectrum[(size_t)bin] = makePrototypeSpectrum(frequency, true);
            outputSpectrum[(size_t)bin] = makePrototypeSpectrum(frequency, false);
        }
        prototypeEditor.responseCurve.pushSpectrumData(
            inputSpectrum.data(), spectrumBins, spectrumSampleRate, true);
        prototypeEditor.responseCurve.pushSpectrumData(
            outputSpectrum.data(), spectrumBins, spectrumSampleRate, false);
        prototypeEditor.responseCurve.refreshForTimer(true);
        const auto paritySnapshot = juce::SystemStats::getEnvironmentVariable(
            "DEFAULT_EQ_PROTOTYPE_PARITY_SNAPSHOT", {});
        if (paritySnapshot.isNotEmpty())
            CHECK(writeRender(prototypeEditor, 1, juce::File(paritySnapshot)),
                  "prototype parity snapshot is written at the literal 752x454 design size");
        const auto paritySnapshot2x = juce::SystemStats::getEnvironmentVariable(
            "DEFAULT_EQ_PROTOTYPE_PARITY_SNAPSHOT_2X", {});
        if (paritySnapshot2x.isNotEmpty())
            CHECK(writeRender(prototypeEditor, 2, juce::File(paritySnapshot2x)),
                  "prototype parity snapshot is written at Retina resolution");
        const auto rta12DbSnapshot2x = juce::SystemStats::getEnvironmentVariable(
            "DEFAULT_EQ_RTA_12DB_SNAPSHOT_2X", {});
        if (rta12DbSnapshot2x.isNotEmpty())
        {
            prototypeEditor.responseCurve.setGainRangeMode(2);
            CHECK(writeRender(prototypeEditor, 2, juce::File(rta12DbSnapshot2x)),
                  "12 dB RTA labels are written at Retina resolution");
            prototypeEditor.responseCurve.setGainRangeMode(0);
        }
        const auto settingsSnapshot = juce::SystemStats::getEnvironmentVariable(
            "DEFAULT_EQ_SETTINGS_SNAPSHOT", {});
        auto snapshotSettings = prototypeEditor.settingsOverlay.getState();
        snapshotSettings.themeMode = default_family::ThemePreferences::automatic;
        snapshotSettings.lightBackground = juce::Colour(0xfff6f6f6);
        snapshotSettings.lightForeground = juce::Colour(0xff050505);
        snapshotSettings.darkBackground = juce::Colour(0xff050505);
        snapshotSettings.darkForeground = juce::Colour(0xfff6f6f6);
        prototypeEditor.settingsOverlay.setState(snapshotSettings);
        prototypeEditor.applyThemeMode(snapshotSettings.themeMode, false);
        prototypeEditor.settingsOverlay.setStatistics({
            prototypeEditor.responseCurve.calculateSpectralStatistics(), 12.4f, 0.87f });
        prototypeEditor.toggleSettingsOverlay();
        CHECK(prototypeEditor.settingsOverlay.isVisible()
                  && prototypeEditor.settingsOverlay.getBounds()
                      == prototypeEditor.responseCurve.getBounds(),
              "logo settings panel opens at the exact RTA size");
        if (settingsSnapshot.isNotEmpty())
        {
            CHECK(writeRender(prototypeEditor, 1, juce::File(settingsSnapshot)),
                  "settings snapshot is written at the literal 752x454 design size");
        }
        const auto manualRangeSnapshot = juce::SystemStats::getEnvironmentVariable(
            "DEFAULT_EQ_GAIN_RANGE_UTF8_SNAPSHOT", {});
        if (manualRangeSnapshot.isNotEmpty())
        {
            snapshotSettings.gainRangeMode = 1;
            prototypeEditor.settingsOverlay.setState(snapshotSettings);
            prototypeEditor.applySettings(snapshotSettings, false);
            CHECK(writeRender(prototypeEditor, 1, juce::File(manualRangeSnapshot)),
                  "manual gain range snapshot preserves the UTF-8 plus-minus sign");
            snapshotSettings.gainRangeMode = 0;
            prototypeEditor.settingsOverlay.setState(snapshotSettings);
            prototypeEditor.applySettings(snapshotSettings, false);
        }
        const auto clickSettingsAt = [&prototypeEditor](juce::Point<float> point)
        {
            const auto now = juce::Time::getCurrentTime();
            juce::MouseEvent click(juce::Desktop::getInstance().getMainMouseSource(), point,
                juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &prototypeEditor.settingsOverlay,
                &prototypeEditor.settingsOverlay, now, point, now, 1, false);
            prototypeEditor.settingsOverlay.mouseDown(click);
        };
        const auto shortcutSnapshot = juce::SystemStats::getEnvironmentVariable(
            "DEFAULT_EQ_SHORTCUTS_SNAPSHOT", {});
        clickSettingsAt({ 24.0f, 150.0f });
        CHECK(prototypeEditor.settingsOverlay.shortcutPageVisible,
              "shortcut summary opens the complete interaction map");
        if (shortcutSnapshot.isNotEmpty())
        {
            CHECK(writeRender(prototypeEditor, 1, juce::File(shortcutSnapshot)),
                  "complete shortcut map snapshot is written at the literal design size");
        }
        clickSettingsAt({ 24.0f, 20.0f });
        CHECK(!prototypeEditor.settingsOverlay.shortcutPageVisible,
              "clicking the complete interaction map returns to settings");
        const auto colourPickerSnapshot = juce::SystemStats::getEnvironmentVariable(
            "DEFAULT_EQ_COLOUR_PICKER_SNAPSHOT", {});
        if (colourPickerSnapshot.isNotEmpty())
        {
            clickSettingsAt({ 24.0f, 102.0f });
            CHECK(writeRender(prototypeEditor, 1, juce::File(colourPickerSnapshot)),
                  "colour picker snapshot is written at the literal design size");
        }
        prototypeEditor.hideSettingsOverlay();
        const auto customPaletteSnapshot = juce::SystemStats::getEnvironmentVariable(
            "DEFAULT_EQ_CUSTOM_PALETTE_SNAPSHOT", {});
        if (customPaletteSnapshot.isNotEmpty())
        {
            snapshotSettings.themeMode = default_family::ThemePreferences::white;
            snapshotSettings.lightBackground = juce::Colour(0xffdcf3d2);
            snapshotSettings.lightForeground = juce::Colour(0xff59131c);
            prototypeEditor.settingsOverlay.setState(snapshotSettings);
            prototypeEditor.applySettings(snapshotSettings, false);
            CHECK(writeRender(prototypeEditor, 1, juce::File(customPaletteSnapshot)),
                  "custom palette snapshot is written at the literal design size");
        }
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
