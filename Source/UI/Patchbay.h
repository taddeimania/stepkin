// Patchbay.h — the 24-point patchbay with draggable cables.
// Drag from any jack to another (output -> input). Grab a plugged input to
// move or pull its cable; right-click a jack to unplug it.
#pragma once

#include "Widgets.h"
#include "../Engine/Engine.h"

class StepkinProcessor;

namespace stepkin::ui
{
struct JackInfo
{
    const char* label;
    bool isOutput;
    int  index;            // In / Out enum value, -1 for MIDI IN
    juce::Point<float> pos; // design-space centre
    const char* tip;
};

class Patchbay final : public juce::Component, public juce::TooltipClient
{
public:
    Patchbay (StepkinProcessor&, std::vector<JackInfo> jacks);

    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    juce::String getTooltip() override;

    void setMidiLevel (float l) { if (std::abs (l - midiLevel) > 0.02f) { midiLevel = l; repaint (midiLedArea()); } }
    void refresh() { repaint(); }

    static constexpr float jackRadius = 17.0f;

private:
    int  jackAt (juce::Point<float>) const;
    int  jackForInput (int in) const;
    int  jackForOutput (int out) const;
    void drawJack (juce::Graphics&, const JackInfo&, bool highlight) const;
    void drawCable (juce::Graphics&, juce::Point<float> a, juce::Point<float> b, juce::Colour, bool floating) const;
    juce::Rectangle<int> midiLedArea() const;
    static juce::Colour cableColour (int input);

    StepkinProcessor& proc;
    std::vector<JackInfo> jacks;

    int dragFrom = -1;             // jack index the floating cable starts at
    juce::Point<float> dragPos;
    int hoverJack = -1;
    juce::Colour dragColour;
    float midiLevel = 0.0f;
};
} // namespace stepkin::ui
