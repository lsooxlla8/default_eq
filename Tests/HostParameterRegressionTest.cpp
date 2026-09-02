#include "../Source/PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <set>
#include <vector>

namespace
{
enum class Kind { floating, boolean, choice };

struct ExpectedParameter
{
    juce::String id;
    juce::String name;
    juce::String label;
    float start = 0.0f;
    float end = 1.0f;
    float interval = 0.0f;
    float skew = 1.0f;
    float defaultPlain = 0.0f;
    Kind kind = Kind::floating;
    juce::StringArray choices;
};

int failures = 0;

void fail(int index, const juce::String& id, const char* field,
          const juce::String& expected, const juce::String& actual)
{
    std::printf("FAIL [%d] %s: %s expected '%s', got '%s'\n", index,
                id.toRawUTF8(), field, expected.toRawUTF8(), actual.toRawUTF8());
    ++failures;
}

bool close(float actual, float expected)
{
    const auto scale = std::max(1.0f, std::abs(expected));
    return std::abs(actual - expected) <= 5.0e-6f * scale;
}

void addFloat(std::vector<ExpectedParameter>& result, const juce::String& id,
              const juce::String& name, float start, float end, float interval,
              float skew, float defaultPlain, const juce::String& label = {})
{
    result.push_back({ id, name, label, start, end, interval, skew,
                       defaultPlain, Kind::floating, {} });
}

void addBool(std::vector<ExpectedParameter>& result, const juce::String& id,
             const juce::String& name, bool defaultPlain)
{
    result.push_back({ id, name, {}, 0.0f, 1.0f, 1.0f, 1.0f,
                       defaultPlain ? 1.0f : 0.0f, Kind::boolean, {} });
}

void addChoice(std::vector<ExpectedParameter>& result, const juce::String& id,
               const juce::String& name, juce::StringArray choices, int defaultIndex)
{
    result.push_back({ id, name, {}, 0.0f, (float)choices.size() - 1.0f, 1.0f,
                       1.0f, (float)defaultIndex, Kind::choice, std::move(choices) });
}

std::vector<ExpectedParameter> makeContract()
{
    std::vector<ExpectedParameter> result;
    result.reserve(13 + kNumBands * 20);

    addFloat(result, "output_gain", "Output Gain", -24.0f, 24.0f, 0.01f, 1.0f, 0.0f, "dB");
    addFloat(result, "scale", "Amount", -2.0f, 2.0f, 0.01f, 1.0f, 1.0f, "%");
    addFloat(result, "shift", "Shift", -48.0f, 48.0f, 0.01f, 1.0f, 0.0f);
    addBool(result, "adaptive_q", "Adaptive Q", false);
    addChoice(result, "oversampling", "Oversampling", { "1x", "2x", "4x", "8x" }, 0);
    addBool(result, "linear_phase", "Linear Phase", false);
    addChoice(result, "linear_quality", "Linear Phase Quality",
              { "Eco 1024", "Medium 2048", "High 4096" }, 2);
    addChoice(result, "auto_gain_mode", "Auto Gain Mode", { "Off", "Regular", "Smart" }, 1);
    addBool(result, "plugin_enabled", "Plugin Enabled", true);
    addFloat(result, "transient_split_strength", "Transient Split Strength",
             0.0f, 100.0f, 0.1f, 1.0f, 100.0f, "%");
    addFloat(result, "transient_split_balance", "Transient Split Balance",
             -50.0f, 50.0f, 0.1f, 1.0f, 0.0f, "%");
    addFloat(result, "transient_split_hold", "Transient Split Hold",
             0.0f, 100.0f, 0.1f, 1.0f, 50.0f, "%");
    addFloat(result, "transient_split_smooth", "Transient Split Smooth",
             0.0f, 100.0f, 0.1f, 1.0f, 50.0f, "%");

    const juce::StringArray types { "Res Low Cut", "Res High Cut", "Notch", "Tilt",
                                    "Band Pass", "Bell", "Low Shelf", "High Shelf",
                                    "Low Cut", "High Cut" };
    const juce::StringArray saturation { "Soft Clip", "Diode", "Triode", "Transistor",
                                         "Tape", "Odd / Even", "Phase Distortion", "Sine Erosion" };
    const float frequencies[] { 80.0f, 250.0f, 500.0f, 1000.0f,
                                2000.0f, 4000.0f, 8000.0f, 12000.0f };

    for (int band = 1; band <= kNumBands; ++band)
    {
        const auto prefix = "b" + juce::String(band) + "_";
        const auto display = "Band " + juce::String(band) + " ";
        addBool(result, prefix + "present", display + "Present", false);
        addBool(result, prefix + "on", display + "On", false);
        addChoice(result, prefix + "type", display + "Type", types, 5);
        addFloat(result, prefix + "slope", display + "Slope", 3.0f, 96.0f, 0.1f, 1.0f, 12.0f, "dB/oct");
        addChoice(result, prefix + "placement_mode", display + "Placement Mode", { "L/R", "M/S", "T/S" }, 0);
        addFloat(result, prefix + "placement", display + "Placement", -100.0f, 100.0f, 0.1f, 1.0f, 0.0f, "%");
        addFloat(result, prefix + "freq", display + "Freq", 20.0f, 20000.0f, 0.001f, 0.5f,
                 frequencies[(size_t)band - 1]);
        addFloat(result, prefix + "q", display + "Q", 0.1f, 24.0f, 0.001f, 0.5f, 1.0f);
        addFloat(result, prefix + "gain", display + "Gain", -36.0f, 36.0f, 0.01f, 1.0f, 0.0f, "dB");
        addFloat(result, prefix + "drive", display + "Drive", 0.0f, 36.0f, 0.01f, 1.0f, 0.0f, "dB");
        addFloat(result, prefix + "drive_character", display + "Drive Character", -1.0f, 1.0f, 0.001f, 1.0f, 0.0f);
        addFloat(result, prefix + "drive_secondary", display + "Drive Secondary", 0.0f, 1.0f, 0.001f, 1.0f, 0.0f);
        addChoice(result, prefix + "sat_mode", display + "Sat Mode", saturation, 0);
        addChoice(result, prefix + "dyn_mode", display + "Dynamic Mode", { "Downward", "Upward" }, 0);
        addChoice(result, prefix + "sc_source", display + "Sidechain Source", { "Internal", "External" }, 0);
        addFloat(result, prefix + "dyn_lookahead", display + "Lookahead", 0.0f, 5.0f, 0.01f, 1.0f, 0.0f, "ms");
        addFloat(result, prefix + "dyn_thresh", display + "Threshold", -60.0f, 0.0f, 0.1f, 1.0f, 0.0f);
        addFloat(result, prefix + "dyn_range", display + "Range", 0.0f, 24.0f, 0.1f, 1.0f, 6.0f, "dB");
        addFloat(result, prefix + "dyn_ratio", display + "Ratio", 1.0f, 20.0f, 0.1f, 0.5f, 4.0f);
        addFloat(result, prefix + "dyn_speed", display + "Speed", 0.0f, 100.0f, 0.01f, 1.0f, 75.0f);
    }
    return result;
}

void compareStrings(int index, const ExpectedParameter& expected,
                    const char* field, const juce::String& actual,
                    const juce::String& wanted)
{
    if (actual != wanted)
        fail(index, expected.id, field, wanted, actual);
}

void compareFloat(int index, const ExpectedParameter& expected,
                  const char* field, float actual, float wanted)
{
    if (!close(actual, wanted))
        fail(index, expected.id, field, juce::String(wanted, 9), juce::String(actual, 9));
}
}

int runHostParameterRegression()
{
    DefaultEqualizerAudioProcessor processor;
    const auto expected = makeContract();
    const auto& actual = processor.getParameters();

    if ((size_t)actual.size() != expected.size())
    {
        std::printf("FAIL: parameter count expected %zu, got %d\n", expected.size(), actual.size());
        ++failures;
    }

    std::set<juce::String> ids;
    const auto count = std::min((size_t)actual.size(), expected.size());
    for (size_t i = 0; i < count; ++i)
    {
        const auto& wanted = expected[i];
        auto* parameter = actual[(int)i];
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged == nullptr)
        {
            fail((int)i, wanted.id, "type", "RangedAudioParameter", "other");
            continue;
        }

        compareStrings((int)i, wanted, "ID", ranged->paramID, wanted.id);
        compareStrings((int)i, wanted, "display name", parameter->getName(1024), wanted.name);
        compareStrings((int)i, wanted, "label", parameter->getLabel(), wanted.label);
        if (!ids.insert(ranged->paramID).second)
            fail((int)i, wanted.id, "ID uniqueness", "unique", "duplicate");
        if (parameter->getParameterIndex() != (int)i)
            fail((int)i, wanted.id, "host index", juce::String((int)i),
                 juce::String(parameter->getParameterIndex()));

        const auto& range = ranged->getNormalisableRange();
        compareFloat((int)i, wanted, "range start", range.start, wanted.start);
        compareFloat((int)i, wanted, "range end", range.end, wanted.end);
        compareFloat((int)i, wanted, "range interval", range.interval, wanted.interval);
        compareFloat((int)i, wanted, "range skew", range.skew, wanted.skew);
        if (range.symmetricSkew)
            fail((int)i, wanted.id, "symmetric skew", "false", "true");
        compareFloat((int)i, wanted, "plain default",
                     ranged->convertFrom0to1(parameter->getDefaultValue()), wanted.defaultPlain);
        compareFloat((int)i, wanted, "normalised default", parameter->getDefaultValue(),
                     range.convertTo0to1(wanted.defaultPlain));

        const bool expectedDiscrete = wanted.kind != Kind::floating;
        const bool expectedBoolean = wanted.kind == Kind::boolean;
        if (!parameter->isAutomatable()) fail((int)i, wanted.id, "automatable", "true", "false");
        if (parameter->isMetaParameter()) fail((int)i, wanted.id, "meta", "false", "true");
        if (parameter->isOrientationInverted()) fail((int)i, wanted.id, "orientation inverted", "false", "true");
        if (parameter->isDiscrete() != expectedDiscrete)
            fail((int)i, wanted.id, "discrete", expectedDiscrete ? "true" : "false",
                 parameter->isDiscrete() ? "true" : "false");
        if (parameter->isBoolean() != expectedBoolean)
            fail((int)i, wanted.id, "boolean", expectedBoolean ? "true" : "false",
                 parameter->isBoolean() ? "true" : "false");
        if (parameter->getCategory() != juce::AudioProcessorParameter::genericParameter)
            fail((int)i, wanted.id, "category", "genericParameter",
                 juce::String((int)parameter->getCategory()));

        if (wanted.kind == Kind::choice)
        {
            const auto values = parameter->getAllValueStrings();
            if (values != wanted.choices)
                fail((int)i, wanted.id, "choice labels", wanted.choices.joinIntoString("|"),
                     values.joinIntoString("|"));
        }
    }

    if (failures == 0)
        std::printf("Host parameter contract: PASS (%zu stable parameters)\n", expected.size());
    return failures == 0 ? 0 : 1;
}
