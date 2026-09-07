#include "ResponseCurveComponent.h"
#include "DefaultFamilyUI.h"
#include "DriveCharacterFormatting.h"
#include "FilterIcon.h"
#include "../DSP/FilterTypes.h"
#include "../DSP/VariableSlope.h"
#include "../PluginProcessor.h"
#include <complex>

static juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}

// ── Hit-testing and interaction ────────────────────────────────────
int ResponseCurveComponent::hitTestNode(float mx, float my) const
{
    for (int b = 0; b < kNumBands; ++b)
    {
        const int idx = b + 1;
        const bool present = proc.apvts.getRawParameterValue(bandId(idx, "present"))->load() > 0.5f;
        if (!present) continue;

        const float freq = displayedBandFrequency(
            proc.apvts.getRawParameterValue(bandId(idx, "freq"))->load());
        const float gain = proc.apvts.getRawParameterValue(bandId(idx, "gain"))->load();

        const float nx = freqToX(freq);
        const float ny = dbToY(gain);

        const float dx = mx - nx;
        const float dy = my - ny;
        if (isWithinBandHitArea(dx, dy))
            return b;
    }

    return -1;
}

// ── Mouse interaction ──────────────────────────────────────────────
void ResponseCurveComponent::mouseDown(const juce::MouseEvent& e)
{
    if (numericEditor.isVisible() && !numericEditor.getBounds().contains(e.getPosition()))
        dismissNumericEditor();
    int hit = hitTestNode((float)e.x, (float)e.y);

    if (e.mods.isLeftButtonDown() && !e.mods.isCommandDown() && !e.mods.isShiftDown()
        && !e.mods.isAltDown() && dynamicRangeHandleBounds().contains((float)e.x, (float)e.y))
    {
        const int idx = selectedBand + 1;
        dynamicRangeDragParam = proc.apvts.getParameter(bandId(idx, "dyn_range"));
        dynamicRangeDragging = dynamicRangeDragParam != nullptr;
        dragging = dynamicRangeDragging;
        if (dynamicRangeDragParam)
        {
            proc.undoManager.beginNewTransaction("Adjust dynamic range");
            dynamicRangeDragParam->beginChangeGesture();
        }
        return;
    }

    if (hit >= 0 && e.mods.isLeftButtonDown() && e.mods.isShiftDown()
        && e.mods.isCommandDown())
    {
        proc.undoManager.beginNewTransaction("Reset band placement");
        if (auto* parameter = proc.apvts.getParameter(bandId(hit + 1, "placement")))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(0.0f));
            parameter->endChangeGesture();
        }
        repaint();
        return;
    }

    const bool popupMarquee = hit < 0 && e.mods.isPopupMenu();
    if (popupMarquee && onContextMenuDismissRequest)
        onContextMenuDismissRequest();
    if (popupMarquee)
    {
        marqueePending = true;
        marqueeDragging = false;
        marqueeStart = marqueeCurrent = e.position;
        return;
    }

    if (e.mods.isPopupMenu() && e.mods.isShiftDown() && hit >= 0)
    {
        selection.fill(false);
        selection[(size_t)hit] = true;
        selectedBand = hit;
        resetBandThreshold(hit);
        return;
    }

    if (e.mods.isPopupMenu() && e.mods.isAltDown() && hit >= 0)
    {
        proc.undoManager.beginNewTransaction("Reset band slope");
        if (auto* parameter = proc.apvts.getParameter(bandId(hit + 1, "slope")))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
            parameter->endChangeGesture();
        }
        repaint();
        return;
    }

    if (e.mods.isPopupMenu() && e.mods.isCommandDown() && hit >= 0)
    {
        if (!selection[(size_t)hit])
        {
            selection.fill(false);
            selection[(size_t)hit] = true;
        }
        selectedBand = hit;
        proc.undoManager.beginNewTransaction("Reset selected band drive and character");
        for (int b = 0; b < kNumBands; ++b)
            if (selection[(size_t)b])
            {
                const int mode = std::clamp((int)proc.apvts.getRawParameterValue(
                    bandId(b + 1, "sat_mode"))->load(), 0, kSaturationModeCount - 1);
                const float characterDefault = mode == static_cast<int>(SaturationType::Tape)
                    || mode == static_cast<int>(SaturationType::PhaseDistortion)
                    || mode == static_cast<int>(SaturationType::SineErosion) ? 0.5f : 0.0f;
                if (auto* drive = proc.apvts.getParameter(bandId(b + 1, "drive")))
                {
                    drive->beginChangeGesture();
                    drive->setValueNotifyingHost(drive->getDefaultValue());
                    drive->endChangeGesture();
                }
                if (auto* character = proc.apvts.getParameter(bandId(b + 1, "drive_character")))
                {
                    character->beginChangeGesture();
                    character->setValueNotifyingHost(character->convertTo0to1(characterDefault));
                    character->endChangeGesture();
                }
            }
        repaint();
        return;
    }

    if (hit < 0 && e.mods.isLeftButtonDown() && !e.mods.isCommandDown()
        && !e.mods.isAltDown())
    {
        const int type = e.mods.isShiftDown()
            ? shiftClickTypeForNewBand(xToFreq((float)e.x), yToDb((float)e.y)) : -1;
        hit = createBandAt((float)e.x, (float)e.y, e.eventTime.toMilliseconds(), type);
        if (hit >= 0)
        {
            // Creation and the immediately following move remain one Undo step.
            beginStaticBandDrag(hit, false);
            repaint();
        }
        return;
    }

    if (e.mods.isPopupMenu() && hit >= 0)
    {
        if (!selection[(size_t)hit]) { selection.fill(false); selection[(size_t)hit] = true; }
        selectedBand = hit;
        const int idx = hit + 1;
        PrototypeContextMenuModel model;
        model.bandNumber = idx;
        model.selectedType = juce::jlimit(0, 9,
            (int)proc.apvts.getRawParameterValue(bandId(idx, "type"))->load());
        const int selectedMode = std::clamp((int)proc.apvts.getRawParameterValue(
            bandId(idx, "placement_mode"))->load(), 0, 2);
        const float selectedPlacement = proc.apvts.getRawParameterValue(
            bandId(idx, "placement"))->load();
        model.selectedRoute = std::abs(selectedPlacement) <= 1.0f ? 1
            : selectedMode == 2 ? (selectedPlacement < 0.0f ? 5 : 6)
            : selectedMode == 1 ? (selectedPlacement < 0.0f ? 3 : 4)
                                : (selectedPlacement < 0.0f ? 0 : 2);
        model.selectedSaturation = std::clamp((int)proc.apvts.getRawParameterValue(
            bandId(idx, "sat_mode"))->load(), 0, kSaturationModeCount - 1);
        model.selectedCount = (int)std::count(selection.begin(), selection.end(), true);

        auto safeThis = juce::Component::SafePointer<ResponseCurveComponent>(this);
        model.toggleBand = [safeThis, idx]
        {
            if (safeThis == nullptr) return;
            auto& self = *safeThis;
            self.proc.undoManager.beginNewTransaction("Enable/disable selected band");
            auto* raw = self.proc.apvts.getRawParameterValue(bandId(idx, "on"));
            auto* parameter = self.proc.apvts.getParameter(bandId(idx, "on"));
            if (parameter != nullptr && raw != nullptr)
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(raw->load() > 0.5f ? 0.0f : 1.0f);
                parameter->endChangeGesture();
            }
            self.repaint();
        };
        model.chooseFilter = [safeThis](int type)
        {
            if (safeThis == nullptr) return;
            auto& self = *safeThis;
            self.proc.undoManager.beginNewTransaction("Set selected band filter type");
            for (int b = 0; b < kNumBands; ++b)
                if (self.selection[(size_t)b])
                {
                    if (auto* parameter = self.proc.apvts.getParameter(bandId(b + 1, "type")))
                    {
                        parameter->beginChangeGesture();
                        parameter->setValueNotifyingHost(parameter->convertTo0to1((float)type));
                        parameter->endChangeGesture();
                    }
                    if (type == 0 || type == 1)
                        if (auto* q = self.proc.apvts.getParameter(bandId(b + 1, "q")))
                        {
                            q->beginChangeGesture();
                            q->setValueNotifyingHost(q->convertTo0to1(
                                deq::filter_types::resonantCutDefaultQ));
                            q->endChangeGesture();
                        }
                    if (ResponseCurveComponent::typeDefaultsToMidSide(type))
                        if (auto* mode = self.proc.apvts.getParameter(
                                bandId(b + 1, "placement_mode")))
                        {
                            mode->beginChangeGesture();
                            mode->setValueNotifyingHost(mode->convertTo0to1(1.0f));
                            mode->endChangeGesture();
                        }
                }
            self.repaint();
        };
        model.chooseRoute = [safeThis](int route)
        {
            if (safeThis == nullptr) return;
            auto& self = *safeThis;
            static constexpr float placements[] {
                -100.0f, 0.0f, 100.0f, -100.0f, 100.0f, -100.0f, 100.0f
            };
            static constexpr float modes[] { 0, 0, 0, 1, 1, 2, 2 };
            route = juce::jlimit(0, 6, route);
            self.proc.undoManager.beginNewTransaction("Set selected band placement");
            for (int b = 0; b < kNumBands; ++b)
                if (self.selection[(size_t)b])
                {
                    if (auto* mode = self.proc.apvts.getParameter(
                            bandId(b + 1, "placement_mode")))
                    {
                        mode->beginChangeGesture();
                        mode->setValueNotifyingHost(mode->convertTo0to1(modes[route]));
                        mode->endChangeGesture();
                    }
                    if (auto* placement = self.proc.apvts.getParameter(
                            bandId(b + 1, "placement")))
                    {
                        placement->beginChangeGesture();
                        placement->setValueNotifyingHost(
                            placement->convertTo0to1(placements[route]));
                        placement->endChangeGesture();
                    }
                }
            self.repaint();
        };
        model.chooseSaturation = [safeThis](int mode)
        {
            if (safeThis == nullptr) return;
            auto& self = *safeThis;
            self.proc.undoManager.beginNewTransaction("Set selected band saturation");
            const float characterDefault = mode == static_cast<int>(SaturationType::Tape)
                || mode == static_cast<int>(SaturationType::PhaseDistortion)
                || mode == static_cast<int>(SaturationType::SineErosion) ? 0.5f : 0.0f;
            const float secondaryDefault = mode == static_cast<int>(SaturationType::Tape)
                ? 0.5f : 0.0f;
            for (int b = 0; b < kNumBands; ++b)
                if (self.selection[(size_t)b])
                    for (const auto& change : {
                        std::pair<const char*, float>{ "sat_mode", (float)mode },
                        { "drive_character", characterDefault },
                        { "drive_secondary", secondaryDefault } })
                        if (auto* parameter = self.proc.apvts.getParameter(
                                bandId(b + 1, change.first)))
                        {
                            parameter->beginChangeGesture();
                            parameter->setValueNotifyingHost(
                                parameter->convertTo0to1(change.second));
                            parameter->endChangeGesture();
                        }
            self.repaint();
        };
        model.resetEqualizer = [safeThis]
        {
            if (safeThis == nullptr) return;
            auto& self = *safeThis;
            self.proc.undoManager.beginNewTransaction("Reset equalizer");
            for (int band = 0; band < kNumBands; ++band)
                self.proc.resetBandToDefaults(band, false);
            self.selection.fill(false);
            self.selectedBand = -1;
            self.repaint();
        };
        model.bypassSelected = [safeThis]
        {
            if (safeThis == nullptr) return;
            auto& self = *safeThis;
            self.proc.undoManager.beginNewTransaction("Bypass selected bands");
            for (int b = 0; b < kNumBands; ++b)
                if (self.selection[(size_t)b])
                    if (auto* parameter = self.proc.apvts.getParameter(bandId(b + 1, "on")))
                    {
                        parameter->beginChangeGesture();
                        parameter->setValueNotifyingHost(0.0f);
                        parameter->endChangeGesture();
                    }
            self.repaint();
        };

        if (onContextMenuRequest)
        {
            const auto pointer = getParentComponent() != nullptr
                ? getParentComponent()->getLocalPoint(this, e.getPosition())
                : e.getPosition();
            onContextMenuRequest(pointer, std::move(model));
        }
        repaint();
        return;
    }

    if (hit >= 0 && e.mods.isAltDown())
    {
        if (!selection[(size_t)hit]) { selection.fill(false); selection[(size_t)hit] = true; }
        selectedBand = hit;
        momentarySoloActive = true;
        proc.soloBand.store(hit, std::memory_order_release);
        repaint();
        return;
    }

    if (hit >= 0 && e.mods.isCommandDown())
    {
        if (!selection[(size_t)hit]) { selection.fill(false); selection[(size_t)hit] = true; }
        selectedBand = hit;
        commandGesturePending = true;
        modifierGestureBand = hit;
        repaint();
        return;
    }

    if (hit >= 0 && e.mods.isShiftDown())
    {
        shiftGesturePending = true;
        modifierGestureBand = hit;
        return;
    }

    if (hit >= 0 && !selection[(size_t)hit])
    {
        selection.fill(false);
        selection[(size_t)hit] = true;
    }
    if (hit < 0) return;
    selectedBand = hit;
    beginStaticBandDrag(hit, true);
}

int ResponseCurveComponent::createBandAt(float x, float y, std::int64_t eventTimeMs, int forcedType)
{
    int freeBand = -1;
    for (int b = 0; b < kNumBands; ++b)
        if (proc.apvts.getRawParameterValue(bandId(b + 1, "present"))->load() < 0.5f)
        {
            freeBand = b;
            break;
        }
    if (freeBand < 0)
        return -1;

    const float displayedFrequency = std::clamp(xToFreq(x), minFreq, maxFreq);
    const float frequency = std::clamp(baseBandFrequency(displayedFrequency), minFreq, maxFreq);
    const float gain = std::clamp(yToDb(y), minBandGainDb, maxBandGainDb);
    proc.undoManager.beginNewTransaction("Create EQ band");
    proc.resetBandToDefaults(freeBand, true, frequency, gain);
    const int newType = forcedType >= 0 ? forcedType : defaultTypeForNewBand(displayedFrequency, gain);
    if (auto* type = proc.apvts.getParameter(bandId(freeBand + 1, "type")))
    {
        type->beginChangeGesture();
        type->setValueNotifyingHost(type->convertTo0to1((float)newType));
        type->endChangeGesture();
    }
    if (deq::filter_types::isResonantCutIndex(newType))
        if (auto* q = proc.apvts.getParameter(bandId(freeBand + 1, "q")))
        {
            q->beginChangeGesture();
            q->setValueNotifyingHost(q->convertTo0to1(deq::filter_types::resonantCutDefaultQ));
            q->endChangeGesture();
        }
    if (typeDefaultsToMidSide(newType))
        if (auto* mode = proc.apvts.getParameter(bandId(freeBand + 1, "placement_mode")))
        {
            mode->beginChangeGesture();
            mode->setValueNotifyingHost(mode->convertTo0to1(1.0f));
            mode->endChangeGesture();
        }

    selectedBand = freeBand;
    selection.fill(false);
    selection[(size_t)freeBand] = true;
    mostRecentlyCreatedBand = freeBand;
    mostRecentCreationTimeMs = eventTimeMs;
    return freeBand;
}

void ResponseCurveComponent::beginStaticBandDrag(int hit, bool beginUndoTransaction)
{
    if (hit < 0 || hit >= kNumBands)
        return;

    dragging = true;
    rangeExpansionAvailable = true;
    if (beginUndoTransaction)
    {
        proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
            ? "Move selected EQ bands" : "Move EQ band");
    }
    groupAnchorFreq = displayedBandFrequency(
        proc.apvts.getRawParameterValue(bandId(hit + 1, "freq"))->load());
    groupAnchorGain = proc.apvts.getRawParameterValue(bandId(hit + 1, "gain"))->load();
    for (int b = 0; b < kNumBands; ++b)
    {
        dragFreqParams[(size_t)b] = dragGainParams[(size_t)b] = dragQParams[(size_t)b] = nullptr;
        if (!selection[(size_t)b]) continue;
        const int type = (int)proc.apvts.getRawParameterValue(bandId(b + 1, "type"))->load();
        dragStartFreq[(size_t)b] = proc.apvts.getRawParameterValue(bandId(b + 1, "freq"))->load();
        dragStartGain[(size_t)b] = proc.apvts.getRawParameterValue(bandId(b + 1, "gain"))->load();
        dragStartQ[(size_t)b] = proc.apvts.getRawParameterValue(bandId(b + 1, "q"))->load();
        dragFreqParams[(size_t)b] = proc.apvts.getParameter(bandId(b + 1, "freq"));
        if (usesQVerticalDrag(type))
            dragQParams[(size_t)b] = proc.apvts.getParameter(bandId(b + 1, "q"));
        else if (usesGainVerticalDrag(type))
            dragGainParams[(size_t)b] = proc.apvts.getParameter(bandId(b + 1, "gain"));
        if (dragFreqParams[(size_t)b]) dragFreqParams[(size_t)b]->beginChangeGesture();
        if (dragGainParams[(size_t)b]) dragGainParams[(size_t)b]->beginChangeGesture();
        if (dragQParams[(size_t)b]) dragQParams[(size_t)b]->beginChangeGesture();
    }
}

void ResponseCurveComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (marqueePending)
    {
        marqueeCurrent = e.position;
        if (!marqueeDragging
            && std::abs(e.getDistanceFromDragStartX()) + std::abs(e.getDistanceFromDragStartY()) > 3)
            marqueeDragging = true;
        if (marqueeDragging) updateMarqueeSelection();
        repaint();
        return;
    }
    if (dynamicRangeDragging && dynamicRangeDragParam != nullptr)
    {
        const float cursorDb = yToDb((float)e.y);
        float range = 0.0f;
        float bestError = std::numeric_limits<float>::max();
        // The handle represents the actual target response, including Amount
        // and the other bands. Invert that same curve so it stays directly
        // under the pointer while dragging.
        constexpr int coarseSteps = 48;
        for (int step = 0; step <= coarseSteps; ++step)
        {
            const float candidate = 24.0f * (float)step / (float)coarseSteps;
            const float error = std::abs(dynamicRangeTargetDb(candidate) - cursorDb);
            if (error < bestError)
            {
                bestError = error;
                range = candidate;
            }
        }
        float low = juce::jmax(0.0f, range - 0.5f);
        float high = juce::jmin(24.0f, range + 0.5f);
        for (int iteration = 0; iteration < 10; ++iteration)
        {
            const float left = low + (high - low) / 3.0f;
            const float right = high - (high - low) / 3.0f;
            if (std::abs(dynamicRangeTargetDb(left) - cursorDb)
                <= std::abs(dynamicRangeTargetDb(right) - cursorDb))
                high = right;
            else
                low = left;
        }
        range = 0.5f * (low + high);
        dynamicRangeDragParam->setValueNotifyingHost(dynamicRangeDragParam->convertTo0to1(range));
        repaint();
        return;
    }
    if (commandGesturePending && std::abs(e.getDistanceFromDragStartY()) + std::abs(e.getDistanceFromDragStartX()) > 3)
    {
        commandGesturePending = false;
        dragging = driveDragging = true;
        proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
            ? "Adjust selected band drive" : "Adjust band drive");
        for (int b = 0; b < kNumBands; ++b)
        {
            dragDriveParams[(size_t)b] = nullptr;
            if (!selection[(size_t)b]) continue;
            dragStartDrive[(size_t)b] = proc.apvts.getRawParameterValue(bandId(b + 1, "drive"))->load();
            dragDriveParams[(size_t)b] = proc.apvts.getParameter(bandId(b + 1, "drive"));
            if (dragDriveParams[(size_t)b]) dragDriveParams[(size_t)b]->beginChangeGesture();
        }
    }
    if (shiftGesturePending && std::abs(e.getDistanceFromDragStartY()) + std::abs(e.getDistanceFromDragStartX()) > 3)
    {
        shiftGesturePending = false;
        if (!selection[(size_t)modifierGestureBand])
        {
            selection.fill(false);
            selection[(size_t)modifierGestureBand] = true;
        }
        selectedBand = modifierGestureBand;
        dragging = thresholdDragging = true;
        proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
            ? "Adjust selected band thresholds" : "Adjust band threshold");
        for (int b = 0; b < kNumBands; ++b)
        {
            dragThresholdParams[(size_t)b] = nullptr;
            if (!selection[(size_t)b]) continue;
            dragStartThreshold[(size_t)b] = proc.apvts.getRawParameterValue(bandId(b + 1, "dyn_thresh"))->load();
            dragThresholdParams[(size_t)b] = proc.apvts.getParameter(bandId(b + 1, "dyn_thresh"));
            if (dragThresholdParams[(size_t)b]) dragThresholdParams[(size_t)b]->beginChangeGesture();
        }
    }
    if (!dragging || selectedBand < 0) return;

    if (driveDragging)
    {
        const float delta = -(float)e.getDistanceFromDragStartY() * 0.18f;
        for (int b = 0; b < kNumBands; ++b)
            if (auto* parameter = dragDriveParams[(size_t)b])
            {
                const float value = juce::jlimit(0.0f, 36.0f, dragStartDrive[(size_t)b] + delta);
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            }
        repaint();
        return;
    }

    if (thresholdDragging)
    {
        const float delta = -(float)e.getDistanceFromDragStartY() * 0.30f;
        for (int b = 0; b < kNumBands; ++b)
            if (auto* parameter = dragThresholdParams[(size_t)b])
            {
                const float value = juce::jlimit(-60.0f, 0.0f, dragStartThreshold[(size_t)b] + delta);
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            }
        repaint();
        return;
    }

    const bool hasGainDrag = std::any_of(dragGainParams.begin(), dragGainParams.end(),
                                        [](const auto* parameter) { return parameter != nullptr; });
    const auto localPointer = getLocalPoint(nullptr, e.getScreenPosition());
    if (hasGainDrag)
        expandAutoRtaRangeNearEdge(localPointer.y);

    const float anchorFreq = std::clamp(xToFreq((float)e.x), minFreq, maxFreq);
    const float ratio = anchorFreq / std::max(1.0f, groupAnchorFreq);
    // The current visible range is also the maximum reachable range for this
    // gesture. This prevents the first edge crossing (+/-12 -> +/-24) from
    // writing a >24 dB value and making the timer immediately jump to +/-36.
    float anchorGain = std::clamp(yToDb((float)e.y), -displayMaxDb, displayMaxDb);
    anchorGain = std::clamp(anchorGain, minBandGainDb, maxBandGainDb);
    const float gainDelta = anchorGain - groupAnchorGain;
    // Cut, Notch and Band Pass vertical movement edit Q in a logarithmic domain while
    // horizontal movement edits frequency. It must not also write Gain: doing
    // both made Band Pass change level during a Q gesture. Other filter types
    // retain the normal vertical Gain gesture.
    for (int b = 0; b < kNumBands; ++b)
    {
        if (!selection[(size_t)b]) continue;
        const float newFreq = std::clamp(dragStartFreq[(size_t)b] * ratio, minFreq, maxFreq);
        const float newGain = std::clamp(dragStartGain[(size_t)b] + gainDelta,
                                         minBandGainDb, maxBandGainDb);
        if (auto* p = dragFreqParams[(size_t)b]) p->setValueNotifyingHost(p->convertTo0to1(newFreq));
        if (auto* p = dragGainParams[(size_t)b]) p->setValueNotifyingHost(p->convertTo0to1(newGain));
        if (auto* p = dragQParams[(size_t)b])
        {
            const float newQ = cutQFromVerticalDrag(dragStartQ[(size_t)b],
                                                    (float)e.getDistanceFromDragStartY());
            p->setValueNotifyingHost(p->convertTo0to1(newQ));
        }
    }
}

bool ResponseCurveComponent::expandAutoRtaRangeNearEdge(int localPointerY)
{
    const float uiScale = juce::jlimit(0.85f, 4.0f,
        juce::jmin((float)getWidth() / 744.0f, (float)getHeight() / 254.0f));
    const int edgeZone = juce::jmax(2, juce::roundToInt(18.0f * uiScale));
    if (gainRangeMode != 0 || displayMaxDb >= maxDisplayDb)
        return false;
    const bool nearEdge = localPointerY <= edgeZone
        || localPointerY >= getHeight() - edgeZone;
    if (!nearEdge)
    {
        // Moving back into the graph is enough to arm the next range step;
        // releasing and grabbing the band again is not required.
        rangeExpansionAvailable = true;
        return false;
    }
    if (!rangeExpansionAvailable)
        return false;
    displayMaxDb = displayMaxDb < 12.0f ? 12.0f
                 : displayMaxDb < 24.0f ? 24.0f : maxDisplayDb;
    rangeExpansionAvailable = false;
    staticLayerDirty = true;
    repaint();
    return true;
}

void ResponseCurveComponent::mouseUp(const juce::MouseEvent& e)
{
    if (marqueePending)
    {
        if (marqueeDragging)
        {
            marqueeCurrent = e.position;
            updateMarqueeSelection();
        }
        else
        {
            selection.fill(false);
            selectedBand = -1;
        }
        marqueePending = marqueeDragging = false;
        repaint();
        return;
    }
    if (momentarySoloActive)
    {
        proc.soloBand.store(-1, std::memory_order_release);
        momentarySoloActive = false;
    }
    if (dynamicRangeDragParam)
    {
        dynamicRangeDragParam->endChangeGesture();
        dynamicRangeDragParam = nullptr;
    }
    if (commandGesturePending && modifierGestureBand >= 0)
    {
        proc.undoManager.beginNewTransaction("Toggle EQ band");
        if (auto* parameter = proc.apvts.getParameter(bandId(modifierGestureBand + 1, "on")))
        {
            const bool enabled = proc.apvts.getRawParameterValue(
                bandId(modifierGestureBand + 1, "on"))->load() > 0.5f;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(enabled ? 0.0f : 1.0f);
            parameter->endChangeGesture();
        }
    }
    if (shiftGesturePending && modifierGestureBand >= 0)
    {
        const int hit = modifierGestureBand;
        selection[(size_t)hit] = !selection[(size_t)hit];
        if (selection[(size_t)hit])
            selectedBand = hit;
        else if (selectedBand == hit)
        {
            selectedBand = -1;
            for (int band = 0; band < kNumBands; ++band)
                if (selection[(size_t)band]) { selectedBand = band; break; }
        }
    }
    for (int b = 0; b < kNumBands; ++b)
    {
        if (dragFreqParams[(size_t)b]) dragFreqParams[(size_t)b]->endChangeGesture();
        if (dragGainParams[(size_t)b]) dragGainParams[(size_t)b]->endChangeGesture();
        if (dragQParams[(size_t)b]) dragQParams[(size_t)b]->endChangeGesture();
        dragFreqParams[(size_t)b] = dragGainParams[(size_t)b] = dragQParams[(size_t)b] = nullptr;
        if (dragDriveParams[(size_t)b]) dragDriveParams[(size_t)b]->endChangeGesture();
        dragDriveParams[(size_t)b] = nullptr;
        if (dragThresholdParams[(size_t)b]) dragThresholdParams[(size_t)b]->endChangeGesture();
        dragThresholdParams[(size_t)b] = nullptr;
    }
    dragging = driveDragging = thresholdDragging = dynamicRangeDragging = false;
    rangeExpansionAvailable = true;
    commandGesturePending = shiftGesturePending = false;
    modifierGestureBand = -1;
    repaint();
}

void ResponseCurveComponent::updateMarqueeSelection()
{
    const auto area = juce::Rectangle<float>::leftTopRightBottom(
        std::min(marqueeStart.x, marqueeCurrent.x),
        std::min(marqueeStart.y, marqueeCurrent.y),
        std::max(marqueeStart.x, marqueeCurrent.x),
        std::max(marqueeStart.y, marqueeCurrent.y));
    selection.fill(false);
    for (int band = 0; band < kNumBands; ++band)
    {
        const int index = band + 1;
        if (proc.apvts.getRawParameterValue(bandId(index, "present"))->load() < 0.5f)
            continue;
        const auto point = juce::Point<float>(
            freqToX(displayedBandFrequency(
                proc.apvts.getRawParameterValue(bandId(index, "freq"))->load())),
            dbToY(proc.apvts.getRawParameterValue(bandId(index, "gain"))->load()));
        selection[(size_t)band] = marqueeContains(area, point);
    }
    if (selectedBand < 0 || !selection[(size_t)selectedBand])
    {
        selectedBand = -1;
        for (int band = 0; band < kNumBands; ++band)
            if (selection[(size_t)band]) { selectedBand = band; break; }
    }
}

bool ResponseCurveComponent::deleteSelectedBands()
{
    bool deletedAny = false;
    for (int band = 0; band < kNumBands; ++band)
        deletedAny = deletedAny || (selection[(size_t)band]
            && proc.apvts.getRawParameterValue(bandId(band + 1, "present"))->load() > 0.5f);
    if (!deletedAny)
        return false;

    proc.undoManager.beginNewTransaction(getSelectionCount() > 1
        ? "Delete selected EQ bands" : "Delete EQ band");
    for (int band = 0; band < kNumBands; ++band)
        if (selection[(size_t)band])
            proc.resetBandToDefaults(band, false);
    selection.fill(false);
    selectedBand = -1;
    mostRecentlyCreatedBand = -1;
    repaint();
    return true;
}

bool ResponseCurveComponent::resetBandThreshold(int band)
{
    if (band < 0 || band >= kNumBands
        || proc.apvts.getRawParameterValue(bandId(band + 1, "present"))->load() < 0.5f)
        return false;
    auto* threshold = proc.apvts.getParameter(bandId(band + 1, "dyn_thresh"));
    if (threshold == nullptr)
        return false;
    proc.undoManager.beginNewTransaction("Reset band threshold");
    threshold->beginChangeGesture();
    threshold->setValueNotifyingHost(threshold->getDefaultValue());
    threshold->endChangeGesture();
    repaint();
    return true;
}

void ResponseCurveComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int existing = hitTestNode((float)e.x, (float)e.y);
    if (existing >= 0)
    {
        const auto elapsedSinceCreation = e.eventTime.toMilliseconds() - mostRecentCreationTimeMs;
        if (existing == mostRecentlyCreatedBand
            && elapsedSinceCreation >= 0 && elapsedSinceCreation < 700)
        {
            // This is the second half of the click sequence that created the
            // band, not an intentional delete gesture on an older node.
            mostRecentlyCreatedBand = -1;
            return;
        }
        proc.undoManager.beginNewTransaction("Delete EQ band");
        proc.resetBandToDefaults(existing, false);
        selection[(size_t) existing] = false;
        selectedBand = -1;
        mostRecentlyCreatedBand = -1;
        repaint();
    }
}

void ResponseCurveComponent::showNumericEditor(int band, const juce::String& suffix, float x, float y)
{
    numericSuffix = suffix;
    numericParameter = proc.apvts.getParameter(bandId(band + 1, suffix.toRawUTF8()));
    if (numericParameter == nullptr) return;
    const float value = proc.apvts.getRawParameterValue(bandId(band + 1, suffix.toRawUTF8()))->load();
    juce::String text;
    if (suffix == "freq") text = value >= 1000.0f ? juce::String(value / 1000.0f, 3) + " kHz" : juce::String(value, 2) + " Hz";
    else if (suffix == "gain") text = juce::String(std::abs(value) < 0.005f ? 0.0f : value, 2) + " dB";
    else if (suffix == "slope") text = juce::String(value, 1) + " dB/oct";
    else text = juce::String(value, 3);
    numericEditor.setText(text, false);
    const int width = 150, height = 30;
    numericEditor.setBounds(juce::jlimit(4, getWidth() - width - 4, (int)x - width / 2),
                            juce::jlimit(4, getHeight() - height - 4, (int)y - 44), width, height);
    numericEditor.setVisible(true);
    numericEditor.toFront(true);
    numericEditor.grabKeyboardFocus();
}

void ResponseCurveComponent::commitNumericEditor()
{
    if (numericParameter == nullptr) return;
    auto source = numericEditor.getText().trim().toLowerCase().replaceCharacter(',', '.');
    double multiplier = 1.0;
    if (numericSuffix == "freq" && (source.contains("khz") || source.endsWithChar('k'))) multiplier = 1000.0;
    const auto digits = source.retainCharacters("-+0123456789.e");
    if (digits.isNotEmpty())
    {
        const double value = digits.getDoubleValue() * multiplier;
        if (std::isfinite(value))
        {
            proc.undoManager.beginNewTransaction("Enter band " + numericSuffix);
            numericParameter->beginChangeGesture();
            numericParameter->setValueNotifyingHost(numericParameter->convertTo0to1((float)value));
            numericParameter->endChangeGesture();
        }
    }
    numericParameter = nullptr;
    numericEditor.setVisible(false);
    grabKeyboardFocus();
}

void ResponseCurveComponent::dismissNumericEditor()
{
    numericParameter = nullptr;
    numericEditor.setVisible(false);
}

void ResponseCurveComponent::mouseMove(const juce::MouseEvent& e)
{
    if (dynamicRangeHandleBounds().contains((float)e.x, (float)e.y))
    {
        hoveredBand = selectedBand;
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        repaint();
        return;
    }
    const int hit = hitTestNode((float)e.x, (float)e.y);
    if (hit != hoveredBand)
    {
        hoveredBand = hit;
        setMouseCursor(hit >= 0 ? juce::MouseCursor::PointingHandCursor
                                : juce::MouseCursor::CrosshairCursor);
        repaint();
    }
}

void ResponseCurveComponent::mouseExit(const juce::MouseEvent&)
{
    hoveredBand = -1;
    repaint();
}

void ResponseCurveComponent::mouseWheelMove(const juce::MouseEvent& e,
                                             const juce::MouseWheelDetails& wheel)
{
    const int hit = hitTestNode((float)e.x, (float)e.y);
    if (hit < 0 || std::abs(wheel.deltaY) < 0.0001f) return;

    selectedBand = hit;
    if (!selection[(size_t)hit]) { selection.fill(false); selection[(size_t)hit] = true; }
    const auto wheelAction = wheelActionForModifiers(e.mods.isCommandDown(),
                                                     e.mods.isAltDown(),
                                                     e.mods.isShiftDown());
    const float increasingSlopeStep = increasingSlopeWheelStep(wheel.deltaY);
    if (wheelAction == WheelAction::slope)
    {
        proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
            ? "Adjust selected band slopes" : "Adjust band slope");
        for (int b = 0; b < kNumBands; ++b)
            if (selection[(size_t)b])
                if (auto* parameter = proc.apvts.getParameter(bandId(b + 1, "slope")))
                {
                    const float slope = proc.apvts.getRawParameterValue(bandId(b + 1, "slope"))->load();
                    parameter->beginChangeGesture();
                    const int type=(int)proc.apvts.getRawParameterValue(bandId(b+1,"type"))->load();
                    const float value = slopeAfterWheelStep(type, slope, increasingSlopeStep);
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
                    parameter->endChangeGesture();
                }
        repaint();
        return;
    }
    if (wheelAction == WheelAction::character)
    {
        proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
            ? "Adjust selected band characters" : "Adjust band character");
        // JUCE reports an upward wheel gesture with a negative delta on the
        // host/platform combination used by this plug-in.
        const float rawStep = increasingSlopeStep * 0.3f;
        const float minimumStep = wheel.isSmooth ? 0.01f : 0.05f;
        const float step = std::copysign(std::max(std::abs(rawStep), minimumStep), rawStep);
        for (int b = 0; b < kNumBands; ++b)
            if (selection[(size_t)b])
                if (auto* parameter = proc.apvts.getParameter(bandId(b + 1, "drive_character")))
                {
                    const int mode = std::clamp((int)proc.apvts.getRawParameterValue(
                        bandId(b + 1, "sat_mode"))->load(), 0, kSaturationModeCount - 1);
                    const float minimum = saturationModeUsesBipolarCharacter(mode) ? -1.0f : 0.0f;
                    const float current = proc.apvts.getRawParameterValue(
                        bandId(b + 1, "drive_character"))->load();
                    parameter->beginChangeGesture();
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(
                        juce::jlimit(minimum, 1.0f, current + step)));
                    parameter->endChangeGesture();
                }
        repaint();
        return;
    }
    if (wheelAction == WheelAction::placement)
    {
        const float rawStep = wheel.deltaY * 120.0f;
        const float minimumStep = wheel.isSmooth ? 2.5f : 12.0f;
        const float placementStep = std::copysign(
            std::max(std::abs(rawStep), minimumStep), rawStep);
        proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
            ? "Adjust selected band placement" : "Adjust band placement");
        for (int b = 0; b < kNumBands; ++b)
            if (selection[(size_t)b])
                if (auto* parameter = proc.apvts.getParameter(bandId(b + 1, "placement")))
                {
                    const float placement = proc.apvts.getRawParameterValue(bandId(b + 1, "placement"))->load();
                    parameter->beginChangeGesture();
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(
                        juce::jlimit(-100.0f, 100.0f, placement + placementStep)));
                    parameter->endChangeGesture();
                }
        repaint();
        return;
    }
    proc.undoManager.beginNewTransaction(std::count(selection.begin(), selection.end(), true) > 1
        ? "Adjust selected band widths" : "Adjust band width");
    for (int b = 0; b < kNumBands; ++b)
        if (selection[(size_t)b])
        {
            const int type = (int)proc.apvts.getRawParameterValue(bandId(b + 1, "type"))->load();
            const float wheelDirection = (type == deq::filter_types::resLowCut
                                          || type == deq::filter_types::resHighCut)
                ? -wheel.deltaY : wheel.deltaY;
            const float factor = std::pow(2.0f, wheelDirection * 0.9f);
            const auto suffix = isClassicCutType(type) ? "slope" : "q";
            if (auto* parameter = proc.apvts.getParameter(bandId(b + 1, suffix)))
            {
                const float current = proc.apvts.getRawParameterValue(bandId(b + 1, suffix))->load();
                const float value = isClassicCutType(type)
                    ? slopeAfterWheelStep(type, current, increasingSlopeStep)
                    : std::clamp(current * factor, 0.1f, 24.0f);
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
                parameter->endChangeGesture();
            }
        }
    repaint();
}
