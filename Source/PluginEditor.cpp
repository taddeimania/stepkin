#include "PluginEditor.h"

using namespace stepkin;
using namespace stepkin::ui;
using juce::Point;
using juce::Rectangle;
using juce::String;

namespace
{
// --- Layout ----------------------------------------------------------------
constexpr float kRow1 = 160.0f;
constexpr float kRow2 = 342.0f;

constexpr float kColDecay   = 118.0f;
constexpr float kColModSw   = 205.0f;
constexpr float kColEgAmt   = 310.0f;
constexpr float kColFreq    = 432.0f;
constexpr float kColWave    = 524.0f;
constexpr float kColLevel   = 594.0f;
constexpr float kColFiltSw  = 668.0f;
constexpr float kColCutoff  = 752.0f;
constexpr float kColRes     = 878.0f;
constexpr float kColMod     = 1004.0f;
constexpr float kColAmpSw   = 1098.0f;
constexpr float kColVol     = 1190.0f;

constexpr float kLargeKnob = 102.0f;
constexpr float kFreqKnob  = 126.0f;
constexpr float kLevelKnob = 48.0f;
constexpr float kStepKnob  = 58.0f;
constexpr float kSwitchW   = 30.0f;
constexpr float kSwitchH   = 62.0f;

constexpr float kStepX0   = 505.0f;
constexpr float kStepDx   = 92.0f;
constexpr float kStepNumY = 462.0f;
constexpr float kPitchY   = 516.0f;
constexpr float kLedY     = 572.0f;
constexpr float kVelY     = 626.0f;

constexpr float kBayX[]  { 1385.0f, 1495.0f, 1605.0f };
constexpr float kBayY0   = 148.0f;
constexpr float kBayDy   = 78.0f;

constexpr float kRailTop = 722.0f;
constexpr float kRailCapY = 736.0f;
constexpr float kRailY   = 766.0f;

float stepX (int i) { return kStepX0 + (float) i * kStepDx; }
float bayY (int r)  { return kBayY0 + (float) r * kBayDy; }

// --- Patchbay jacks --------------------------------------------------------
const char* inputTip (int in)
{
    switch (in)
    {
        case inTrigger:    return "Rising edge fires all three envelopes at the current step's velocity, without stepping the sequencer.\nNormalled to the sequencer clock: patch here and the clock stops firing the envelopes.";
        case inVcaCv:      return "Added to the amp envelope, 0 to +8 V. Opens the amplifier independently of the envelope.";
        case inVelocity:   return "Velocity for all three envelopes, 0 to +5 V.\nNormalled to the step VELOCITY knobs.";
        case inVcaDecay:   return "Amp decay time, -5 to +5 V, summed with the AMP DECAY knob.\nCentre the knob for the widest range.";
        case inExtAudio:   return "An external signal in place of the noise generator in the mixer, level set by NOISE / EXT LEVEL.";
        case inVcfDecay:   return "Filter decay time, -5 to +5 V, summed with the FILTER DECAY knob.";
        case inNoiseLevel: return "Added to the NOISE / EXT LEVEL knob, 0 to +8 V.";
        case inVcoDecay:   return "Pitch decay time, -5 to +5 V, summed with the PITCH DECAY knob.";
        case inVcfMod:     return "Replaces noise as the filter's modulation source. Depth still follows the NOISE / FILTER MOD knob;\nturn it fully up for complete control of the cutoff.";
        case inVco1Cv:     return "1 V/oct pitch CV for oscillator 1, summed with its FREQUENCY knob, EG AMOUNT and the step pitches.";
        case inFmAmt:      return "Added to the 1>2 FM AMOUNT knob, 0 to +8 V.";
        case inVco2Cv:     return "1 V/oct pitch CV for oscillator 2.";
        case inTempo:      return "1 V/oct on the clock, summed with the RATE knob. Only while SYNC is off.\nA step CV here gives every step its own length.";
        case inRunStop:    return "Gate: +5 V runs the sequencer, 0 V stops it.";
        case inAdvClock:   return "External clock: each rising edge advances one step. Takes over from RATE and host sync.";
        default:           return "";
    }
}

const char* outputTip (int out)
{
    switch (out)
    {
        case outVca:      return "The main output, after the amplifier and the VOLUME knob.";
        case outVcaEg:    return "A copy of the amp envelope, 0 to +8 V. Follows the step velocity, AMP DECAY and the attack switch.";
        case outVcfEg:    return "A copy of the filter envelope, 0 to +8 V.";
        case outVcoEg:    return "A copy of the pitch envelope, 0 to +8 V.";
        case outVco1:     return "Oscillator 1 audio, +/-5 V. Useful as a modulation source as well as a sound.";
        case outVco2:     return "Oscillator 2 audio, +/-5 V. Try it in FILTER MOD.";
        case outTrigger:  return "A 1 ms, +5 V pulse each time the envelopes fire.";
        case outVelocity: return "The current step's VELOCITY knob, 0 to +5 V.";
        case outPitch:    return "The current step's PITCH knob, -5 to +5 V. Live even with STEP PITCH TO set to OFF.";
        default:          return "";
    }
}

std::vector<JackInfo> makeJacks()
{
    struct Cell { const char* label; bool out; int idx; };
    const Cell grid[8][3] {
        { { "TRIGGER",      false, inTrigger    }, { "AMP CV",        false, inVcaCv    }, { "AMP OUT",     true,  outVca      } },
        { { "VELOCITY",     false, inVelocity   }, { "AMP DECAY",     false, inVcaDecay }, { "AMP EG",      true,  outVcaEg    } },
        { { "EXT AUDIO",    false, inExtAudio   }, { "FILTER DECAY",  false, inVcfDecay }, { "FILTER EG",   true,  outVcfEg    } },
        { { "NOISE LEVEL",  false, inNoiseLevel }, { "PITCH DECAY",   false, inVcoDecay }, { "PITCH EG",    true,  outVcoEg    } },
        { { "FILTER MOD",   false, inVcfMod     }, { "OSC 1 CV",      false, inVco1Cv   }, { "OSC 1",       true,  outVco1     } },
        { { "1>2 FM AMT",   false, inFmAmt      }, { "OSC 2 CV",      false, inVco2Cv   }, { "OSC 2",       true,  outVco2     } },
        { { "RATE",         false, inTempo      }, { "RUN / STOP",    false, inRunStop  }, { "ADV / CLOCK", false, inAdvClock  } },
        { { "TRIGGER",      true,  outTrigger   }, { "VELOCITY",      true,  outVelocity }, { "PITCH",      true,  outPitch    } },
    };

    std::vector<JackInfo> jacks;
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 3; ++c)
        {
            const auto& cell = grid[r][c];
            jacks.push_back ({ cell.label, cell.out, cell.idx,
                               { kBayX[c], bayY (r) },
                               cell.out ? outputTip (cell.idx) : inputTip (cell.idx) });
        }
    return jacks;
}
} // namespace

// ---------------------------------------------------------------------------
StepkinPanel::StepkinPanel (StepkinProcessor& p) : proc (p), apvts (p.apvts)
{
    setLookAndFeel (&lnf);
    setSize (designW, designH);
    setOpaque (true);

    // --- Oscillators and modulation ------------------------------------------
    addKnob (vcoDecay, ids::vcoDecay, { kColDecay, kRow1 }, kLargeKnob,
             "Decay time of the pitch envelope, shared by both oscillators.\n"
             "Short settings give a sharp beater click; long ones give slow booms.\n"
             "Does nothing while both EG AMOUNT knobs are centred.");
    addKnob (fmAmount, ids::fmAmount, { kColDecay, kRow2 }, kLargeKnob,
             "How much oscillator 1 modulates oscillator 2's frequency (linear FM).\n"
             "The result depends on both pitches: complex, springy, bell-like or aggressive.\n"
             "More obvious with HARD SYNC off.");

    addSwitch (pitchModSwitch, ids::pitchMod, 3, { kColModSw, kRow1 },
               "Which oscillators the step PITCH knobs reach.\n"
               "Up: both. Centre: neither, though the PITCH jack stays live. Down: oscillator 2 only.");
    addSwitch (hardSyncSwitch, ids::hardSync, 2, { kColModSw, kRow2 },
               "Resets oscillator 2 every time oscillator 1 completes a cycle, locking their pitches together\n"
               "and giving oscillator 2 a sharp, metallic edge. With sync on, oscillator 2's FREQUENCY knob\n"
               "changes its timbre rather than its pitch.");

    for (int v = 0; v < 2; ++v)
    {
        const String n = "oscillator " + String (v + 1);
        addKnob (vcoEgAmt[v], ids::vcoEgAmt (v), { kColEgAmt, v == 0 ? kRow1 : kRow2 }, kLargeKnob,
                 "How much the pitch envelope moves " + n + ". Centre: no modulation.\n"
                 "Right of centre spikes the pitch upwards on each hit; left of centre drops it.\n"
                 "Small amounts read as natural percussion, large ones as exaggerated.");
        addKnob (vcoFreq[v], ids::vcoFreq (v), { kColFreq, v == 0 ? kRow1 : kRow2 }, kFreqKnob,
                 "Starting pitch of " + n + ", over ten octaves. Centre is the reference pitch,\n"
                 "which matches the centre of the step PITCH knobs.");
        addSwitch (waveSwitch[v], ids::vcoWave (v), 2, { kColWave, v == 0 ? kRow1 : kRow2 },
                   "Waveform for " + n + ".\nUp: pulse, rich and hollow, good for hard bass and metallic FM.\n"
                   "Down: triangle, smooth and warm, good for toms and classic analog drums.");
        addKnob (vcoLevel[v], ids::vcoLevel (v), { kColLevel, v == 0 ? 150.0f : 360.0f }, kLevelKnob,
                 "Level of " + n + " into the mixer. Pushing the mixer hard clips it, which may be what you want.");
    }

    addKnob (noiseLevel, ids::noiseLevel, { kColLevel, 256.0f }, kLevelKnob,
             "Level of the noise generator, or of whatever is patched into EXT AUDIO.\n"
             "A little noise under a pitched sound adds attack and depth; a lot makes snares and hats.");

    // --- Filter ---------------------------------------------------------------
    addSwitch (filterSwitch, ids::filterMode, 2, { kColFiltSw, kRow1 },
               "Filter response.\nUp: high pass, for bright, thin, snappy sounds.\nDown: low pass, for thick, dark, natural ones.\n"
               "Both are 4-pole, 24 dB per octave.");
    addKnob (cutoff, ids::cutoff, { kColCutoff, kRow1 }, kLargeKnob,
             "Where the filter starts to cut, 20 Hz to 20 kHz.");
    addKnob (resonance, ids::resonance, { kColRes, kRow1 }, kLargeKnob,
             "Feedback around the filter: a peak at the cutoff, thinner lows, and self-oscillation near the top.\n"
             "Resonance also exaggerates whatever is modulating the cutoff.");
    addKnob (vcfDecay, ids::vcfDecay, { kColCutoff, kRow2 }, kLargeKnob,
             "Decay time of the filter envelope. Short is a tap or click, around the middle is a natural sweep,\n"
             "long holds the filter open. Does nothing while FILTER EG AMOUNT is centred.");
    addKnob (vcfEgAmt, ids::vcfEgAmt, { kColRes, kRow2 }, kLargeKnob,
             "How much the filter envelope moves the cutoff, in either direction.\n"
             "Right of centre is articulate and aggressive; left of centre blooms downwards.");
    addKnob (noiseVcfMod, ids::noiseVcfMod, { kColMod, kRow2 }, kLargeKnob,
             "How hard noise shakes the cutoff: dirty, distorted, lo-fi, and sizzle on cymbals.\n"
             "A cable in FILTER MOD takes noise's place and this knob becomes its depth.");

    for (int i = 0; i < 3; ++i)
        addLed (envLeds[i], { kColMod, 126.0f + (float) i * 32.0f }, 16.0f);

    // --- Amplifier ------------------------------------------------------------
    addSwitch (vcaAttackSwitch, ids::vcaAttack, 2, { kColAmpSw, kRow1 },
               "Attack time of the amp envelope.\nUp: 1 ms, immediate snap for hats and kicks.\n"
               "Down: 100 ms, a soft swell for mallets, brushes and gongs. On a bass line it reads a little like glide.");
    addKnob (volume, ids::volume, { kColVol, kRow1 }, kLargeKnob, "Output level.");
    addKnob (vcaDecay, ids::vcaDecay, { kColVol, kRow2 }, kLargeKnob,
             "Decay time of the amp envelope, and so how long each hit lasts.\n"
             "Short is a tight hat; around the middle lets the body ring; long washes steps together.");

    // --- Sequencer ------------------------------------------------------------
    for (int i = 0; i < kNumSteps; ++i)
    {
        stepPitch[i].kind = Knob::Kind::step;
        stepPitch[i].bipolar = true;
        addKnob (stepPitch[i], ids::stepPitch (i), { stepX (i), kPitchY }, kStepKnob,
                 "Step " + String (i + 1) + " pitch. Centre adds nothing; the full sweep is ten octaves.\n"
                 "Reaches the oscillators only when STEP PITCH TO is not OFF, but the PITCH jack is always live.");

        stepVelocity[i].kind = Knob::Kind::step;
        addKnob (stepVelocity[i], ids::stepVelocity (i), { stepX (i), kVelY }, kStepKnob,
                 "Step " + String (i + 1) + " velocity: how hard this step hits.\n"
                 "It sets the level of all three envelopes, so it changes punch as well as volume,\n"
                 "and a harder hit rings slightly longer. Fully down and the step is silent.");

        addLed (stepLeds[i], { stepX (i), kLedY }, 20.0f);
    }

    addKnob (rate, ids::rate, { 238.0f, 552.0f }, kLargeKnob,
             "Free-running step rate, from a half step per second up into the audio range,\n"
             "where the eight steps become one waveform cycle. Ignored while SYNC is on.");

    addButton (triggerBtn, { 112.0f, 556.0f }, 58.0f, 58.0f,
               "Sounds the current step without moving on. Handy for dialling a single step in.", true);
    triggerBtn.onClick = [this] { proc.uiTrigger(); };

    addButton (runStopBtn, { 196.0f, 652.0f }, 52.0f, 52.0f,
               "Starts and stops the sequencer.\nWith SYNC on, it also has to wait for the host transport.", true);
    runStopBtn.onClick = [this] { proc.uiTogglePlay(); };
    addLed (runLed, { 152.0f, 652.0f }, 18.0f);

    addButton (advanceBtn, { 298.0f, 652.0f }, 52.0f, 52.0f,
               "Moves on one step without sounding the step it lands on.", true);
    advanceBtn.onClick = [this] { proc.uiAdvance(); };

    // --- Patchbay -------------------------------------------------------------
    patchbay = std::make_unique<Patchbay> (proc, makeJacks());
    patchbay->setBounds (0, 0, designW, designH);
    addAndMakeVisible (*patchbay);

    buildRail();
    startTimerHz (30);
}

StepkinPanel::~StepkinPanel()
{
    setLookAndFeel (nullptr);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
float StepkinPanel::getParam (const String& id) const
{
    if (auto* p = apvts.getParameter (id))
        return p->convertFrom0to1 (p->getValue());
    return 0.0f;
}

void StepkinPanel::setParam (const String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

void StepkinPanel::toggleParam (const String& id)
{
    setParam (id, getParam (id) > 0.5f ? 0.0f : 1.0f);
}

void StepkinPanel::addKnob (Knob& k, const String& id, Point<float> centre, float size, const String& tip)
{
    k.setTooltip (tip);
    k.setBounds (Rectangle<float> (size, size).withCentre (centre).toNearestInt());
    addAndMakeVisible (k);
    attachments.push_back (std::make_unique<SliderAttachment> (apvts, id, k));
    if (auto* p = apvts.getParameter (id))
        k.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

void StepkinPanel::addButton (PanelButton& b, Point<float> centre, float w, float h, const String& tip, bool round)
{
    b.setTooltip (tip);
    b.setRound (round);
    b.setBounds (Rectangle<float> (w, h).withCentre (centre).toNearestInt());
    addAndMakeVisible (b);
}

void StepkinPanel::addToggle (PanelButton& b, const String& id, Point<float> centre, float w, float h, const String& tip)
{
    addButton (b, centre, w, h, tip);
    b.setFontHeight (10.5f);
    b.onClick = [this, id] { toggleParam (id); };
}

void StepkinPanel::addLed (Led& l, Point<float> centre, float size)
{
    l.setBounds (Rectangle<float> (size, size).withCentre (centre).toNearestInt());
    addAndMakeVisible (l);
}

void StepkinPanel::addSwitch (std::unique_ptr<BatSwitch>& sw, const String& id, int positions,
                              Point<float> centre, const String& tip)
{
    if (auto* p = apvts.getParameter (id))
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            sw = std::make_unique<BatSwitch> (*ranged, positions);
            sw->setTooltip (tip);
            sw->setBounds (Rectangle<float> (kSwitchW, kSwitchH).withCentre (centre).toNearestInt());
            addAndMakeVisible (*sw);
        }
    }
}

// ---------------------------------------------------------------------------
// Utility rail: presets, sync, extras
// ---------------------------------------------------------------------------
void StepkinPanel::buildRail()
{
    auto place = [] (juce::Component& c, float cx, float w, float h)
    {
        c.setBounds (Rectangle<float> (w, h).withCentre ({ cx, kRailY }).toNearestInt());
    };

    // Presets
    addButton (prevBtn, { 78.0f, kRailY }, 30.0f, 30.0f, "Previous preset.");
    prevBtn.onClick = [this] { proc.presets.step (-1); };

    presetDisplay.setTooltip ("The loaded preset. Click to choose another, rename or delete.\n"
                              "A star means the panel has been changed since it loaded.");
    presetDisplay.getText = [this]
    {
        return proc.presets.currentName() + (proc.presets.isEdited() ? " *" : "");
    };
    presetDisplay.onOpen = [this] { showPresetMenu(); };
    place (presetDisplay, 215.0f, 230.0f, 32.0f);
    addAndMakeVisible (presetDisplay);

    addButton (nextPresetBtn, { 348.0f, kRailY }, 30.0f, 30.0f, "Next preset.");
    nextPresetBtn.onClick = [this] { proc.presets.step (1); };

    addButton (saveBtn, { 400.0f, kRailY }, 66.0f, 30.0f,
               "Save the panel, cables and all, as a user preset.");
    saveBtn.onClick = [this] { savePresetAs(); };

    // Clock
    addToggle (syncBtn, ids::sync, { 490.0f, kRailY }, 70.0f, 30.0f,
               "Follow the host: the sequencer runs only while the transport does, locked to its tempo,\n"
               "and the step length below sets the division. Off, the RATE knob is in charge.");

    stepLenDisplay.setTooltip ("Length of one step while SYNC is on.");
    stepLenDisplay.caption = "STEP";
    stepLenDisplay.getText = [this]
    {
        return ids::syncRateNames()[juce::roundToInt (getParam (ids::syncRate))];
    };
    stepLenDisplay.buildMenu = [this] (juce::PopupMenu& m)
    {
        const int cur = juce::roundToInt (getParam (ids::syncRate));
        for (int i = 0; i < ids::syncRateNames().size(); ++i)
            m.addItem (i + 1, ids::syncRateNames()[i], true, i == cur);
    };
    stepLenDisplay.onChoose = [this] (int r) { setParam (ids::syncRate, (float) (r - 1)); };
    place (stepLenDisplay, 585.0f, 100.0f, 32.0f);
    addAndMakeVisible (stepLenDisplay);

    // MIDI
    midiDisplay.setTooltip ("What incoming notes do, and which channel they are taken from.\n"
                            "Trigger + Pitch plays the voice from a clip: the note sets the pitch and its velocity the hit.");
    midiDisplay.caption = "MIDI";
    midiDisplay.getText = [this]
    {
        const int mode = juce::roundToInt (getParam (ids::midiMode));
        const int ch = juce::roundToInt (getParam (ids::midiChannel));
        return ids::midiModeNames()[mode] + (ch == 0 ? String ("  /  All") : "  /  Ch " + String (ch));
    };
    midiDisplay.buildMenu = [this] (juce::PopupMenu& m)
    {
        const int mode = juce::roundToInt (getParam (ids::midiMode));
        const int ch = juce::roundToInt (getParam (ids::midiChannel));
        m.addSectionHeader ("Notes");
        for (int i = 0; i < ids::midiModeNames().size(); ++i)
            m.addItem (i + 1, ids::midiModeNames()[i], true, i == mode);
        m.addSectionHeader ("Channel");
        m.addItem (100, "All", true, ch == 0);
        for (int i = 1; i <= 16; ++i)
            m.addItem (100 + i, String (i), true, ch == i);
    };
    midiDisplay.onChoose = [this] (int r)
    {
        if (r >= 100)
            setParam (ids::midiChannel, (float) (r - 100));
        else
            setParam (ids::midiMode, (float) (r - 1));
    };
    place (midiDisplay, 730.0f, 170.0f, 32.0f);
    addAndMakeVisible (midiDisplay);
    addLed (midiLed, { 828.0f, kRailY }, 16.0f);

    // Extras
    addButton (diceBtn, { 882.0f, kRailY }, 62.0f, 30.0f,
               "Roll new step pitches and velocities. Click for both;\nhold for pitches only, so a rhythm you like survives.");
    diceBtn.onClick = [this] { proc.randomiseSteps (true, true); };
    diceBtn.onLongPress = [this] { proc.randomiseSteps (true, false); };

    addButton (clearBtn, { 990.0f, kRailY }, 118.0f, 30.0f, "Pull every patch cable out.");
    clearBtn.onClick = [this]
    {
        proc.clearPatch();
        proc.presets.markEdited();
        patchbay->refresh();
    };

    addKnob (fineTune, ids::fineTune, { 1082.0f, kRailY }, 38.0f,
             "Fine tune, plus or minus 50 cents. Kept with this instance rather than with the preset.");
    addKnob (drift, ids::drift, { 1150.0f, kRailY }, 38.0f,
             "How much the oscillators wander, the way an analog pair never quite agrees. Zero is perfectly stable.");
}

void StepkinPanel::showPresetMenu()
{
    juce::PopupMenu m;
    const auto entries = proc.presets.list();
    const int current = proc.presets.currentIndex();

    m.addSectionHeader ("Factory");
    bool inUser = false;
    for (int i = 0; i < entries.size(); ++i)
    {
        if (! entries[i].factory && ! inUser)
        {
            m.addSectionHeader ("Yours");
            inUser = true;
        }
        m.addItem (i + 1, entries[i].name, true, i == current);
    }
    if (! inUser)
    {
        m.addSectionHeader ("Yours");
        m.addItem (-1, "(none saved yet)", false, false);
    }

    m.addSeparator();
    m.addItem (9001, "Save as...");
    m.addItem (9002, "Delete this preset", proc.presets.currentIsUser());
    m.addItem (9003, "Reveal preset folder");

    auto safe = juce::Component::SafePointer<StepkinPanel> (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetDisplay).withMinimumWidth (240),
                     [safe] (int r)
                     {
                         if (safe == nullptr || r <= 0)
                             return;
                         if (r == 9001) { safe->savePresetAs(); return; }
                         if (r == 9002) { safe->confirmDelete(); return; }
                         if (r == 9003)
                         {
                             auto folder = PresetManager::presetFolder();
                             folder.createDirectory();
                             folder.revealToUser();
                             return;
                         }
                         safe->proc.presets.loadIndex (r - 1);
                         safe->patchbay->refresh();
                     });
}

void StepkinPanel::savePresetAs()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name this patch.", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.presets.currentName(), "Name");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    auto safe = juce::Component::SafePointer<StepkinPanel> (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        const auto name = w->getTextEditorContents ("name");
        std::unique_ptr<juce::AlertWindow> owned (w);
        if (safe == nullptr || result == 0)
            return;
        if (safe->proc.presets.saveAs (name))
            safe->statusText = "Saved";
        else
            safe->statusText = "Could not save";
        safe->presetDisplay.refresh();
    }), false);
}

void StepkinPanel::confirmDelete()
{
    const auto name = proc.presets.currentName();
    auto safe = juce::Component::SafePointer<StepkinPanel> (this);
    juce::NativeMessageBox::showYesNoBox (juce::MessageBoxIconType::QuestionIcon,
        "Delete preset", "Move \"" + name + "\" to the trash?", this,
        juce::ModalCallbackFunction::create ([safe, name] (int result)
        {
            if (safe == nullptr || result == 0)
                return;
            for (auto& e : safe->proc.presets.list())
                if (! e.factory && e.name == name)
                {
                    safe->proc.presets.remove (e);
                    break;
                }
            safe->presetDisplay.refresh();
        }));
}

// ---------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------
void StepkinPanel::timerCallback()
{
    ++blinkCounter;
    const bool blinkPhase = (blinkCounter / 8) % 2 == 0;

    const int step = proc.uiStep.load();
    const bool running = proc.uiRunning.load();
    const float amp = proc.uiVcaEnv.load();

    for (int i = 0; i < kNumSteps; ++i)
    {
        const bool here = i == step;
        stepLeds[i].setLevel (here ? (running ? juce::jmax (0.35f, amp) : 0.5f) : 0.0f);
    }

    runStopBtn.setLit (proc.uiPlayOn.load());
    runStopBtn.setBlink (proc.uiPlayOn.load() && ! running);
    runStopBtn.setBlinkPhase (blinkPhase);
    runLed.setLevel (running ? 1.0f : 0.0f);

    const uint32_t steps = proc.uiStepCount.load();
    triggerBtn.setFlash (steps != lastStepCount ? 1.0f : juce::jmax (0.0f, amp));
    lastStepCount = steps;

    envLeds[0].setLevel (proc.uiVcoEnv.load());
    envLeds[1].setLevel (proc.uiVcfEnv.load());
    envLeds[2].setLevel (proc.uiVcaEnv.load());

    const uint32_t midi = proc.uiMidi.load();
    if (midi != lastMidi) { midiFlash = 1.0f; lastMidi = midi; }
    midiFlash *= 0.82f;
    midiLed.setLevel (midiFlash);

    // The rate knob is only in charge when sync is off.
    rate.setAlpha (getParam (ids::sync) > 0.5f ? 0.45f : 1.0f);
    stepLenDisplay.setAlpha (getParam (ids::sync) > 0.5f ? 1.0f : 0.45f);
    syncBtn.setLit (getParam (ids::sync) > 0.5f);

    if (const int v = proc.patchVersion.load(); v != lastPatchVersion)
    {
        lastPatchVersion = v;
        patchbay->refresh();
    }

    presetDisplay.refresh();
    stepLenDisplay.refresh();
    midiDisplay.refresh();

    // Status: host tempo, or the free rate as a tempo.
    String st;
    if (getParam (ids::sync) > 0.5f)
        st = String (proc.uiHostBpm.load(), 1) + " BPM";
    else
    {
        const float hz = getParam (ids::rate);
        st = hz < 100.0f ? String (hz, 2) + " Hz" : String (juce::roundToInt (hz)) + " Hz";
    }
    st << "   " << ids::noteNameForHz (proc.uiVcoHz[0].load());
    if (st != statusText)
    {
        statusText = st;
        repaint (Rectangle<int> (1170, (int) kRailTop, 320, designH - (int) kRailTop));
    }

    // Keep the oscillator readouts fresh.
    repaint (Rectangle<int> (360, 214, 130, 24));
    repaint (Rectangle<int> (360, 396, 130, 24));
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------
void StepkinPanel::paintWood (juce::Graphics& g, Rectangle<float> r) const
{
    juce::ColourGradient grad (colours::wood.brighter (0.12f), r.getX(), r.getY(),
                               colours::woodDark, r.getRight(), r.getY(), false);
    g.setGradientFill (grad);
    g.fillRect (r);
    juce::Random rnd (1234 + (int) r.getX());
    for (int i = 0; i < 26; ++i)
    {
        const float x = r.getX() + rnd.nextFloat() * r.getWidth();
        juce::Path grain;
        grain.startNewSubPath (x, r.getY());
        for (float y = r.getY(); y < r.getBottom(); y += 40.0f)
            grain.lineTo (x + std::sin (y * 0.013f + (float) i) * 2.5f, y + 40.0f);
        g.setColour (colours::woodDark.withAlpha (0.25f + rnd.nextFloat() * 0.3f));
        g.strokePath (grain, juce::PathStrokeType (0.6f + rnd.nextFloat()));
    }
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRect (r, 1.0f);
}

void StepkinPanel::paintSections (juce::Graphics& g) const
{
    auto box = [&] (float x0, float y0, float x1, float y1)
    {
        g.setColour (colours::ink.withAlpha (0.9f));
        g.drawRoundedRectangle ({ x0, y0, x1 - x0, y1 - y0 }, 3.0f, 1.6f);
    };

    box (60.0f, 80.0f, 640.0f, 424.0f);     // oscillators
    box (648.0f, 80.0f, 1062.0f, 424.0f);   // filter
    box (1070.0f, 80.0f, 1252.0f, 424.0f);  // amplifier
    box (60.0f, 432.0f, 1252.0f, 712.0f);   // sequencer
    box (1330.0f, 80.0f, 1660.0f, 712.0f);  // patchbay
}

void StepkinPanel::paintLabels (juce::Graphics& g) const
{
    auto label = [&] (const char* t, float x, float y, float h = 12.0f)
    {
        drawText (g, t, { x, y }, h);
    };
    auto small = [&] (const char* t, float x, float y)
    {
        drawText (g, t, { x, y }, 9.0f, colours::inkDim);
    };

    // Section headings
    drawInverseText (g, "OSCILLATORS", { 150.0f, 80.0f }, 11.0f);
    drawInverseText (g, "FILTER", { 720.0f, 80.0f }, 11.0f);
    drawInverseText (g, "AMPLIFIER", { 1140.0f, 80.0f }, 11.0f);
    drawInverseText (g, "8-STEP SEQUENCER", { 190.0f, 432.0f }, 11.0f);
    drawInverseText (g, "PATCHBAY", { 1420.0f, 80.0f }, 11.0f);

    // Row 1
    label ("PITCH DECAY", kColDecay, 102.0f);
    label ("STEP PITCH TO", kColModSw, 102.0f, 10.5f);
    label ("OSC 1 EG AMOUNT", kColEgAmt, 102.0f, 11.0f);
    label ("OSC 1 FREQUENCY", kColFreq, 90.0f, 11.0f);
    label ("OSC 1 WAVE", kColWave, 102.0f, 10.0f);
    label ("OSC 1 LEVEL", kColLevel, 118.0f, 9.5f);
    label ("FILTER", kColFiltSw, 102.0f, 10.5f);
    label ("CUTOFF", kColCutoff, 100.0f);
    label ("RESONANCE", kColRes, 100.0f);
    label ("ENVELOPES", kColMod, 102.0f, 9.5f);
    label ("AMP EG", kColAmpSw, 102.0f, 10.0f);
    label ("VOLUME", kColVol, 100.0f);

    // Row 2
    label ("1>2 FM AMOUNT", kColDecay, 284.0f, 11.0f);
    label ("HARD SYNC", kColModSw, 284.0f, 10.5f);
    label ("OSC 2 EG AMOUNT", kColEgAmt, 284.0f, 11.0f);
    label ("OSC 2 FREQUENCY", kColFreq, 272.0f, 11.0f);
    label ("OSC 2 WAVE", kColWave, 284.0f, 10.0f);
    label ("OSC 2 LEVEL", kColLevel, 328.0f, 9.5f);
    label ("NOISE / EXT LEVEL", kColLevel, 224.0f, 9.0f);
    label ("FILTER DECAY", kColCutoff, 282.0f);
    label ("FILTER EG AMOUNT", kColRes, 282.0f, 11.0f);
    label ("NOISE / FILTER MOD", kColMod, 282.0f, 10.5f);
    label ("AMP DECAY", kColVol, 282.0f);

    // Bipolar knob ends
    for (float x : { kColEgAmt, kColRes })
    {
        small ("(-)", x - 36.0f, 218.0f);
        small ("(+)", x + 36.0f, 218.0f);
    }
    small ("(-)", kColEgAmt - 36.0f, 400.0f);
    small ("(+)", kColEgAmt + 36.0f, 400.0f);
    small ("(-)", kColRes - 36.0f, 400.0f);
    small ("(+)", kColRes + 36.0f, 400.0f);

    // Switch position labels. The wave switch gets drawn waveforms rather than
    // words, which is both tidier and quicker to read.
    small ("1 & 2", kColModSw + 28.0f, kRow1 - 22.0f);
    small ("OFF",   kColModSw + 28.0f, kRow1);
    small ("2",     kColModSw + 28.0f, kRow1 + 22.0f);
    small ("ON",  kColModSw + 26.0f, kRow2 - 20.0f);
    small ("OFF", kColModSw + 26.0f, kRow2 + 20.0f);

    auto pulseGlyph = [&] (float cx, float cy)
    {
        const float w = 7.0f, h = 5.0f;
        juce::Path p;
        p.startNewSubPath (cx - w, cy + h);
        p.lineTo (cx - w, cy - h);
        p.lineTo (cx, cy - h);
        p.lineTo (cx, cy + h);
        p.lineTo (cx + w, cy + h);
        p.lineTo (cx + w, cy - h);
        g.setColour (colours::ink.withAlpha (0.85f));
        g.strokePath (p, juce::PathStrokeType (1.4f));
    };
    auto triGlyph = [&] (float cx, float cy)
    {
        const float w = 8.0f, h = 5.0f;
        juce::Path p;
        p.startNewSubPath (cx - w, cy + h);
        p.lineTo (cx - w * 0.33f, cy - h);
        p.lineTo (cx + w * 0.33f, cy + h);
        p.lineTo (cx + w, cy - h);
        g.setColour (colours::ink.withAlpha (0.85f));
        g.strokePath (p, juce::PathStrokeType (1.4f));
    };
    for (float y : { kRow1, kRow2 })
    {
        pulseGlyph (kColWave + 24.0f, y - 20.0f);
        triGlyph   (kColWave + 24.0f, y + 20.0f);
    }

    small ("HP", kColFiltSw + 22.0f, kRow1 - 20.0f);
    small ("LP", kColFiltSw + 22.0f, kRow1 + 20.0f);
    small ("FAST", kColAmpSw + 24.0f, kRow1 - 20.0f);
    small ("SLOW", kColAmpSw + 24.0f, kRow1 + 20.0f);

    // Cutoff scale
    small ("20 Hz", kColCutoff - 48.0f, kRow1 + 50.0f);
    small ("20 kHz", kColCutoff + 48.0f, kRow1 + 50.0f);

    // Envelope indicator labels
    small ("PITCH", kColMod - 44.0f, 126.0f);
    small ("FILTER", kColMod - 44.0f, 158.0f);
    small ("AMP", kColMod - 44.0f, 190.0f);

    // Oscillator pitch readouts
    g.setColour (colours::ledAmber.withAlpha (0.9f));
    g.setFont (panelFont (12.5f));
    for (int v = 0; v < 2; ++v)
    {
        const auto r = Rectangle<float> (130.0f, 20.0f).withCentre ({ kColFreq, v == 0 ? 226.0f : 408.0f });
        g.drawText (ids::noteNameForHz (proc.uiVcoHz[v].load()), r, juce::Justification::centred, false);
    }

    // Sequencer
    label ("TRIGGER", 112.0f, 508.0f, 10.5f);
    label ("RATE", 238.0f, 484.0f);
    label ("RUN / STOP", 206.0f, 694.0f, 10.0f);
    label ("ADVANCE", 298.0f, 694.0f, 10.0f);
    drawText (g, "PITCH", { 452.0f, kPitchY }, 10.5f);
    drawText (g, "VELOCITY", { 452.0f, kVelY }, 10.5f);
    for (int i = 0; i < kNumSteps; ++i)
        drawText (g, String (i + 1), { stepX (i), kStepNumY }, 13.0f);

    // Patchbay column captions
    drawText (g, "IN", { kBayX[0], 112.0f }, 10.0f, colours::inkDim);
    drawText (g, "IN", { kBayX[1], 112.0f }, 10.0f, colours::inkDim);
    drawInverseText (g, "OUT", { kBayX[2], 112.0f }, 10.0f);
}

void StepkinPanel::paintRail (juce::Graphics& g) const
{
    const Rectangle<float> rail (32.0f, kRailTop, (float) designW - 64.0f, (float) designH - kRailTop - 6.0f);
    g.setColour (colours::rail);
    g.fillRoundedRectangle (rail, 5.0f);
    g.setColour (colours::ink.withAlpha (0.18f));
    g.drawRoundedRectangle (rail, 5.0f, 1.0f);

    auto cap = [&] (const char* t, float x) { drawText (g, t, { x, kRailCapY }, 8.5f, colours::inkDim); };
    cap ("PRESET", 215.0f);
    cap ("CLOCK", 537.0f);
    cap ("MIDI", 730.0f);
    cap ("STEPS", 882.0f);
    cap ("CABLES", 990.0f);
    cap ("FINE", 1082.0f);
    cap ("DRIFT", 1150.0f);

    g.setColour (colours::ledAmber.withAlpha (0.85f));
    g.setFont (panelFont (13.0f));
    g.drawText (statusText, Rectangle<float> (1180.0f, kRailY - 16.0f, 180.0f, 32.0f),
                juce::Justification::centredLeft, false);

    // Wordmark
    g.setColour (colours::ink.withAlpha (0.92f));
    g.setFont (panelFont (19.0f));
    g.drawText ("STEPKIN", Rectangle<float> (1400.0f, kRailY - 18.0f, 120.0f, 36.0f),
                juce::Justification::centredLeft, false);
    g.setColour (colours::inkDim);
    g.setFont (panelFont (8.5f, false));
    g.drawFittedText ("SEMI-MODULAR\nPERCUSSION SYNTHESISER",
                      Rectangle<float> (1524.0f, kRailY - 18.0f, 150.0f, 36.0f).toNearestInt(),
                      juce::Justification::centredLeft, 2);
}

void StepkinPanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    paintWood (g, { 0.0f, 0.0f, 32.0f, kRailTop });
    paintWood (g, { (float) designW - 32.0f, 0.0f, 32.0f, kRailTop });
    paintSections (g);
    paintLabels (g);
    paintRail (g);
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------
StepkinEditor::StepkinEditor (StepkinProcessor& p)
    : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    const double ratio = (double) StepkinPanel::designW / StepkinPanel::designH;
    setResizeLimits (980, (int) (980 / ratio), 2600, (int) (2600 / ratio));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (ratio);
    setSize (1340, (int) (1340 / ratio));
}

void StepkinEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
}

void StepkinEditor::resized()
{
    const float s = (float) getWidth() / (float) StepkinPanel::designW;
    panel.setTransform (juce::AffineTransform::scale (s));
    panel.setBounds (0, 0, StepkinPanel::designW, StepkinPanel::designH);
}
