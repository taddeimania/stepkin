// EngineTests.cpp — engine checks that need no JUCE and no audio device.
// Build: part of the StepkinEngineTests target, run with ctest.

#include "Engine/Engine.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace stepkin;

namespace
{
int failures = 0;
int checks   = 0;

void check (bool ok, const std::string& what)
{
    ++checks;
    if (! ok)
    {
        ++failures;
        std::printf ("  FAIL  %s\n", what.c_str());
    }
}

void checkNear (double a, double b, double tol, const std::string& what)
{
    ++checks;
    if (! (std::abs (a - b) <= tol))
    {
        ++failures;
        std::printf ("  FAIL  %s  (got %.6g, expected %.6g +/- %.3g)\n", what.c_str(), a, b, tol);
    }
}

void section (const char* name) { std::printf ("%s\n", name); }

constexpr double kSr = 48000.0;

// Render a block and return it.
std::vector<float> renderFree (Engine& e, int numSamples)
{
    std::vector<float> out ((size_t) numSamples, 0.0f);
    HostInfo h; // not playing, not valid
    e.beginBlock (h, numSamples);
    e.render (out.data(), numSamples);
    return out;
}

double peak (const std::vector<float>& v)
{
    double p = 0.0;
    for (auto s : v) p = std::max (p, (double) std::abs (s));
    return p;
}

double rms (const std::vector<float>& v)
{
    double a = 0.0;
    for (auto s : v) a += (double) s * s;
    return v.empty() ? 0.0 : std::sqrt (a / (double) v.size());
}

bool allFinite (const std::vector<float>& v)
{
    for (auto s : v) if (! std::isfinite (s)) return false;
    return true;
}

// Goertzel magnitude of one bin.
double magAt (const std::vector<float>& v, double hz, double sr)
{
    const double w = 2.0 * M_PI * hz / sr;
    const double c = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (auto x : v)
    {
        const double s0 = (double) x + c * s1 - s2;
        s2 = s1; s1 = s0;
    }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) v.size();
}

// A panel set up as a simple audible kick: oscillator 1 only, filter open.
EngineParams basicPatch()
{
    EngineParams p;
    p.sync = false;
    p.rateHz = 8.0f;
    p.velocity.fill (1.0f);
    p.pitch.fill (0.0f);
    p.vcoFreq = { 0.5f, 0.5f };
    p.vcoLevel = { 1.0f, 0.0f };
    p.noiseLevel = 0.0f;
    p.cutoffHz = 18000.0f;
    p.resonance = 0.0f;
    p.vcfEgAmt = 0.0f;
    p.vcoEgAmt = { 0.0f, 0.0f };
    p.vcaDecay = 0.2f;
    p.volume = 0.8f;
    p.drift = 0.0f;
    p.midiMode = MidiMode::off;
    return p;
}

// ---------------------------------------------------------------------------
void testWaveforms()
{
    section ("waveforms");

    // A band-limited shape at a high fundamental should keep its aliases far
    // below the fundamental.
    for (int w = 0; w < 2; ++w)
    {
        Engine e;
        auto p = basicPatch();
        p.wave[0] = w == 0 ? Wave::square : Wave::triangle;
        // 2093 Hz (C7): harmonics above Nyquist would fold down audibly.
        p.vcoFreq[0] = 0.5f + (float) (3.0 / kVcoOctaves);
        p.vcaDecay = 10.0f;
        e.setParams (p);
        e.prepare (kSr);
        e.setPlayOn (true);

        renderFree (e, 4096);                 // settle and trigger
        auto block = renderFree (e, 16384);

        const double f0 = kC4Hz * 8.0;
        const double fund = magAt (block, f0, kSr);
        check (fund > 1.0e-3, std::string ("fundamental present, wave ") + std::to_string (w));

        // Inharmonic fold-down points for a 2093 Hz source at 48 kHz.
        double worst = 0.0;
        for (int k = 23; k <= 45; k += 2)      // odd harmonics beyond Nyquist
        {
            const double h = f0 * k;
            double folded = std::fmod (h, kSr);
            if (folded > kSr * 0.5) folded = kSr - folded;
            if (folded > 200.0 && folded < kSr * 0.45)
                worst = std::max (worst, magAt (block, folded, kSr));
        }
        const double db = 20.0 * std::log10 (std::max (1.0e-12, worst / fund));
        check (db < -55.0, std::string ("aliasing below -55 dBc, wave ") + std::to_string (w)
                           + " (got " + std::to_string (db) + " dB)");
    }
}

void testHardSync()
{
    section ("hard sync");

    Engine e;
    auto p = basicPatch();
    p.wave = { Wave::triangle, Wave::triangle };
    p.vcoLevel = { 0.0f, 1.0f };               // listen to oscillator 2 alone
    p.vcoFreq[0] = 0.5f;                       // 261.6 Hz
    p.vcoFreq[1] = 0.5f + (float) (1.37 / kVcoOctaves); // an unrelated higher pitch
    p.hardSync = true;
    p.vcaDecay = 10.0f;
    e.setParams (p);
    e.prepare (kSr);
    e.setPlayOn (true);
    renderFree (e, 4096);
    auto block = renderFree (e, 16384);

    // Synced, oscillator 2 must repeat at oscillator 1's rate, so energy
    // appears at the master fundamental.
    const double atMaster = magAt (block, kC4Hz, kSr);
    check (atMaster > 1.0e-4, "synced oscillator 2 has energy at the master fundamental");

    // Unsynced, it should not.
    Engine e2;
    p.hardSync = false;
    e2.setParams (p);
    e2.prepare (kSr);
    e2.setPlayOn (true);
    renderFree (e2, 4096);
    auto block2 = renderFree (e2, 16384);
    const double atMasterFree = magAt (block2, kC4Hz, kSr);
    check (atMaster > atMasterFree * 4.0, "sync locks oscillator 2 to oscillator 1");
}

void testFm()
{
    section ("frequency modulation");

    auto run = [] (float amount)
    {
        Engine e;
        auto p = basicPatch();
        p.vcoLevel = { 0.0f, 1.0f };
        p.vcoFreq[0] = 0.5f;
        p.vcoFreq[1] = 0.5f + (float) (2.0 / kVcoOctaves);
        p.fmAmount = amount;
        p.vcaDecay = 10.0f;
        e.setParams (p);
        e.prepare (kSr);
        e.setPlayOn (true);
        renderFree (e, 4096);
        return renderFree (e, 16384);
    };

    auto dry = run (0.0f);
    auto wet = run (0.6f);
    check (allFinite (wet), "FM output is finite");

    // FM creates sidebands at the carrier plus and minus the modulator.
    const double carrier = kC4Hz * 4.0;
    const double side = magAt (wet, carrier - kC4Hz, kSr);
    const double sideDry = magAt (dry, carrier - kC4Hz, kSr);
    check (side > sideDry * 8.0, "FM creates sidebands around the carrier");
}

void testFilter()
{
    section ("filter");

    // Drive the filter with noise and compare energy either side of cutoff.
    auto energyAt = [] (FilterMode mode, double probeHz)
    {
        Engine e;
        auto p = basicPatch();
        p.vcoLevel = { 0.0f, 0.0f };
        p.noiseLevel = 1.0f;
        p.filterMode = mode;
        p.cutoffHz = 1000.0f;
        p.resonance = 0.0f;
        p.vcaDecay = 10.0f;
        e.setParams (p);
        e.prepare (kSr);
        e.setPlayOn (true);
        renderFree (e, 8192);
        auto block = renderFree (e, 32768);
        // Average a few neighbouring bins to smooth the noise.
        double a = 0.0;
        for (int i = -2; i <= 2; ++i)
            a += magAt (block, probeHz * std::pow (2.0, i * 0.02), kSr);
        return a / 5.0;
    };

    const double lpLow  = energyAt (FilterMode::lowPass, 250.0);
    const double lpHigh = energyAt (FilterMode::lowPass, 8000.0);
    check (lpLow > lpHigh * 8.0, "low pass attenuates above cutoff");

    const double hpLow  = energyAt (FilterMode::highPass, 125.0);
    const double hpHigh = energyAt (FilterMode::highPass, 8000.0);
    check (hpHigh > hpLow * 8.0, "high pass attenuates below cutoff");
}

void testSelfOscillation()
{
    section ("filter self-oscillation");

    Engine e;
    auto p = basicPatch();
    p.vcoLevel = { 0.0f, 0.0f };
    p.noiseLevel = 0.0f;
    p.resonance = 1.0f;
    p.cutoffHz = 440.0f;
    p.vcaDecay = 10.0f;
    e.setParams (p);
    e.prepare (kSr);
    e.setPlayOn (true);
    renderFree (e, 24000);
    auto block = renderFree (e, 16384);

    check (allFinite (block), "self-oscillation stays finite");
    const double atCutoff = magAt (block, 440.0, kSr);
    check (atCutoff > 1.0e-3, "the filter sings at cutoff with resonance at maximum");
}

void testEnvelopesAndVelocity()
{
    section ("envelopes and velocity");

    // A step at zero velocity makes no sound.
    {
        Engine e;
        auto p = basicPatch();
        p.velocity.fill (0.0f);
        e.setParams (p);
        e.prepare (kSr);
        e.setPlayOn (true);
        auto block = renderFree (e, 24000);
        check (peak (block) < 1.0e-4, "velocity at minimum produces no output");
    }

    // Louder steps are louder.
    auto levelFor = [] (float vel)
    {
        Engine e;
        auto p = basicPatch();
        p.velocity.fill (vel);
        e.setParams (p);
        e.prepare (kSr);
        e.setPlayOn (true);
        return peak (renderFree (e, 12000));
    };
    const double quiet = levelFor (0.3f);
    const double loud  = levelFor (1.0f);
    check (loud > quiet * 1.8, "velocity sets how loud a step hits");

    // Velocity also loads the envelope, so a hard hit rings longer.
    auto tailFor = [] (float vel)
    {
        Engine e;
        auto p = basicPatch();
        p.rateHz = 1.0f;         // one step per second: plenty of room for the tail
        p.velocity.fill (vel);
        p.vcaDecay = 0.3f;
        e.setParams (p);
        e.prepare (kSr);
        e.setPlayOn (true);
        renderFree (e, 64);
        auto block = renderFree (e, 24000);
        // Energy in the second half of the tail, relative to this hit's peak.
        std::vector<float> tail (block.begin() + 12000, block.end());
        const double p0 = peak (block);
        return p0 > 0.0 ? rms (tail) / p0 : 0.0;
    };
    check (tailFor (1.0f) > tailFor (0.35f) * 1.1, "a harder hit decays a little more slowly");

    // The pitch envelope sweeps the oscillator.
    {
        Engine e;
        auto p = basicPatch();
        p.rateHz = 1.0f;
        p.vcoEgAmt[0] = 1.0f;
        p.vcoDecay = 0.2f;
        p.vcoFreq[0] = 0.4f;
        e.setParams (p);
        e.prepare (kSr);
        e.setPlayOn (true);
        renderFree (e, 16);
        const double atAttack = e.currentVcoHz (0);
        renderFree (e, 24000);
        const double atRest = e.currentVcoHz (0);
        check (atAttack > atRest * 4.0, "a positive EG amount spikes the pitch on the attack");
    }

    // A negative amount dips instead.
    {
        Engine e;
        auto p = basicPatch();
        p.rateHz = 1.0f;
        p.vcoEgAmt[0] = -1.0f;
        p.vcoDecay = 0.2f;
        p.vcoFreq[0] = 0.6f;
        e.setParams (p);
        e.prepare (kSr);
        e.setPlayOn (true);
        renderFree (e, 16);
        const double atAttack = e.currentVcoHz (0);
        renderFree (e, 24000);
        const double atRest = e.currentVcoHz (0);
        check (atAttack < atRest * 0.25, "a negative EG amount drops the pitch on the attack");
    }
}

void testSequencer()
{
    section ("sequencer");

    Engine e;
    auto p = basicPatch();
    p.rateHz = 100.0f;
    e.setParams (p);
    e.prepare (kSr);
    e.setPlayOn (true);

    // First clock plays step 1, then it walks 2..8 and wraps.
    // 100 steps/s: 1000 blocks of 24 samples is half a second, about 50 steps.
    std::vector<int> seen;
    int last = -1;
    for (int i = 0; i < 1000; ++i)
    {
        renderFree (e, 24);
        if (e.currentStep() != last)
        {
            last = e.currentStep();
            seen.push_back (last);
        }
    }
    check (seen.size() > 12, "the sequencer keeps stepping");
    bool walksInOrder = true;
    for (size_t i = 1; i < seen.size(); ++i)
        walksInOrder = walksInOrder && seen[i] == (seen[i - 1] + 1) % kNumSteps;
    check (walksInOrder, "steps advance one at a time and wrap after eight");
    check (seen[0] == 0, "the sequencer starts on step 1");

    // ADVANCE moves on without making a sound.
    {
        Engine e2;
        auto q = basicPatch();
        q.rateHz = 0.01f;    // effectively stopped
        e2.setParams (q);
        e2.prepare (kSr);
        e2.setPlayOn (false);
        const int before = e2.currentStep();
        e2.pressAdvance();
        check (e2.currentStep() == (before + 1) % kNumSteps, "ADVANCE moves one step on");
        auto block = renderFree (e2, 4800);
        check (peak (block) < 1.0e-4, "ADVANCE does not sound the step it lands on");

        // TRIGGER sounds the current step without moving.
        const int at = e2.currentStep();
        e2.pressTrigger();
        auto block2 = renderFree (e2, 4800);
        check (e2.currentStep() == at, "TRIGGER stays on the same step");
        check (peak (block2) > 1.0e-3, "TRIGGER sounds the current step");
    }
}

void testHostSync()
{
    section ("host sync");

    Engine e;
    auto p = basicPatch();
    p.sync = true;
    p.syncStepBeats = 0.25;      // sixteenth notes
    e.setParams (p);
    e.prepare (kSr);
    e.setPlayOn (true);

    // Not playing: nothing moves.
    {
        HostInfo h; h.valid = true; h.playing = false; h.bpm = 120.0; h.ppq = 0.0;
        std::vector<float> out (512, 0.0f);
        e.beginBlock (h, 512);
        e.render (out.data(), 512);
        check (peak (out) < 1.0e-4, "a stopped host leaves the sequencer silent");
        check (! e.isRunning(), "the sequencer reports stopped when the host is stopped");
    }

    // Playing at 120 BPM: one sixteenth is 0.125 s, so two seconds is 16 steps.
    {
        const int blockSize = 256;
        const double ppqPerBlock = (double) blockSize * 120.0 / 60.0 / kSr;
        double ppq = 0.0;
        const uint32_t before = e.stepCount();
        std::vector<float> out ((size_t) blockSize, 0.0f);
        for (int i = 0; i < (int) (2.0 * kSr / blockSize); ++i)
        {
            HostInfo h; h.valid = true; h.playing = true; h.bpm = 120.0; h.ppq = ppq;
            e.beginBlock (h, blockSize);
            e.render (out.data(), blockSize);
            ppq += ppqPerBlock;
        }
        const double steps = (double) (e.stepCount() - before);
        checkNear (steps, 16.0, 1.0, "16 sixteenth-note steps in two seconds at 120 BPM");
        check (e.isRunning(), "the sequencer runs while the host plays");
    }
}

void testPatchbay()
{
    section ("patchbay");

    // VELOCITY input overrides the step knobs.
    {
        Engine e;
        auto p = basicPatch();
        p.velocity.fill (0.0f);        // silent on its own
        p.rateHz = 4.0f;
        e.setParams (p);
        e.prepare (kSr);
        auto patch = emptyPatch();
        patch[inVelocity] = outVcaEg;  // not a real musical patch, but non-zero at rest? no:
        e.setPatch (patch);
        e.setPlayOn (true);
        auto block = renderFree (e, 12000);
        // VCA EG is zero while silent, so this stays silent; the point is only
        // that patching does not crash or blow up.
        check (allFinite (block), "a feedback patch stays finite");
    }

    // EXT AUDIO replaces the noise generator in the mixer.
    {
        Engine e;
        auto p = basicPatch();
        p.vcoLevel = { 1.0f, 0.0f };
        p.noiseLevel = 1.0f;
        p.vcaDecay = 10.0f;
        e.setParams (p);
        e.prepare (kSr);
        auto patch = emptyPatch();
        patch[inExtAudio] = outVco1;   // oscillator 1 into the third mixer channel
        e.setPatch (patch);
        e.setPlayOn (true);
        renderFree (e, 4096);
        auto block = renderFree (e, 16384);
        check (allFinite (block), "EXT AUDIO patch is finite");
        // With noise replaced by a tone, the output should be tonal: strong at
        // the fundamental rather than spread out.
        const double fund = magAt (block, kC4Hz, kSr);
        check (fund > 5.0e-3, "EXT AUDIO replaces the noise generator with the patched signal");
    }

    // TRIGGER input takes the clock's place for the envelopes.
    {
        Engine e;
        auto p = basicPatch();
        p.rateHz = 20.0f;
        e.setParams (p);
        e.prepare (kSr);
        auto patch = emptyPatch();
        patch[inTrigger] = outVelocity;   // a steady 5 V, so no rising edges after the first
        e.setPatch (patch);
        e.setPlayOn (true);
        renderFree (e, 2000);
        const uint32_t steps = e.stepCount();
        auto block = renderFree (e, 24000);
        check (e.stepCount() > steps, "the sequencer keeps stepping with TRIGGER patched");
        check (allFinite (block), "TRIGGER patch is finite");
    }

    // ADV / CLOCK drives the sequencer from an external edge.
    {
        Engine e;
        auto p = basicPatch();
        p.rateHz = 200.0f;              // would be fast if it were in charge
        e.setParams (p);
        e.prepare (kSr);
        auto patch = emptyPatch();
        patch[inAdvClock] = outVcaEg;   // nothing is firing, so no edges arrive
        e.setPatch (patch);
        e.setPlayOn (true);
        const uint32_t before = e.stepCount();
        renderFree (e, 24000);
        check (e.stepCount() == before, "a patched ADV / CLOCK takes over from the rate control");
    }

    // Every output can feed every input without producing NaNs.
    {
        for (int in = 0; in < kNumInputs; ++in)
            for (int out = 0; out < kNumOutputs; ++out)
            {
                Engine e;
                auto p = basicPatch();
                p.rateHz = 30.0f;
                p.noiseLevel = 0.4f;
                p.resonance = 0.7f;
                p.fmAmount = 0.5f;
                p.hardSync = true;
                e.setParams (p);
                e.prepare (kSr);
                auto patch = emptyPatch();
                patch[in] = out;
                e.setPatch (patch);
                e.setPlayOn (true);
                auto block = renderFree (e, 4800);
                if (! allFinite (block) || peak (block) > 1.01)
                {
                    check (false, "patch out " + std::to_string (out) + " -> in " + std::to_string (in)
                                  + " stays finite and in range");
                    return;
                }
            }
        check (true, "all 135 single-cable patches stay finite and in range");
    }
}

void testMidi()
{
    section ("MIDI");

    // A note triggers the voice and transposes it.
    {
        Engine e;
        auto p = basicPatch();
        p.midiMode = MidiMode::triggerAndPitch;
        p.rateHz = 0.01f;
        p.vcoFreq[0] = 0.5f;
        e.setParams (p);
        e.prepare (kSr);
        e.setPlayOn (false);
        renderFree (e, 64);
        e.noteOn (72, 1.0f);             // an octave above middle C
        auto block = renderFree (e, 4800);
        check (peak (block) > 1.0e-3, "a MIDI note sounds the voice");
        checkNear (e.currentVcoHz (0), kC4Hz * 2.0, kC4Hz * 0.08, "a MIDI note transposes the oscillator");
    }

    // Note velocity carries through to the envelopes.
    {
        auto levelFor = [] (float v)
        {
            Engine e;
            auto p = basicPatch();
            p.midiMode = MidiMode::triggerAndPitch;
            p.rateHz = 0.01f;
            e.setParams (p);
            e.prepare (kSr);
            e.setPlayOn (false);
            renderFree (e, 64);
            e.noteOn (60, v);
            return peak (renderFree (e, 4800));
        };
        check (levelFor (1.0f) > levelFor (0.3f) * 1.8, "MIDI velocity sets how hard the voice hits");
    }

    // Pitch-only mode leaves triggering to the sequencer.
    {
        Engine e;
        auto p = basicPatch();
        p.midiMode = MidiMode::pitchOnly;
        p.rateHz = 0.01f;
        e.setParams (p);
        e.prepare (kSr);
        e.setPlayOn (false);
        renderFree (e, 64);
        e.noteOn (72, 1.0f);
        auto block = renderFree (e, 4800);
        check (peak (block) < 1.0e-4, "pitch-only mode does not trigger from MIDI");
        checkNear (e.currentVcoHz (0), kC4Hz * 2.0, kC4Hz * 0.08, "pitch-only mode still transposes");
    }
}

void testPitchRouting()
{
    section ("sequencer pitch routing");

    auto hzFor = [] (PitchMod mode, int which)
    {
        Engine e;
        auto p = basicPatch();
        p.pitchMod = mode;
        p.rateHz = 0.01f;
        p.pitch.fill (0.4f);         // two octaves up at a +/-5 octave range
        e.setParams (p);
        e.prepare (kSr);
        e.setPlayOn (true);
        renderFree (e, 256);
        return e.currentVcoHz (which);
    };

    checkNear (hzFor (PitchMod::off, 0), kC4Hz, kC4Hz * 0.05, "OFF leaves oscillator 1 alone");
    checkNear (hzFor (PitchMod::off, 1), kC4Hz, kC4Hz * 0.05, "OFF leaves oscillator 2 alone");
    checkNear (hzFor (PitchMod::both, 0), kC4Hz * 4.0, kC4Hz * 0.2, "both: oscillator 1 follows the step");
    checkNear (hzFor (PitchMod::both, 1), kC4Hz * 4.0, kC4Hz * 0.2, "both: oscillator 2 follows the step");
    checkNear (hzFor (PitchMod::vco2, 0), kC4Hz, kC4Hz * 0.05, "oscillator 2 only: oscillator 1 stays put");
    checkNear (hzFor (PitchMod::vco2, 1), kC4Hz * 4.0, kC4Hz * 0.2, "oscillator 2 only: oscillator 2 follows");

    // The PITCH jack carries the step voltage whatever the switch says.
    Engine e;
    auto p = basicPatch();
    p.pitchMod = PitchMod::off;
    p.pitch.fill (0.4f);
    e.setParams (p);
    e.prepare (kSr);
    e.setPlayOn (true);
    renderFree (e, 256);
    checkNear (e.outputVoltage (outPitch), 2.0, 0.01, "the PITCH jack follows the step knob with the switch off");
}

void testOutputRange()
{
    section ("output range");

    Engine e;
    auto p = basicPatch();
    // Everything turned up at once.
    p.vcoLevel = { 1.0f, 1.0f };
    p.noiseLevel = 1.0f;
    p.resonance = 1.0f;
    p.fmAmount = 1.0f;
    p.hardSync = true;
    p.vcfEgAmt = 1.0f;
    p.vcoEgAmt = { 1.0f, -1.0f };
    p.volume = 1.0f;
    p.rateHz = 60.0f;
    e.setParams (p);
    e.prepare (kSr);
    e.setPlayOn (true);
    auto block = renderFree (e, 48000);
    check (allFinite (block), "everything at maximum stays finite");
    check (peak (block) <= 1.0, "output never leaves -1..+1");
    check (rms (block) > 0.05, "everything at maximum is actually loud");
}

void testSampleRates()
{
    section ("sample rates");

    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        Engine e;
        auto p = basicPatch();
        p.rateHz = 10.0f;
        p.noiseLevel = 0.3f;
        e.setParams (p);
        e.prepare (sr);
        e.setPlayOn (true);
        std::vector<float> out (2048, 0.0f);
        HostInfo h;
        for (int i = 0; i < 20; ++i)
        {
            e.beginBlock (h, 2048);
            e.render (out.data(), 2048);
            if (! allFinite (out))
                break;
        }
        check (allFinite (out), "stable at " + std::to_string ((int) sr) + " Hz");
    }
}
} // namespace

int main()
{
    std::printf ("Stepkin engine tests\n\n");

    testWaveforms();
    testHardSync();
    testFm();
    testFilter();
    testSelfOscillation();
    testEnvelopesAndVelocity();
    testSequencer();
    testHostSync();
    testPatchbay();
    testMidi();
    testPitchRouting();
    testOutputRange();
    testSampleRates();

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
