#include "Patchbay.h"
#include "../PluginProcessor.h"

namespace stepkin::ui
{
using namespace juce;

Patchbay::Patchbay (StepkinProcessor& p, std::vector<JackInfo> j) : proc (p), jacks (std::move (j))
{
    setInterceptsMouseClicks (true, false);
}

Colour Patchbay::cableColour (int input)
{
    static const Colour palette[] {
        Colour (0xffe8893a), Colour (0xff3fb3a8), Colour (0xffe6c449), Colour (0xffd9524a),
        Colour (0xff5b8fd9), Colour (0xff8cc152), Colour (0xffc46bc0), Colour (0xffe9e4d8)
    };
    return palette[(size_t) (input % 8)];
}

int Patchbay::jackAt (Point<float> p) const
{
    for (int i = 0; i < (int) jacks.size(); ++i)
        if (p.getDistanceFrom (jacks[(size_t) i].pos) <= jackRadius + 3.0f)
            return i;
    return -1;
}

int Patchbay::jackForInput (int in) const
{
    for (int i = 0; i < (int) jacks.size(); ++i)
        if (! jacks[(size_t) i].isOutput && jacks[(size_t) i].index == in)
            return i;
    return -1;
}

int Patchbay::jackForOutput (int out) const
{
    for (int i = 0; i < (int) jacks.size(); ++i)
        if (jacks[(size_t) i].isOutput && jacks[(size_t) i].index == out)
            return i;
    return -1;
}

bool Patchbay::hitTest (int x, int y)
{
    return jackAt ({ (float) x, (float) y }) >= 0;
}

Rectangle<int> Patchbay::midiLedArea() const
{
    for (auto& j : jacks)
        if (j.index < 0)
            return Rectangle<float> (16.0f, 16.0f).withCentre (j.pos.translated (-31.0f, -24.0f)).toNearestInt().expanded (4);
    return {};
}

void Patchbay::drawJack (Graphics& g, const JackInfo& j, bool highlight) const
{
    const auto c = j.pos;
    const float r = jackRadius;

    // Label above
    const auto labelPos = c.translated (0.0f, -24.0f);
    if (j.isOutput)
        drawInverseText (g, j.label, labelPos, 9.5f);
    else
        drawText (g, j.label, labelPos, 9.5f, colours::ink, true);

    // Hex nut
    Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const float a = (float) i * MathConstants<float>::pi / 3.0f;
        const Point<float> p (c.x + r * std::cos (a), c.y + r * std::sin (a));
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setColour (colours::knob);
    g.fillPath (hex);
    g.setColour (highlight ? colours::ledAmber : colours::ink);
    g.strokePath (hex, PathStrokeType (highlight ? 2.2f : 1.5f));
    g.drawEllipse (Rectangle<float> (r * 1.22f, r * 1.22f).withCentre (c), 1.6f);
    g.setColour (Colour (0xff030303));
    g.fillEllipse (Rectangle<float> (r * 0.72f, r * 0.72f).withCentre (c));
    g.setColour (colours::ink.withAlpha (0.55f));
    g.drawEllipse (Rectangle<float> (r * 0.72f, r * 0.72f).withCentre (c), 1.0f);
}

void Patchbay::drawCable (Graphics& g, Point<float> a, Point<float> b, Colour col, bool floating) const
{
    // A cable hangs from both plugs. A mostly vertical run would otherwise
    // droop clear off the panel, so it bows out sideways instead, away from
    // whichever edge of the bay it is nearer.
    const float dist = a.getDistanceFrom (b);
    const bool  upright = std::abs (b.y - a.y) > std::abs (b.x - a.x);
    const float sag = upright ? 10.0f + dist * 0.03f : 18.0f + dist * 0.30f;
    float bow = 0.0f;
    if (upright)
    {
        const float mid = (a.x + b.x) * 0.5f;
        const float centre = jacks.empty() ? mid : (jacks.front().pos.x + jacks.back().pos.x) * 0.5f;
        bow = (mid > centre ? -1.0f : 1.0f) * (14.0f + dist * 0.05f);
    }

    Path p;
    p.startNewSubPath (a);
    p.cubicTo (a.translated (bow, sag), b.translated (bow, sag), b);

    g.setColour (Colours::black.withAlpha (0.35f));
    g.strokePath (p, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::rounded), AffineTransform::translation (1.5f, 3.0f));
    g.setColour (col.withAlpha (floating ? 0.70f : 0.82f));
    g.strokePath (p, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colours::white.withAlpha (0.18f));
    g.strokePath (p, PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded), AffineTransform::translation (-1.2f, -1.2f));

    auto plug = [&] (Point<float> c)
    {
        g.setColour (col.darker (0.45f));
        g.fillEllipse (Rectangle<float> (15.0f, 15.0f).withCentre (c));
        g.setColour (col);
        g.fillEllipse (Rectangle<float> (11.0f, 11.0f).withCentre (c));
        g.setColour (Colours::white.withAlpha (0.35f));
        g.fillEllipse (Rectangle<float> (4.0f, 3.0f).withCentre (c.translated (-2.0f, -2.5f)));
    };
    plug (a);
    plug (b);
}

void Patchbay::paint (Graphics& g)
{
    // Jacks
    for (int i = 0; i < (int) jacks.size(); ++i)
    {
        const auto& j = jacks[(size_t) i];
        bool hl = i == hoverJack;
        if (dragFrom >= 0 && i != dragFrom && j.index >= 0)
            hl = hl || (jacks[(size_t) dragFrom].isOutput != j.isOutput && i == jackAt (dragPos));
        drawJack (g, j, hl);
    }

    // MIDI activity LED
    for (auto& j : jacks)
        if (j.index < 0)
        {
            const auto c = j.pos.translated (-31.0f, -24.0f);
            g.setColour (colours::ink);
            g.drawEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (c), 1.3f);
            g.setColour (colours::knob.interpolatedWith (colours::ledAmber, midiLevel));
            g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (c));
        }

    // Cables
    for (int in = 0; in < kNumInputs; ++in)
    {
        const int src = proc.sourceFor (in);
        if (src < 0) continue;
        const int ja = jackForOutput (src), jb = jackForInput (in);
        if (ja < 0 || jb < 0) continue;
        drawCable (g, jacks[(size_t) ja].pos, jacks[(size_t) jb].pos, cableColour (in), false);
    }

    if (dragFrom >= 0)
        drawCable (g, jacks[(size_t) dragFrom].pos, dragPos, dragColour, true);
}

void Patchbay::mouseDown (const MouseEvent& e)
{
    const int j = jackAt (e.position);
    if (j < 0) return;
    const auto& jack = jacks[(size_t) j];
    if (jack.index < 0) return; // MIDI IN is the host's MIDI, not patchable

    if (e.mods.isPopupMenu())
    {
        if (jack.isOutput)
        {
            for (int in = 0; in < kNumInputs; ++in)
                if (proc.sourceFor (in) == jack.index)
                    proc.disconnect (in);
        }
        else
        {
            proc.disconnect (jack.index);
        }
        proc.presets.markEdited();
        repaint();
        return;
    }

    if (! jack.isOutput && proc.sourceFor (jack.index) >= 0)
    {
        // Pull the plug out of this input and carry it.
        const int src = proc.sourceFor (jack.index);
        dragColour = cableColour (jack.index);
        proc.disconnect (jack.index);
        proc.presets.markEdited();
        dragFrom = jackForOutput (src);
    }
    else
    {
        dragFrom = j;
        int colourIndex = 0;
        for (int in = 0; in < kNumInputs; ++in)
            if (proc.sourceFor (in) < 0) { colourIndex = in; break; }
        dragColour = jack.isOutput ? cableColour (colourIndex) : cableColour (jack.index);
    }
    dragPos = e.position;
    repaint();
}

void Patchbay::mouseDrag (const MouseEvent& e)
{
    if (dragFrom < 0) return;
    dragPos = e.position;
    repaint();
}

void Patchbay::mouseUp (const MouseEvent& e)
{
    if (dragFrom < 0) return;
    const int target = jackAt (e.position);
    if (target >= 0 && target != dragFrom)
    {
        const auto& a = jacks[(size_t) dragFrom];
        const auto& b = jacks[(size_t) target];
        if (a.isOutput != b.isOutput && a.index >= 0 && b.index >= 0)
        {
            const auto& out = a.isOutput ? a : b;
            const auto& in  = a.isOutput ? b : a;
            proc.connect (out.index, in.index);
            proc.presets.markEdited();
        }
    }
    dragFrom = -1;
    repaint();
}

void Patchbay::mouseMove (const MouseEvent& e)
{
    const int j = jackAt (e.position);
    if (j != hoverJack) { hoverJack = j; repaint(); }
}

void Patchbay::mouseExit (const MouseEvent&)
{
    if (hoverJack != -1) { hoverJack = -1; repaint(); }
}

String Patchbay::getTooltip()
{
    const int j = jackAt (getMouseXYRelative().toFloat());
    if (j < 0) return {};
    const auto& jack = jacks[(size_t) j];
    String s = String (jack.isOutput ? "OUT  " : "IN  ") + jack.label + "\n" + jack.tip;
    if (jack.index >= 0)
        s << (jack.isOutput ? "\nDrag to an input to patch. Right-click to unplug all."
                            : "\nDrag from an output to here. Grab a plugged cable to move it; right-click to unplug.");
    return s;
}
} // namespace stepkin::ui
