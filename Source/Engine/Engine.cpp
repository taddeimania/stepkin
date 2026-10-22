#include "Engine.h"

namespace stepkin
{
using dsp::clamp;

namespace
{
    constexpr double kOversample    = 4.0;
    constexpr double kVcoEgOctaves  = 6.0;    // VCO EG AMOUNT at full: octaves of pitch sweep
    constexpr double kVcfEgOctaves  = 7.0;    // VCF EG AMOUNT at full: octaves of cutoff sweep
    constexpr double kNoiseModOct   = 4.0;    // NOISE / VCF MOD at full: octaves of noise jitter
    constexpr double kFmHz          = 4000.0; // 1->2 FM AMOUNT at full: peak frequency deviation
    constexpr double kMaxVcoHz      = 16000.0;
    constexpr double kTriggerSec    = 0.001;
    constexpr double kFastAttackSec = 0.001;
    constexpr double kSlowAttackSec = 0.100;
    constexpr double kMinRateHz     = 0.5;
    constexpr double kMaxRateHz     = 800.0;

    // Velocity loads the envelope generators, so a harder hit also rings
    // slightly longer. A quiet step keeps about 60% of the knob's decay.
    inline double velocityDecayScale (double v) { return 0.6 + 0.4 * clamp (v, 0.0, 1.0); }

    inline double levelTaper (double x) { x = clamp (x, 0.0, 1.0); return x * std::sqrt (x); }
}

Engine::Engine() = default;

void Engine::prepare (double hostSampleRate)
{
    hostRate = hostSampleRate > 0.0 ? hostSampleRate : 48000.0;
    fs = hostRate * kOversample;

    ladder.prepare (fs);
    vcoEnv.prepare (fs);
    vcfEnv.prepare (fs);
    vcaEnv.prepare (fs);
    dcBlock.prepare (hostRate);
    decimator.reset();

    for (auto& s : pitchSmooth)    s.prepare (fs, 0.002);
    for (auto& s : vcoLevelSmooth) s.prepare (fs, 0.008);
    for (auto& s : vcoEgSmooth)    s.prepare (fs, 0.008);
    noiseLevelSmooth.prepare (fs, 0.008);
    cutoffSmooth.prepare (fs, 0.004);
    resSmooth.prepare (fs, 0.008);
    vcfEgSmooth.prepare (fs, 0.008);
    volSmooth.prepare (fs, 0.010);
    fmSmooth.prepare (fs, 0.006);

    reset();
}

void Engine::reset()
{
    for (auto& o : osc) o.reset();
    ladder.reset();
    vcoEnv.reset();
    vcfEnv.reset();
    vcaEnv.reset();
    decimator.reset();
    outV.fill (0.0);

    for (int v = 0; v < 2; ++v)
    {
        pitchSmooth[(size_t) v].snap ((params.vcoFreq[(size_t) v] - 0.5) * kVcoOctaves);
        vcoLevelSmooth[(size_t) v].snap (levelTaper (params.vcoLevel[(size_t) v]));
        vcoEgSmooth[(size_t) v].snap (params.vcoEgAmt[(size_t) v]);
    }
    noiseLevelSmooth.snap (levelTaper (params.noiseLevel));
    cutoffSmooth.snap (std::log2 (clamp (params.cutoffHz, 20.0f, 20000.0f) / 20.0));
    resSmooth.snap (params.resonance);
    vcfEgSmooth.snap (params.vcfEgAmt);
    volSmooth.snap (params.volume * params.volume);
    fmSmooth.snap (params.fmAmount);

    doReset();
    internalPhase = 1.0;
    hostWasPlaying = false;
    havePpq = false;
    triggerOutTicks = 0;
    numHeld = 0;
    triggerIn.reset();
    clockIn.reset();
    runStopIn.reset();
}

// ---------------------------------------------------------------------------
// Transport
// ---------------------------------------------------------------------------
void Engine::beginBlock (const HostInfo& h, int numSamples) noexcept
{
    host = h;
    const bool hostPlaying = h.valid && h.playing;
    const double len = std::max (1.0e-3, params.syncStepBeats);

    if (params.sync)
    {
        hostPpqPerTick = std::max (1.0, h.bpm) / 60.0 / fs;
        if (hostPlaying)
        {
            const bool started = ! hostWasPlaying;
            const bool jumped  = havePpq && std::abs (h.ppq - expectedPpq) > 0.05;
            if (started || jumped)
            {
                doReset();
                lastGrid = (int64_t) std::ceil (h.ppq / len - 1.0e-7) - 1;
            }
            else if (std::abs (lastGridLen - len) > 1.0e-9)
            {
                lastGrid = (int64_t) std::floor (h.ppq / len + 1.0e-9);
            }
            hostPpq = h.ppq;
            expectedPpq = h.ppq + (double) numSamples * std::max (1.0, h.bpm) / 60.0 / hostRate;
            havePpq = true;
        }
        else
        {
            havePpq = false;
        }
    }
    lastGridLen = len;

    hostPlayingNow = hostPlaying;
    hostWasPlaying = hostPlaying;
    running = (params.sync ? hostPlaying : true) && playOn;
}

void Engine::setPlayOn (bool on) noexcept
{
    if (on && ! playOn && ! params.sync)
        internalPhase = 1.0; // first pulse right away
    playOn = on;
    running = (params.sync ? hostPlayingNow : true) && playOn;
}

void Engine::doReset() noexcept
{
    stepIndex = 0;
    armed = true;
}

double Engine::stepVolts (int step) const noexcept
{
    const auto i = (size_t) ((step % kNumSteps + kNumSteps) % kNumSteps);
    return params.pitch[i] * (kPitchOctaves * 0.5);
}

double Engine::currentVelocity() const noexcept
{
    // The VELOCITY jack is normalled to the step VELOCITY knobs.
    if (isPatched (inVelocity))
        return clamp (inV (inVelocity) / 5.0, 0.0, 1.0);
    return clamp ((double) params.velocity[(size_t) stepIndex], 0.0, 1.0);
}

void Engine::advanceStep() noexcept
{
    if (armed)
    {
        armed = false;   // play step 1 on the first clock after a reset
        return;
    }
    stepIndex = (stepIndex + 1) % kNumSteps;
}

void Engine::fireEnvelopes (double velocity) noexcept
{
    const bool a = vcoEnv.trigger (velocity);
    const bool b = vcfEnv.trigger (velocity);
    const bool c = vcaEnv.trigger (velocity);
    if (a || b || c)
        triggerOutTicks = std::max (1, (int) (kTriggerSec * fs));
}

void Engine::clockPulse() noexcept
{
    ++stepCounter;
    if (ticksSincePulse > 8.0)
        pulsePeriod = ticksSincePulse;
    ticksSincePulse = 0.0;

    advanceStep();

    // The TRIGGER jack is normalled to the sequencer clock; patching it takes
    // the clock's place as the envelope trigger, and the sequencer keeps
    // stepping on its own.
    if (! isPatched (inTrigger))
        fireEnvelopes (currentVelocity());
}

void Engine::pressAdvance() noexcept
{
    // Manual step without triggering the note it lands on.
    armed = false;
    stepIndex = (stepIndex + 1) % kNumSteps;
}

void Engine::pressTrigger() noexcept
{
    fireEnvelopes (currentVelocity());
}

// ---------------------------------------------------------------------------
// MIDI
// ---------------------------------------------------------------------------
void Engine::noteOn (int note, float velocity) noexcept
{
    if (params.midiMode == MidiMode::off)
        return;

    int w = 0;
    for (int i = 0; i < numHeld; ++i)
        if (heldNotes[(size_t) i] != note)
            heldNotes[(size_t) w++] = heldNotes[(size_t) i];
    numHeld = w;
    if (numHeld == (int) heldNotes.size())
    {
        for (int i = 1; i < numHeld; ++i)
            heldNotes[(size_t) (i - 1)] = heldNotes[(size_t) i];
        --numHeld;
    }
    heldNotes[(size_t) numHeld++] = note;

    midiPitchV   = (note - 60) / 12.0;
    midiVelocity = clamp ((double) velocity, 0.0, 1.0);

    if (params.midiMode == MidiMode::triggerAndPitch)
        fireEnvelopes (midiVelocity > 0.0 ? midiVelocity : currentVelocity());
}

void Engine::noteOff (int note) noexcept
{
    int w = 0;
    for (int i = 0; i < numHeld; ++i)
        if (heldNotes[(size_t) i] != note)
            heldNotes[(size_t) w++] = heldNotes[(size_t) i];
    numHeld = w;
    if (numHeld > 0)
        midiPitchV = (heldNotes[(size_t) (numHeld - 1)] - 60) / 12.0;
    // The last pitch is held after release, the way a CV output would hold it.
}

void Engine::allNotesOff() noexcept
{
    numHeld = 0;
}

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------
double Engine::inV (int input) const noexcept
{
    const int src = patch[(size_t) input];
    return src >= 0 && src < kNumOutputs ? outV[(size_t) src] : 0.0;
}

void Engine::render (float* out, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        const double a = tick();
        const double b = tick();
        const double c = tick();
        const double d = tick();
        const double y = dcBlock.process (decimator.process (a, b, c, d));
        out[i] = (float) clamp (std::tanh (y * 2.2) * 0.9, -1.0, 1.0);
    }
}

double Engine::tick() noexcept
{
    // ---- Patched gate inputs ------------------------------------------------
    if (isPatched (inRunStop))
    {
        const int e = runStopIn.process (inV (inRunStop));
        if (e > 0)      setPlayOn (true);
        else if (e < 0) setPlayOn (false);
    }
    else runStopIn.reset();

    // ---- Clock --------------------------------------------------------------
    ticksSincePulse += 1.0;
    const bool clockPatched = isPatched (inAdvClock);
    if (clockPatched)
    {
        // An external clock takes over; the RATE control is ignored.
        if (clockIn.process (inV (inAdvClock)) > 0 && running)
            clockPulse();
    }
    else clockIn.reset();

    if (! clockPatched)
    {
        if (params.sync)
        {
            if (hostPlayingNow)
            {
                const double len = std::max (1.0e-3, params.syncStepBeats);
                const auto g = (int64_t) std::floor (hostPpq / len + 1.0e-9);
                if (g > lastGrid)
                {
                    lastGrid = g;
                    if (running)
                        clockPulse();
                }
                hostPpq += hostPpqPerTick;
            }
        }
        else if (running)
        {
            // The TEMPO jack is 1 V/octave on the clock oscillator.
            double rate = clamp (params.rateHz, kMinRateHz, kMaxRateHz);
            if (isPatched (inTempo))
                rate *= std::exp2 (clamp (inV (inTempo), -5.0, 5.0));
            rate = clamp (rate, 0.01, 0.45 * fs);

            internalPhase += rate / fs;
            if (internalPhase >= 1.0)
            {
                internalPhase -= std::floor (internalPhase);
                clockPulse();
            }
        }
    }

    // A patched TRIGGER fires the envelopes at the current velocity without
    // moving the sequencer on.
    if (isPatched (inTrigger))
    {
        if (triggerIn.process (inV (inTrigger)) > 0)
            fireEnvelopes (currentVelocity());
    }
    else triggerIn.reset();

    // ---- Envelopes ----------------------------------------------------------
    // The DECAY jacks are summed with their knobs: +/-5 V covers half the
    // knob's travel in each direction.
    auto decayFor = [this] (float knobSeconds, int cvInput)
    {
        double n = normFromDecaySeconds (std::max (0.005, (double) knobSeconds));
        if (isPatched (cvInput))
            n += clamp (inV (cvInput), -5.0, 5.0) / 10.0;
        return decaySecondsFromNorm (clamp (n, 0.0, 1.0));
    };

    const double vel = currentVelocity();
    const double stretch = velocityDecayScale (vel);

    vcoEnv.setTimes (kFastAttackSec, decayFor (params.vcoDecay, inVcoDecay) * stretch);
    vcfEnv.setTimes (kFastAttackSec, decayFor (params.vcfDecay, inVcfDecay) * stretch);
    vcaEnv.setTimes (params.vcaSlowAttack ? kSlowAttackSec : kFastAttackSec,
                     decayFor (params.vcaDecay, inVcaDecay) * stretch);

    const double eVco = vcoEnv.tick();
    const double eVcf = vcfEnv.tick();
    const double eVca = vcaEnv.tick();

    // ---- Analog drift -------------------------------------------------------
    if (--driftCounter <= 0)
    {
        driftCounter = (int) (0.7 * fs);
        driftTarget[0] = rng.bipolar();
        driftTarget[1] = rng.bipolar();
    }
    const double driftCoef = 1.0 / (1.5 * fs);
    for (int v = 0; v < 2; ++v)
        driftValue[(size_t) v] += (driftTarget[(size_t) v] - driftValue[(size_t) v]) * driftCoef;

    // ---- Noise --------------------------------------------------------------
    noiseSample = noiseRng.bipolar();

    // ---- Oscillators --------------------------------------------------------
    const double stepV = stepVolts (stepIndex);
    const bool pitchToVco1 = params.pitchMod == PitchMod::both;
    const bool pitchToVco2 = params.pitchMod == PitchMod::both || params.pitchMod == PitchMod::vco2;
    const double common = midiPitchV + params.fineCents / 1200.0;

    auto pitchVolts = [&] (int v, bool seqReaches)
    {
        const auto vi = (size_t) v;
        return pitchSmooth[vi].next ((params.vcoFreq[vi] - 0.5) * kVcoOctaves)
             + (seqReaches ? stepV : 0.0)
             + vcoEgSmooth[vi].next (params.vcoEgAmt[vi]) * eVco * kVcoEgOctaves
             + inV (v == 0 ? inVco1Cv : inVco2Cv)
             + common
             + driftValue[vi] * params.drift * (6.0 / 1200.0);
    };

    // Oscillator 1
    const double hz1 = clamp (kC4Hz * std::exp2 (pitchVolts (0, pitchToVco1)), 0.5, std::min (kMaxVcoHz, 0.45 * fs));
    lastVcoHz[0] = hz1;
    const bool wrapped1 = osc[0].advance (hz1 / fs);
    const double t1 = osc[0].getPhase(), dt1 = osc[0].getRate();
    const double vco1 = params.wave[0] == Wave::square ? dsp::pulseWave (t1, dt1) : dsp::triangleWave (t1, dt1);

    // Oscillator 2: linear FM from oscillator 1, then optional hard sync.
    double fmAmt = fmSmooth.next (params.fmAmount);
    if (isPatched (inFmAmt))
        fmAmt += clamp (inV (inFmAmt), 0.0, 8.0) / 8.0;
    fmAmt = clamp (fmAmt, 0.0, 1.0);

    const double hz2Base = clamp (kC4Hz * std::exp2 (pitchVolts (1, pitchToVco2)), 0.5, std::min (kMaxVcoHz, 0.45 * fs));
    lastVcoHz[1] = hz2Base;
    // Linear FM: a fixed frequency deviation, so the timbre changes with the
    // interval between the two oscillators rather than staying constant. Deep
    // settings push the frequency below zero and the phase runs backwards
    // through it, which is where the bell-like and clangorous tones come from.
    const double nyq = std::min (kMaxVcoHz, 0.45 * fs);
    const double hz2 = clamp (hz2Base + fmAmt * fmAmt * kFmHz * vco1, -nyq, nyq);
    osc[1].advance (hz2 / fs);
    if (params.hardSync && wrapped1)
        osc[1].syncTo (osc[0].getWrapFrac());
    const double t2 = osc[1].getPhase(), dt2 = osc[1].getRate();
    const double vco2 = params.wave[1] == Wave::square ? dsp::pulseWave (t2, dt2) : dsp::triangleWave (t2, dt2);

    // ---- Mixer --------------------------------------------------------------
    // An external audio signal takes the noise generator's place in the mixer.
    const double thirdChannel = isPatched (inExtAudio) ? clamp (inV (inExtAudio) / 5.0, -2.0, 2.0) : noiseSample;

    double noiseLvl = (double) params.noiseLevel;
    if (isPatched (inNoiseLevel))
        noiseLvl += clamp (inV (inNoiseLevel), 0.0, 8.0) / 8.0;

    double sum = vcoLevelSmooth[0].next (levelTaper (params.vcoLevel[0])) * vco1
               + vcoLevelSmooth[1].next (levelTaper (params.vcoLevel[1])) * vco2
               + noiseLevelSmooth.next (levelTaper (clamp (noiseLvl, 0.0, 1.0))) * thirdChannel;

    // Pushing the mixer hard clips, the way the hardware's summing stage does.
    constexpr double bias = 0.10;
    const double mix = std::tanh (sum * 0.9 + bias) - std::tanh (bias);

    // ---- Filter -------------------------------------------------------------
    double oct = cutoffSmooth.next (std::log2 (clamp (params.cutoffHz, 20.0f, 20000.0f) / 20.0));
    oct += vcfEgSmooth.next (params.vcfEgAmt) * eVcf * kVcfEgOctaves;

    // A cable in VCF MOD replaces noise as the modulation source, with the
    // same knob setting the depth.
    const double modAmt = clamp ((double) params.noiseVcfMod, 0.0, 1.0);
    if (modAmt > 0.0)
        oct += isPatched (inVcfMod) ? modAmt * clamp (inV (inVcfMod), -5.0, 5.0)
                                    : modAmt * noiseSample * kNoiseModOct;

    const double fc = 20.0 * std::exp2 (clamp (oct, -1.0, 10.5));
    const bool hp = params.filterMode == FilterMode::highPass;
    // Resonance reaches self-oscillation sooner in high-pass mode.
    const double kRes = resSmooth.next (params.resonance) * (hp ? 4.3 : 4.6);
    const double floorNoise = rng.bipolar() * 2.0e-4; // analog noise floor; also seeds self-oscillation
    double y = ladder.process (mix + floorNoise, fc, kRes, hp);

    // ---- VCA ----------------------------------------------------------------
    double gain = eVca;
    if (isPatched (inVcaCv))
        gain += clamp (inV (inVcaCv), 0.0, 8.0) / 8.0;
    gain = clamp (gain, 0.0, 1.0);
    const double vol = volSmooth.next (params.volume * params.volume);
    y *= gain * vol;

    // ---- Patch outputs (volts) ----------------------------------------------
    outV[outVca]      = y * 5.0;
    outV[outVcaEg]    = eVca * 8.0;
    outV[outVcfEg]    = eVcf * 8.0;
    outV[outVcoEg]    = eVco * 8.0;
    outV[outVco1]     = vco1 * 5.0;
    outV[outVco2]     = vco2 * 5.0;
    outV[outVelocity] = clamp ((double) params.velocity[(size_t) stepIndex], 0.0, 1.0) * 5.0;
    outV[outPitch]    = stepV;
    outV[outTrigger]  = triggerOutTicks > 0 ? 5.0 : 0.0;
    if (triggerOutTicks > 0)
        --triggerOutTicks;

    return y;
}
} // namespace stepkin
