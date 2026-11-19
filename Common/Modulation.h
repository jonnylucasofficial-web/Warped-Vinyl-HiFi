// Chase-Bliss-style "Gen 2" modulation of front-panel knobs:
//   * Expression / CV: assigned knobs sweep between the knob position and
//     the bottom (Sweep = B) or top (Sweep = T) of their travel.
//   * Ramping: when nothing is plugged into the expression jack, assigned
//     knobs move automatically (rise or fall) and either ramp-and-hold or
//     bounce back and forth. Ramping restarts whenever the pedal is engaged.
//   * Tap tempo with averaging.
#pragma once
#include <array>
#include <cmath>
#include "DspUtil.h"

namespace cbdsp
{
template <int N>
struct KnobModulator
{
    struct Config
    {
        std::array<bool, N> assigned {};     // left-bank dips
        std::array<bool, N> rise {};         // right-bank direction dips (true = rise)
        bool sweepTop   = false;             // Sweep dip: false = B(ottom), true = T(op)
        bool bounce     = false;             // Bounce dip: back-and-forth vs ramp-and-hold
        bool polarity   = false;             // invert expression travel
        bool expConnected = false;           // expression pedal / CV connected?
        bool rampEnabled = true;             // pedals without ramping set this false
    };

    Config cfg;
    float rampPos = 0.0f;   // 0..1 position within the ramp
    int   rampDir = 1;      // bounce direction
    bool  holdActive = false;

    void resetRamp() { rampPos = 0.0f; rampDir = 1; }

    // Advance the ramp by dt seconds, with a full one-way travel of rampSeconds.
    void advance (float dt, float rampSeconds)
    {
        if (cfg.expConnected || ! cfg.rampEnabled) return;
        const float inc = dt / std::max (0.01f, rampSeconds);
        if (cfg.bounce)
        {
            rampPos += inc * (float) rampDir;
            if (rampPos >= 1.0f) { rampPos = 2.0f - rampPos; rampDir = -1; }
            if (rampPos <= 0.0f) { rampPos = -rampPos;       rampDir =  1; }
            rampPos = clamp01 (rampPos);
        }
        else
        {
            rampPos = std::min (1.0f, rampPos + inc);
        }
    }

    bool anyAssigned() const
    {
        for (bool a : cfg.assigned) if (a) return true;
        return false;
    }

    // Map knob values to effective values.
    // exp: expression position 0..1 (heel..toe)
    std::array<float, N> apply (const std::array<float, N>& knobs, float exp) const
    {
        std::array<float, N> out = knobs;
        const bool rampingNow = ! cfg.expConnected && cfg.rampEnabled;
        const bool holdAll = holdActive && ! anyAssigned();

        for (int i = 0; i < N; ++i)
        {
            if (! (cfg.assigned[i] || (holdAll && rampingNow))) continue;

            const float k  = knobs[i];
            const float lo = cfg.sweepTop ? k : 0.0f;
            const float hi = cfg.sweepTop ? 1.0f : k;

            float t;
            if (cfg.expConnected)
            {
                t = cfg.polarity ? 1.0f - exp : exp;
                if (! cfg.rise[i]) t = 1.0f - t;
            }
            else if (cfg.rampEnabled)
            {
                t = cfg.rise[i] ? rampPos : 1.0f - rampPos;
            }
            else continue;

            out[i] = lo + t * (hi - lo);
        }
        return out;
    }
};

//------------------------------------------------------------------------------
struct TapTempo
{
    double lastTap = -1.0;
    double intervals[4] {};
    int count = 0;
    double period = 0.5; // seconds per tap (quarter note)

    // returns true if a new period was computed
    bool tap (double nowSeconds)
    {
        bool updated = false;
        if (lastTap >= 0.0)
        {
            const double d = nowSeconds - lastTap;
            if (d > 0.06 && d < 4.0)
            {
                for (int i = 3; i > 0; --i) intervals[i] = intervals[i - 1];
                intervals[0] = d;
                count = std::min (count + 1, 4);
                double sum = 0; for (int i = 0; i < count; ++i) sum += intervals[i];
                period = sum / count;
                updated = true;
            }
            else count = 0;
        }
        lastTap = nowSeconds;
        return updated;
    }
};
} // namespace cbdsp
