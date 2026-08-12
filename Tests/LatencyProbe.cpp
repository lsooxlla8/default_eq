#include <juce_dsp/juce_dsp.h>
#include <cstdio>

int main()
{
    std::printf("minimum_phase=0\nlinear_phase=2048\n");
    for (int order = 1; order <= 3; ++order)
    {
        juce::dsp::Oversampling<float> os(
            2, order, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os.initProcessing(512);
        const auto latency = juce::roundToInt(os.getLatencyInSamples());
        std::printf("drive_%dx=%d\n", 1 << order, latency);
        if (latency <= 0)
            return 1;
    }
    return 0;
}
