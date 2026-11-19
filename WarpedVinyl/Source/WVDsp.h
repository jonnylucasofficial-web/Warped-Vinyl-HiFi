// Warped Vinyl HiFi – DSP core.
//
// Analog vibrato/chorus: a bucket-brigade-style delay line whose delay time is
// swept by a shapeable LFO. The dry path stays clean; MIX blends dry and wet
// (MIX full CW = pure vibrato, around noon = chorus).
//
//   in ─┬──────────────────────────────────────────────┐
//       └► anti-alias LPF ► BBD delay (LAG + DEPTH·LFO) ►│ reconstruction LPF
//          ► BBD headroom/saturation ► TONE ─────────────┴► MIX ► out
//
// LFO:  RPM sets the rate. WARP moves the peak of the wave (the "center point"):
//       noon = symmetrical, CCW = fast rise / slow fall, CW = slow rise / fast
//       fall. The two shape toggles curve the rising and falling halves
//       independently (sine-ish / linear / exponential).
#pragma once
#include <vector>
#include "../../Common/DspUtil.h"

namespace wv
{
using namespace cbdsp;

enum class Shape { Smooth = 0, Linear = 1, Expo = 2 };

inline float shapeCurve (float u, Shape s)
{
    switch (s)
    {
        case Shape::Smooth: return 0.5f - 0.5f * (float) std::cos (kPi * u);
        case Shape::Expo:   return u * u * u;
        default:            return u;
    }
}

struct Lfo
{
    double phase = 0.0;
    float warp = 0.5f;
    Shape rise = Shape::Linear, fall = Shape::Linear;

    // unipolar output 0..1
    float value() const
    {
        const float w = 0.03f + 0.94f * clamp01 (warp);
        const float p = (float) phase;
        if (p < w) return shapeCurve (p / w, rise);
        return shapeCurve ((1.0f - p) / (1.0f - w), fall);
    }
    void advance (double inc) { phase += inc; phase -= std::floor (phase); }
};

struct Params
{
    float tone = 0.6f, lag = 0.0f, mix = 0.5f, depth = 0.5f, warp = 0.5f;
    float rateHz = 1.0f;
    Shape rise = Shape::Linear, fall = Shape::Linear;
    bool on = true;
};

inline float rpmKnobToHz (float k) { return 0.1f * std::pow (120.0f, clamp01 (k)); } // 0.1–12 Hz

class DelayChannel
{
public:
    void prepare (float sampleRate, int maxSamples)
    {
        fs = sampleRate;
        buf.assign ((size_t) maxSamples + 8, 0.0f); w = 0;
        aaIn1.lowpass (9500.0, 0.54, fs);   aaIn2.lowpass (9500.0, 1.31, fs);
        aaOut1.lowpass (9500.0, 0.54, fs);  aaOut2.lowpass (9500.0, 1.31, fs);
        aaIn1.reset(); aaIn2.reset(); aaOut1.reset(); aaOut2.reset();
        toneLP.reset(); hp.setCutoff (30.0f, fs); hp.reset();
    }

    void setTone (float t)
    {
        const float fc = 1100.0f * std::pow (2.0f, clamp01 (t) * 4.2f);   // ~1.1k – 20k
        toneLP.setCutoff (std::min (fc, 0.45f * fs), fs);
    }

    inline float process (float x, float delaySamples, float noise)
    {
        float in = aaIn2.process (aaIn1.process (x)) + noise;
        buf[(size_t) w] = in;

        const int size = (int) buf.size();
        float rp = (float) w - delaySamples;
        while (rp < 0) rp += (float) size;
        const int i1 = (int) rp;
        const float f = rp - (float) i1;
        const int i0 = (i1 - 1 + size) % size, i2 = (i1 + 1) % size, i3 = (i1 + 2) % size;
        const float y0 = buf[(size_t) i0], y1 = buf[(size_t) i1], y2 = buf[(size_t) i2], y3 = buf[(size_t) i3];
        // 4-point cubic Hermite interpolation
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        float wet = ((c3 * f + c2) * f + c1) * f + y1;

        if (++w >= size) w = 0;

        wet = aaOut2.process (aaOut1.process (wet));
        wet = 1.6f * fastTanh (wet / 1.6f);           // BBD headroom
        wet = toneLP.lp (wet);
        return hp.hp (wet);
    }

private:
    float fs = 48000.0f;
    std::vector<float> buf; int w = 0;
    Biquad aaIn1, aaIn2, aaOut1, aaOut2;
    OnePole toneLP, hp;
};

class Engine
{
public:
    void prepare (float sampleRate, int numChannels)
    {
        fs = sampleRate;
        const int maxSamples = (int) (0.040f * fs) + 16;
        chans.resize ((size_t) std::max (1, numChannels));
        for (auto& c : chans) c.prepare (fs, maxSamples);
        baseSm.setLength (0.08f, fs); depthSm.setLength (0.05f, fs);
        mixSm.setLength (0.03f, fs); bypassSm.setLength (0.012f, fs);
        baseSm.reset (lagToMs (p.lag) * 0.001f * fs);
        depthSm.reset (depthToMs (p.depth) * 0.001f * fs);
        mixSm.reset (p.mix); bypassSm.reset (p.on ? 1.0f : 0.0f);
        lastTone = -1.0f;
    }

    static float lagToMs (float lag)     { return 5.0f + 15.0f * clamp01 (lag); }        // 5–20 ms
    static float depthToMs (float depth) { return 6.0f * std::pow (clamp01 (depth), 1.6f); }

    void setParams (const Params& np)
    {
        p = np;
        baseSm.set (lagToMs (p.lag) * 0.001f * fs);
        depthSm.set (depthToMs (p.depth) * 0.001f * fs);
        mixSm.set (clamp01 (p.mix));
        bypassSm.set (p.on ? 1.0f : 0.0f);
        lfo.warp = p.warp; lfo.rise = p.rise; lfo.fall = p.fall;
        if (std::fabs (p.tone - lastTone) > 0.001f)
        {
            for (auto& c : chans) c.setTone (p.tone);
            lastTone = p.tone;
        }
        noiseGain = dbToGain (-100.0f + 14.0f * clamp01 (p.lag));   // hiss rises with LAG
    }

    void resetPhase() { lfo.phase = 0.0; }
    double getPhase() const { return lfo.phase; }

    // Process a block of up to `numChannels` channels in place.
    void process (float* const* data, int numChannels, int numSamples)
    {
        const double inc = (double) p.rateHz / fs;
        const int nc = std::min (numChannels, (int) chans.size());
        for (int n = 0; n < numSamples; ++n)
        {
            const float base = baseSm.next(), depth = depthSm.next();
            const float mix = mixSm.next(), on = bypassSm.next();
            const float d = base + depth * lfo.value();
            lfo.advance (inc);
            const float nz = noise.next() * noiseGain;

            for (int c = 0; c < nc; ++c)
            {
                const float x = data[c][n];
                const float wet = chans[(size_t) c].process (x, d, nz);
                const float y = (x + mix * (wet - x)) * (1.0f + 0.55f * (1.0f - std::fabs (2.0f * mix - 1.0f)));
                data[c][n] = x + on * (y - x);
            }
        }
    }

private:
    float fs = 48000.0f, lastTone = -1.0f, noiseGain = 0.0f;
    Params p;
    Lfo lfo;
    Noise noise;
    std::vector<DelayChannel> chans;
    Smoother baseSm, depthSm, mixSm, bypassSm;
};
} // namespace wv
