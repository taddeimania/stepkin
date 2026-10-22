#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace stepkin;

namespace
{
const juce::Identifier kPatchTag   { "Patch" };
const juce::Identifier kCableTag   { "Cable" };
const juce::Identifier kOutAttr    { "out" };
const juce::Identifier kInAttr     { "in" };
const juce::Identifier kPresetAttr { "presetName" };
const juce::Identifier kPlayAttr   { "playOn" };
const juce::Identifier kVersion    { "stateVersion" };

// Settings that belong to this instance in this session, not to a sound.
const juce::StringArray& instanceParams()
{
    static const juce::StringArray ids {
        stepkin::ids::sync, stepkin::ids::midiChannel, stepkin::ids::midiMode, stepkin::ids::fineTune };
    return ids;
}
} // namespace

StepkinProcessor::StepkinProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "StepkinState", createParameterLayout())
{
    for (auto* p : getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            params[withId->paramID] = apvts.getRawParameterValue (withId->paramID);

    for (auto& s : patchSrc)
        s.store (-1);

    // Any panel change marks the loaded preset as edited (shown as "*").
    for (auto& kv : params)
        if (! instanceParams().contains (kv.first))
            apvts.addParameterListener (kv.first, this);
}

StepkinProcessor::~StepkinProcessor()
{
    for (auto& kv : params)
        apvts.removeParameterListener (kv.first, this);
}

void StepkinProcessor::parameterChanged (const juce::String&, float)
{
    presets.markEdited();
}

float StepkinProcessor::get (const juce::String& id) const
{
    auto it = params.find (id);
    jassert (it != params.end());
    return it != params.end() ? it->second->load() : 0.0f;
}

bool StepkinProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void StepkinProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (readParams());
    engine.prepare (sampleRate);
    engine.setPlayOn (playOnRestore);
    scratch.assign ((size_t) juce::jmax (512, samplesPerBlock), 0.0f);
}

EngineParams StepkinProcessor::readParams() const
{
    EngineParams p;

    for (int i = 0; i < kNumSteps; ++i)
    {
        p.pitch[(size_t) i]    = get (ids::stepPitch (i));
        p.velocity[(size_t) i] = get (ids::stepVelocity (i));
    }
    p.pitchMod = (PitchMod) juce::jlimit (0, 2, juce::roundToInt (get (ids::pitchMod)));

    for (int v = 0; v < 2; ++v)
    {
        p.vcoFreq[(size_t) v]  = get (ids::vcoFreq (v));
        p.wave[(size_t) v]     = (Wave) juce::jlimit (0, 1, juce::roundToInt (get (ids::vcoWave (v))));
        p.vcoEgAmt[(size_t) v] = get (ids::vcoEgAmt (v));
        p.vcoLevel[(size_t) v] = get (ids::vcoLevel (v));
    }
    p.hardSync   = get (ids::hardSync) > 0.5f;
    p.fmAmount   = get (ids::fmAmount);
    p.noiseLevel = get (ids::noiseLevel);

    p.filterMode  = (FilterMode) juce::jlimit (0, 1, juce::roundToInt (get (ids::filterMode)));
    p.cutoffHz    = get (ids::cutoff);
    p.resonance   = get (ids::resonance);
    p.vcfEgAmt    = get (ids::vcfEgAmt);
    p.noiseVcfMod = get (ids::noiseVcfMod);

    p.vcoDecay = get (ids::vcoDecay);
    p.vcfDecay = get (ids::vcfDecay);
    p.vcaDecay = get (ids::vcaDecay);
    p.vcaSlowAttack = juce::roundToInt (get (ids::vcaAttack)) == 1;

    p.volume = get (ids::volume);

    p.rateHz = get (ids::rate);
    p.sync = get (ids::sync) > 0.5f;
    p.syncStepBeats = ids::syncRateBeats (juce::roundToInt (get (ids::syncRate)));

    p.midiMode  = (MidiMode) juce::jlimit (0, 2, juce::roundToInt (get (ids::midiMode)));
    p.fineCents = get (ids::fineTune);
    p.drift     = get (ids::drift);
    return p;
}

void StepkinProcessor::handleMidi (const juce::MidiMessage& m)
{
    const int ch = juce::roundToInt (get (ids::midiChannel));
    if (ch != 0 && m.getChannel() != ch)
        return;

    if (m.isNoteOn())
    {
        engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        uiMidi.fetch_add (1);
    }
    else if (m.isNoteOff())
    {
        engine.noteOff (m.getNoteNumber());
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        engine.allNotesOff();
    }
}

void StepkinProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = audio.getNumSamples();
    audio.clear();
    if (numSamples == 0)
        return;
    if ((int) scratch.size() < numSamples)
        scratch.resize ((size_t) numSamples); // only if the host exceeds the announced block size

    engine.setParams (readParams());
    engine.setPatch (currentPatch());

    // --- Host transport ------------------------------------------------------
    HostInfo host;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            host.valid   = true;
            host.playing = pos->getIsPlaying();
            host.bpm     = pos->getBpm().orFallback (120.0);
            host.ppq     = pos->getPpqPosition().orFallback (0.0);
        }
    }
    engine.beginBlock (host, numSamples);

    // --- Panel actions -------------------------------------------------------
    if (pendingPlayState.exchange (false))
        engine.setPlayOn (pendingPlayValue.load());
    if (const int t = playToggles.load(); t != seenPlayToggles)
    {
        if (((t - seenPlayToggles) & 1) != 0)
            engine.togglePlay();
        seenPlayToggles = t;
    }
    if (const int a = advancePresses.load(); a != seenAdvancePresses)
    {
        for (int i = seenAdvancePresses; i != a; ++i)
            engine.pressAdvance();
        seenAdvancePresses = a;
    }
    if (const int t = triggerPresses.load(); t != seenTriggerPresses)
    {
        for (int i = seenTriggerPresses; i != t; ++i)
            engine.pressTrigger();
        seenTriggerPresses = t;
    }

    // --- Render, split at MIDI events ----------------------------------------
    float* out = scratch.data();
    int cursor = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        if (at > cursor)
        {
            engine.render (out + cursor, at - cursor);
            cursor = at;
        }
        handleMidi (meta.getMessage());
    }
    if (cursor < numSamples)
        engine.render (out + cursor, numSamples - cursor);

    for (int c = 0; c < audio.getNumChannels(); ++c)
        audio.copyFrom (c, 0, out, numSamples);

    // --- Readouts ------------------------------------------------------------
    uiStep.store (engine.currentStep());
    uiPlayOn.store (engine.isPlayOn());
    uiRunning.store (engine.isRunning());
    uiHostPlaying.store (host.valid && host.playing);
    uiHostBpm.store (host.bpm);
    uiStepCount.store (engine.stepCount());
    uiVcoEnv.store ((float) engine.vcoEnvLevel());
    uiVcfEnv.store ((float) engine.vcfEnvLevel());
    uiVcaEnv.store ((float) engine.vcaEnvLevel());
    for (int v = 0; v < 2; ++v)
        uiVcoHz[v].store ((float) engine.currentVcoHz (v));
}

// ---------------------------------------------------------------------------
// Patchbay
// ---------------------------------------------------------------------------
void StepkinProcessor::connect (int output, int input)
{
    if (input < 0 || input >= kNumInputs || output < 0 || output >= kNumOutputs)
        return;
    patchSrc[(size_t) input].store (output);
    patchVersion.fetch_add (1);
}

void StepkinProcessor::disconnect (int input)
{
    if (input < 0 || input >= kNumInputs)
        return;
    patchSrc[(size_t) input].store (-1);
    patchVersion.fetch_add (1);
}

void StepkinProcessor::clearPatch()
{
    for (auto& s : patchSrc)
        s.store (-1);
    patchVersion.fetch_add (1);
}

Patch StepkinProcessor::currentPatch() const noexcept
{
    Patch p;
    for (int i = 0; i < kNumInputs; ++i)
        p[(size_t) i] = patchSrc[(size_t) i].load();
    return p;
}

void StepkinProcessor::setPatch (const Patch& p)
{
    for (int i = 0; i < kNumInputs; ++i)
        patchSrc[(size_t) i].store (p[(size_t) i]);
    patchVersion.fetch_add (1);
}

// ---------------------------------------------------------------------------
// Dice
// ---------------------------------------------------------------------------
void StepkinProcessor::randomiseSteps (bool pitch, bool velocity)
{
    for (int i = 0; i < kNumSteps; ++i)
    {
        if (pitch)
            if (auto* p = apvts.getParameter (ids::stepPitch (i)))
            {
                // Small intervals most of the time, with the occasional leap.
                const float r = random.nextFloat() * 2.0f - 1.0f;
                p->setValueNotifyingHost (p->convertTo0to1 (r * r * r * 0.6f));
            }
        if (velocity)
            if (auto* p = apvts.getParameter (ids::stepVelocity (i)))
            {
                // Roughly a third of steps land silent, the rest spread wide.
                const float r = random.nextFloat();
                const float v = r < 0.3f ? 0.0f : 0.35f + random.nextFloat() * 0.65f;
                p->setValueNotifyingHost (p->convertTo0to1 (v));
            }
    }
    presets.markEdited();
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
juce::ValueTree StepkinProcessor::captureState()
{
    auto state = apvts.copyState();
    state.setProperty (kVersion, 1, nullptr);
    state.setProperty (kPresetAttr, presets.currentName(), nullptr);
    state.setProperty (kPlayAttr, uiPlayOn.load(), nullptr);

    if (auto old = state.getChildWithName (kPatchTag); old.isValid())
        state.removeChild (old, nullptr);

    juce::ValueTree patch (kPatchTag);
    for (int i = 0; i < kNumInputs; ++i)
    {
        const int src = patchSrc[(size_t) i].load();
        if (src >= 0)
        {
            juce::ValueTree cable (kCableTag);
            cable.setProperty (kOutAttr, src, nullptr);
            cable.setProperty (kInAttr, i, nullptr);
            patch.appendChild (cable, nullptr);
        }
    }
    state.appendChild (patch, nullptr);
    return state;
}

void StepkinProcessor::restoreState (const juce::ValueTree& stateIn, bool asPreset)
{
    if (! stateIn.hasType (apvts.state.getType()))
        return;

    auto state = stateIn.createCopy();

    // Patch cables
    Patch patch = emptyPatch();
    if (auto pt = state.getChildWithName (kPatchTag); pt.isValid())
    {
        for (auto cable : pt)
        {
            const int o = cable.getProperty (kOutAttr, -1);
            const int i = cable.getProperty (kInAttr, -1);
            if (i >= 0 && i < kNumInputs && o >= 0 && o < kNumOutputs)
                patch[(size_t) i] = o;
        }
        state.removeChild (pt, nullptr);
    }

    if (asPreset)
    {
        // Keep this instance's own settings.
        for (auto& id : instanceParams())
            for (auto child : state)
                if (child.getProperty ("id").toString() == id)
                    if (auto* param = apvts.getParameter (id))
                        child.setProperty ("value", param->convertFrom0to1 (param->getValue()), nullptr);
    }
    else if (state.hasProperty (kPlayAttr))
    {
        playOnRestore = (bool) state[kPlayAttr];
        pendingPlayValue.store (playOnRestore);
        pendingPlayState.store (true);
    }

    apvts.replaceState (state);
    setPatch (patch);
}

void StepkinProcessor::applyFactoryPreset (const Preset& preset)
{
    // Back to defaults, keeping per-instance settings.
    for (auto* p : getParameters())
        if (auto* withId = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! instanceParams().contains (withId->paramID))
                withId->setValueNotifyingHost (withId->getDefaultValue());

    for (const auto& [id, value] : preset.values)
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));

    Patch patch = emptyPatch();
    for (const auto& [o, i] : preset.cables)
        if (i >= 0 && i < kNumInputs)
            patch[(size_t) i] = o;
    setPatch (patch);
}

void StepkinProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = captureState().createXml())
        copyXmlToBinary (*xml, destData);
}

void StepkinProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;
    const auto state = juce::ValueTree::fromXml (*xml);
    restoreState (state, false);
    presets.restoreName (state.getProperty (kPresetAttr, "Init").toString());
}

// ---------------------------------------------------------------------------
// Programs (the host's preset list mirrors the panel's)
// ---------------------------------------------------------------------------
// Only the factory presets are exposed as host programs, and the count never
// changes. Hosts latch the program count when they load the plugin, so a list
// that grew as user presets were saved left the host clamping to its stale
// maximum and writing that index straight back, which loaded the last factory
// preset over whatever had just been chosen or saved.
int StepkinProcessor::getNumPrograms() { return (int) factoryPresets().size(); }

int StepkinProcessor::getCurrentProgram()
{
    // Track the panel while it is on a factory preset; hold the last one while
    // it is on a user preset, since those have no program of their own.
    const int index = presets.currentIndex();
    if (index >= 0 && index < getNumPrograms() && ! presets.currentIsUser())
        lastProgram = index;
    return lastProgram;
}

void StepkinProcessor::setCurrentProgram (int i)
{
    i = juce::jlimit (0, getNumPrograms() - 1, i);

    // While a user preset is loaded we report lastProgram as a placeholder, so
    // the host echoing that same index back is not a request to change program.
    if (i == lastProgram && presets.currentIsUser())
        return;

    lastProgram = i;
    presets.loadIndex (i);   // factory presets occupy the first entries of list()
}

const juce::String StepkinProcessor::getProgramName (int index)
{
    const auto& factory = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) factory.size())
         ? factory[(size_t) index].name : juce::String();
}

juce::AudioProcessorEditor* StepkinProcessor::createEditor()
{
    return new StepkinEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StepkinProcessor();
}
