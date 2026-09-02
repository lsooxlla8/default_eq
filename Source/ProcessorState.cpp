#include "PluginProcessor.h"

namespace
{
juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}

juce::ValueTree findStateParameter(juce::ValueTree state, const juce::String& id)
{
    for (auto child : state)
        if (child.getProperty("id").toString() == id)
            return child;
    return {};
}

float readStateParameter(juce::ValueTree state, const juce::String& id, float fallback)
{
    if (auto child = findStateParameter(state, id); child.isValid())
        return (float)child.getProperty("value", fallback);
    return (float)state.getProperty(id, fallback);
}

void writeStateParameter(juce::ValueTree state, const juce::String& id, float value)
{
    if (auto child = findStateParameter(state, id); child.isValid())
        child.setProperty("value", value, nullptr);
    else
    {
        juce::ValueTree parameter("PARAM");
        parameter.setProperty("id", id, nullptr);
        parameter.setProperty("value", value, nullptr);
        state.appendChild(parameter, nullptr);
    }
    state.removeProperty(id, nullptr);
}

void removeStateParameter(juce::ValueTree state, const juce::String& id)
{
    state.removeProperty(id, nullptr);
    if (auto child = findStateParameter(state, id); child.isValid())
        state.removeChild(child, nullptr);
}
}

void DefaultEqualizerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree root("DEFAULT_EQ_STATE");
    root.setProperty("schemaVersion", 9, nullptr);
    auto current = apvts.copyState();
    current.setProperty("stateRole", "current", nullptr);
    root.appendChild(current, nullptr);
    std::unique_ptr<juce::XmlElement> xml (root.createXml());
    copyXmlToBinary(*xml, destData);
}

void DefaultEqualizerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary(data, sizeInBytes));
    if (!xmlState)
        return;

    const auto restored = juce::ValueTree::fromXml(*xmlState);
    int restoredSchema = 0;
    bool recognizedState = false;
    if (restored.hasType("DEFAULT_EQ_STATE"))
    {
        recognizedState = true;
        restoredSchema = (int) restored.getProperty("schemaVersion", 0);
        juce::ValueTree current;
        if (restoredSchema >= 4)
        {
            for (auto child : restored)
                if (child.getProperty("stateRole").toString() == "current")
                    current = child.createCopy();
        }
        else
        {
            // v1-v3 stored two A/B snapshots. Preserve the state that was
            // audible when the project was saved, then discard the obsolete
            // comparison slot.
            const auto active = restored.getProperty("activeSlot", "A").toString();
            for (auto child : restored)
                if (child.getProperty("snapshotSlot").toString() == active)
                    current = child.createCopy();
        }
        if (!current.isValid()) return;
        current.removeProperty("stateRole", nullptr);
        current.removeProperty("snapshotSlot", nullptr);
        for (int i = 1; i <= kNumBands; ++i)
        {
            removeStateParameter(current, bandId(i, "link"));
            removeStateParameter(current, bandId(i, "dyn_on"));
        }
        if (restoredSchema < 6)
            for (int i = 1; i <= kNumBands; ++i)
            {
                const int route = (int)readStateParameter(current, bandId(i, "ch"), 0.0f);
                writeStateParameter(current, bandId(i, "placement_mode"), route >= 3 ? 1.0f : 0.0f);
                writeStateParameter(current, bandId(i, "placement"),
                    route == 1 || route == 3 ? -100.0f : route == 2 || route == 4 ? 100.0f : 0.0f);
                removeStateParameter(current, bandId(i, "ch"));
                const int mode = (int)readStateParameter(current, bandId(i, "sat_mode"), 0.0f);
                const float legacyDrive = readStateParameter(current, bandId(i, "drive"), 0.0f);
                const float legacyCharacter = readStateParameter(current, bandId(i, "drive_character"), 0.0f) * 0.01f;
                writeStateParameter(current, bandId(i, "drive"), legacyDrive * 0.36f);
                writeStateParameter(current, bandId(i, "drive_character"),
                    mode == 3 || mode == 4 || mode == 6 ? 2.0f * legacyCharacter - 1.0f : legacyCharacter);
                writeStateParameter(current, bandId(i, "drive_secondary"),
                    readStateParameter(current, bandId(i, "drive_secondary"), 0.0f) * 0.01f);
                removeStateParameter(current, bandId(i, "drive_tone"));
            }
        if (restoredSchema < 8)
            for (int i = 1; i <= kNumBands; ++i)
                writeStateParameter(current, bandId(i, "present"),
                    readStateParameter(current, bandId(i, "on"), 0.0f));
        apvts.replaceState(current);
    }
    else if (restored.hasType(apvts.state.getType()))
    {
        recognizedState = true;
        // Migration from upstream FreeEQ8-compatible state: preserve matching IDs,
        // and write the current single-state schema on the next save.
        auto current = restored.createCopy();
        for (int i = 1; i <= kNumBands; ++i)
        {
            removeStateParameter(current, bandId(i, "link"));
            removeStateParameter(current, bandId(i, "dyn_on"));
            const int route = (int)readStateParameter(current, bandId(i, "ch"), 0.0f);
            writeStateParameter(current, bandId(i, "placement_mode"), route >= 3 ? 1.0f : 0.0f);
            writeStateParameter(current, bandId(i, "placement"),
                route == 1 || route == 3 ? -100.0f : route == 2 || route == 4 ? 100.0f : 0.0f);
            removeStateParameter(current, bandId(i, "ch"));
            const int mode = (int)readStateParameter(current, bandId(i, "sat_mode"), 0.0f);
            const float legacyDrive = readStateParameter(current, bandId(i, "drive"), 0.0f);
            const float legacyCharacter = readStateParameter(current, bandId(i, "drive_character"), 0.0f) * 0.01f;
            writeStateParameter(current, bandId(i, "drive"), legacyDrive * 0.36f);
            writeStateParameter(current, bandId(i, "drive_character"),
                mode == 3 || mode == 4 || mode == 6 ? 2.0f * legacyCharacter - 1.0f : legacyCharacter);
            writeStateParameter(current, bandId(i, "drive_secondary"),
                readStateParameter(current, bandId(i, "drive_secondary"), 0.0f) * 0.01f);
            removeStateParameter(current, bandId(i, "drive_tone"));
            writeStateParameter(current, bandId(i, "present"),
                readStateParameter(current, bandId(i, "on"), 0.0f));
        }
        apvts.replaceState(current);
        restoredSchema = 0;
    }
    if (!recognizedState) return;

    if (restoredSchema < 2)
    {
        // v1 inherited FreeEQ8's choice parameter: 0/1/2 meant 12/24/48.
        // v2 exposes a continuous 0..48 dB/oct value for every filter type.
        const auto migrateCurrent = [this]
        {
            for (int i = 1; i <= kNumBands; ++i)
            {
                const auto id = bandId(i, "slope");
                const float legacy = apvts.getRawParameterValue(id)->load();
                const float migrated = legacy < 0.5f ? 12.0f : (legacy < 1.5f ? 24.0f : 48.0f);
                if (auto* parameter = apvts.getParameter(id))
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(migrated));
            }
        };

        migrateCurrent();
    }
    if (restoredSchema < 3)
    {
        const auto migrateAutoGain = [this]
        {
            const bool oldEnabled = (bool) apvts.state.getProperty("auto_gain", false);
            if (auto* mode = apvts.getParameter("auto_gain_mode"))
                mode->setValueNotifyingHost(mode->convertTo0to1(oldEnabled ? 2.0f : 0.0f));
            apvts.state.removeProperty("auto_gain", nullptr);
        };
        migrateAutoGain();
    }
    if (restoredSchema < 4)
    {
        // The v3 lookahead control was a boolean selecting a fixed 5 ms.
        // Preserve the audible timing when loading those projects into the
        // continuous 0..5 ms parameter.
        for (int i = 1; i <= kNumBands; ++i)
        {
            const auto id = bandId(i, "dyn_lookahead");
            if (apvts.getRawParameterValue(id)->load() > 0.5f)
                if (auto* parameter = apvts.getParameter(id))
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(5.0f));
        }
    }

    // Ask the background worker to rebuild the FIR from the restored
    // state; audio thread will keep using the previous kernel until then.
    requestLinearPhaseRebuild();

    updateReportedLatency();
}
