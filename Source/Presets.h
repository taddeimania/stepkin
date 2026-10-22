// Presets.h — factory patches. Values are written as knob positions (0..1, or
// -1..+1 for bipolar knobs) and converted to parameter units here, so the
// tables read like a patch sheet.
#pragma once

#include "Params.h"
#include <vector>

namespace stepkin
{
struct Preset
{
    juce::String name;
    juce::String notes;
    std::vector<std::pair<juce::String, float>> values;   // plain parameter values
    std::vector<std::pair<int, int>> cables;              // { output, input }
};

namespace presetdetail
{
    inline float cutoffFromKnob (float n) { return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, n)); }
    inline float decayFromKnob (float n)  { return (float) decaySecondsFromNorm (n); }
    inline float rateFromKnob (float n)   { return 0.5f * std::pow (1600.0f, juce::jlimit (0.0f, 1.0f, n)); }

    class Builder
    {
    public:
        Builder (juce::String n, juce::String nt) { p.name = std::move (n); p.notes = std::move (nt); }

        Builder& steps (std::array<float, kNumSteps> pitch, std::array<float, kNumSteps> velocity)
        {
            for (int i = 0; i < kNumSteps; ++i)
            {
                set (ids::stepPitch (i), pitch[(size_t) i]);
                set (ids::stepVelocity (i), velocity[(size_t) i]);
            }
            return *this;
        }
        Builder& pitchMod (PitchMod m) { set (ids::pitchMod, (float) (int) m); return *this; }

        Builder& osc (int v, float freqKnob, Wave w, float egAmt, float level)
        {
            set (ids::vcoFreq (v), freqKnob);
            set (ids::vcoWave (v), (float) (int) w);
            set (ids::vcoEgAmt (v), egAmt);
            set (ids::vcoLevel (v), level);
            return *this;
        }
        Builder& osc2Mod (bool sync, float fm) { set (ids::hardSync, sync ? 1.0f : 0.0f); set (ids::fmAmount, fm); return *this; }
        Builder& noise (float level) { set (ids::noiseLevel, level); return *this; }

        Builder& filter (FilterMode mode, float cutoffKnob, float res, float egAmt, float mod)
        {
            set (ids::filterMode, (float) (int) mode);
            set (ids::cutoff, cutoffFromKnob (cutoffKnob));
            set (ids::resonance, res);
            set (ids::vcfEgAmt, egAmt);
            set (ids::noiseVcfMod, mod);
            return *this;
        }
        Builder& decays (float vcoKnob, float vcfKnob, float vcaKnob, bool slowAttack = false)
        {
            set (ids::vcoDecay, decayFromKnob (vcoKnob));
            set (ids::vcfDecay, decayFromKnob (vcfKnob));
            set (ids::vcaDecay, decayFromKnob (vcaKnob));
            set (ids::vcaAttack, slowAttack ? 1.0f : 0.0f);
            return *this;
        }
        Builder& volume (float v) { set (ids::volume, v); return *this; }
        Builder& freeRate (float knob)
        {
            set (ids::sync, 0.0f);
            set (ids::rate, rateFromKnob (knob));
            return *this;
        }
        Builder& stepLength (int index) { set (ids::syncRate, (float) index); return *this; }
        Builder& cable (int out, int in) { p.cables.emplace_back (out, in); return *this; }

        Preset build() { return p; }

    private:
        void set (const juce::String& id, float v) { p.values.emplace_back (id, v); }
        Preset p;
    };
} // namespace presetdetail

inline const std::vector<Preset>& factoryPresets()
{
    using namespace presetdetail;
    using A = std::array<float, kNumSteps>;

    static const std::vector<Preset> presets = []
    {
        std::vector<Preset> v;

        v.push_back (Builder ("Init", "The default panel: one triangle oscillator, filter open, steps all even.")
            .steps (A { 0, 0, 0, 0, 0, 0, 0, 0 }, A { 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f })
            .pitchMod (PitchMod::off)
            .osc (0, 0.5f, Wave::triangle, 0.0f, 0.8f)
            .osc (1, 0.5f, Wave::triangle, 0.0f, 0.0f)
            .osc2Mod (false, 0.0f)
            .noise (0.0f)
            .filter (FilterMode::lowPass, 0.63f, 0.15f, 0.0f, 0.0f)
            .decays (0.33f, 0.4f, 0.52f)
            .volume (0.75f)
            .build());

        v.push_back (Builder ("Deep Kick", "Low triangle, a steep pitch drop and a tight amplitude decay. "
                                           "Pitch Decay sets how much beater you hear.")
            .steps (A { 0, 0, 0, 0, 0, 0, 0, 0 }, A { 1.0f, 0.0f, 0.45f, 0.0f, 0.9f, 0.0f, 0.35f, 0.0f })
            .pitchMod (PitchMod::off)
            .osc (0, 0.33f, Wave::triangle, 0.55f, 0.9f)
            .osc (1, 0.5f, Wave::triangle, 0.0f, 0.0f)
            .osc2Mod (false, 0.0f)
            .noise (0.08f)
            .filter (FilterMode::lowPass, 0.615f, 0.15f, 0.2f, 0.0f)
            .decays (0.2f, 0.35f, 0.49f)
            .volume (0.8f)
            .build());

        v.push_back (Builder ("Quick Hats", "Noise through the high pass. The step PITCH knobs feed AMP DECAY, "
                                            "so each step gets its own length.")
            .steps (A { -0.5f, -0.3f, 0.4f, -0.4f, -0.5f, -0.2f, 0.5f, -0.35f },
                    A { 1.0f, 0.5f, 0.8f, 0.45f, 1.0f, 0.5f, 0.9f, 0.6f })
            .pitchMod (PitchMod::off)
            .osc (0, 0.5f, Wave::triangle, 0.0f, 0.0f)
            .osc (1, 0.5f, Wave::triangle, 0.0f, 0.0f)
            .osc2Mod (false, 0.0f)
            .noise (1.0f)
            .filter (FilterMode::highPass, 0.825f, 0.25f, 0.2f, 0.0f)
            .decays (0.3f, 0.26f, 0.26f)
            .volume (0.7f)
            .cable (outPitch, inVcaDecay)
            .build());

        v.push_back (Builder ("Tuned Bass", "Both oscillators follow the step pitches, with the slow amp attack "
                                            "standing in for glide. Tune the two oscillators together.")
            .steps (A { 0.0f, 0.0f, 0.117f, 0.0f, 0.25f, 0.117f, -0.083f, 0.0f },
                    A { 1.0f, 0.4f, 0.85f, 0.4f, 1.0f, 0.6f, 0.8f, 0.45f })
            .pitchMod (PitchMod::both)
            .osc (0, 0.38f, Wave::square, 0.08f, 0.6f)
            .osc (1, 0.38f, Wave::triangle, 0.0f, 0.5f)
            .osc2Mod (false, 0.0f)
            .noise (0.0f)
            .filter (FilterMode::lowPass, 0.515f, 0.35f, 0.45f, 0.0f)
            .decays (0.2f, 0.41f, 0.5f, true)
            .volume (0.75f)
            .build());

        v.push_back (Builder ("Bell Toys", "Hard sync and deep FM. Oscillator 2's FREQUENCY knob is the main "
                                           "voice here: move it and the whole timbre moves with it.")
            .steps (A { 0, 0, 0.1f, 0, 0, -0.1f, 0.2f, 0 },
                    A { 1.0f, 0.3f, 0.6f, 0.3f, 0.9f, 0.35f, 0.7f, 0.5f })
            .pitchMod (PitchMod::vco2)
            .osc (0, 0.46f, Wave::square, 0.0f, 0.15f)
            .osc (1, 0.72f, Wave::square, 0.3f, 0.85f)
            .osc2Mod (true, 0.55f)
            .noise (0.0f)
            .filter (FilterMode::highPass, 0.695f, 0.3f, 0.4f, 0.35f)
            .decays (0.26f, 0.37f, 0.53f)
            .volume (0.7f)
            .build());

        v.push_back (Builder ("Kick And Snare", "A body tuned low with noise on top. The step VELOCITY knobs "
                                                "decide which hits read as kick and which as snare.")
            .steps (A { -0.25f, 0, 0.3f, 0, -0.25f, 0, 0.3f, 0.15f },
                    A { 1.0f, 0.25f, 0.8f, 0.3f, 1.0f, 0.25f, 0.85f, 0.6f })
            .pitchMod (PitchMod::both)
            .osc (0, 0.4f, Wave::triangle, 0.35f, 0.6f)
            .osc (1, 0.56f, Wave::square, 0.2f, 0.25f)
            .osc2Mod (false, 0.12f)
            .noise (0.6f)
            .filter (FilterMode::lowPass, 0.6f, 0.4f, 0.5f, 0.1f)
            .decays (0.21f, 0.33f, 0.44f)
            .volume (0.75f)
            .build());

        v.push_back (Builder ("Filter Voice", "No oscillators: the filter is turned up until it sings, and the "
                                              "step PITCH knobs play it through the FILTER MOD jack.")
            .steps (A { 0.0f, 0.2f, 0.35f, 0.2f, 0.5f, 0.35f, 0.15f, 0.0f },
                    A { 1.0f, 0.5f, 0.9f, 0.5f, 1.0f, 0.6f, 0.85f, 0.5f })
            .pitchMod (PitchMod::off)
            .osc (0, 0.5f, Wave::triangle, 0.0f, 0.0f)
            .osc (1, 0.5f, Wave::triangle, 0.0f, 0.12f)
            .osc2Mod (false, 0.0f)
            .noise (0.1f)
            .filter (FilterMode::lowPass, 0.42f, 1.0f, 0.25f, 1.0f)
            .decays (0.3f, 0.45f, 0.55f)
            .volume (0.6f)
            .cable (outPitch, inVcfMod)
            .build());

        v.push_back (Builder ("Big Toms", "A long pitch fall on a triangle. Raise Pitch Decay for the rubbery, "
                                          "exaggerated version.")
            .steps (A { -0.2f, 0, 0.1f, -0.1f, 0.25f, 0, -0.15f, 0.05f },
                    A { 1.0f, 0.0f, 0.7f, 0.5f, 0.95f, 0.0f, 0.8f, 0.4f })
            .pitchMod (PitchMod::both)
            .osc (0, 0.42f, Wave::triangle, 0.4f, 0.85f)
            .osc (1, 0.47f, Wave::triangle, 0.4f, 0.3f)
            .osc2Mod (false, 0.0f)
            .noise (0.15f)
            .filter (FilterMode::lowPass, 0.62f, 0.3f, 0.3f, 0.05f)
            .decays (0.36f, 0.45f, 0.58f)
            .volume (0.75f)
            .build());

        v.push_back (Builder ("Swung Steps", "Free-running, with the step PITCH knobs patched to RATE so each "
                                             "step sets its own length. Low settings make very long steps.")
            .steps (A { 0.25f, -0.2f, 0.25f, -0.2f, 0.25f, -0.2f, 0.3f, -0.1f },
                    A { 1.0f, 0.5f, 0.9f, 0.45f, 1.0f, 0.5f, 0.85f, 0.6f })
            .pitchMod (PitchMod::off)
            .osc (0, 0.45f, Wave::square, 0.3f, 0.5f)
            .osc (1, 0.5f, Wave::triangle, 0.0f, 0.0f)
            .osc2Mod (false, 0.0f)
            .noise (0.35f)
            .filter (FilterMode::lowPass, 0.66f, 0.3f, 0.4f, 0.1f)
            .decays (0.24f, 0.32f, 0.4f)
            .volume (0.7f)
            .freeRate (0.33f)
            .cable (outPitch, inTempo)
            .build());

        v.push_back (Builder ("Steps As A Waveform", "The clock is run up into the audio range, so the eight "
                                                     "steps become one cycle. RATE is now the pitch.")
            .steps (A { 0.3f, -0.15f, 0.45f, -0.3f, 0.2f, -0.45f, 0.35f, -0.2f },
                    A { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f })
            .pitchMod (PitchMod::both)
            .osc (0, 0.5f, Wave::triangle, 0.0f, 0.5f)
            .osc (1, 0.56f, Wave::triangle, 0.0f, 0.3f)
            .osc2Mod (false, 0.0f)
            .noise (0.0f)
            .filter (FilterMode::lowPass, 0.7f, 0.35f, 0.2f, 0.0f)
            .decays (0.1f, 0.2f, 0.3f)
            .volume (0.6f)
            .freeRate (0.72f)
            .build());

        v.push_back (Builder ("Vowel Drum", "Oscillator 2 shakes the filter at audio rate. The NOISE / FILTER MOD "
                                            "knob decides how much the drum talks.")
            .steps (A { 0.0f, 0.1f, -0.1f, 0.2f, 0.0f, 0.15f, -0.05f, 0.1f },
                    A { 1.0f, 0.4f, 0.8f, 0.45f, 1.0f, 0.5f, 0.75f, 0.55f })
            .pitchMod (PitchMod::vco2)
            .osc (0, 0.4f, Wave::triangle, 0.3f, 0.8f)
            .osc (1, 0.6f, Wave::triangle, 0.0f, 0.0f)
            .osc2Mod (false, 0.0f)
            .noise (0.0f)
            .filter (FilterMode::lowPass, 0.57f, 0.2f, 0.35f, 0.5f)
            .decays (0.23f, 0.4f, 0.5f)
            .volume (0.75f)
            .cable (outVco2, inVcfMod)
            .build());

        return v;
    }();

    return presets;
}
} // namespace stepkin
