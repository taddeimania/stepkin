#include "Widgets.h"

namespace stepkin::ui
{
using namespace juce;

// ---------------------------------------------------------------------------
// Type
// ---------------------------------------------------------------------------
Font panelFont (float height, bool bold)
{
   #if JUCE_MAC
    Font f (FontOptions ("Helvetica Neue", height, bold ? Font::bold : Font::plain));
   #else
    Font f (FontOptions (height, bold ? Font::bold : Font::plain));
   #endif
    f.setHorizontalScale (0.84f);
    return f;
}

void drawText (Graphics& g, const String& text, Point<float> c, float height, Colour colour, bool bold)
{
    g.setColour (colour);
    g.setFont (panelFont (height, bold));
    const float w = jmax (30.0f, height * 0.8f * (float) text.length() + 16.0f);
    g.drawText (text, Rectangle<float> (c.x - w * 0.5f, c.y - height, w, height * 2.0f), Justification::centred, false);
}

void drawInverseText (Graphics& g, const String& text, Point<float> c, float height)
{
    const auto f = panelFont (height, true);
    const float w = GlyphArrangement::getStringWidth (f, text) + 6.0f;
    const auto r = Rectangle<float> (w, height + 3.0f).withCentre (c);
    g.setColour (colours::ink);
    g.fillRect (r);
    g.setColour (colours::panel);
    g.setFont (f);
    g.drawText (text, r, Justification::centred, false);
}

// ---------------------------------------------------------------------------
// Knob
// ---------------------------------------------------------------------------
Knob::Knob (Kind k, bool bip) : kind (k), bipolar (bip)
{
    setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    setPopupDisplayEnabled (true, true, nullptr, 900);
    setMouseDragSensitivity (k == Kind::step || k == Kind::trim ? 260 : 300);
    setVelocityBasedMode (false);
}

double Knob::snapValue (double attempted, DragMode)
{
    if (bipolar)
    {
        const double mid = (getMinimum() + getMaximum()) * 0.5;
        if (std::abs (attempted - mid) < 0.025 * (getMaximum() - getMinimum()))
            return mid;
    }
    return attempted;
}

PanelLookAndFeel::PanelLookAndFeel()
{
    setColour (BubbleComponent::backgroundColourId, Colour (0xf0101011));
    setColour (BubbleComponent::outlineColourId, colours::ledAmber);
    setColour (TooltipWindow::backgroundColourId, Colour (0xf0101011));
    setColour (TooltipWindow::textColourId, colours::ink);
    setColour (TooltipWindow::outlineColourId, colours::inkDim);
    setColour (PopupMenu::backgroundColourId, Colour (0xff18181a));
    setColour (PopupMenu::textColourId, colours::ink);
    setColour (PopupMenu::highlightedBackgroundColourId, colours::ledAmber.withAlpha (0.85f));
    setColour (PopupMenu::highlightedTextColourId, Colours::black);
    setColour (PopupMenu::headerTextColourId, colours::inkDim);
    setColour (AlertWindow::backgroundColourId, Colour (0xff18181a));
    setColour (AlertWindow::textColourId, colours::ink);
    setColour (AlertWindow::outlineColourId, colours::inkDim);
    setColour (TextEditor::backgroundColourId, Colour (0xff0b0b0c));
    setColour (TextEditor::textColourId, colours::ink);
    setColour (TextEditor::outlineColourId, colours::inkDim);
    setColour (TextEditor::focusedOutlineColourId, colours::ledAmber);
    setColour (TextButton::buttonColourId, Colour (0xff2a2a2d));
    setColour (TextButton::textColourOffId, colours::ink);
}

namespace
{
    Point<float> polar (Point<float> c, float r, float a) { return { c.x + r * std::sin (a), c.y - r * std::cos (a) }; }

    Path scallopedCircle (Point<float> c, float r, int lobes, float depth, float rot)
    {
        Path p;
        constexpr int n = 180;
        for (int i = 0; i <= n; ++i)
        {
            const float a = (float) i / (float) n * MathConstants<float>::twoPi;
            const float rr = r * (1.0f + depth * std::cos ((float) lobes * (a - rot)));
            const auto pt = polar (c, rr, a);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        p.closeSubPath();
        return p;
    }

    void ticks (Graphics& g, Point<float> c, float r0, float r1, int count, float start, float end, float thick, Colour col)
    {
        g.setColour (col);
        for (int i = 0; i < count; ++i)
        {
            const float a = start + (end - start) * (float) i / (float) jmax (1, count - 1);
            g.drawLine (Line<float> (polar (c, r0, a), polar (c, r1, a)), thick);
        }
    }

    void pointer (Graphics& g, Point<float> c, float r, float a, float thick)
    {
        g.setColour (colours::ink);
        g.drawLine (Line<float> (polar (c, r * 0.08f, a), polar (c, r, a)), thick);
    }
}

void PanelLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, Slider& s)
{
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = bounds.getCentre();
    const float R = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 1.0f;
    const float a = startAngle + pos * (endAngle - startAngle);

    auto* knob = dynamic_cast<Knob*> (&s);
    const auto kind = knob != nullptr ? knob->kind : Knob::Kind::large;
    const bool bip = knob != nullptr && knob->bipolar;
    const Colour ink = colours::ink;
    const Colour body = colours::knob;

    switch (kind)
    {
        case Knob::Kind::step:
        {
            // Small knob with a long pointer, like the sequencer row.
            if (bip)
                ticks (g, c, R * 0.72f, R * 1.0f, 3, startAngle, endAngle, jmax (1.0f, R * 0.05f), ink.withAlpha (0.75f));
            const float r = R * 0.62f;
            g.setColour (body);
            g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c));
            g.setColour (ink);
            g.drawEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c), jmax (1.4f, R * 0.07f));
            pointer (g, c, R * 0.60f, a, jmax (2.6f, R * 0.16f));
            break;
        }
        case Knob::Kind::level:
        {
            ticks (g, c, R * 0.74f, R * 0.98f, 31, startAngle, endAngle, jmax (1.0f, R * 0.035f), ink.withAlpha (0.85f));
            const auto edge = scallopedCircle (c, R * 0.66f, 14, 0.05f, a);
            g.setColour (body);
            g.fillPath (edge);
            g.setColour (ink);
            g.strokePath (edge, PathStrokeType (jmax (1.6f, R * 0.06f)));
            const float rc = R * 0.42f;
            g.drawEllipse (Rectangle<float> (rc * 2, rc * 2).withCentre (c), jmax (2.0f, R * 0.08f));
            pointer (g, c, R * 0.72f, a, jmax (3.0f, R * 0.12f));
            break;
        }
        case Knob::Kind::trim:
        {
            ticks (g, c, R * 0.80f, R * 0.98f, bip ? 3 : 11, startAngle, endAngle, 1.0f, ink.withAlpha (0.7f));
            const float r = R * 0.68f;
            g.setColour (body);
            g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c));
            g.setColour (ink.withAlpha (0.9f));
            g.drawEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c), 1.4f);
            pointer (g, c, r * 0.95f, a, 2.4f);
            break;
        }
        case Knob::Kind::large:
        default:
        {
            const int n = knob != nullptr && knob->detents > 0 ? knob->detents : 21;
            ticks (g, c, R * 0.88f, R * 0.99f, n, startAngle, endAngle, jmax (1.1f, R * 0.028f), ink.withAlpha (0.8f));
            if (bip)
                ticks (g, c, R * 0.84f, R * 1.0f, 1, (startAngle + endAngle) * 0.5f, (startAngle + endAngle) * 0.5f, 2.2f, ink);
            // Skirt ring
            const float rs = R * 0.80f;
            g.setColour (body);
            g.fillEllipse (Rectangle<float> (rs * 2, rs * 2).withCentre (c));
            g.setColour (ink);
            g.drawEllipse (Rectangle<float> (rs * 2, rs * 2).withCentre (c), jmax (1.6f, R * 0.035f));
            // Knurled cap
            const auto edge = scallopedCircle (c, R * 0.66f, 18, 0.035f, a);
            g.setColour (body);
            g.fillPath (edge);
            g.setColour (ink);
            g.strokePath (edge, PathStrokeType (jmax (2.2f, R * 0.06f)));
            const float rc = R * 0.44f;
            g.drawEllipse (Rectangle<float> (rc * 2, rc * 2).withCentre (c), jmax (2.0f, R * 0.05f));
            pointer (g, c, R * 0.74f, a, jmax (3.2f, R * 0.085f));
            break;
        }
    }

    if (s.isMouseOverOrDragging())
    {
        g.setColour (colours::ledAmber.withAlpha (0.10f));
        g.fillEllipse (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c));
    }
}

// ---------------------------------------------------------------------------
// PanelButton
// ---------------------------------------------------------------------------
PanelButton::PanelButton (String l) : label (std::move (l))
{
    setRepaintsOnMouseActivity (true);
}

void PanelButton::setLit (bool b)    { if (b != lit) { lit = b; repaint(); } }
void PanelButton::setBlink (bool b)  { if (b != blink) { blink = b; repaint(); } }
void PanelButton::setFlash (float f)
{
    f = jlimit (0.0f, 1.0f, f);
    if (std::abs (f - flash) > 0.02f) { flash = f; repaint(); }
}

void PanelButton::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = lit && (! blink || blinkPhase);

    if (round)
    {
        // Soft round push-button, as on the transport row.
        const float d = jmin (r.getWidth(), r.getHeight());
        const auto circle = Rectangle<float> (d, d).withCentre (r.getCentre());
        g.setColour (colours::ink.withAlpha (0.9f));
        g.drawEllipse (circle, 1.4f);
        const auto inner = circle.reduced (3.0f);
        if (on)
        {
            g.setColour (colours::lit.withAlpha (0.22f));
            g.fillEllipse (circle.expanded (2.5f));
            g.setColour (colours::lit);
        }
        else
        {
            g.setColour (pressed ? colours::panelHi.brighter (0.2f) : colours::knob);
        }
        g.fillEllipse (inner);
        if (! on)
        {
            g.setColour (colours::ink.withAlpha (0.35f));
            g.drawEllipse (inner, 1.0f);
        }
        if (isMouseOver() && ! on)
        {
            g.setColour (colours::ledAmber.withAlpha (0.14f));
            g.fillEllipse (inner);
        }
        if (flash > 0.0f)
        {
            g.setColour (colours::ledAmber.withAlpha (0.55f * flash));
            g.drawEllipse (circle.expanded (1.5f), 2.5f);
        }
        return;
    }

    // Outer outline (double-line panel button)
    g.setColour (colours::ink.withAlpha (0.95f));
    g.drawRoundedRectangle (r, 3.5f, 1.4f);
    auto inner = r.reduced (3.0f);

    if (on)
    {
        g.setColour (colours::lit.withAlpha (0.25f));
        g.fillRoundedRectangle (r.expanded (2.0f), 5.0f);
        g.setColour (colours::lit);
        g.fillRoundedRectangle (inner, 2.0f);
    }
    else
    {
        g.setColour (pressed ? colours::panelHi.brighter (0.15f) : colours::knob);
        g.fillRoundedRectangle (inner, 2.0f);
        g.setColour (colours::ink.withAlpha (0.8f));
        g.drawRoundedRectangle (inner, 2.0f, 1.0f);
    }

    if (flash > 0.0f)
    {
        g.setColour (colours::ledAmber.withAlpha (0.55f * flash));
        g.drawRoundedRectangle (r.expanded (1.5f), 4.5f, 2.5f);
    }
    if (isMouseOver() && ! on)
    {
        g.setColour (colours::ledAmber.withAlpha (0.12f));
        g.fillRoundedRectangle (inner, 2.0f);
    }

    g.setColour (on ? colours::knob : colours::ink);
    g.setFont (panelFont (fontH));
    g.drawText (label, inner.translated (0.0f, pressed ? 1.0f : 0.0f), Justification::centred, false);
}

void PanelButton::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    pressed = true;
    longFired = false;
    if (onLongPress)
        startTimer (600);
    if (onPress)
        onPress();
    repaint();
}

void PanelButton::mouseUp (const MouseEvent& e)
{
    if (! pressed)
        return;
    stopTimer();
    pressed = false;
    if (onRelease)
        onRelease();
    if (! longFired && onClick && getLocalBounds().contains (e.getPosition()))
        onClick();
    repaint();
}

void PanelButton::timerCallback()
{
    stopTimer();
    if (pressed && onLongPress)
    {
        longFired = true;
        onLongPress();
    }
}

// ---------------------------------------------------------------------------
// LED
// ---------------------------------------------------------------------------
void Led::setLevel (float newLevel)
{
    newLevel = jlimit (0.0f, 1.0f, newLevel);
    if (std::abs (newLevel - level) > 0.01f)
    {
        level = newLevel;
        repaint();
    }
}

void Led::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.30f;

    if (level > 0.0f)
    {
        ColourGradient glow (colour.withAlpha (0.6f * level), c.x, c.y, colour.withAlpha (0.0f), c.x + r * 1.7f, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (b);
    }
    g.setColour (colours::ink.withAlpha (0.9f));
    g.drawEllipse (Rectangle<float> (r * 2.0f + 2.0f, r * 2.0f + 2.0f).withCentre (c), 1.3f);
    const Colour off = colours::knob;
    const Colour face = off.interpolatedWith (colour, level);
    g.setColour (face);
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    if (level > 0.0f)
    {
        g.setColour (Colours::white.withAlpha (0.45f * level));
        g.fillEllipse (Rectangle<float> (r * 0.7f, r * 0.5f).withCentre (c.translated (-r * 0.25f, -r * 0.35f)));
    }
}

// ---------------------------------------------------------------------------
// BatSwitch
// ---------------------------------------------------------------------------
BatSwitch::BatSwitch (RangedAudioParameter& p, int numPositions)
    : attachment (p, [this] (float v) { position = jlimit (0, positions - 1, roundToInt (v)); repaint(); }),
      positions (jlimit (2, 3, numPositions))
{
    attachment.sendInitialUpdate();
}

BatSwitch::~BatSwitch() = default;

void BatSwitch::setPosition (int pos)
{
    pos = jlimit (0, positions - 1, pos);
    if (pos != position)
        attachment.setValueAsCompleteGesture ((float) pos);
}

void BatSwitch::mouseDown (const MouseEvent& e)
{
    const float y = (float) e.y / (float) jmax (1, getHeight());
    if (positions == 2)
        setPosition (y < 0.5f ? 0 : 1);
    else
        setPosition (y < 0.36f ? 0 : (y > 0.64f ? 2 : 1));
}

void BatSwitch::mouseDrag (const MouseEvent& e) { mouseDown (e); }

void BatSwitch::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float r = jmin (b.getWidth(), b.getHeight() * 0.5f) * 0.42f;

    // Hex nut
    Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const float a = MathConstants<float>::pi / 6.0f + (float) i * MathConstants<float>::pi / 3.0f;
        const Point<float> p (c.x + r * std::cos (a), c.y + r * std::sin (a));
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setColour (colours::knob);
    g.fillPath (hex);
    g.setColour (colours::ink);
    g.strokePath (hex, PathStrokeType (1.4f));
    g.drawEllipse (Rectangle<float> (r * 1.15f, r * 1.15f).withCentre (c), 1.3f);

    // Bat lever
    const float len = b.getHeight() * 0.36f;
    float dir = 0.0f;
    if (positions == 2)
        dir = position == 0 ? -1.0f : 1.0f;
    else
        dir = position == 0 ? -1.0f : (position == 2 ? 1.0f : 0.0f);

    if (std::abs (dir) < 0.01f)
    {
        g.setColour (colours::ink);
        g.fillEllipse (Rectangle<float> (r * 0.75f, r * 0.75f).withCentre (c));
        g.setColour (colours::knob);
        g.fillEllipse (Rectangle<float> (r * 0.35f, r * 0.35f).withCentre (c));
    }
    else
    {
        const Point<float> tip (c.x, c.y + dir * len);
        g.setColour (colours::ink);
        g.drawLine (Line<float> (c, tip), r * 0.42f);
        g.fillEllipse (Rectangle<float> (r * 0.6f, r * 0.6f).withCentre (tip));
    }
}

// ---------------------------------------------------------------------------
// ChoiceDisplay
// ---------------------------------------------------------------------------
void ChoiceDisplay::refresh()
{
    if (getText == nullptr)
    {
        repaint();
        return;
    }
    const auto text = getText();
    if (text != shown)
    {
        shown = text;
        repaint();
    }
}

void ChoiceDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Colour (0xff09090a));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (isMouseOver() ? colours::ledAmber.withAlpha (0.8f) : colours::inkDim.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);

    g.setColour (colours::ledAmber);
    g.setFont (panelFont (jmin (17.0f, r.getHeight() * 0.55f), true));
    if (getText != nullptr)
        shown = getText();
    g.drawText (shown, r.reduced (8.0f, 0.0f), Justification::centredLeft, true);

    // Drop-down hint
    const float s = 4.0f;
    const auto cx = r.getRight() - 11.0f, cy = r.getCentreY();
    Path tri;
    tri.addTriangle (cx - s, cy - s * 0.5f, cx + s, cy - s * 0.5f, cx, cy + s * 0.7f);
    g.setColour (colours::inkDim);
    g.fillPath (tri);
}

void ChoiceDisplay::mouseDown (const MouseEvent&)
{
    if (onOpen)
    {
        onOpen();
        return;
    }
    if (! buildMenu)
        return;
    PopupMenu m;
    buildMenu (m);
    auto safe = Component::SafePointer<ChoiceDisplay> (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                     [safe] (int result)
                     {
                         if (safe != nullptr && result != 0 && safe->onChoose)
                             safe->onChoose (result);
                     });
}
} // namespace stepkin::ui
