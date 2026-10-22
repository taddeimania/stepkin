// Params.h — parameter IDs and layout for Stepkin.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/Engine.h"

namespace stepkin::ids
{
inline juce::String stepPitch (int i)    { return "step" + juce::String (i + 1) + "_pitch"; }
inline juce::String stepVelocity (int i) { return "step" + juce::String (i + 1) + "_velocity"; }
inline juce::String vcoFreq (int v)      { return "vco" + juce::String (v + 1) + "_freq"; }
inline juce::String vcoWave (int v)      { return "vco" + juce::String (v + 1) + "_wave"; }
inline juce::String vcoLevel (int v)     { return "vco" + juce::String (v + 1) + "_level"; }
inline juce::String vcoEgAmt (int v)     { return "vco" + juce::String (v + 1) + "_eg_amt"; }

inline constexpr const char* pitchMod     = "seq_pitch_mod";
inline constexpr const char* hardSync     = "hard_sync";
inline constexpr const char* fmAmount     = "fm_amount";
inline constexpr const char* noiseLevel   = "noise_level";
inline constexpr const char* filterMode   = "filter_mode";
inline constexpr const char* cutoff       = "cutoff";
inline constexpr const char* resonance    = "resonance";
inline constexpr const char* vcfEgAmt     = "vcf_eg_amt";
inline constexpr const char* noiseVcfMod  = "noise_vcf_mod";
inline constexpr const char* vcoDecay     = "vco_decay";
inline constexpr const char* vcfDecay     = "vcf_decay";
inline constexpr const char* vcaDecay     = "vca_decay";
inline constexpr const char* vcaAttack    = "vca_attack_slow";
inline constexpr const char* volume       = "volume";
inline constexpr const char* rate         = "rate";
inline constexpr const char* sync         = "sync";
inline constexpr const char* syncRate     = "sync_rate";
inline constexpr const char* midiMode     = "midi_mode";
inline constexpr const char* midiChannel  = "midi_channel";
inline constexpr const char* fineTune     = "fine_tune";
inline constexpr const char* drift        = "drift";

inline const juce::StringArray& waveNames()      { static const juce::StringArray n { "Pulse", "Triangle" }; return n; }
inline const juce::StringArray& pitchModNames()  { static const juce::StringArray n { "Osc 1 & 2", "Off", "Osc 2" }; return n; }
inline const juce::StringArray& filterNames()    { static const juce::StringArray n { "Low Pass", "High Pass" }; return n; }
inline const juce::StringArray& vcaAttackNames() { static const juce::StringArray n { "Fast", "Slow" }; return n; }
inline const juce::StringArray& midiModeNames()  { static const juce::StringArray n { "Trigger + Pitch", "Pitch Only", "Off" }; return n; }
inline const juce::StringArray& syncRateNames()  { static const juce::StringArray n { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" }; return n; }

inline double syncRateBeats (int index)
{
    static constexpr double beats[] { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
    return beats[juce::jlimit (0, 7, index)];
}

inline juce::String noteNameForHz (double hz)
{
    static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (hz <= 0.0 || ! std::isfinite (hz))
        return "-";
    const double midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
    const int n = juce::roundToInt (midi);
    const int cents = juce::roundToInt ((midi - n) * 100.0);
    juce::String s = juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
    if (cents != 0)
        s << (cents > 0 ? " +" : " ") << cents << "c";
    return s;
}
} // namespace stepkin::ids

namespace stepkin
{
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; });

    auto bipolarPct = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return (v >= 0 ? "+" : "") + String (roundToInt (v * 100.0f)) + "%"; });

    auto seconds = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
    {
        return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s";
    });

    // Exponential knob: value = lo * (hi/lo)^n
    auto expRange = [] (float lo, float hi)
    {
        return NormalisableRange<float> (lo, hi,
            [] (float start, float end, float n) { return start * std::pow (end / start, n); },
            [] (float start, float end, float v) { return std::log (v / start) / std::log (end / start); });
    };

    // --- Sequencer -----------------------------------------------------------
    for (int i = 0; i < kNumSteps; ++i)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::stepPitch (i), 1 },
            "Step " + String (i + 1) + " Pitch", NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            {
                const double semis = v * (kPitchOctaves * 0.5) * 12.0;
                return (v >= 0 ? "+" : "") + String (semis, 1) + " st";
            })));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::stepVelocity (i), 1 },
            "Step " + String (i + 1) + " Velocity", NormalisableRange<float> (0.0f, 1.0f), 0.8f, pct));
    }
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::pitchMod, 1 },
        "Seq Pitch Mod", ids::pitchModNames(), 1));

    // --- Oscillators ---------------------------------------------------------
    for (int v = 0; v < 2; ++v)
    {
        const String n = "Osc " + String (v + 1);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::vcoFreq (v), 1 }, n + " Frequency",
            NormalisableRange<float> (0.0f, 1.0f), 0.5f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int)
            {
                return ids::noteNameForHz (kC4Hz * std::pow (2.0, (x - 0.5) * kVcoOctaves));
            })));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::vcoWave (v), 1 }, n + " Wave",
            ids::waveNames(), 1));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::vcoEgAmt (v), 1 }, n + " EG Amount",
            NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolarPct));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::vcoLevel (v), 1 }, n + " Level",
            NormalisableRange<float> (0.0f, 1.0f), v == 0 ? 0.8f : 0.0f, pct));
    }
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::hardSync, 1 }, "Hard Sync", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::fmAmount, 1 }, "1>2 FM Amount",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::noiseLevel, 1 }, "Noise / Ext Level",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));

    // --- Filter --------------------------------------------------------------
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::filterMode, 1 }, "Filter Mode",
        ids::filterNames(), 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::cutoff, 1 }, "Cutoff",
        expRange (20.0f, 20000.0f), 1500.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            return v < 1000.0f ? String (roundToInt (v)) + " Hz" : String (v / 1000.0f, 2) + " kHz";
        })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::resonance, 1 }, "Resonance",
        NormalisableRange<float> (0.0f, 1.0f), 0.15f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::vcfEgAmt, 1 }, "Filter EG Amount",
        NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolarPct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::noiseVcfMod, 1 }, "Noise / Filter Mod",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));

    // --- Envelopes -----------------------------------------------------------
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::vcoDecay, 1 }, "Pitch Decay",
        expRange (0.005f, 10.0f), 0.03f, seconds));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::vcfDecay, 1 }, "Filter Decay",
        expRange (0.005f, 10.0f), 0.10f, seconds));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::vcaDecay, 1 }, "Amp Decay",
        expRange (0.005f, 10.0f), 0.25f, seconds));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::vcaAttack, 1 }, "Amp Attack",
        ids::vcaAttackNames(), 0));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::volume, 1 }, "Volume",
        NormalisableRange<float> (0.0f, 1.0f), 0.75f, pct));

    // --- Clock ---------------------------------------------------------------
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::rate, 1 }, "Rate",
        expRange (0.5f, 800.0f), 8.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            // Steps per second, shown alongside the tempo that implies if a
            // step is a sixteenth note.
            if (v < 100.0f)
                return String (v, 2) + " Hz  (" + String (roundToInt (v * 15.0f)) + " BPM)";
            return String (roundToInt (v)) + " Hz";
        })));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::sync, 1 }, "Sync To Host", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::syncRate, 1 }, "Step Length",
        ids::syncRateNames(), 4));

    // --- Globals -------------------------------------------------------------
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::midiMode, 1 }, "MIDI Mode",
        ids::midiModeNames(), 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::midiChannel, 1 }, "MIDI Channel", 0, 16, 0,
        AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return v == 0 ? String ("All") : String (v); })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::fineTune, 1 }, "Fine Tune",
        NormalisableRange<float> (-50.0f, 50.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            return (v >= 0 ? "+" : "") + String (v, 1) + " cents";
        })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::drift, 1 }, "Analog Drift",
        NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct));

    return layout;
}
} // namespace stepkin
