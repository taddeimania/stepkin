# Stepkin

A semi-modular analog-style percussion synthesiser, built as a VST3 / AU
instrument for Ableton Live. Two oscillators with hard sync and linear FM, a
noise generator, a switchable low-pass / high-pass ladder filter, three decay
envelopes driven by a per-step velocity, an eight-step sequencer with pitch and
velocity per step, and a 24-point patchbay with working cables.

Sibling to [Fractone](../harm): same panel language, same preset system, same
"follows Live unless you tell it not to" approach to the clock.

---

## Building

Needs CMake 3.22+ and a C++20 compiler. JUCE 8.0.9 is fetched on first
configure.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The VST3, AU and a standalone app are copied into the system plug-in folders
after each build (`~/Library/Audio/Plug-Ins/VST3` and `/Components` on macOS).
Pass `-DSTEPKIN_COPY_PLUGIN=OFF` to skip that.

Useful options:

| Option | Default | What it does |
| --- | --- | --- |
| `STEPKIN_JUCE_DIR` | *(empty)* | Use a local JUCE checkout instead of downloading one |
| `STEPKIN_COPY_PLUGIN` | `ON` (not Windows) | Install into the system plug-in folders after building |
| `STEPKIN_BUILD_TOOLS` | `OFF` | Also build the headless dev tools |

For a universal macOS binary:

```sh
cmake -B build -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_BUILD_TYPE=Release
```

### Tests

```sh
cmake --build build --target StepkinEngineTests && ./build/StepkinEngineTests
```

The engine has no JUCE dependency, so the tests build and run anywhere. They
cover waveform band-limiting, hard sync, FM sidebands, both filter modes,
self-oscillation, velocity response, the sequencer and its buttons, host sync
timing, every single-cable patch, MIDI, and stability across sample rates.

With `-DSTEPKIN_BUILD_TOOLS=ON` there are two more:

```sh
./build/StepkinSnapshot panel.png 0 1.0   # render the panel headlessly
./build/StepkinSnapshot out.wav 2 8       # render preset 2 for 8 seconds
./build/StepkinSnapshot --preset-test     # preset / cable / session round trip
./build/StepkinRender demo.wav            # five patches straight from the engine
```

---

## The panel

### Oscillators

Two oscillators, each with a **FREQUENCY** knob covering ten octaves (centre is
the reference pitch, and matches the centre of the step PITCH knobs) and a
**WAVE** switch for pulse or triangle. Pulse is rich and hollow, good for hard
bass and metallic FM; triangle is smooth and warm, good for toms and classic
analog drum sounds.

**HARD SYNC** resets oscillator 2 every time oscillator 1 completes a cycle.
Their pitches lock together and oscillator 2's FREQUENCY knob becomes a timbre
control. **1>2 FM AMOUNT** is linear frequency modulation of oscillator 2 by
oscillator 1 — a fixed frequency deviation, so the character depends on the
interval between the two. Deep settings push oscillator 2's frequency through
zero and its phase runs backwards through that region, which is where the
bell-like and clangorous tones live. FM is much more obvious with sync off.

### Modulation

One pitch envelope serves both oscillators. **PITCH DECAY** sets its length;
**OSC 1 / OSC 2 EG AMOUNT** set how much of it each oscillator hears, and both
are bipolar. Right of centre spikes the pitch upwards on every hit — the
difference between a silly noise and a kick drum. Left of centre drops it, for
boingy, video-game-ish sounds. Centred, nothing happens and PITCH DECAY does
nothing either.

**STEP PITCH TO** chooses which oscillators the step PITCH knobs reach: both,
neither, or oscillator 2 alone. With it off the PITCH jack on the patchbay stays
live, which is how the hat and swing patches work.

### Mixer

Levels for oscillator 1, oscillator 2, and the noise generator (or whatever is
patched into EXT AUDIO). The summing stage saturates, so low levels stay clean
and dynamic while high ones get aggressive and clipped.

### Filter

A 4-pole, 24 dB/octave ladder, switchable between **low pass** and **high
pass**. **CUTOFF** runs 20 Hz to 20 kHz. **RESONANCE** peaks at the cutoff,
thins the low end, and self-oscillates near the top — turn the oscillators down
and you have a sine wave to play with. **FILTER DECAY** and a bipolar **FILTER
EG AMOUNT** work the same way the pitch pair do. **NOISE / FILTER MOD** shakes
the cutoff with noise for dirty, lo-fi, sizzling textures; patch anything into
FILTER MOD and it takes noise's place with this knob as its depth.

### Amplifier

**AMP DECAY** sets how long each hit lasts: short for tight hats, middling to
let the body ring, long to wash steps together. **AMP EG** switches the attack
between 1 ms and 100 ms — the slow setting softens mallets and brushes, and on a
bass line reads a little like glide.

### Sequencer

Eight steps, each with a **PITCH** knob (bipolar, ten octaves end to end) and a
**VELOCITY** knob. Velocity is the expressive control: it sets the level of all
three envelopes at once, so it changes punch and timbre as well as volume, and a
harder hit rings slightly longer. A step with velocity fully down is silent,
which is how you write a rhythm.

**TRIGGER** sounds the current step without moving on. **ADVANCE** moves on
without sounding the step it lands on. **RUN / STOP** starts and stops the
sequencer.

**RATE** is the free-running step rate, from half a step per second up into the
audio range, where the eight steps stop being a rhythm and become one cycle of a
waveform. It is ignored while SYNC is on.

---

## Clock and Live

With **SYNC** lit the sequencer follows the host: it runs only while the
transport does, locks to its tempo, and takes its step length from the rail
(1/16 by default). Starting the transport resets to step 1, and so does a
locate, so the pattern lines up with the bar however you move around the set.

Switch SYNC off and the RATE knob takes over, free-running regardless of the
transport. The ADV / CLOCK jack overrides both.

### MIDI

**Trigger + Pitch** (the default) makes the voice playable from a clip: each
note fires the envelopes at its own velocity and transposes both oscillators
from middle C. The sequencer keeps running underneath if it is running.
**Pitch Only** leaves triggering to the sequencer and uses notes to transpose.
**Off** ignores notes. The channel filter is next to the mode.

---

## Patchbay

Twenty-four points: fifteen inputs in plain text, nine outputs in reversed
boxes. Drag from any jack to another, grab a plugged input to move its cable,
right-click to unplug. Cables are saved with the preset and with the Live set.

Several jacks are normalled, meaning something is already wired to them until
you patch over it:

| Jack | Normalled to | Patching it |
| --- | --- | --- |
| **TRIGGER** in | the sequencer clock | the clock stops firing the envelopes; your trigger does, and the sequencer keeps stepping on its own |
| **VELOCITY** in | the step VELOCITY knobs | 0–5 V sets the envelope level instead |
| **EXT AUDIO** | the noise generator | replaces noise in the mixer, at the NOISE / EXT LEVEL knob |
| **FILTER MOD** | the noise generator | replaces noise as the filter's modulator, depth still on the knob |
| **ADV / CLOCK** | RATE / host sync | each rising edge is one step; RATE and sync are ignored |

The three **DECAY** inputs are summed with their knobs over ±5 V, so centring
the knob gives an incoming CV the widest range. **AMP CV**, **NOISE LEVEL** and
**1>2 FM AMT** are summed over 0–8 V. The **CV** inputs are 1 V/octave.

A few things worth trying, all of which are factory presets:

- **PITCH → AMP DECAY** gives every step its own length. Hats.
- **PITCH → FILTER MOD** with resonance up plays the self-oscillating filter.
- **PITCH → RATE** (sync off) gives every step its own duration. Swing.
- **OSC 2 → FILTER MOD** shakes the cutoff at audio rate. Vocal, talking drums.

---

## Presets

Factory presets come first in the list, then yours. Saving captures every knob,
switch and cable — the same XML the plugin stores in a Live set — into
`~/Library/Application Support/Stepkin/Presets/*.stepkin`. The list also appears
as the plugin's program list, so Live's own preset chooser works.

`FINE`, `DRIFT`, `SYNC`, the MIDI mode and the MIDI channel belong to the
instance rather than the sound, so loading a preset leaves them alone.

**DICE** rolls new step pitches and velocities; hold it to roll pitches only and
keep a rhythm you like.

---

## Layout

| Source | What it holds |
| --- | --- |
| `Source/Engine/Dsp.h` | Band-limited shapes, the syncable oscillator, the ladder, envelopes, decimation |
| `Source/Engine/Engine.{h,cpp}` | The whole instrument as plain C++: voice, sequencer, transport, patchbay |
| `Source/Params.h` | Parameter IDs, ranges and value formatting |
| `Source/Presets.h` | Factory patches, written as knob positions |
| `Source/PresetManager.{h,cpp}` | User presets on disk, and the program list |
| `Source/PluginProcessor.{h,cpp}` | JUCE plumbing, state, MIDI, the patch cables |
| `Source/PluginEditor.{h,cpp}` | The panel: layout, labels, animation, the rail |
| `Source/UI/` | Knobs, switches, buttons, LEDs, the patchbay component |
| `Tests/EngineTests.cpp` | Engine checks, no JUCE needed |
| `Tools/` | Headless snapshot and render tools |

The engine runs at four times the host sample rate and is decimated back down
through two halfband stages, which keeps the oscillators, hard sync and
through-zero FM clean.

---

## Notes

Stepkin is an original instrument. It is modelled on a well-known semi-modular
percussion synthesiser in the way any analog-style instrument is modelled on the
tradition it comes from: the signal flow, the control set and the panel
arrangement will feel familiar to anyone who has used one. No trademarked name,
wordmark, logo or artwork from any manufacturer appears anywhere in this
project, and none should be added.
