#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace deq::loudness
{
class KWeightedMeter
{
public:
    void prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(1.0, newSampleRate);
        const auto shelf = makeHighShelf(sampleRate, 1681.974450955533,
                                         3.999843853973347, 0.7071752369554196);
        const auto highPass = makeHighPass(sampleRate, 38.13547087602444,
                                           0.5003270373238773);
        for (auto& channel : channels)
        {
            channel.shelf.coefficients = shelf;
            channel.highPass.coefficients = highPass;
        }
        segmentLength = std::max<int64_t>(1, (int64_t)std::llround(sampleRate * 0.1));
        reset();
    }

    void reset() noexcept
    {
        for (auto& channel : channels)
        {
            channel.shelf.reset();
            channel.highPass.reset();
        }
        clearWindow();
    }

    void clearWindow() noexcept
    {
        segmentEnergy.fill(0.0);
        segmentFrames.fill(0);
        currentSegment = 0;
        accumulatedFrames = 0;
        completedSegments = 0;
        windowEnergy = 0.0;
        windowFrames = 0;
        integratedEnergy = 0.0;
        integratedFrames = 0;
    }

    void pushSample(float left, float right) noexcept
    {
        const double weightedLeft = channels[0].process(left);
        const double weightedRight = channels[1].process(right);
        const double frameEnergy = weightedLeft * weightedLeft
            + weightedRight * weightedRight;
        segmentEnergy[(size_t)currentSegment] += frameEnergy;
        integratedEnergy += frameEnergy;
        ++integratedFrames;
        ++segmentFrames[(size_t)currentSegment];
        accumulatedFrames = std::min<int64_t>(accumulatedFrames + 1,
                                              segmentLength * segmentCount);
        if (segmentFrames[(size_t)currentSegment] >= segmentLength)
        {
            ++completedSegments;
            if (completedSegments >= segmentCount)
            {
                windowEnergy = 0.0;
                windowFrames = 0;
                for (int segment = 0; segment < segmentCount; ++segment)
                {
                    windowEnergy += segmentEnergy[(size_t)segment];
                    windowFrames += segmentFrames[(size_t)segment];
                }
            }
            currentSegment = (currentSegment + 1) % segmentCount;
            segmentEnergy[(size_t)currentSegment] = 0.0;
            segmentFrames[(size_t)currentSegment] = 0;
        }
    }

    int64_t getFrameCount() const noexcept { return accumulatedFrames; }
    int64_t getCompletedSegmentCount() const noexcept { return completedSegments; }
    bool hasCompleteWindow() const noexcept { return windowFrames > 0; }

    float getLoudnessLufs() const noexcept
    {
        if (windowFrames <= 0 || windowEnergy <= 1.0e-20)
            return -std::numeric_limits<float>::infinity();
        return (float)(-0.691 + 10.0 * std::log10(windowEnergy / (double)windowFrames));
    }

    float getIntegratedLoudnessLufs() const noexcept
    {
        if (integratedFrames <= 0 || integratedEnergy <= 1.0e-20)
            return -std::numeric_limits<float>::infinity();
        return (float)(-0.691
            + 10.0 * std::log10(integratedEnergy / (double)integratedFrames));
    }

private:
    struct Coefficients
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    };

    struct Biquad
    {
        Coefficients coefficients;
        double z1 = 0.0, z2 = 0.0;

        void reset() noexcept { z1 = z2 = 0.0; }

        double process(double input) noexcept
        {
            const double output = coefficients.b0 * input + z1;
            z1 = coefficients.b1 * input - coefficients.a1 * output + z2;
            z2 = coefficients.b2 * input - coefficients.a2 * output;
            return output;
        }
    };

    struct Channel
    {
        Biquad shelf, highPass;
        double process(double input) noexcept
        {
            return highPass.process(shelf.process(input));
        }
    };

    static Coefficients makeHighShelf(double sr, double frequency,
                                      double gainDb, double q) noexcept
    {
        const double k = std::tan(3.14159265358979323846 * frequency / sr);
        const double vh = std::pow(10.0, gainDb / 20.0);
        const double vb = std::pow(vh, 0.4996667741545416);
        const double denominator = 1.0 + k / q + k * k;
        return {
            (vh + vb * k / q + k * k) / denominator,
            2.0 * (k * k - vh) / denominator,
            (vh - vb * k / q + k * k) / denominator,
            2.0 * (k * k - 1.0) / denominator,
            (1.0 - k / q + k * k) / denominator
        };
    }

    static Coefficients makeHighPass(double sr, double frequency, double q) noexcept
    {
        const double k = std::tan(3.14159265358979323846 * frequency / sr);
        const double denominator = 1.0 + k / q + k * k;
        return {
            1.0 / denominator,
            -2.0 / denominator,
            1.0 / denominator,
            2.0 * (k * k - 1.0) / denominator,
            (1.0 - k / q + k * k) / denominator
        };
    }

    double sampleRate = 48000.0;
    std::array<Channel, 2> channels;
    static constexpr int segmentCount = 4;
    std::array<double, segmentCount> segmentEnergy {};
    std::array<int64_t, segmentCount> segmentFrames {};
    int currentSegment = 0;
    int64_t segmentLength = 4800;
    int64_t accumulatedFrames = 0;
    int64_t completedSegments = 0;
    double windowEnergy = 0.0;
    int64_t windowFrames = 0;
    double integratedEnergy = 0.0;
    int64_t integratedFrames = 0;
};
}
