// Offline DSP tests: stability, level calibration, modulation behaviour.
#ifdef TEST_BAM
#include "../BrothersAM/Source/AMDsp.h"
#endif
#ifdef TEST_WV
#include "../WarpedVinyl/Source/WVDsp.h"
#endif
#include "../Common/Modulation.h"
#include <cstdio>
#include <vector>
#include <cmath>

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf ("FAIL: " __VA_ARGS__); std::printf ("\n"); } } while (0)

// Plucked-string-ish test signal: decaying harmonic series, peak ~0.35.
static std::vector<float> pluck (float fs, float seconds, float f0 = 110.0f)
{
    std::vector<float> s ((size_t) (fs * seconds));
    for (size_t i = 0; i < s.size(); ++i)
    {
        const float t = (float) i / fs, env = std::exp (-t * 2.5f);
        float v = 0;
        for (int h = 1; h <= 12; ++h)
            v += std::sin (2 * (float) cbdsp::kPi * f0 * h * t) * std::exp (-0.35f * h) / (float) h;
        s[i] = 0.35f * env * v;
    }
    return s;
}

static double rms (const std::vector<float>& v)
{
    double a = 0; for (float x : v) a += (double) x * x; return std::sqrt (a / (double) v.size());
}
static bool finite (const std::vector<float>& v) { for (float x : v) if (! std::isfinite (x)) return false; return true; }
static float peak (const std::vector<float>& v) { float p = 0; for (float x : v) p = std::max (p, std::fabs (x)); return p; }

#ifdef TEST_BAM
static void testBrothers()
{
    const float fs = 48000.0f * 4;   // plugin runs the core 4x oversampled
    auto in = pluck (fs, 1.5f);
    const double inRms = rms (in);
    const char* names[] = { "Boost", "Overdrive", "Distortion" };

    std::printf ("\n== Brothers AM: level vs bypass (gain/vol/tone at noon) ==\n");
    for (int ch = 1; ch <= 2; ++ch)
    for (int m = 0; m < 3; ++m)
    {
        for (float gain : { 0.0f, 0.5f, 1.0f })
        {
            bam::Engine e;
            bam::EngineParams p;
            auto& c = ch == 1 ? p.ch1 : p.ch2;
            c.mode = (bam::Mode) m; c.gain = gain; c.volume = 0.5f; c.tone = 0.5f;
            (ch == 1 ? p.on1 : p.on2) = true;
            e.prepare (fs); e.setParams (p);
            std::vector<float> out (in.size());
            for (size_t i = 0; i < in.size(); ++i) out[i] = e.process (in[i]);
            CHECK (finite (out), "Brothers ch%d %s produced non-finite output", ch, names[m]);
            const double db = 20 * std::log10 (rms (out) / inRms);
            std::printf ("  ch%d %-10s gain %.1f : %+6.1f dB  (peak %.2f)\n", ch, names[m], gain, db, peak (out));
            if (gain == 0.5f) CHECK (std::fabs (db) < 7.0, "ch%d %s level at noon too far from unity (%.1f dB)", ch, names[m], db);
        }
    }

    // Stacked + booster + hi gain + master, worst case
    {
        bam::Engine e; bam::EngineParams p;
        p.ch1 = { 1.0f, 1.0f, 1.0f, 1.0f, bam::Mode::Distortion, true };
        p.ch2 = { 1.0f, 1.0f, 1.0f, 1.0f, bam::Mode::Distortion, true };
        p.booster = bam::Booster::Full; p.on1 = p.on2 = true;
        e.prepare (fs); e.setParams (p);
        std::vector<float> out (in.size());
        for (size_t i = 0; i < in.size(); ++i) out[i] = e.process (in[i]);
        CHECK (finite (out), "worst-case stack non-finite");
        std::printf ("  worst-case stack peak %.2f\n", peak (out));
        CHECK (peak (out) < 30.0f, "worst-case stack absurdly loud");
    }

    // Booster lifts treble more than bass
    {
        auto tone = [&] (float f, bam::Booster b)
        {
            bam::Engine e; bam::EngineParams p;
            p.ch1 = { 0.0f, 0.5f, 0.5f, 0.5f, bam::Mode::Boost, false };
            p.on1 = true; p.booster = b;
            e.prepare (fs); e.setParams (p);
            std::vector<float> o ((size_t) fs / 2);
            for (size_t i = 0; i < o.size(); ++i) o[i] = e.process (0.05f * std::sin (2 * (float) cbdsp::kPi * f * (float) i / fs));
            return rms (o);
        };
        for (auto b : { bam::Booster::Full, bam::Booster::Lite })
        {
            const double tilt = 20 * std::log10 ((tone (3000, b) / tone (100, b)) / (tone (3000, bam::Booster::Off) / tone (100, bam::Booster::Off)));
            std::printf ("  booster %s treble tilt: %+5.1f dB (3k vs 100Hz)\n", b == bam::Booster::Full ? "FULL" : "LITE", tilt);
            CHECK (tilt > 3.0, "booster should emphasise treble");
        }
    }

    // Master mode: single channel level should not depend on VOL 1
    {
        auto lvl = [&] (float vol1)
        {
            bam::Engine e; bam::EngineParams p;
            p.ch1.volume = vol1; p.on1 = true; p.masterMode = true; p.ch2.volume = 0.5f;
            e.prepare (fs); e.setParams (p);
            std::vector<float> o (in.size());
            for (size_t i = 0; i < in.size(); ++i) o[i] = e.process (in[i]);
            return rms (o);
        };
        CHECK (std::fabs (lvl (0.1f) - lvl (0.9f)) < 1e-6, "master mode: VOL1 should be ignored with one channel on");
    }
}

#endif
#ifdef TEST_WV
static void testVinyl()
{
    const float fs = 48000.0f;
    std::printf ("\n== Warped Vinyl HiFi ==\n");
    auto in = pluck (fs, 2.0f, 220.0f);

    for (float mix : { 0.0f, 0.5f, 1.0f })
    {
        wv::Engine e; wv::Params p; p.mix = mix; p.depth = 0.6f; p.rateHz = 2.0f;
        e.prepare (fs, 2); e.setParams (p);
        std::vector<float> l = in, r = in;
        float* d[2] = { l.data(), r.data() };
        e.process (d, 2, (int) l.size());
        CHECK (finite (l), "WV non-finite at mix %.1f", mix);
        std::printf ("  mix %.1f : %+5.1f dB vs dry\n", mix, 20 * std::log10 (rms (l) / rms (in)));
    }

    // Vibrato pitch deviation: feed a 1 kHz sine, full wet, measure instantaneous freq range
    {
        wv::Engine e; wv::Params p; p.mix = 1.0f; p.depth = 1.0f; p.rateHz = 5.0f; p.tone = 1.0f;
        e.prepare (fs, 1); e.setParams (p);
        std::vector<float> s ((size_t) fs * 2);
        for (size_t i = 0; i < s.size(); ++i) s[i] = 0.3f * std::sin (2 * (float) cbdsp::kPi * 1000.0f * (float) i / fs);
        float* d[1] = { s.data() };
        e.process (d, 1, (int) s.size());
        // zero-crossing period analysis over second half
        double lastZ = -1, minF = 1e9, maxF = 0;
        for (size_t i = s.size() / 2; i < s.size(); ++i)
            if (s[i - 1] < 0 && s[i] >= 0)
            {
                const double z = (double) (i - 1) + s[i - 1] / (s[i - 1] - s[i]);
                if (lastZ > 0) { const double f = fs / (z - lastZ); minF = std::min (minF, f); maxF = std::max (maxF, f); }
                lastZ = z;
            }
        const double cents = 1200 * std::log2 (maxF / minF);
        std::printf ("  vibrato depth max @5Hz: %.0f – %.0f Hz (%.0f cents p-p)\n", minF, maxF, cents);
        CHECK (cents > 50 && cents < 2400, "vibrato depth implausible");
    }

    // Warp skews the LFO peak
    {
        wv::Lfo l; l.warp = 0.0f; l.phase = 0.1; float a = l.value();
        l.warp = 1.0f; float b = l.value();
        CHECK (a > b, "warp CCW should rise faster than CW");
        l.warp = 0.5f; l.phase = 0.5; CHECK (std::fabs (l.value() - 1.0f) < 0.05f, "symmetric peak at center");
    }
}

#endif
#ifdef TEST_WV
static void testModulation()
{
    std::printf ("\n== Ramp / expression ==\n");
    cbdsp::KnobModulator<3> m;
    m.cfg.assigned = { true, false, true };
    m.cfg.rise = { true, true, false };
    std::array<float, 3> knobs { 0.8f, 0.5f, 0.6f };
    auto o = m.apply (knobs, 0);
    CHECK (o[0] == 0.0f && o[1] == 0.5f && std::fabs (o[2] - 0.6f) < 1e-6, "ramp start values");
    m.advance (1.0f, 1.0f);
    o = m.apply (knobs, 0);
    CHECK (std::fabs (o[0] - 0.8f) < 1e-6 && std::fabs (o[2]) < 1e-6, "ramp end values");
    m.advance (1.0f, 1.0f);
    CHECK (m.rampPos == 1.0f, "ramp-and-hold holds");
    m.cfg.bounce = true; m.advance (0.5f, 1.0f);
    CHECK (std::fabs (m.rampPos - 0.5f) < 1e-5, "bounce reverses");
    m.cfg.expConnected = true; m.cfg.sweepTop = true;
    o = m.apply (knobs, 1.0f);
    CHECK (std::fabs (o[0] - 1.0f) < 1e-6, "expression toe sweeps to top");
    cbdsp::TapTempo t; t.tap (0); t.tap (0.5); t.tap (1.0);
    CHECK (std::fabs (t.period - 0.5) < 1e-9, "tap tempo average");
    std::printf ("  ok\n");
}

#endif
int main()
{
#ifdef TEST_BAM
    testBrothers();
#endif
#ifdef TEST_WV
    testVinyl();
    testModulation();
#endif
    std::printf ("\n%s (%d failures)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}
