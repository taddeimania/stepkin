// Dsp.h — building blocks for the Stepkin percussion voice: band-limited
// triangle and pulse shapes, a hard-syncable oscillator, a 4-pole ladder with
// selectable low-pass / high-pass response, decay envelopes with a velocity
// input, halfband decimation and a few utilities. No JUCE dependency.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace stepkin::dsp
{
constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline double clamp (double v, double lo, double hi) { return std::min (hi, std::max (lo, v)); }

// ---------------------------------------------------------------------------
// Band-limiting residuals
// ---------------------------------------------------------------------------

// Step discontinuity at phase 0 (phase t in [0,1), increment dt).
inline double polyBlep (double t, double dt) noexcept
{
    if (dt <= 0.0)
        return 0.0;
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

// Slope discontinuity at phase 0 — the integral of polyBlep, used for
// triangles. Returned in units of "phase", so scale by the slope change
// times dt when applying it.
inline double polyBlamp (double t, double dt) noexcept
{
    if (dt <= 0.0)
        return 0.0;
    if (t < dt)
    {
        t = t / dt - 1.0;
        return -(1.0 / 3.0) * t * t * t;
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt + 1.0;
        return (1.0 / 3.0) * t * t * t;
    }
    return 0.0;
}

// Pulse with duty cycle `duty` (fraction of the cycle spent high), in -1..+1.
inline double pulseWave (double t, double dt, double duty = 0.5) noexcept
{
    duty = clamp (duty, 0.01, 0.99);
    double v = t < duty ? 1.0 : -1.0;
    v += polyBlep (t, dt);
    double t2 = t - duty;
    if (t2 < 0.0)
        t2 += 1.0;
    v -= polyBlep (t2, dt);
    return v;
}

// Triangle in -1..+1: +1 at phase 0, -1 at phase 0.5. Slope changes by -8 at
// phase 0 and by +8 at phase 0.5, each corrected with a polyBLAMP residual.
inline double triangleWave (double t, double dt) noexcept
{
    double v = 4.0 * std::abs (t - 0.5) - 1.0;
    v -= 8.0 * dt * polyBlamp (t, dt);
    double t2 = t - 0.5;
    if (t2 < 0.0)
        t2 += 1.0;
    v += 8.0 * dt * polyBlamp (t2, dt);
    return v;
}

// ---------------------------------------------------------------------------
// Oscillator with optional hard sync.
//
// The increment may be negative: deep linear FM drives the frequency below
// zero, and running the phase backwards through that region (through-zero FM)
// keeps the tone going instead of stalling the core at a fixed phase.
//
// advance() returns true on the sample where the phase wrapped forwards, which
// the master oscillator uses to reset the slave. syncTo() restarts the phase at
// the same sub-sample position the master wrapped at, so a synced pair stays
// phase-locked rather than drifting by up to one sample per cycle.
// ---------------------------------------------------------------------------
class Oscillator
{
public:
    void reset() noexcept { phase = 0.0; inc = 0.0; wrapFrac = 0.0; }

    // Returns true if the phase wrapped forwards during this step.
    bool advance (double increment) noexcept
    {
        inc = increment;
        phase += inc;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            // How far into this sample the wrap happened, in samples.
            wrapFrac = inc > 0.0 ? phase / inc : 0.0;
            return true;
        }
        if (phase < 0.0)
        {
            phase -= std::floor (phase);
            return false;
        }
        return false;
    }

    // Restart at the master's wrap point.
    void syncTo (double masterWrapFrac) noexcept
    {
        phase = clamp (masterWrapFrac, 0.0, 1.0) * std::abs (inc);
    }

    double getPhase() const noexcept { return phase; }
    double getInc() const noexcept { return inc; }
    // Band-limiting residuals only care about how fast the phase is moving.
    double getRate() const noexcept { return std::abs (inc); }
    double getWrapFrac() const noexcept { return wrapFrac; }

private:
    double phase = 0.0, inc = 0.0, wrapFrac = 0.0;
};

// ---------------------------------------------------------------------------
// 4-pole ladder with selectable low-pass / high-pass response.
//
// Four cascaded one-pole TPT sections with global feedback, solved implicitly
// for the linear part and saturated at the input. The five taps (the saturated
// input plus each stage output) are mixed to give either a 24 dB/oct low-pass
// (0,0,0,0,1) or a 24 dB/oct high-pass (1,-4,6,-4,1). Self-oscillates as k
// approaches 4.
// ---------------------------------------------------------------------------
class Ladder
{
public:
    void prepare (double sampleRate) noexcept { fs = sampleRate; reset(); }
    void reset() noexcept { s.fill (0.0); }

    double process (double x, double cutoffHz, double k, bool highPass) noexcept
    {
        cutoffHz = clamp (cutoffHz, 5.0, 0.45 * fs);
        const double g  = std::tan (kPi * cutoffHz / fs);
        const double G  = g / (1.0 + g);
        const double G2 = G * G;
        const double G3 = G2 * G;
        const double G4 = G2 * G2;

        const double S = (G3 * s[0] + G2 * s[1] + G * s[2] + s[3]) / (1.0 + g);

        // Partial passband-loss compensation keeps high resonance usable.
        const double in = x * (1.0 + 0.45 * k);
        double u = (in - k * S) / (1.0 + k * G4);
        u = std::tanh (u);

        std::array<double, 5> tap {};
        tap[0] = u;
        double y = u;
        for (int i = 0; i < 4; ++i)
        {
            const double v = (y - s[(size_t) i]) * G;
            y = v + s[(size_t) i];
            s[(size_t) i] = y + v;
            tap[(size_t) (i + 1)] = y;
        }

        if (! highPass)
            return tap[4];
        return tap[0] - 4.0 * tap[1] + 6.0 * tap[2] - 4.0 * tap[3] + tap[4];
    }

private:
    double fs = 48000.0;
    std::array<double, 4> s {};
};

// ---------------------------------------------------------------------------
// Decay envelope with a velocity input.
//
// A trigger latches the velocity, runs a short attack to that level, then
// decays exponentially. Velocity sets the peak amplitude and also stretches
// the decay a little, the way a hardware envelope loaded by a velocity CV
// does. Retriggering during the attack is ignored; during the decay it
// restarts the attack from the current level.
// ---------------------------------------------------------------------------
class DecayEnvelope
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        lastA = lastD = -1.0;
        setTimes (0.001, 0.2);
        reset();
    }

    void setTimes (double attackSec, double decaySec) noexcept
    {
        attackSec = std::max (0.0002, attackSec);
        decaySec  = std::max (0.002, decaySec);
        if (std::abs (attackSec - lastA) > 1.0e-9)
        {
            // RC charge toward an overshoot target, reaching the peak after attackSec.
            constexpr double target = 1.2;
            const double tau = attackSec / std::log (target / (target - 1.0));
            attackCoef = std::exp (-1.0 / (tau * fs));
            lastA = attackSec;
        }
        if (std::abs (decaySec - lastD) > 1.0e-9)
        {
            // Exponential fall to about -40 dB over decaySec.
            const double tau = decaySec / std::log (100.0);
            decayCoef = std::exp (-1.0 / (tau * fs));
            lastD = decaySec;
        }
    }

    // velocity in 0..1. Returns true if the envelope actually (re)started.
    bool trigger (double velocity) noexcept
    {
        velocity = clamp (velocity, 0.0, 1.0);
        if (stage == Stage::attack)
            return false;
        peak  = velocity;
        stage = velocity > 0.0 ? Stage::attack : Stage::idle;
        if (velocity <= 0.0)
            value = 0.0;
        return stage == Stage::attack;
    }

    double tick() noexcept
    {
        switch (stage)
        {
            case Stage::attack:
            {
                const double target = peak * 1.2;
                value = target + (value - target) * attackCoef;
                if (value >= peak)
                {
                    value = peak;
                    stage = Stage::decay;
                }
                break;
            }
            case Stage::decay:
                value *= decayCoef;
                if (value < 1.0e-5)
                {
                    value = 0.0;
                    stage = Stage::idle;
                }
                break;
            case Stage::idle:
                break;
        }
        return value;
    }

    double level() const noexcept { return value; }
    bool   isActive() const noexcept { return stage != Stage::idle; }
    void   reset() noexcept { value = 0.0; peak = 0.0; stage = Stage::idle; }

private:
    enum class Stage { idle, attack, decay };

    double fs = 48000.0;
    double value = 0.0, peak = 0.0;
    double attackCoef = 0.0, decayCoef = 0.0;
    double lastA = -1.0, lastD = -1.0;
    Stage  stage = Stage::idle;
};

// ---------------------------------------------------------------------------
// One-pole smoother
// ---------------------------------------------------------------------------
class Smoother
{
public:
    void prepare (double sampleRate, double timeSec) noexcept
    {
        coef = std::exp (-1.0 / (std::max (1.0e-4, timeSec) * sampleRate));
    }
    void   snap (double v) noexcept { y = v; }
    double next (double target) noexcept { y = target + (y - target) * coef; return y; }
    double value() const noexcept { return y; }

private:
    double coef = 0.0, y = 0.0;
};

// ---------------------------------------------------------------------------
// 2:1 halfband FIR decimator (windowed sinc, Blackman). Two of these in series
// take the engine's 4x internal rate down to the host rate.
// ---------------------------------------------------------------------------
class Decimator2
{
public:
    static constexpr int kTaps = 63;

    Decimator2()
    {
        const int mid = kTaps / 2;
        double sum = 0.0;
        for (int i = 0; i < kTaps; ++i)
        {
            const int    n = i - mid;
            const double x = n == 0 ? 0.5 : std::sin (kPi * 0.5 * n) / (kPi * n);
            const double w = 0.42 - 0.5 * std::cos (kTwoPi * i / (kTaps - 1)) + 0.08 * std::cos (2.0 * kTwoPi * i / (kTaps - 1));
            h[(size_t) i] = x * w;
            sum += h[(size_t) i];
        }
        for (auto& c : h)
            c /= sum;
        reset();
    }

    void reset() noexcept { buf.fill (0.0); pos = 0; }

    // Push two oversampled inputs, get one output.
    double process (double a, double b) noexcept
    {
        push (a);
        push (b);
        double acc = 0.0;
        for (int i = 0; i < kTaps; ++i)
            acc += h[(size_t) i] * buf[(size_t) (pos + i)];
        return acc;
    }

private:
    void push (double v) noexcept
    {
        pos = (pos == 0 ? kTaps : pos) - 1;
        buf[(size_t) pos] = v;
        buf[(size_t) (pos + kTaps)] = v;
    }

    std::array<double, kTaps> h {};
    std::array<double, kTaps * 2> buf {};
    int pos = 0;
};

// 4:1 decimator built from two halfband stages.
class Decimator4
{
public:
    void reset() noexcept { a.reset(); b.reset(); }

    // Push four oversampled inputs, get one output.
    double process (double x0, double x1, double x2, double x3) noexcept
    {
        return b.process (a.process (x0, x1), a.process (x2, x3));
    }

private:
    Decimator2 a, b;
};

// ---------------------------------------------------------------------------
// DC blocker
// ---------------------------------------------------------------------------
class DcBlocker
{
public:
    void   prepare (double sampleRate) noexcept { r = std::exp (-kTwoPi * 8.0 / sampleRate); x1 = y1 = 0.0; }
    double process (double x) noexcept { const double y = x - x1 + r * y1; x1 = x; y1 = y; return y; }

private:
    double r = 0.999, x1 = 0.0, y1 = 0.0;
};

// ---------------------------------------------------------------------------
// Tiny RNG (xorshift)
// ---------------------------------------------------------------------------
class Rng
{
public:
    explicit Rng (uint64_t seed = 0x9E3779B97F4A7C15ull) : s (seed ? seed : 1) {}
    uint64_t next() noexcept { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
    double   bipolar() noexcept { return (double) (next() >> 11) * (2.0 / 9007199254740992.0) - 1.0; }

private:
    uint64_t s;
};

// Schmitt-trigger edge detector for gate / trigger / clock inputs (volts).
class EdgeDetector
{
public:
    // Returns +1 on a rising edge, -1 on a falling edge, 0 otherwise.
    int process (double v) noexcept
    {
        if (! high && v > 1.0)  { high = true;  return 1; }
        if (high && v < 0.5)    { high = false; return -1; }
        return 0;
    }
    bool isHigh() const noexcept { return high; }
    void reset() noexcept { high = false; }

private:
    bool high = false;
};
} // namespace stepkin::dsp
