The first release of Stepkin, a semi-modular analog-style percussion synthesiser for any DAW that hosts VST3 or Audio Unit instruments.

### What's in it
- **Two oscillators with hard sync and linear FM.** Each picks a pulse or a triangle. Sync locks oscillator 2 to oscillator 1 and turns its FREQUENCY knob into a timbre control; FM is a fixed frequency deviation, so the character follows the interval between the two and goes through zero at deep settings for bell-like and clangorous tones.
- **A noise generator**, usable as a sound source in the mixer or as a modulator that shakes the filter cutoff.
- **A 4-pole ladder filter**, switchable between low pass and high pass, that saturates as you drive it and self-oscillates near full resonance.
- **Three decay envelopes** for pitch, filter and amplitude, with bipolar amount knobs for the first two and a selectable 1 ms or 100 ms amplitude attack.
- **An 8-step sequencer** with a pitch knob and a velocity knob per step. Velocity sets the level of all three envelopes at once, so it shapes punch and timbre as well as volume, and a step turned fully down is silent.
- **A 24-point patchbay** with draggable cables and the normalisations you would expect: patching TRIGGER takes the clock's place for the envelopes, EXT AUDIO displaces noise in the mixer, FILTER MOD displaces noise as the modulator.
- **Follows your DAW's tempo and transport**, with the step length selectable from 1/1 to 1/16T, or runs free from its own RATE knob all the way up into the audio range, where the eight steps become one cycle of a waveform.
- **MIDI**: notes can fire the envelopes at their own velocity and transpose the oscillators, or transpose only and leave the triggering to the sequencer.
- **Presets**: 11 factory patches, plus your own saved as files carrying every knob, switch and cable. A DICE button rolls new step pitches and velocities.

### Install
Download the file for your system below.

- **macOS** (universal, Apple silicon + Intel): unzip, move `Stepkin.vst3` into `~/Library/Audio/Plug-Ins/VST3` and `Stepkin.component` into `~/Library/Audio/Plug-Ins/Components`, then run:
  ```sh
  xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Stepkin.vst3 ~/Library/Audio/Plug-Ins/Components/Stepkin.component
  ```
  The build isn't signed with an Apple developer certificate, so without this step your DAW won't load it. `Stepkin.app` is a standalone version you can run without a DAW.
- **Windows**: unzip and copy the `Stepkin.vst3` folder into `C:\Program Files\Common Files\VST3`. `Stepkin.exe` is the standalone version.
- **Linux**: extract, then copy `Stepkin.vst3` into `~/.vst3`.

Then rescan plug-ins in your DAW. Stepkin makes its own sound: drop it on a MIDI track and press play. The README has a section-by-section guide to the panel.
