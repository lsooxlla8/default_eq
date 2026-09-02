#include "PluginProcessor.h"
#include "DSP/FilterTypes.h"
#include "DSP/VariableSlope.h"
#include <complex>

namespace
{
juce::String bandId(int idx, const char* suffix)
{
    return "b" + juce::String(idx) + "_" + suffix;
}
}

void DefaultEqualizerAudioProcessor::buildLinearPhaseMagnitude()
{
    // Build composite magnitude response for the linear phase FIR.
    // Use the same logic as ResponseCurveComponent but at FFT resolution.
    // Centered L/R static bands are synthesized into this FIR. Asymmetrically
    // placed L/R, M/S or T/S bands,
    // dynamic modulation, and per-band drive remain in the post-FIR stage.
    const int numBins = LinearPhaseEngine::firLength / 2 + 1;

    // Use pre-allocated member buffer (avoid heap allocation on audio thread)
    linPhaseMagBuf.resize((size_t)numBins);
    std::fill(linPhaseMagBuf.begin(), linPhaseMagBuf.end(), 0.0f);
    float* magDb = linPhaseMagBuf.data();
    const float amount = apvts.getRawParameterValue("scale")->load();
    const float shiftSemitones = apvts.getRawParameterValue("shift")->load();

    for (int b = 0; b < kNumBands; ++b)
    {
        const int idx = b + 1;
        const bool on = apvts.getRawParameterValue(bandId(idx, "present"))->load() > 0.5f
            && apvts.getRawParameterValue(bandId(idx, "on"))->load() > 0.5f;
        if (!on) continue;
        const bool routed = apvts.getRawParameterValue(bandId(idx, "placement_mode"))->load() > 0.5f
            || std::abs(apvts.getRawParameterValue(bandId(idx, "placement"))->load()) > 0.001f
            || apvts.getRawParameterValue(bandId(idx, "dyn_thresh"))->load() < -0.05f;
        if (routed) continue; // routed bands use their minimum-phase post stage

        const int t = (int)apvts.getRawParameterValue(bandId(idx, "type"))->load();
        const auto tp = deq::filter_types::fromParameterIndex(t);

        const float freq = shiftedFrequency(
            apvts.getRawParameterValue(bandId(idx, "freq"))->load(), shiftSemitones);
        float q    = apvts.getRawParameterValue(bandId(idx, "q"))->load();
        const float rawGain = apvts.getRawParameterValue(bandId(idx, "gain"))->load();
        const bool gainBearing = variable_slope::distributesGain(tp);
        const float gain = gainBearing ? rawGain * amount : rawGain;

        // Apply adaptive Q in linear phase magnitude build
        const bool adaptiveQ = apvts.getRawParameterValue("adaptive_q")->load() > 0.5f;
        if (adaptiveQ)
            q = calculateAdaptiveQ(q, rawGain);

        const float slope = apvts.getRawParameterValue(bandId(idx, "slope"))->load();
        constexpr bool decramp = true;
        const bool cut = zl_filter::isClassicCut(tp) || zl_filter::isResonantCut(tp);
        double responseFreq = freq;
        if (zl_filter::isResonantCut(tp))
            q = EQBand::amountResonantCutQ(q, amount);
        if (cut)
        {
            const double neutralEdge = (tp == Biquad::Type::LowPass || tp == Biquad::Type::ResLowPass)
                ? sr * 0.45 : 10.0;
            responseFreq = neutralEdge * std::pow(std::max(1.0e-9, freq / neutralEdge),
                                                   std::max(0.0f, amount));
            responseFreq = std::clamp(responseFreq, 10.0, sr * 0.45);
        }

        for (int i = 0; i < numBins; ++i)
        {
            const double f = (double)i / (double)(numBins - 1) * sr * 0.5;
            if (f < 1.0) continue;

            const auto rawResponse = variable_slope::response(tp, sr, responseFreq, q, gain,
                                                              slope, decramp, f);
            const double mix = cut ? (double)EQBand::cutAmountMix(amount)
                                   : (double)std::clamp(amount, 0.0f, 1.0f);
            const auto response = gainBearing ? rawResponse
                : std::complex<double>(1.0, 0.0)
                    + mix * (rawResponse - std::complex<double>(1.0, 0.0));
            magDb[(size_t)i] += (float)(20.0 * std::log10(
                std::max(std::abs(response), 1.0e-15)));
        }
    }

    linearPhaseEngine.rebuildFromMagnitude(magDb, numBins, currentLinearPhaseLatency() * 2);
}
