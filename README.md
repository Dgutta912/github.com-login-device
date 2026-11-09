# Gutta Radio Ready

A one-knob-per-stage mix finisher for your **master bus**. Put it last in the
chain on your stereo mix bus, turn the four knobs, and your mix lands at
radio/streaming loudness with the mud cleaned out, the vocal presence lifted,
and the peaks locked under a hard ceiling.

Made for DGUTTA912. Red and black, obviously.

**Scope:** stereo whole-mix / master bus only. This is not a vocal-chain
plugin — it is built to sit across the full mix.

---

## The four knobs

| Knob      | What it does                                                                                                                                                                                                                              |
| --------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **CLEAN** | Low-end clean-up. A 35 Hz high-pass is always on to kill sub rumble. Turning CLEAN up deepens a gentle cut around 300 Hz (up to −4 dB) that pulls mud and boxiness out of the full mix. Start ~30.                                        |
| **PUNCH** | Glue compression for the whole mix. Blends threshold (−4 dB → −18 dB) and ratio (2:1 → 4:1) together, with a slow 30 ms attack so drums and transients still hit, and a 150 ms release. Start ~35. Watch the GR meter: 2–4 dB on peaks is the sweet spot. |
| **AIR**   | Top-end polish. Adds presence at 4 kHz (up to +4 dB) so vocals and snares step forward, plus a high shelf at 10 kHz (up to +6 dB) for that open, expensive air. Start ~30 — a little goes a long way on a full mix.                        |
| **LOUD**  | The maximizer. Pushes the mix toward radio loudness (up to +18 dB of drive) with a touch of tanh saturation warmth that grows as you push, into a stereo-linked limiter. The louder you go, the more the limiter works — back off if GR lives above ~6 dB. |

Plus:

- **INPUT / OUTPUT** trim sliders (−24…+24 dB) for gain staging.
- **BYPASS** switch for honest A/B checks.
- **IN / OUT level meters** and a **GR (gain reduction) meter** so you can see
  what the compression + limiting is actually doing.

## LISTEN — Auto mode

Flip on **LISTEN** and the plugin stops guessing blind. While your mix
plays, it listens in real time:

- A five-band filter bank watches **sub (<120 Hz)**, the **mud region
  (250–350 Hz)**, **mids (~1 kHz)**, **presence (~4 kHz)** and **air
  (9 kHz+)**, plus the overall loudness (RMS) and peak/crest factor, all
  smoothed over a few seconds of playback so it judges the mix, not a
  single kick drum.
- It compares what it hears against a fixed radio-ready reference balance
  and adds **gentle automatic corrections on top of your knob settings**:
  mud building up → more CLEAN cut; presence/air dull → more AIR boost;
  mix quiet → more LOUD drive into the limiter; mix already hot or
  over-squashed → it backs LOUD off. Hot peaks are still caught instantly
  by the limiter itself.
- Corrections are **bounded** (at most +35 points of Clean or Air, +30/−15
  of Loud), **slow** (they drift over ~2 seconds — the auto logic never
  pumps), and they **fade back to zero** when you switch LISTEN off.
- The window shows you what it's doing: a red **LISTENING** indicator, the
  loudness it hears, each band's balance, and the exact correction it's
  applying (e.g. `AUTO: CLEAN +6 AIR +9 LOUD +4`).

Your knobs stay the boss — they're the base settings, LISTEN just rides on
top. With LISTEN off, the plugin behaves exactly like the static version.

**Honest note:** LISTEN is real DSP analysis — filters, levels and simple
rules aimed at a radio-ready reference balance and loudness. It is *not* an
ML model and it doesn't "know" your song. It will reliably catch mud,
dullness and loudness problems; whether the result is *perfect* is still
decided by your ears.

## Radio-ready targets used

- **Ceiling: −1.0 dBFS true sample peak limit**, enforced by the limiter and
  again as a final safety clamp after the output trim, so nothing you do with
  the knobs can push the output over −1.0 dB. That's the standard headroom
  streaming encoders (Spotify, Apple Music, YouTube) want so their AAC/Opus
  conversion doesn't clip.
- **Loudness:** LOUD is calibrated so that mid settings land a typical hip-hop
  mix in the **−14…−9 LUFS integrated** neighbourhood — the range streaming
  normalization is built around. Push LOUD for a hotter, more aggressive
  master; pull it back for dynamics. Check your final number in your DAW's
  loudness meter — this plugin gives you the push and the ceiling, your ears
  pick the spot.
- **Stereo-linked limiting** so the stereo image never wobbles when one side
  peaks.

## Chain (for the curious)

Input trim → [LISTEN analysis listens here] → 35 Hz high-pass + 300 Hz mud
cut (CLEAN) → glue compressor (PUNCH) → 4 kHz presence + 10 kHz air shelf
(AIR) → tanh saturation → linked peak limiter with −1.0 dB ceiling (LOUD)
→ output trim → ceiling safety clamp. Zero added latency — no lookahead —
so it's safe to track through and automate.

---

## Build it on your Mac (for LUNA / Logic)

You need a Mac with Xcode installed. No paid developer account needed for
your own use.

1. Install **Xcode** from the App Store, open it once to finish setup, then
   install the command-line tools:

   ```bash
   xcode-select --install
   ```

2. Install **CMake** (if you don't have it):

   ```bash
   brew install cmake
   ```

   (No Homebrew? Download the CMake app from cmake.org and drag it to
   Applications, then run `sudo "/Applications/CMake.app/Contents/bin/cmake-gui" --install` once.)

3. Download or copy this `radio-ready-plugin` folder to your Mac, open
   Terminal in that folder, and run:

   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release -j 4
   ```

   The first configure downloads JUCE automatically — give it a minute.

4. When the build finishes, the plugins are **copied automatically** to the
   standard system locations:

   - VST3: `~/Library/Audio/Plug-Ins/VST3/Gutta Radio Ready.vst3`
   - AU: `~/Library/Audio/Plug-Ins/Components/Gutta Radio Ready.component`
   - Standalone app: `build/DGUTTA_RadioReady_artefacts/Release/Standalone/Gutta Radio Ready.app`

   (The built bundles also live under
   `build/DGUTTA_RadioReady_artefacts/Release/` if you ever need to copy them
   by hand.)

5. **Use it:**
   - **LUNA:** restart LUNA, it scans on launch. Find **Gutta Radio Ready**
     under Audio Units / VST3 effects and drop it on your MAIN / mix bus.
   - **Logic Pro:** restart Logic, it validates the AU on launch. Find it
     under **Audio FX → Audio Units → DGUTTA912 → Gutta Radio Ready** and put
     it on your Stereo Out.
   - If a DAW doesn't see it, rescan your plug-ins in that DAW's plug-in
     manager.

   First launch note: because you built it yourself it is not notarized by
   Apple. If macOS blocks it, right-click the `.vst3` / `.component` in
   Finder → **Open**, or allow it in **System Settings → Privacy & Security**.

## Windows (brief)

Install Visual Studio 2022 (Desktop development with C++) and CMake, then
from a Developer Command Prompt in this folder:

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

You get the VST3 (`Gutta Radio Ready.vst3`, copied to
`C:\Program Files\Common Files\VST3`) and the Standalone `.exe`. No AU on
Windows — AU is Apple-only.

## Linux (brief)

Needs the usual JUCE dev packages (`libasound2-dev libx11-dev libxrandr-dev
libxinerama-dev libxcursor-dev libfreetype-dev` etc.), then the same two
CMake commands. Produces VST3 + Standalone.

---

## Project layout

```
CMakeLists.txt          Build script — pulls JUCE 8.0.4 automatically
Source/
  PluginProcessor.h/.cpp  Parameters + the full DSP chain
  PluginEditor.h/.cpp     Red-and-black UI, knobs, meters
```

Formats: **VST3, AU, Standalone** — all from the same source.

Version 1.0.0 — DGUTTA912.
