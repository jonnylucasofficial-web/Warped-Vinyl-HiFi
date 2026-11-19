// Shared, dependency-free DSP building blocks (plain C++17) so the cores can be
// unit-tested without JUCE.
#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace cbdsp
{
constexpr double kPi = 3.14159265358979323846;

inline float clamp01 (float x) { return std::min (1.0f, std::max (0.0f, x)); }
inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }

// Prevent denormals in long-decaying filter states.
inline float undenorm (float x) { return (std::fabs (x) < 1.0e-20f) ? 0.0f : x; }

//------------------------------------------------------------------------------
// One-pole filters (matched-pole TPT form)
struct OnePole
{
    float g = 0.0f, s = 0.0f;
    void setCutoff (float fc, float fs)
    {
        fc = std::min (fc, 0.49f * fs);
        const float wd = (float) std::tan (kPi * fc / fs);
        g = wd / (1.0f + wd);
    }
    void reset (float v = 0.0f) { s = v; }
    // returns lowpass; highpass = x - lp
    float lp (float x)
    {
        const float v = (x - s) * g;
        const float y = v + s;
        s = undenorm (y + v);
        return y;
    }
    float hp (float x) { return x - lp (x); }
};

//------------------------------------------------------------------------------
// RBJ biquad (transposed direct form II)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }
    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = undenorm (b1 * x - a1 * y + z2);
        z2 = undenorm (b2 * x - a2 * y);
        return y;
    }
    void setNormalised (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
    void lowpass (double fc, double q, double fs)
    {
        fc = std::min (fc, 0.49 * fs);
        const double w = 2 * kPi * fc / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        setNormalised ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double fc, double q, double fs)
    {
        fc = std::min (fc, 0.49 * fs);
        const double w = 2 * kPi * fc / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        setNormalised ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void peak (double fc, double q, double gainDb, double fs)
    {
        fc = std::min (fc, 0.49 * fs);
        const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * fc / fs;
        const double c = std::cos (w), al = std::sin (w) / (2 * q);
        setNormalised (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void highShelf (double fc, double gainDb, double fs, double slope = 0.8)
    {
        fc = std::min (fc, 0.49 * fs);
        const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * fc / fs;
        const double c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / slope - 1) + 2);
        const double sa = 2 * std::sqrt (A) * al;
        setNormalised (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c),
                       A * ((A + 1) + (A - 1) * c - sa), (A + 1) - (A - 1) * c + sa,
                       2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
    void lowShelf (double fc, double gainDb, double fs, double slope = 0.8)
    {
        fc = std::min (fc, 0.49 * fs);
        const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * fc / fs;
        const double c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / slope - 1) + 2);
        const double sa = 2 * std::sqrt (A) * al;
        setNormalised (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c),
                       A * ((A + 1) - (A - 1) * c - sa), (A + 1) + (A - 1) * c + sa,
                       -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }
};

//------------------------------------------------------------------------------
// Linear parameter smoother
struct Smoother
{
    float current = 0, target = 0, step = 0; int remaining = 0, length = 64;
    void setLength (float seconds, float fs) { length = std::max (1, (int) (seconds * fs)); }
    void reset (float v) { current = target = v; remaining = 0; }
    void set (float v)
    {
        if (v == target) return;
        target = v; remaining = length; step = (target - current) / (float) length;
    }
    float next()
    {
        if (remaining > 0) { current += step; if (--remaining == 0) current = target; }
        return current;
    }
};

//------------------------------------------------------------------------------
// Tiny deterministic noise source (xorshift) – used for BBD hiss.
struct Noise
{
    uint32_t state = 0x12345678u;
    float next()
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return (float) ((double) state / 4294967295.0 * 2.0 - 1.0);
    }
};

// Soft saturators -------------------------------------------------------------
inline float fastTanh (float x)
{
    // Pade approximation, accurate to ~1e-3 in [-3,3], hard limited beyond.
    if (x > 4.97f) return 1.0f;
    if (x < -4.97f) return -1.0f;
    const float x2 = x * x;
    const float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return std::max (-1.0f, std::min (1.0f, a / b));
}

// Diode pair in an op-amp feedback loop: logarithmic compression (soft).
inline float diodeFeedback (float v, float vf) { return vf * std::asinh (v / vf); }

// Diode pair to ground after a series resistor: tighter, harder knee.
inline float diodeToGround (float v, float vf) { return vf * fastTanh (v / vf); }

// Asymmetric germanium-transistor style clip (even harmonics).
inline float germanium (float v)
{
    return v >= 0.0f ? 1.1f * fastTanh (v / 1.1f) : 0.55f * fastTanh (v / 0.55f);
}
} // namespace cbdsp
