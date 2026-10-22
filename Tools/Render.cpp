// Render.cpp — a dev tool that renders a few panel settings straight from the
// engine to a WAV file, with no JUCE and no audio device. Useful for checking
// how a patch sounds, and for A/B-ing engine changes.
//
//   cmake -B build -DSTEPKIN_BUILD_TOOLS=ON
//   cmake --build build --target StepkinRender
//   ./build/StepkinRender out.wav

#include "Engine/Engine.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace stepkin;

namespace
{
constexpr double kSr = 48000.0;

void writeWav (const std::string& path, const std::vector<float>& samples, double sampleRate)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (f == nullptr)
    {
        std::printf ("could not open %s\n", path.c_str());
        return;
    }

    const uint32_t dataBytes = (uint32_t) (samples.size() * 2);
    const uint32_t rate = (uint32_t) sampleRate;
    auto u32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };

    std::fwrite ("RIFF", 1, 4, f);  u32 (36 + dataBytes);  std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f);  u32 (16);  u16 (1);  u16 (1);
    u32 (rate);  u32 (rate * 2);  u16 (2);  u16 (16);
    std::fwrite ("data", 1, 4, f);  u32 (dataBytes);

    for (float s : samples)
    {
        const double clamped = s < -1.0 ? -1.0 : (s > 1.0 ? 1.0 : s);
        u16 ((uint16_t) (int16_t) (clamped * 32767.0));
    }
    std::fclose (f);
    std::printf ("wrote %s  (%.2f s)\n", path.c_str(), (double) samples.size() / sampleRate);
}

EngineParams init()
{
    EngineParams p;
    p.sync = false;
    p.rateHz = 8.0f;
    p.pitch.fill (0.0f);
    p.velocity.fill (1.0f);
    p.pitchMod = PitchMod::off;
    p.vcoFreq = { 0.5f, 0.5f };
    p.wave = { Wave::triangle, Wave::triangle };
    p.vcoEgAmt = { 0.0f, 0.0f };
    p.vcoLevel = { 0.8f, 0.0f };
    p.noiseLevel = 0.0f;
    p.cutoffHz = 2000.0f;
    p.resonance = 0.0f;
    p.vcfEgAmt = 0.0f;
    p.noiseVcfMod = 0.0f;
    p.vcoDecay = 0.03f;
    p.vcfDecay = 0.1f;
    p.vcaDecay = 0.25f;
    p.volume = 0.8f;
    p.drift = 0.15f;
    p.midiMode = MidiMode::off;
    return p;
}

struct Patch
{
    const char* name;
    EngineParams params;
    stepkin::Patch cables = emptyPatch();
};

std::vector<Patch> patches()
{
    std::vector<Patch> out;

    {
        // Kick: low triangle with a steep pitch drop and a tight amplitude decay.
        auto p = init();
        p.vcoFreq[0] = 0.33f;
        p.vcoEgAmt[0] = 0.55f;
        p.vcoDecay = 0.02f;
        p.vcaDecay = 0.18f;
        p.cutoffHz = 1400.0f;
        p.resonance = 0.15f;
        p.velocity = { 1.0f, 0.0f, 0.45f, 0.0f, 0.9f, 0.0f, 0.35f, 0.0f };
        out.push_back ({ "kick", p });
    }
    {
        // Snare: noise plus a tuned body, with the filter snapping open.
        auto p = init();
        p.vcoFreq[0] = 0.52f;
        p.vcoFreq[1] = 0.58f;
        p.wave = { Wave::square, Wave::triangle };
        p.vcoLevel = { 0.35f, 0.3f };
        p.noiseLevel = 0.8f;
        p.vcoEgAmt[0] = 0.25f;
        p.vcoDecay = 0.015f;
        p.cutoffHz = 900.0f;
        p.resonance = 0.4f;
        p.vcfEgAmt = 0.5f;
        p.vcfDecay = 0.05f;
        p.vcaDecay = 0.12f;
        p.velocity = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.5f };
        out.push_back ({ "snare", p });
    }
    {
        // Hats: noise through the high pass, length set per step by the PITCH
        // knobs feeding VCA DECAY.
        auto p = init();
        p.vcoLevel = { 0.0f, 0.0f };
        p.noiseLevel = 1.0f;
        p.filterMode = FilterMode::highPass;
        p.cutoffHz = 6000.0f;
        p.resonance = 0.25f;
        p.vcfEgAmt = 0.2f;
        p.vcfDecay = 0.03f;
        p.vcaDecay = 0.03f;
        p.pitch = { -0.5f, -0.3f, 0.4f, -0.4f, -0.5f, -0.2f, 0.5f, -0.35f };
        p.velocity = { 1.0f, 0.5f, 0.8f, 0.45f, 1.0f, 0.5f, 0.9f, 0.6f };
        auto c = emptyPatch();
        c[inVcaDecay] = outPitch;
        out.push_back ({ "hats", p, c });
    }
    {
        // Tuned sequence: both oscillators follow the step pitches.
        auto p = init();
        p.pitchMod = PitchMod::both;
        p.vcoFreq = { 0.38f, 0.38f };
        p.wave = { Wave::square, Wave::triangle };
        p.vcoLevel = { 0.6f, 0.5f };
        p.vcoEgAmt[0] = 0.08f;
        p.vcoDecay = 0.02f;
        p.cutoffHz = 700.0f;
        p.resonance = 0.35f;
        p.vcfEgAmt = 0.45f;
        p.vcfDecay = 0.12f;
        p.vcaDecay = 0.22f;
        p.vcaSlowAttack = true;
        p.pitch = { 0.0f, 0.0f, 0.117f, 0.0f, 0.25f, 0.117f, -0.083f, 0.0f };
        p.velocity = { 1.0f, 0.4f, 0.85f, 0.4f, 1.0f, 0.6f, 0.8f, 0.45f };
        out.push_back ({ "sequence", p });
    }
    {
        // Metallic: hard sync and deep FM, filter shaken by noise.
        auto p = init();
        p.vcoFreq = { 0.46f, 0.72f };
        p.wave = { Wave::square, Wave::square };
        p.vcoLevel = { 0.15f, 0.8f };
        p.hardSync = true;
        p.fmAmount = 0.55f;
        p.vcoEgAmt[1] = 0.3f;
        p.vcoDecay = 0.04f;
        p.filterMode = FilterMode::highPass;
        p.cutoffHz = 2500.0f;
        p.resonance = 0.3f;
        p.noiseVcfMod = 0.35f;
        p.vcfEgAmt = 0.4f;
        p.vcfDecay = 0.08f;
        p.vcaDecay = 0.3f;
        p.velocity = { 1.0f, 0.3f, 0.6f, 0.3f, 0.9f, 0.35f, 0.7f, 0.5f };
        out.push_back ({ "metallic", p });
    }

    return out;
}
} // namespace

int main (int argc, char** argv)
{
    const std::string path = argc > 1 ? argv[1] : "stepkin-demo.wav";
    const double stepsPerSecond = 8.0;          // sixteenths at 120 BPM
    const int barSamples = (int) (kSr * 16.0 / stepsPerSecond); // two bars of eight steps

    std::vector<float> all;
    for (auto& patch : patches())
    {
        Engine e;
        e.setParams (patch.params);
        e.prepare (kSr);
        e.setPatch (patch.cables);
        e.setPlayOn (true);

        std::vector<float> block ((size_t) barSamples, 0.0f);
        HostInfo h;
        e.beginBlock (h, barSamples);
        e.render (block.data(), barSamples);

        double p = 0.0;
        for (float s : block) p = std::max (p, (double) std::abs (s));
        std::printf ("%-10s peak %.3f\n", patch.name, p);

        all.insert (all.end(), block.begin(), block.end());
        all.insert (all.end(), (size_t) (kSr * 0.4), 0.0f);   // a breath between patches
    }

    writeWav (path, all, kSr);
    return 0;
}
