#pragma once

// Filter-design equations adapted from ZLEqualizer, revision
// 02c517e35f0ef8460c15815f303051dffdb0895a (AGPL-3.0).
// Copyright (C) 2026 zsliu98. See THIRD_PARTY_NOTICES.md.

#include "Biquad.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <complex>

namespace zl_filter
{
constexpr int maxStages = 16;
constexpr std::array<float, 7> slopes { 6.0f, 12.0f, 24.0f, 36.0f, 48.0f, 72.0f, 96.0f };

inline bool isClassicCut(Biquad::Type t) noexcept
{ return t == Biquad::Type::HighPass || t == Biquad::Type::LowPass; }
inline bool isResonantCut(Biquad::Type t) noexcept
{ return t == Biquad::Type::ResHighPass || t == Biquad::Type::ResLowPass; }
inline bool supportsSix(Biquad::Type t) noexcept
{ return isResonantCut(t) || t == Biquad::Type::LowShelf || t == Biquad::Type::HighShelf || t == Biquad::Type::Tilt; }

constexpr std::array<double, 9> flatTiltFrequencies {
    10.0, 40.0, 160.0, 640.0, 2560.0, 10240.0, 40960.0, 163840.0, 655360.0
};

inline int flatTiltStageCount(double sampleRate) noexcept
{
    if (sampleRate < 12500.0) return 5;
    if (sampleRate < 50000.0) return 6;
    if (sampleRate < 200000.0) return 7;
    if (sampleRate < 800000.0) return 8;
    return 9;
}

inline float snapSlope(Biquad::Type t, float value) noexcept
{
    const size_t first = supportsSix(t) ? 0u : 1u;
    size_t best = first;
    for (size_t i = first + 1; i < slopes.size(); ++i)
        if (std::abs(value - slopes[i]) < std::abs(value - slopes[best])) best = i;
    return slopes[best];
}
inline int orderForSlope(Biquad::Type t, float slope) noexcept
{
    const auto snapped = snapSlope(t, slope);
    return std::max(1, (int)std::lround(snapped / 6.0f));
}

namespace coeff
{
constexpr double sigma = 2.00143, pi = 3.14159265358979323846;
constexpr double pi2 = pi*pi, twoPi2=2*pi2, fourPi2=4*pi2, sigma2=sigma*sigma, sigma4=sigma2*sigma2;
constexpr double phi0=(pi-sigma)/(pi+sigma), lpA1=2*phi0, lpA2=phi0*phi0;
constexpr double lpG=1.0/(1.0+lpA1+lpA2), bpA1=phi0-1.0, bpA2=-phi0;
constexpr double bpMul=fourPi2/(pi*(1.0+phi0)), dbExp=0.16609640474436813;
struct Phi { double p1,p2,d; };
inline double wrap(double w)
{
    if (w <= 0.4996419767299294) { const auto t=std::tan(pi*w); return sigma2+pi2/(t*t); }
    if (w >= 0.5919981009) return 1.0/(w*w);
    const double t=(w-0.4996419767299294)*10.8276522968691715;
    return t*(t*(1.4079737347730696*t-2.5538880428234330)-0.0064417990487326)+4.0057345308722958;
}
inline double phi(double w2) { const auto r=std::sqrt(w2+sigma2); return (pi-r)/(pi+r); }
inline Phi phiSq(double w2,double z2)
{
    const auto k=w2*(2*z2-1)+sigma2;
    const auto v=std::sqrt(std::abs(w2*w2+2*sigma2*w2*(2*z2-1)+sigma4));
    const auto s=std::sqrt(std::abs(2*(v+k))), d=pi2+pi*s+v;
    return {(twoPi2-2*v)/d,(pi2-pi*s+v)/d,d};
}
inline Phi notchPhi(double w2)
{
    const auto k=sigma2-w2, v=std::abs(w2-sigma2), s=std::sqrt(std::abs(2*(v+k))), d=pi2+pi*s+v;
    return {(twoPi2-2*v)/d,(pi2-pi*s+v)/d,d};
}
using C=std::array<double,5>;
inline C one(bool hp,double w)
{
    const auto w2=wrap(w), b=phi(w2);
    const auto f=hp ? std::sqrt(w2)/(2*pi)*(1+b) : (1+b)/(1+phi0);
    return {b,0,f,hp?-f:f*phi0,0};
}
inline C oneShelf(Biquad::Type t,double w,double g)
{
    const auto w2=wrap(w), gl=std::exp2(g*dbExp);
    double a,b,f;
    if (t==Biquad::Type::LowShelf) { a=phi(w2/gl); b=phi(w2*gl); f=gl*(1+b)/(1+a); }
    else if (t==Biquad::Type::HighShelf) { a=phi(w2*gl); b=phi(w2/gl); f=(1+b)/(1+a); }
    else { const auto gh=std::sqrt(gl); a=phi(w2*gl); b=phi(w2/gl); f=(1/gh)*(1+b)/(1+a); }
    return {b,0,f,f*a,0};
}
inline C pass(bool hp,double w,double q)
{
    const auto w2=wrap(w); const auto p=phiSq(w2,1/(4*q*q));
    const auto f=hp ? w2/p.d : lpG*(1+p.p1+p.p2);
    return {p.p1,p.p2,f,hp?-2*f:f*lpA1,hp?f:f*lpA2};
}
inline C band(bool notch,double w,double q)
{
    const auto w2=wrap(w); const auto z=1/(2*q); const auto b=phiSq(w2,z*z);
    if (notch) { const auto a=notchPhi(w2); const auto f=a.d/b.d; return {b.p1,b.p2,f,f*a.p1,f*a.p2}; }
    const auto f=std::sqrt(w2)*z*bpMul/b.d; return {b.p1,b.p2,f,f*bpA1,f*bpA2};
}
inline C peak(double w,double g,double q)
{
    const auto w2=wrap(w), z2=1/(4*q*q), gl=std::exp2(g*dbExp);
    const auto a=phiSq(w2,z2*gl); const auto b=phiSq(w2,z2/gl); const auto f=a.d/b.d;
    return {b.p1,b.p2,f,f*a.p1,f*a.p2};
}
inline C shelf(Biquad::Type t,double w,double g,double q)
{
    const auto w2=wrap(w), z2=1/(4*q*q), gh=std::exp2(g*dbExp*0.5);
    Phi a{},b{}; double f=1;
    if (t==Biquad::Type::LowShelf) { a=phiSq(w2/gh,z2); b=phiSq(w2*gh,z2); f=gh*gh*a.d/b.d; }
    else if (t==Biquad::Type::HighShelf) { a=phiSq(w2*gh,z2); b=phiSq(w2/gh,z2); f=a.d/b.d; }
    else { a=phiSq(w2*gh,z2); b=phiSq(w2/gh,z2); f=(1/gh)*a.d/b.d; }
    return {b.p1,b.p2,f,f*a.p1,f*a.p2};
}
}

inline void assign(Biquad& b,const coeff::C& c,int rampSamples)
{ b.setCoefficients(c[2],c[3],c[4],c[0],c[1],rampSamples); }

struct Cascade
{
    std::array<Biquad,maxStages> stages{};
    int count=0;
    bool coefficientRamping=false;
    void reset() { for (auto& s:stages) s.reset(); coefficientRamping=false; }
    void configure(Biquad::Type type,double sr,double f,double q,double gain,float slope,
                   int rampSamples=0)
    {
        count=0; coefficientRamping=rampSamples>0;
        f=std::clamp(f,10.0,sr*0.49); q=std::clamp(q,0.1,24.0);
        const int n=orderForSlope(type,slope); const double w=f/sr;
        auto add=[&](const coeff::C& c){ if(count<maxStages) assign(stages[(size_t)count++],c,rampSamples); };
        if(type==Biquad::Type::Tilt)
        {
            // ZLEqualizer's Flat Tilt: a bank of very broad first-order high
            // shelves, normalised so the requested pivot remains at 0 dB.
            const int number=flatTiltStageCount(sr);
            const double shelfGain=gain*0.5;
            const double shelfLinear=std::pow(10.0,shelfGain/20.0);
            const double shelfLinearSq=shelfLinear*shelfLinear;
            double magnitudeSq=1.0;
            for(int i=0;i<number;++i)
            {
                const double ratioSq=(f*f)/(flatTiltFrequencies[(size_t)i]*flatTiltFrequencies[(size_t)i]);
                magnitudeSq *= (shelfLinearSq*ratioSq+shelfLinear)/(ratioSq+shelfLinear);
            }
            const double makeup=1.0/std::sqrt(std::max(1.0e-24,magnitudeSq));
            for(int i=0;i<number;++i)
            {
                auto c=coeff::oneShelf(Biquad::Type::HighShelf,
                                       flatTiltFrequencies[(size_t)i]/sr,shelfGain);
                if(i==number-1) { c[2]*=makeup; c[3]*=makeup; c[4]*=makeup; }
                add(c);
            }
            return;
        }
        if (isResonantCut(type))
        {
            const bool hp=type==Biquad::Type::ResHighPass;
            if(n==1) { add(coeff::one(hp,w)); return; }
            const int number=n/2; const auto theta0=kPi/(double)number/4.0;
            const auto scale=std::pow(std::sqrt(2.0)*q,1.0/(double)number);
            const auto rb=std::log10(std::sqrt(2.0)*q)/std::pow((double)n,1.5)*12.0;
            for(int i=0;i<number;++i) { const auto centered=(double)i-number/2.0+0.5;
                const auto sq=1.0/(2.0*std::cos(theta0*(2*i+1)))*scale*std::pow(2.0,centered*rb);
                add(coeff::pass(hp,w,sq)); }
            return;
        }
        if(type==Biquad::Type::Bandpass || type==Biquad::Type::Notch)
        {
            const int number=n/2; const auto halfbw=std::asinh(0.5/q)/std::log(2.0);
            const auto wl=w/std::pow(2.0,halfbw), g=std::pow(10.0,(-6.0/n)/20.0);
            const auto sq=type==Biquad::Type::Bandpass
                ? std::sqrt(1-g*g)*wl*w/g/(w*w-wl*wl)
                : g*wl*w/std::sqrt(1-g*g)/(w*w-wl*wl);
            const auto c=coeff::band(type==Biquad::Type::Notch,w,sq);
            for(int i=0;i<number;++i) add(c); return;
        }
        if(type==Biquad::Type::Bell && n==2) { add(coeff::peak(w,gain,q)); return; }
        if(type==Biquad::Type::Bell)
        {
            const auto halfbw=std::asinh(0.5/q)/std::log(2.0), scale=std::pow(2.0,halfbw);
            const double w1=w/scale,w2=w*scale; const int number=n/2;
            const auto design=[&](double wf,double g) { const auto each=g/number;
                const auto theta0=kPi/(double)number/4.0;
                for(int i=0;i<number;++i) add(coeff::shelf(Biquad::Type::LowShelf,wf,each,1.0/(2*std::cos(theta0*(2*i+1))))); };
            design(w1,-gain); design(w2,gain); return;
        }
        if(type==Biquad::Type::LowShelf || type==Biquad::Type::HighShelf)
        {
            if(n==1) { add(coeff::oneShelf(type,w,gain)); return; }
            const int number=n/2; const auto each=n==2?gain:gain/number;
            const auto theta0=kPi/(double)number/4.0;
            const auto qm=std::sqrt(q*std::sqrt(2.0))/std::sqrt(2.0);
            const auto scale=std::pow(std::sqrt(2.0)*qm,1.0/(double)number);
            const auto rb=std::log10(std::sqrt(2.0)*qm)/std::pow((double)n,1.5)*12.0;
            for(int i=0;i<number;++i) { const auto centered=(double)i-number/2.0+0.5;
                const auto sq=1.0/(2*std::cos(theta0*(2*i+1)))*scale*std::pow(2.0,centered*rb);
                add(coeff::shelf(type,w,each,sq)); } return;
        }
    }
    float processL(float x)
    {
        if(coefficientRamping)
        {
            bool remains=false;
            for(int i=0;i<count;++i)
            {
                stages[(size_t)i].advanceCoefficientRamp();
                remains=remains||stages[(size_t)i].coefficientRampRemaining>0;
            }
            coefficientRamping=remains;
        }
        for(int i=0;i<count;++i)x=stages[(size_t)i].processL(x);
        return x;
    }
    float processR(float x) { for(int i=0;i<count;++i)x=stages[(size_t)i].processR(x); return x; }
    void processStereo(float& left, float& right)
    {
        if(coefficientRamping)
        {
            bool remains=false;
            for(int i=0;i<count;++i)
            {
                stages[(size_t)i].advanceCoefficientRamp();
                remains=remains||stages[(size_t)i].coefficientRampRemaining>0;
            }
            coefficientRamping=remains;
        }
        if (count == 1)
        {
            stages[0].processStereo(left, right);
            return;
        }
        for(int i=0;i<count;++i)
            stages[(size_t)i].processStereo(left, right);
    }
    bool isCoefficientRamping() const noexcept { return coefficientRamping; }
    void processStereoBlock(float* left, float* right, int numSamples)
    {
        if (coefficientRamping)
        {
            for (int sample = 0; sample < numSamples; ++sample)
                processStereo(left[sample], right[sample]);
            return;
        }
        for (int stage = 0; stage < count; ++stage)
            stages[(size_t)stage].processStereoBlock(left, right, numSamples);
    }
    std::complex<double> response(double frequency,double sr) const
    {
        std::complex<double> h{1,0}; const auto z=std::polar(1.0,-2*kPi*frequency/sr);
        for(int i=0;i<count;++i) { const auto& b=stages[(size_t)i]; h*=(b.b0+b.b1*z+b.b2*z*z)/(1.0+b.a1*z+b.a2*z*z); }
        return h;
    }
};
}
