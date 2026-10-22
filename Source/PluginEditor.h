#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/Widgets.h"
#include "UI/Patchbay.h"

// The panel is drawn in a fixed 1700 x 800 design space and scaled to fit.
class StepkinPanel final : public juce::Component, private juce::Timer
{
public:
    static constexpr int designW = 1700;
    static constexpr int designH = 800;

    explicit StepkinPanel (StepkinProcessor&);
    ~StepkinPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    using Knob = stepkin::ui::Knob;
    using PanelButton = stepkin::ui::PanelButton;
    using Led = stepkin::ui::Led;
    using BatSwitch = stepkin::ui::BatSwitch;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;

    float getParam (const juce::String& id) const;
    void  setParam (const juce::String& id, float plainValue);
    void  toggleParam (const juce::String& id);

    void addKnob (Knob&, const juce::String& id, juce::Point<float> centre, float size, const juce::String& tip);
    void addButton (PanelButton&, juce::Point<float> centre, float w, float h, const juce::String& tip, bool round = false);
    void addToggle (PanelButton&, const juce::String& id, juce::Point<float> centre, float w, float h, const juce::String& tip);
    void addLed (Led&, juce::Point<float> centre, float size = 20.0f);
    void addSwitch (std::unique_ptr<BatSwitch>&, const juce::String& id, int positions,
                    juce::Point<float> centre, const juce::String& tip);

    void buildRail();
    void showPresetMenu();
    void savePresetAs();
    void confirmDelete();

    void paintSections (juce::Graphics&) const;
    void paintWood (juce::Graphics&, juce::Rectangle<float>) const;
    void paintLabels (juce::Graphics&) const;
    void paintRail (juce::Graphics&) const;

    StepkinProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    stepkin::ui::PanelLookAndFeel lnf;

    // Oscillators and modulation
    Knob vcoDecay { Knob::Kind::large }, fmAmount { Knob::Kind::large };
    Knob vcoEgAmt[2] { Knob { Knob::Kind::large, true }, Knob { Knob::Kind::large, true } };
    Knob vcoFreq[2] { Knob { Knob::Kind::large }, Knob { Knob::Kind::large } };
    std::unique_ptr<BatSwitch> pitchModSwitch, hardSyncSwitch, waveSwitch[2];

    // Mixer
    Knob vcoLevel[2] { Knob { Knob::Kind::level }, Knob { Knob::Kind::level } };
    Knob noiseLevel { Knob::Kind::level };

    // Filter and amplifier
    std::unique_ptr<BatSwitch> filterSwitch, vcaAttackSwitch;
    Knob cutoff { Knob::Kind::large }, resonance { Knob::Kind::large };
    Knob vcfDecay { Knob::Kind::large }, vcfEgAmt { Knob::Kind::large, true };
    Knob noiseVcfMod { Knob::Kind::large };
    Knob volume { Knob::Kind::large }, vcaDecay { Knob::Kind::large };

    // Sequencer
    Knob stepPitch[stepkin::kNumSteps] { };
    Knob stepVelocity[stepkin::kNumSteps] { };
    Led  stepLeds[stepkin::kNumSteps];
    Knob rate { Knob::Kind::large };
    PanelButton triggerBtn { "" }, runStopBtn { "" }, advanceBtn { "" };
    Led runLed { stepkin::ui::colours::led };

    // Readouts
    Led envLeds[3] { Led { stepkin::ui::colours::ledAmber },
                     Led { stepkin::ui::colours::ledAmber },
                     Led { stepkin::ui::colours::ledAmber } };
    Led midiLed { stepkin::ui::colours::ledAmber };

    // Patchbay
    std::unique_ptr<stepkin::ui::Patchbay> patchbay;

    // Utility rail
    PanelButton prevBtn { "<" }, nextPresetBtn { ">" }, saveBtn { "SAVE" };
    stepkin::ui::ChoiceDisplay presetDisplay, stepLenDisplay, midiDisplay;
    PanelButton syncBtn { "SYNC" }, diceBtn { "DICE" }, clearBtn { "CLEAR CABLES" };
    Knob fineTune { Knob::Kind::trim, true }, drift { Knob::Kind::trim };
    juce::String statusText;

    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    int blinkCounter = 0;
    uint32_t lastStepCount = 0;
    uint32_t lastMidi = 0;
    float midiFlash = 0.0f;
    int lastPatchVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepkinPanel)
};

class StepkinEditor final : public juce::AudioProcessorEditor
{
public:
    explicit StepkinEditor (StepkinProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    StepkinPanel panel;
    juce::TooltipWindow tooltips { this, 800 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepkinEditor)
};
