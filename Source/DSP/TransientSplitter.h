#pragma once

// Transient/steady separation adapted from ZLSplitter revision
// 2f50824ab925eeff7950986eac640dab43c3ce67 (AGPL-3.0).
// Copyright (C) 2026 zsliu98. The FFT backend is JUCE rather than KFR;
// the 75%-overlap windows, 5x5 median masks and parameter transforms are kept.

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>

class TransientSplitter
{
    struct Median5
    {
        std::array<float,5> values{}; int pos=0;
        void clear(){ values.fill(0); pos=0; }
        void insert(float x){ values[(size_t)pos]=x; pos=(pos+1)%5; }
        float median() const { auto v=values; std::sort(v.begin(),v.end()); return v[2]; }
    };
    struct Channel
    {
        int order=10,n=1024,hop=256,pos=0,count=0,linePos=0,delayPos=0;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> input,output,fftData,magnitude,mask,window,delay;
        std::array<std::vector<float>,3> lines;
        std::vector<Median5> timeMedian;
        void prepare(int newOrder)
        {
            order=newOrder; n=1<<order; hop=n/4; pos=count=linePos=delayPos=0;
            fft=std::make_unique<juce::dsp::FFT>(order);
            input.assign((size_t)n,0); output.assign((size_t)n,0); fftData.assign((size_t)2*n,0);
            magnitude.assign((size_t)n/2+1,0); mask.assign(magnitude.size(),0);
            window.resize((size_t)n); for(int i=0;i<n;++i) window[(size_t)i]=0.5f-0.5f*std::cos(2.0f*juce::MathConstants<float>::pi*i/n);
            for(auto& l:lines) l.assign((size_t)2*n,0);
            timeMedian.assign(magnitude.size(),{});
            delay.assign((size_t)(n+2*hop+1),0);
        }
        void reset()
        {
            std::fill(input.begin(),input.end(),0); std::fill(output.begin(),output.end(),0);
            std::fill(mask.begin(),mask.end(),0); std::fill(delay.begin(),delay.end(),0);
            for(auto& l:lines)std::fill(l.begin(),l.end(),0); for(auto& m:timeMedian)m.clear();
            pos=count=linePos=delayPos=0;
        }
        static float portion(float transient,float steady,float balance,float separation)
        {
            const float t=transient*balance,tt=t*t,ss=steady*steady;
            const float p=tt/std::max(tt+ss,1.0e-8f);
            return std::clamp((p-0.5f)*separation,-5.0f,0.5f)+0.5f;
        }
        void processFrame(float balance,float separation,float hold,float smooth)
        {
            std::fill(fftData.begin(),fftData.end(),0);
            for(int i=0;i<n;++i) fftData[(size_t)i]=input[(size_t)((pos+i)%n)]*window[(size_t)i];
            fft->performRealOnlyForwardTransform(fftData.data(),true);
            const int bins=n/2+1;
            magnitude[0]=std::abs(fftData[0]); magnitude[(size_t)bins-1]=std::abs(fftData[1]);
            for(int i=1;i<bins-1;++i) magnitude[(size_t)i]=std::hypot(fftData[(size_t)2*i],fftData[(size_t)2*i+1]);
            Median5 freqMedian; freqMedian.clear(); freqMedian.insert(magnitude[0]); freqMedian.insert(magnitude[0]);
            freqMedian.insert(magnitude[0]); freqMedian.insert(magnitude[std::min(1,bins-1)]);
            for(int i=0;i<bins;++i)
            {
                freqMedian.insert(magnitude[(size_t)std::min(bins-1,i+2)]);
                timeMedian[(size_t)i].insert(magnitude[(size_t)i]);
                const float current=portion(freqMedian.median(),timeMedian[(size_t)i].median(),balance,separation);
                mask[(size_t)i]=std::max(mask[(size_t)i]*hold,current);
            }
            lines[(size_t)linePos]=fftData; linePos=(linePos+1)%3; fftData=lines[(size_t)linePos];
            float mean=0; for(float v:mask)mean+=v; mean/=std::max(1,bins);
            mean=std::clamp((mean-0.5f)*std::sqrt(separation),-0.5f,0.5f)+0.5f;
            const auto apply=[&](int re,int im,int bin){ const float m=(mean-mask[(size_t)bin])*smooth+mask[(size_t)bin]; fftData[(size_t)re]*=m; if(im>=0)fftData[(size_t)im]*=m; };
            apply(0,-1,0); apply(1,-1,bins-1); for(int i=1;i<bins-1;++i)apply(2*i,2*i+1,i);
            fft->performRealOnlyInverseTransform(fftData.data());
            for(int i=0;i<n;++i) output[(size_t)((pos+i)%n)] += fftData[(size_t)i]*window[(size_t)i]*(2.0f/3.0f);
        }
        void process(const float* in,float* transient,float* sustain,int samples,float balance,float separation,float hold,float smooth)
        {
            const int delaySize=(int)delay.size();
            for(int i=0;i<samples;++i)
            {
                input[(size_t)pos]=in[i]; transient[i]=output[(size_t)pos]; output[(size_t)pos]=0;
                const int read=(delayPos+1)%delaySize; const float dry=delay[(size_t)read]; delay[(size_t)delayPos]=in[i]; delayPos=read;
                sustain[i]=dry-transient[i]; pos=(pos+1)%n;
                if(++count==hop){count=0;processFrame(balance,separation,hold,smooth);}
            }
        }
    };
public:
    void prepare(double sampleRate,int maxBlock)
    {
        juce::ignoreUnused(maxBlock); int order=sampleRate<=50000?10:sampleRate<=100000?11:sampleRate<=200000?12:13;
        for(auto& c:channels)c.prepare(order); latencySamples=(1<<order)+2*((1<<order)/4);
    }
    void reset(){for(auto& c:channels)c.reset();}
    int latency()const noexcept{return latencySamples;}
    void setParameters(float strengthPercent,float balancePercent,float holdPercent,float smoothPercent)
    {
        separation=std::exp(std::clamp(strengthPercent,0.0f,100.0f)*0.01f*4.0f)-1.0f;
        balance=std::pow(16.0f,std::clamp(balancePercent,-50.0f,50.0f)*0.01f+0.5f-0.75f);
        const float h=std::clamp(holdPercent,0.0f,100.0f)*0.01f;
        hold=(32.0f-std::pow(32.0f,1.0f-h))/31.0f*0.75f+0.24f;
        smooth=std::clamp(smoothPercent,0.0f,100.0f)*0.01f;
    }
    void process(const juce::AudioBuffer<float>& input,juce::AudioBuffer<float>& transient,juce::AudioBuffer<float>& sustain,int samples)
    {
        for(int ch=0;ch<2;++ch) channels[(size_t)ch].process(input.getReadPointer(std::min(ch,input.getNumChannels()-1)),
            transient.getWritePointer(ch),sustain.getWritePointer(ch),samples,balance,separation,hold,smooth);
    }
private:
    std::array<Channel,2> channels; int latencySamples=1536;
    float balance=0.5f,separation=std::exp(4.0f)-1.0f,hold=0.9f,smooth=0.5f;
};
