# Warped Vinyl HiFi — plugin version

A JUCE audio plugin (VST3 / AU / Standalone) modelled on the Chase Bliss **Warped Vinyl HiFi** analog vibrato/chorus. It's a personal recreation for studio use and isn't affiliated with Chase Bliss or Analog.Man.

> **About accuracy.** This is a circuit-informed model built from public
> information: product pages, reviews, manual excerpts and King of Tone, Beano Boost
> and BBD circuit knowledge. They are **not** measured against the hardware: there was no
> pedal to null-test against, and the full official manual couldn't be downloaded
> from the build environment. It will sound in the same family, not bit-identical.
> The way to get closer is to record DIs through the real pedals and A/B them. See
> "Matching your own pedal" below.

---
## Warped Vinyl HiFi

A bucket-brigade-style modulated delay, modelled with:
- anti-alias and reconstruction filters
- BBD headroom saturation
- hiss that rises with LAG, as owners report on the hardware

Throughout, the dry path stays clean.

| Control | What it does |
|---|---|
| TONE | Brightness of the modulated signal (~1.1 kHz → 20 kHz) |
| LAG | Delay between wet and dry, 5–20 ms. Higher = lusher, thicker chorus (HiFi replaces the mkII's Volume knob with this) |
| MIX | Dry/wet. Full CW = pure vibrato; around noon = chorus |
| RPM | Rate, 0.1–12 Hz (6–720 rpm). Tap overrides it; turn the knob to take back control |
| DEPTH | Pitch-bend amount (up to ~±3 semitones at extreme settings) |
| WARP | Moves the peak of the wave. Noon = symmetrical; CCW = fast rise / slow fall; CW = slow rise / fast fall |
| RISE / FALL toggles | Curve of each half of the wave: SMOOTH / LINEAR / EXPO |
| TAP DIV toggle | 1 / 2 / 4 cycles per bar (3 / 6 / 8 with the TAP DIV dip) |
| RAMP | Ramp speed, 0.5–30 s per sweep |
| BYPASS footswitch | On/off. Ramping restarts each time you engage |
| TAP footswitch | Tap tempo (averages your last 4 taps). **Hold** = a momentary "spell" of ramping. If nothing is assigned, it ramps everything |

**Dip switches — Ramp/Expression bank:**
- LAG, MIX, RPM, DEPTH, WARP choose which knobs ramping or the expression pedal moves. If "Expression / CV connected" is ticked, the expression pedal moves them; otherwise they ramp automatically.
- **BOUNCE** — on = ramp back and forth continuously; off = ramp and hold.
- **SWEEP** — B = between 0 and the knob; T = between the knob and max.
- **POLARITY** — reverses expression travel.

**Dip switches — Direction/Tap bank:**
- LAG…WARP each set **RISE** or **FALL** for that knob.
- **TAP DIV** — use 1-2-4 or 3-6-8 divisions.
- **TAP CTRL** — **P** = tap sets RPM; **R** = tap sets ramp speed. Ramp divisions are 2× slower.

**Plugin extras:**
- **Sync tap to host tempo** — drives tap from your DAW's BPM.
- A live wave display.
- 4 preset slots (Program Change 0–3).
- 7 factory programs.

---
## MIDI

Both plugins take MIDI input on any channel (omni). In Ableton, route a MIDI track's output to the plugin.

| CC | Warped Vinyl HiFi |
|---|---|
| 14 | Tone |
| 15 | Lag |
| 16 | Mix |
| 17 | RPM |
| 18 | Depth |
| 19 | Warp |
| 20 | Ramp speed |
| 21 / 22 / 23 | Tap Div / Rise / Fall |
| 24 / 25 | – |
| 61–68 | Left dip bank |
| 71–78 | Right dip bank (71–77) |
| 93 | Tap (any value) |
| 100 | Expression |
| 102 / 103 | Bypass (102) |
| Program Change 0–3 | Recall preset 1–4 |

The Brothers AM numbers for CC 15–19, 67 (Sweep) and 71 (Hi Gain 1) follow the published Brothers AM MIDI chart. The rest are filled in to fit that pattern. Every control is also a normal DAW automation parameter.

---
## Building

**Requirements:** CMake ≥ 3.22 and a C++17 compiler (Xcode on Mac, Visual Studio 2022 on Windows). JUCE 8.0.4 is fetched automatically if `JUCE/` isn't present.

1. Configure:

   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   ```

2. Build:

   ```bash
   cmake --build build --config Release --parallel
   ```

On a Mac, add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` to the configure step for a universal binary.

**Where the built plugins end up:**
- VST3: `build/<Plugin>/<Plugin>_artefacts/Release/VST3/`
- AU (Mac only): `build/<Plugin>/<Plugin>_artefacts/Release/AU/`

**Installing on a Mac:**
1. Copy the `.vst3` into `~/Library/Audio/Plug-Ins/VST3/` and the `.component` into `~/Library/Audio/Plug-Ins/Components/`.
2. The builds aren't code-signed, so clear the quarantine flag:

   ```bash
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/*.vst3 ~/Library/Audio/Plug-Ins/Components/*.component
   ```

**Building with GitHub instead:** push this folder to a GitHub repo and `.github/workflows/build.yml` builds Mac (universal AU + VST3), Windows and Linux on every push. Download the results from the run's *Artifacts*.

**Tests:**
- `build/dsp_tests` checks DSP stability and behaviour.

## Matching your own pedal

If you own the pedal, record a DI and the same take through it at a few settings. The voicing constants are grouped at the top of each class in `WarpedVinyl/Source/WVDsp.h` (Lag/Depth ranges, BBD filter, Tone sweep, RPM range), and can be tuned against the recordings.
