# Producers Heaven

A VST3 plugin for FL Studio with four tools, each one a floating orb on the home screen:

| Orb | What it does |
|---|---|
| **Drum Forge** | Generates a full 8-piece kit (Kick, 808, Snare, Clap, Hi-Hat, Open Hat, Perc, Rim) the moment you open it. Pick a vibe (Trap, Boom Bap, Drill, Lo-Fi, House), re-roll any single sound with its dice button, click a pad to hear it, **drag it straight into FL** (Channel Rack, Playlist, or a sampler). "Save Kit" writes all 8 WAVs to a folder. |
| **Beat Doctor** | Hit **LISTEN**, play your beat in FL, hit **STOP**. It detects BPM, scores Punch / Low End / Brightness / Dynamics / Width, and gives specific tips (clipping, weak kick sub, dull snare, harsh hats, robotic hat velocities, muddy low mids, stereo low end, over-compression...). Tips about a drum come with a **Generate** button that makes a replacement sound designed to fix that problem, ready to drag in. |
| **Vibe Match** | Drop in a beat or loop (WAV, MP3, FLAC, AIFF, OGG), or click to browse. It detects tempo, groove, swing and style, then generates a new drum loop in the same lane. Choose 1/2/4 bars and how far it can wander (Similar to Wild). Drag out the **WAV loop** or the **MIDI** (straight into FL's piano roll / FPC). "New Sounds" keeps the idea but rolls a new kit. |
| **FX Heaven** | Seven effects for the whole beat: **Distortion, Underwater, Lo-Fi, Telephone, Vinyl, Echo** (synced to FL's tempo) and **Heaven Verb**. Each has an on switch and an amount knob, plus Master Mix / Output / Preview volume. Every control can be automated in FL. |

Bonus: the Drum Forge kit is playable from MIDI (notes 35-47, standard drum map).

Everything you drag out is also saved in **Documents/Producers Heaven/** (Drum Forge, Beat Doctor, Vibe Match, Kits).

---

## Getting the .vst3 file (pick one)

### Option A: let GitHub build it for you (no coding tools needed)

1. Make a free account at github.com and create a **new repository** (Private is fine).
2. Click **"uploading an existing file"** and drag in everything from this folder (`Source`, `Tests`, `CMakeLists.txt`, `README.md`). Commit.
3. The hidden `.github` folder often gets skipped by drag-and-drop, so add it by hand: **Add file > Create new file**, name it `.github/workflows/build.yml`, paste in the contents of that file from this folder, and commit.
4. Open the **Actions** tab. A build starts automatically and takes about 10 minutes. (If it doesn't start, click "Build Producers Heaven" > **Run workflow**.)
5. When it shows a green check, open the run and download **ProducersHeaven-Windows** (or **-macOS**) at the bottom.

### Option B: build it on your own PC (Windows)

1. Install **Visual Studio 2022 Community** with the **"Desktop development with C++"** workload (includes CMake), and **Git**.
2. Open **"x64 Native Tools Command Prompt for VS 2022"**, `cd` into this folder, and run:
   ```
   cmake -B build -A x64
   cmake --build build --config Release --target ProducersHeaven_VST3
   ```
3. The plugin will be at `build\ProducersHeaven_artefacts\Release\VST3\Producers Heaven.vst3`.

(Mac: install Xcode + CMake, run the same two commands without `-A x64`.)

---

## Installing in FL Studio

1. Copy the whole **`Producers Heaven.vst3`** folder into:
   - Windows: `C:\Program Files\Common Files\VST3\`
   - Mac: `~/Library/Audio/Plug-Ins/VST3/`
2. In FL Studio: **Options > Manage plugins > Find installed plugins** (Start scan). Star it so it shows up in your list.
3. Load **Producers Heaven** as an **effect on your Master mixer track** (or the track your beat runs through). That's how Beat Doctor hears your beat and how FX Heaven processes it. Drum Forge and Vibe Match work from any slot.

There's also a Standalone app in the Windows download if you want to try it outside FL.

---

## Good to know

- **The tips and loop matching are built into the plugin.** They work by analyzing the audio (onsets, tempo, frequency balance), with no internet connection or AI service involved. They work best on beats where the drums are clearly audible. Vibe Match recreates the groove and feel with new drums. It doesn't copy melodies or samples.
- **All the drum sounds are synthesized** by the plugin itself, so every one is original and royalty-free.
- Built with the [JUCE](https://juce.com) framework. Using and sharing it yourself is fine. If you ever want to **sell** it, check JUCE's licence terms first.

## Project layout

```
Source/Core/       drum synthesis, beat analysis, loop generation (plain C++, unit-tested)
Source/DSP/        FX Heaven effects
Source/UI/         floating-orb home screen + the four pages
Tests/             EngineTest.cpp (engine tests), UIHarness.cpp (drives every page, renders screenshots)
```
