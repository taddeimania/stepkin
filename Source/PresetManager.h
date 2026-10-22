// PresetManager.h — factory presets plus user presets saved as files.
//
// User presets live in   <user app data>/Stepkin/Presets/*.stepkin
//   macOS:   ~/Library/Application Support/Stepkin/Presets
//   Windows: %APPDATA%\Stepkin\Presets
//   Linux:   ~/.config/Stepkin/Presets
// Each file is the same XML the plugin stores in a session, so a preset
// carries every knob, switch and patch cable.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Presets.h"

class StepkinProcessor;

namespace stepkin
{
class PresetManager
{
public:
    struct Entry
    {
        juce::String name;
        bool factory = true;
        int factoryIndex = -1;
        juce::File file;
    };

    explicit PresetManager (StepkinProcessor& p) : proc (p) {}

    static juce::File presetFolder();
    static constexpr const char* extension = ".stepkin";

    // Factory entries first, then user presets sorted by name.
    juce::Array<Entry> list() const;

    bool load (const Entry& e);
    bool loadIndex (int index);              // index into list()
    void step (int delta);                   // previous / next

    // Save the current panel as a user preset. Overwrites a user preset with the same name.
    bool saveAs (const juce::String& name);
    bool remove (const Entry& e);

    juce::String currentName() const { return current; }
    void restoreName (const juce::String& name);   // after the host restores a session
    void markEdited() { if (juce::Time::getMillisecondCounter() > ignoreEditsUntil.load()) edited = true; }
    bool isEdited() const { return edited.load(); }
    bool currentIsUser() const;
    int  currentIndex() const;

    static juce::String sanitise (const juce::String& name);

private:
    StepkinProcessor& proc;
    juce::String current { "Init" };
    bool currentUser = false;
    std::atomic<bool> edited { false };
    std::atomic<juce::uint32> ignoreEditsUntil { 0 };
    void settle() { edited = false; ignoreEditsUntil = juce::Time::getMillisecondCounter() + 300; }
};
} // namespace stepkin
