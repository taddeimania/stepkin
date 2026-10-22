// Engine.h — the complete Stepkin instrument as plain C++ (no JUCE):
// two oscillators with hard sync and linear FM, a white noise generator, a
// three-channel saturating mixer, a 4-pole ladder switchable between low-pass
// and high-pass, three decay envelopes driven by a per-step velocity, an
// 8-step sequencer with pitch and velocity per step, the transport, and a
// 24-point patchbay with the hardware's normalisations.
//
// Everything runs at four times the host rate and is decimated on output.
#pragma once

#include "Dsp.h"

#include <array>
#include <cstdint>

namespace stepkin
{
constexpr int kNumSteps = 8;

// ---------------------------------------------------------------------------
// Patch points — 15 inputs and 9 outputs, as on the hardware.
// ---------------------------------------------------------------------------
enum In : int
{
    inTrigger, inVcaCv,
    inVelocity, inVcaDecay,
    inExtAudio, inVcfDecay,
    inNoiseLevel, inVcoDecay,
    inVcfMod, inVco1Cv,
    inFmAmt, inVco2Cv,
    inTempo, inRunStop, inAdvClock,
    kNumInputs
};

enum Out : int
{
    outVca, outVcaEg, outVcfEg, outVcoEg,
    outVco1, outVco2,
    outTrigger, outVelocity, outPitch,
    kNumOutputs
};

using Patch = std::array<int, kNumInputs>; // source output per input, -1 = unpatched

inline Patch emptyPatch() { Patch p; p.fill (-1); return p; }

// Oscillator waveform: the switch is up for the pulse, down for the triangle.
enum class Wave { square = 0, triangle = 1 };

// Which oscillators the step PITCH knobs reach. The switch is up for both,
// centred for off, down for oscillator 2 alone.
enum class PitchMod { both = 0, off = 1, vco2 = 2 };

enum class FilterMode { lowPass = 0, highPass = 1 };

// What incoming MIDI notes do.
enum class MidiMode
{
    triggerAndPitch = 0, // note fires the envelopes at its velocity and transposes the oscillators
    pitchOnly       = 1, // note transposes only; the sequencer keeps triggering
    off             = 2
};

// ---------------------------------------------------------------------------
// Panel state
// ---------------------------------------------------------------------------
struct EngineParams
{
    // Sequencer: PITCH knobs are bipolar (-1..+1), VELOCITY knobs are 0..1.
    std::array<float, kNumSteps> pitch {};
    std::array<float, kNumSteps> velocity {};
    PitchMod pitchMod = PitchMod::off;

    // Clock
    float  rateHz = 8.0f;          // free-running step rate, 0.5..800 Hz
    bool   sync = true;            // follow the host transport
    double syncStepBeats = 0.25;   // length of one step in quarter notes when synced

    // Oscillators
    std::array<float, 2> vcoFreq { 0.5f, 0.5f };   // 0..1, centre = the reference pitch
    std::array<Wave, 2>  wave { Wave::triangle, Wave::triangle };
    std::array<float, 2> vcoEgAmt { 0.0f, 0.0f };  // -1..+1
    bool  hardSync = false;
    float fmAmount = 0.0f;         // 0..1

    // Mixer 0..1
    std::array<float, 2> vcoLevel { 0.7f, 0.0f };
    float noiseLevel = 0.0f;

    // Filter
    FilterMode filterMode = FilterMode::lowPass;
    float cutoffHz   = 1200.0f;
    float resonance  = 0.0f;       // 0..1
    float vcfEgAmt   = 0.0f;       // -1..+1
    float noiseVcfMod = 0.0f;      // 0..1

    // Envelopes (seconds)
    float vcoDecay = 0.05f;
    float vcfDecay = 0.12f;
    float vcaDecay = 0.25f;
    bool  vcaSlowAttack = false;   // 1 ms when false, 100 ms when true

    // Output
    float volume = 0.75f;          // 0..1

    // Globals
    MidiMode midiMode = MidiMode::triggerAndPitch;
    float fineCents = 0.0f;        // +/-50
    float drift     = 0.25f;       // analog pitch drift amount 0..1
};

struct HostInfo
{
    bool   valid   = false;
    bool   playing = false;
    double ppq     = 0.0;
    double bpm     = 120.0;
};

// Mapping helpers shared with the plugin layer.
inline double decaySecondsFromNorm (double n) { return 0.005 * std::pow (2000.0, dsp::clamp (n, 0.0, 1.0)); }
inline double normFromDecaySeconds (double s) { return std::log (s / 0.005) / std::log (2000.0); }

constexpr double kC4Hz        = 261.6255653005986;
constexpr double kVcoOctaves   = 10.0;   // full sweep of a FREQUENCY knob
constexpr double kPitchOctaves = 10.0;   // full sweep of a step PITCH knob (+/-5 V)

// ---------------------------------------------------------------------------
class Engine
{
public:
    Engine();

    void prepare (double hostSampleRate);
    void reset();

    void setParams (const EngineParams& p) noexcept { params = p; }
    const EngineParams& getParams() const noexcept { return params; }
    void setPatch (const Patch& p) noexcept { patch = p; }

    // Call once per host block before render(); render() may then be called
    // several times to split the block at MIDI events.
    void beginBlock (const HostInfo& host, int numSamples) noexcept;
    void render (float* out, int numSamples) noexcept;

    // --- Performance & MIDI --------------------------------------------------
    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void allNotesOff() noexcept;

    // --- Panel buttons -------------------------------------------------------
    void pressAdvance() noexcept;   // step on without triggering
    void pressTrigger() noexcept;   // fire the current step without advancing
    void setPlayOn (bool on) noexcept;
    void togglePlay() noexcept { setPlayOn (! playOn); }

    // --- State for the panel -------------------------------------------------
    bool   isPlayOn() const noexcept   { return playOn; }
    bool   isRunning() const noexcept  { return running; }
    int    currentStep() const noexcept { return stepIndex; }
    uint32_t stepCount() const noexcept { return stepCounter; }
    double vcoEnvLevel() const noexcept { return vcoEnv.level(); }
    double vcfEnvLevel() const noexcept { return vcfEnv.level(); }
    double vcaEnvLevel() const noexcept { return vcaEnv.level(); }
    double currentVcoHz (int vco) const noexcept { return lastVcoHz[(size_t) vco]; }
    double outputVoltage (int o) const noexcept { return outV[(size_t) o]; }

    // Volts a step PITCH knob produces, which is also what the PITCH jack puts out.
    double stepVolts (int step) const noexcept;

private:
    double tick() noexcept;
    double inV (int input) const noexcept;
    bool   isPatched (int input) const noexcept { return patch[(size_t) input] >= 0; }

    void clockPulse() noexcept;      // advance a step and fire the envelopes
    void advanceStep() noexcept;     // advance without firing
    void fireEnvelopes (double velocity) noexcept;
    double currentVelocity() const noexcept;
    void doReset() noexcept;

    EngineParams params;
    Patch patch = emptyPatch();

    double hostRate = 48000.0, fs = 192000.0;

    // Voice
    std::array<dsp::Oscillator, 2> osc;
    dsp::Ladder ladder;
    dsp::DecayEnvelope vcoEnv, vcfEnv, vcaEnv;
    dsp::Decimator4 decimator;
    dsp::DcBlocker dcBlock;
    std::array<dsp::Smoother, 2> pitchSmooth;
    std::array<dsp::Smoother, 2> vcoLevelSmooth;
    dsp::Smoother noiseLevelSmooth, cutoffSmooth, resSmooth, vcfEgSmooth, volSmooth, fmSmooth;
    std::array<dsp::Smoother, 2> vcoEgSmooth;
    dsp::Rng rng, noiseRng;
    std::array<double, 2> driftTarget {}, driftValue {};
    int driftCounter = 0;
    double noiseSample = 0.0;

    // Patch voltages (previous tick)
    std::array<double, kNumOutputs> outV {};

    // Clocking
    double internalPhase = 0.0;
    HostInfo host;
    double hostPpq = 0.0, hostPpqPerTick = 0.0;
    int64_t lastGrid = 0;
    double lastGridLen = 0.25;
    bool hostWasPlaying = false;
    bool hostPlayingNow = false;
    double expectedPpq = 0.0;
    bool   havePpq = false;

    bool playOn  = true;
    bool running = false;
    bool armed   = true;              // the first clock after a reset plays step 1
    int  stepIndex = 0;
    uint32_t stepCounter = 0;
    double ticksSincePulse = 0.0, pulsePeriod = 4800.0;

    // Gates / triggers
    dsp::EdgeDetector triggerIn, clockIn, runStopIn;
    int triggerOutTicks = 0;

    // MIDI
    std::array<int, 16> heldNotes {};
    int numHeld = 0;
    double midiPitchV = 0.0;
    double midiVelocity = 1.0;

    // Panel readouts
    std::array<double, 2> lastVcoHz { kC4Hz, kC4Hz };
};
} // namespace stepkin
