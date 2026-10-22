// Widgets.h — Stepkin panel controls: knobs, rectangular buttons, LEDs,
// two- and three-position bat switches, and small readouts.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

namespace stepkin::ui
{
namespace colours
{
    inline const juce::Colour panel     { 0xff141415 };
    inline const juce::Colour panelHi   { 0xff1d1d1f };
    inline const juce::Colour ink       { 0xfff1ede4 };
    inline const juce::Colour inkDim    { 0xff8f8b82 };
    inline const juce::Colour line      { 0xffd9d4c8 };
    inline const juce::Colour knob      { 0xff0c0c0d };
    inline const juce::Colour led       { 0xffff5a36 };   // warm red-orange
    inline const juce::Colour ledAmber  { 0xffffb238 };
    inline const juce::Colour lit       { 0xfff6e7c4 };
    inline const juce::Colour rail      { 0xff1c1c1e };
    inline const juce::Colour wood      { 0xff6a4126 };
    inline const juce::Colour woodDark  { 0xff3e2414 };
}

juce::Font panelFont (float height, bool bold = true);
void drawText (juce::Graphics&, const juce::String&, juce::Point<float> centre, float height,
               juce::Colour = colours::ink, bool bold = true);
void drawInverseText (juce::Graphics&, const juce::String&, juce::Point<float> centre, float height);

// ---------------------------------------------------------------------------
// Knob
// ---------------------------------------------------------------------------
class Knob final : public juce::Slider
{
public:
    enum class Kind { step, large, level, trim };

    Knob (Kind k = Kind::large, bool bipolar = false);
    Kind kind;
    bool bipolar;
    int  detents = 0;          // >0 draws that many tick marks

private:
    double snapValue (double attempted, DragMode) override;
};

class PanelLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Font getSliderPopupFont (juce::Slider&) override { return panelFont (14.0f); }
    int getSliderPopupPlacement (juce::Slider&) override { return juce::BubbleComponent::above; }
    juce::Font getPopupMenuFont() override { return panelFont (15.0f, false); }
};

// ---------------------------------------------------------------------------
// Rectangular panel button
// ---------------------------------------------------------------------------
class PanelButton final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit PanelButton (juce::String label);

    std::function<void()> onClick;        // fired on release (unless a long press fired)
    std::function<void()> onLongPress;    // ~0.6 s hold
    std::function<void()> onPress;        // momentary down
    std::function<void()> onRelease;      // momentary up

    void setLit (bool);
    void setBlink (bool);                 // lit state blinks
    void setFlash (float);                // brief brighten
    void setBlinkPhase (bool on) { if (blinkPhase != on) { blinkPhase = on; if (blink) repaint(); } }
    void setLabel (const juce::String& t) { label = t; repaint(); }
    void setFontHeight (float h) { fontH = h; repaint(); }
    void setRound (bool r) { round = r; repaint(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    juce::String label;
    bool lit = false, blink = false, blinkPhase = true, pressed = false, longFired = false, round = false;
    float flash = 0.0f, fontH = 11.5f;
};

// ---------------------------------------------------------------------------
// LED
// ---------------------------------------------------------------------------
class Led final : public juce::Component
{
public:
    explicit Led (juce::Colour c = colours::led) : colour (c) { setInterceptsMouseClicks (false, false); }
    void setLevel (float);
    void paint (juce::Graphics&) override;

private:
    juce::Colour colour;
    float level = 0.0f;
};

// ---------------------------------------------------------------------------
// Vertical bat switch with two or three positions. Position 0 is up.
// ---------------------------------------------------------------------------
class BatSwitch final : public juce::Component, public juce::SettableTooltipClient
{
public:
    BatSwitch (juce::RangedAudioParameter& param, int numPositions);
    ~BatSwitch() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void setPosition (int pos);
    juce::ParameterAttachment attachment;
    int positions = 2;
    int position = 0;
};

// ---------------------------------------------------------------------------
// Click-to-choose value display for the utility rail
// ---------------------------------------------------------------------------
class ChoiceDisplay final : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<juce::String()> getText;
    std::function<void (juce::PopupMenu&)> buildMenu;
    std::function<void (int)> onChoose;
    std::function<void()> onOpen;          // replaces the menu when set
    juce::String caption;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    // Repaints only when the text it shows has actually changed, so this is
    // cheap to call from a timer. Nothing else repaints these: choosing from
    // the menu changes a parameter, not the component, and the menu does not
    // necessarily cover the display it was opened from.
    void refresh();

private:
    juce::String shown;
};
} // namespace stepkin::ui
