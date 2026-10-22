#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/Engine.h"
#include "Params.h"
#include "PresetManager.h"

#include <atomic>
#include <unordered_map>

class StepkinProcessor final : public juce::AudioProcessor,
                               private juce::AudioProcessorValueTreeState::Listener
{
public:
    StepkinProcessor();
    ~StepkinProcessor() override;

    // --- AudioProcessor ------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Patchbay (message thread) -------------------------------------------
    int  sourceFor (int input) const noexcept { return patchSrc[(size_t) input].load(); }
    void connect (int output, int input);
    void disconnect (int input);
    void clearPatch();
    stepkin::Patch currentPatch() const noexcept;
    void setPatch (const stepkin::Patch&);
    std::atomic<int> patchVersion { 0 };

    // --- Panel actions (message thread) --------------------------------------
    void uiTogglePlay()    { playToggles.fetch_add (1); }
    void uiAdvance()       { advancePresses.fetch_add (1); }
    void uiTrigger()       { triggerPresses.fetch_add (1); }

    // --- State (presets & host) ----------------------------------------------
    juce::ValueTree captureState();
    // Restores a state. When `asPreset` is true the per-instance settings
    // (host sync, MIDI channel and mode, fine tune) are left as they are.
    void restoreState (const juce::ValueTree& state, bool asPreset);
    void applyFactoryPreset (const stepkin::Preset&);

    // Randomises the sixteen step knobs, leaving everything else alone.
    void randomiseSteps (bool pitch, bool velocity);

    juce::AudioProcessorValueTreeState apvts;
    stepkin::PresetManager presets { *this };

    // --- Readouts for the panel ----------------------------------------------
    std::atomic<int>  uiStep { 0 };
    std::atomic<bool> uiPlayOn { true }, uiRunning { false }, uiHostPlaying { false };
    std::atomic<uint32_t> uiStepCount { 0 }, uiMidi { 0 };
    std::atomic<float> uiVcoEnv { 0.0f }, uiVcfEnv { 0.0f }, uiVcaEnv { 0.0f };
    std::atomic<float> uiVcoHz[2] { { 261.6f }, { 261.6f } };
    std::atomic<double> uiHostBpm { 120.0 };

    float get (const juce::String& id) const;

private:
    stepkin::EngineParams readParams() const;
    void handleMidi (const juce::MidiMessage&);
    void parameterChanged (const juce::String& id, float) override;

    stepkin::Engine engine;
    std::array<std::atomic<int>, stepkin::kNumInputs> patchSrc;
    std::unordered_map<juce::String, std::atomic<float>*> params;

    std::atomic<int>  playToggles { 0 }, advancePresses { 0 }, triggerPresses { 0 };
    int seenPlayToggles = 0, seenAdvancePresses = 0, seenTriggerPresses = 0;
    bool playOnRestore = true;
    std::atomic<bool> pendingPlayState { false };
    std::atomic<bool> pendingPlayValue { true };

    // The factory program the host last saw. Held while a user preset is
    // loaded, since user presets are not exposed as host programs.
    int lastProgram = 0;

    juce::Random random;
    std::vector<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepkinProcessor)
};
