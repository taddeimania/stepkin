// Snapshot.cpp — dev tool.
//   StepkinSnapshot panel.png [presetIndex] [scale]     render the panel
//   StepkinSnapshot sound.wav [presetIndex] [seconds]   render audio at 120 BPM (host playing)
//   StepkinSnapshot --preset-test                       round-trip presets, cables and state
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
struct FakeHost final : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setIsPlaying (true);
        info.setBpm (bpm);
        info.setPpqPosition (ppq);
        return info;
    }
};
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    if (argc > 1 && juce::String (argv[1]) == "--menu-probe")
    {
        using namespace stepkin;
        auto p = std::make_unique<StepkinProcessor>();
        const int factoryCount = (int) factoryPresets().size();
        const int programsAtStart = p->getNumPrograms();
        std::printf ("factory presets: %d   getNumPrograms() at construction: %d\n", factoryCount, programsAtStart);

        p->presets.saveAs ("ZZ Menu Probe");
        auto entries = p->presets.list();
        int userIdx = -1;
        for (int i = 0; i < entries.size(); ++i)
            if (! entries[i].factory)
                userIdx = i;
        std::printf ("after save: list size %d, user preset at index %d, currentIndex() %d, currentIsUser %d\n",
                     entries.size(), userIdx, p->presets.currentIndex(), (int) p->presets.currentIsUser());
        std::printf ("getNumPrograms() now: %d   getCurrentProgram(): %d\n",
                     p->getNumPrograms(), p->getCurrentProgram());

        p->presets.loadIndex (0);
        p->presets.loadIndex (userIdx);
        std::printf ("menu click loadIndex(%d) -> \"%s\", currentIndex() %d\n",
                     userIdx, p->presets.currentName().toRawUTF8(), p->presets.currentIndex());

        const int clamped = juce::jlimit (0, programsAtStart - 1, p->getCurrentProgram());
        std::printf ("a host holding the old program count would clamp %d -> %d\n",
                     p->getCurrentProgram(), clamped);
        p->setCurrentProgram (clamped);
        std::printf ("after that write-back: \"%s\"\n", p->presets.currentName().toRawUTF8());

        std::printf ("\nmidi mode, as the editor reads it:\n");
        for (int want = 0; want < 3; ++want)
        {
            auto* mp = p->apvts.getParameter (ids::midiMode);
            mp->setValueNotifyingHost (mp->convertTo0to1 ((float) want));
            const float editorRead = mp->convertFrom0to1 (mp->getValue());
            std::printf ("  set %d -> apvts raw %.4f, editor getParam %.4f, roundToInt %d  %s\n",
                         want, p->get (ids::midiMode), editorRead, juce::roundToInt (editorRead),
                         juce::roundToInt (editorRead) == want ? "" : "  <-- MISMATCH");
        }

        std::printf ("\nmidi menu tick flags, built the way the editor builds them:\n");
        for (int want = 0; want < 3; ++want)
        {
            auto* mp = p->apvts.getParameter (ids::midiMode);
            mp->setValueNotifyingHost (mp->convertTo0to1 ((float) want));

            const int mode = juce::roundToInt (mp->convertFrom0to1 (mp->getValue()));
            juce::PopupMenu m;
            m.addSectionHeader ("Notes");
            for (int i = 0; i < ids::midiModeNames().size(); ++i)
                m.addItem (i + 1, ids::midiModeNames()[i], true, i == mode);

            juce::String ticked = "(none)";
            juce::PopupMenu::MenuItemIterator it (m);
            while (it.next())
                if (it.getItem().isTicked)
                    ticked = it.getItem().text;
            std::printf ("  mode %d (%s) -> menu ticks \"%s\"  %s\n", want,
                         ids::midiModeNames()[want].toRawUTF8(), ticked.toRawUTF8(),
                         ticked == ids::midiModeNames()[want] ? "" : "  <-- WRONG");
        }

        for (auto& e : p->presets.list())
            if (! e.factory && e.name == "ZZ Menu Probe")
                p->presets.remove (e);
        return 0;
    }

    if (argc > 1 && juce::String (argv[1]) == "--preset-test")
    {
        int fails = 0;
        auto expect = [&] (bool ok, const char* what)
        {
            std::printf ("  %s %s\n", ok ? "ok  " : "FAIL", what);
            if (! ok) ++fails;
        };
        using namespace stepkin;

        auto p = std::make_unique<StepkinProcessor>();

        // "Quick Hats" is index 2 and patches PITCH -> AMP DECAY.
        p->setCurrentProgram (2);
        const float cutoff = p->get (ids::cutoff);
        expect (p->sourceFor (inVcaDecay) == outPitch, "factory preset patches its cables");
        expect (p->presets.saveAs ("Test Patch"), "save user preset");
        expect (PresetManager::presetFolder().getChildFile ("Test Patch.stepkin").existsAsFile(),
                "file written to presets folder");

        p->setCurrentProgram (0);
        expect (p->sourceFor (inVcaDecay) == -1, "Init clears the cables");

        bool loaded = false;
        for (auto& e : p->presets.list())
            if (! e.factory && e.name == "Test Patch")
                loaded = p->presets.load (e);
        expect (loaded, "user preset listed and loaded");
        expect (p->sourceFor (inVcaDecay) == outPitch, "user preset restores cables");
        expect (std::abs (p->get (ids::cutoff) - cutoff) < 0.01f, "user preset restores knobs");
        expect (p->presets.currentIsUser() && p->presets.currentName() == "Test Patch",
                "current preset is the user preset");

        // Instance settings survive a preset change.
        if (auto* ch = p->apvts.getParameter (ids::midiChannel))
            ch->setValueNotifyingHost (ch->convertTo0to1 (5.0f));
        p->setCurrentProgram (1);
        expect (juce::roundToInt (p->get (ids::midiChannel)) == 5, "preset load keeps MIDI channel");

        // Every factory preset loads, and the ones with cables get them.
        int withCables = 0;
        for (int i = 0; i < (int) factoryPresets().size(); ++i)
        {
            p->setCurrentProgram (i);
            for (int in = 0; in < kNumInputs; ++in)
                if (p->sourceFor (in) >= 0)
                    { ++withCables; break; }
        }
        expect (withCables >= 4, "several factory presets ship with cables");

        // Host session round trip.
        p->setCurrentProgram (10); // Vowel Drum: OSC 2 -> FILTER MOD
        juce::MemoryBlock mb;
        p->getStateInformation (mb);
        auto q = std::make_unique<StepkinProcessor>();
        q->setStateInformation (mb.getData(), (int) mb.getSize());
        expect (q->sourceFor (inVcfMod) == outVco2, "session restore keeps cables");
        expect (q->presets.currentName() == p->presets.currentName(), "session restore keeps preset name");
        expect (std::abs (q->get (ids::resonance) - p->get (ids::resonance)) < 1e-4f, "session restore keeps knobs");
        for (int i = 0; i < kNumSteps; ++i)
            if (std::abs (q->get (ids::stepPitch (i)) - p->get (ids::stepPitch (i))) > 1e-4f)
            {
                expect (false, "session restore keeps every step");
                break;
            }

        // Dice moves the steps and nothing else.
        const float before = p->get (ids::cutoff);
        p->randomiseSteps (true, true);
        expect (std::abs (p->get (ids::cutoff) - before) < 1e-6f, "dice leaves the sound alone");

        // A host latches the program count when it loads the plugin. Saving a
        // user preset used to grow that list, so the host clamped to its stale
        // maximum and wrote the index back, loading the last factory preset
        // over whatever had just been saved or chosen.
        {
            auto h = std::make_unique<StepkinProcessor>();
            const int programsAtLoad = h->getNumPrograms();
            expect (programsAtLoad == (int) factoryPresets().size(),
                    "only the factory presets are exposed as host programs");

            h->presets.saveAs ("zzz Host Clamp");
            expect (h->getNumPrograms() == programsAtLoad, "saving a user preset does not change the program count");

            auto hostEcho = [&h, programsAtLoad]
            {
                h->setCurrentProgram (juce::jlimit (0, programsAtLoad - 1, h->getCurrentProgram()));
            };

            hostEcho();
            expect (h->presets.currentName() == "zzz Host Clamp", "the host echo leaves a just-saved user preset loaded");

            // And the same for choosing one from the panel menu.
            h->presets.loadIndex (0);
            int userIdx = -1;
            const auto all = h->presets.list();
            for (int i = 0; i < all.size(); ++i)
                if (! all[i].factory && all[i].name == "zzz Host Clamp")
                    userIdx = i;
            h->presets.loadIndex (userIdx);
            expect (h->presets.currentName() == "zzz Host Clamp", "the panel menu loads a user preset");
            hostEcho();
            expect (h->presets.currentName() == "zzz Host Clamp", "the host echo leaves a chosen user preset loaded");

            // Factory programs still work from the host's own list.
            h->setCurrentProgram (4);
            expect (h->presets.currentName() == factoryPresets()[4].name, "the host can still select factory programs");
            expect (h->getCurrentProgram() == 4, "the host reads back the factory program it selected");
            expect (h->getProgramName (4) == factoryPresets()[4].name, "program names come from the factory list");

            for (auto& e : h->presets.list())
                if (! e.factory && e.name == "zzz Host Clamp")
                    h->presets.remove (e);
        }

        for (auto& e : p->presets.list())
            if (! e.factory && e.name == "Test Patch")
                expect (p->presets.remove (e), "delete user preset");

        std::printf ("%d failures\n", fails);
        return fails == 0 ? 0 : 1;
    }

    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "panel.png"));
    const int preset = argc > 2 ? juce::String (argv[2]).getIntValue() : 0;
    const double extra = argc > 3 ? juce::String (argv[3]).getDoubleValue() : 0.0;

    auto proc = std::make_unique<StepkinProcessor>();
    proc->setPlayConfigDetails (0, 2, 48000.0, 512);
    proc->prepareToPlay (48000.0, 512);
    proc->setCurrentProgram (preset);

    FakeHost host;
    proc->setPlayHead (&host);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;

    if (out.hasFileExtension ("wav"))
    {
        const double seconds = extra > 0.0 ? extra : 8.0;
        const int blocks = (int) (seconds * 48000.0 / 512.0);
        juce::AudioBuffer<float> all (2, blocks * 512);
        double peak = 0.0, sumSq = 0.0;
        bool finite = true;
        for (int b = 0; b < blocks; ++b)
        {
            proc->processBlock (buf, midi);
            host.ppq += 512.0 * host.bpm / 60.0 / 48000.0;
            for (int c = 0; c < 2; ++c)
                all.copyFrom (c, b * 512, buf, c, 0, 512);
            for (int i = 0; i < 512; ++i)
            {
                const float s = buf.getSample (0, i);
                if (! std::isfinite (s)) finite = false;
                peak = std::max (peak, (double) std::abs (s));
                sumSq += (double) s * s;
            }
        }
        out.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (out), 48000.0, 2, 24, {}, 0));
        const bool ok = w != nullptr && w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
        w.reset();
        std::printf ("%-22s peak %.3f  rms %.4f  %s  step %d  %s\n",
                     proc->getProgramName (preset).toRawUTF8(), peak, std::sqrt (sumSq / (blocks * 512.0)),
                     finite ? "finite" : "NAN!", proc->uiStep.load(), ok ? "" : "WRITE FAILED");
        proc.reset();
        return ok && finite ? 0 : 1;
    }

    for (int i = 0; i < 40; ++i)
        proc->processBlock (buf, midi);

    const float scale = extra > 0.0 ? (float) extra : 1.0f;
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
    ed->setSize (juce::roundToInt (StepkinPanel::designW * scale), juce::roundToInt (StepkinPanel::designH * scale));

    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat png;
    const bool ok = png.writeImageToStream (img, os);
    std::printf ("%s %s (%dx%d)\n", ok ? "wrote" : "FAILED", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());

    ed.reset();
    proc.reset();
    return ok ? 0 : 1;
}
