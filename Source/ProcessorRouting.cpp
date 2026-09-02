#include "PluginProcessor.h"

void DefaultEqualizerAudioProcessor::updateReportedLatency() noexcept
{
    const bool linear = linearPhaseParam->load(std::memory_order_relaxed) > 0.5f;
    const int order = (int)oversamplingParam->load(std::memory_order_relaxed);
    bool normalDriven = false;
    bool transientDriven = false;
    bool transientRouting = false;
    for (const auto& p : bandParams)
    {
        const bool active=p.present->load(std::memory_order_relaxed)>0.5f&&p.on->load(std::memory_order_relaxed)>0.5f;
        const bool ts=active&&(int)p.placementMode->load(std::memory_order_relaxed)==2;
        transientRouting=transientRouting||ts;
        const bool driven=active&&p.drive->load(std::memory_order_relaxed)>0.0001f;
        normalDriven=normalDriven||(driven&&!ts);
        transientDriven=transientDriven||(driven&&ts);
    }
    const auto* os = (normalDriven||transientDriven)&&order>0&&order<=kNumOversamplingOrders
        ? oversamplers[(size_t)(order - 1)].get() : nullptr;
    const int onePassLatency=os!=nullptr?juce::roundToInt(os->getLatencyInSamples()):0;
    // T/S drive runs before the recombined signal enters any ordinary-band
    // drive, so mixed routing legitimately traverses two oversampling passes.
    const int osLatency=(normalDriven?onePassLatency:0)+(transientDriven?onePassLatency:0);
    setLatencySamples((linear ? currentLinearPhaseLatency() : 0) + osLatency
                      + (transientRouting ? transientSplitter.latency() : 0)
                      + requestedLookaheadSamples());
}

int DefaultEqualizerAudioProcessor::requestedLookaheadSamples() const noexcept
{
    float maximumMs = 0.0f;
    for (const auto& p : bandParams)
        if (p.present->load(std::memory_order_relaxed) > 0.5f
            && p.on->load(std::memory_order_relaxed) > 0.5f
            && p.dynThresh->load(std::memory_order_relaxed) < -0.05f
            && p.dynLookahead->load(std::memory_order_relaxed) > 0.001f)
            maximumMs = std::max(maximumMs, p.dynLookahead->load(std::memory_order_relaxed));
    return juce::roundToInt(sr * maximumMs * 0.001);
}

int DefaultEqualizerAudioProcessor::requestedBandLookaheadSamples(
    int index,int maximumDelaySamples) const noexcept
{
    if(index<0||index>=kNumBands)return 0;
    const auto& p=bandParams[(size_t)index];
    if(p.present->load(std::memory_order_relaxed)<0.5f
        ||p.on->load(std::memory_order_relaxed)<0.5f
        ||p.dynThresh->load(std::memory_order_relaxed)>=-0.05f)
        return 0;
    return std::clamp(juce::roundToInt(sr*p.dynLookahead->load(std::memory_order_relaxed)*0.001),
                      0,maximumDelaySamples);
}

void DefaultEqualizerAudioProcessor::routeLookahead(juce::AudioBuffer<float>& buffer,
                                                      const float* sidechainL,
                                                      const float* sidechainR,
                                                      int maximumDelaySamples) noexcept
{
    juce::ignoreUnused(sidechainR);
    if (maximumDelaySamples <= 0) return;
    const int ringSize = lookaheadDelayBuffer.getNumSamples();
    if(ringSize<=0||ringSize<=maximumDelaySamples)return;
    const int samples = buffer.getNumSamples();
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;
    auto* delayL = lookaheadDelayBuffer.getWritePointer(0);
    auto* delayR = lookaheadDelayBuffer.getWritePointer(1);
    auto* externalDelayL=externalLookaheadDelayBuffer.getWritePointer(0);
    auto* externalDelayR=externalLookaheadDelayBuffer.getWritePointer(1);
    std::array<float*,kNumBands> internalL{},internalR{},externalL{},externalR{};
    std::array<int,kNumBands> detectorDelays{};
    for(int band=0;band<kNumBands;++band)
    {
        internalL[(size_t)band]=internalBandDetectorBuffers[(size_t)band].getWritePointer(0);
        internalR[(size_t)band]=internalBandDetectorBuffers[(size_t)band].getWritePointer(1);
        externalL[(size_t)band]=externalBandDetectorBuffers[(size_t)band].getWritePointer(0);
        externalR[(size_t)band]=externalBandDetectorBuffers[(size_t)band].getWritePointer(1);
        detectorDelays[(size_t)band]=maximumDelaySamples
            -requestedBandLookaheadSamples(band,maximumDelaySamples);
    }
    for(int i=0;i<samples;++i)
    {
        const float dryL=detectorInputBuffer.getSample(0,i);
        const float dryR=detectorInputBuffer.getSample(1,i);
        const float externalDryL=sidechainL!=nullptr?externalDetectorInputBuffer.getSample(0,i):0.0f;
        const float externalDryR=sidechainL!=nullptr?externalDetectorInputBuffer.getSample(1,i):0.0f;
        for(int band=0;band<kNumBands;++band)
        {
            const int delay=detectorDelays[(size_t)band];
            int read=lookaheadWritePosition-delay;
            if(read<0)read+=ringSize;
            internalL[(size_t)band][i]=delay==0?dryL:delayL[read];
            internalR[(size_t)band][i]=delay==0?dryR:delayR[read];
            externalL[(size_t)band][i]=delay==0?externalDryL:externalDelayL[read];
            externalR[(size_t)band][i]=delay==0?externalDryR:externalDelayR[read];
        }
        int audioRead=lookaheadWritePosition-maximumDelaySamples;
        if(audioRead<0)audioRead+=ringSize;
        const float wetL=maximumDelaySamples==0?dryL:delayL[audioRead];
        const float wetR=maximumDelaySamples==0?dryR:delayR[audioRead];
        const float mix = lookaheadMix.getNextValue();
        left[i]=dryL+mix*(wetL-dryL);
        if(buffer.getNumChannels()>1)right[i]=dryR+mix*(wetR-dryR);
        delayL[lookaheadWritePosition]=dryL;
        delayR[lookaheadWritePosition]=dryR;
        externalDelayL[lookaheadWritePosition]=externalDryL;
        externalDelayR[lookaheadWritePosition]=externalDryR;
        if(++lookaheadWritePosition>=ringSize)lookaheadWritePosition=0;
    }
}
