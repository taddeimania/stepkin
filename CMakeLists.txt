cmake_minimum_required(VERSION 3.22)

# macOS 11+. Builds for the machine you're on by default; for a universal
# binary pass -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64". Set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(Stepkin VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE ---------------------------------------------------------------------
# By default JUCE is downloaded on first configure. To use a local checkout:
#   cmake -B build -DSTEPKIN_JUCE_DIR=/path/to/JUCE
set(STEPKIN_JUCE_DIR "" CACHE PATH "Optional local JUCE checkout")

if(STEPKIN_JUCE_DIR)
    add_subdirectory(${STEPKIN_JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- Plugin -------------------------------------------------------------------
# Copies the built plugin into the system plug-in folders after each build:
#   macOS   ~/Library/Audio/Plug-Ins/VST3 (and /Components for AU)
#   Linux   ~/.vst3
if(WIN32)
    set(_stepkin_copy_default OFF)
else()
    set(_stepkin_copy_default ON)
endif()
option(STEPKIN_COPY_PLUGIN "Install the plugin into the system plug-in folders after building" ${_stepkin_copy_default})

set(_stepkin_formats VST3 Standalone)
if(APPLE)
    list(APPEND _stepkin_formats AU)
endif()

juce_add_plugin(Stepkin
    COMPANY_NAME                "Taddei"
    BUNDLE_ID                   com.taddei.stepkin
    PLUGIN_MANUFACTURER_CODE    Tadd
    PLUGIN_CODE                 Stpk
    PRODUCT_NAME                "Stepkin"
    FORMATS                     ${_stepkin_formats}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Instrument Synth Drum
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD     ${STEPKIN_COPY_PLUGIN})

set(STEPKIN_SOURCES
    Source/Engine/Engine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp
    Source/UI/Widgets.cpp
    Source/UI/Patchbay.cpp)

target_sources(Stepkin PRIVATE ${STEPKIN_SOURCES})
target_include_directories(Stepkin PRIVATE Source)

target_compile_definitions(Stepkin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Stepkin
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Engine tests (no JUCE needed) --------------------------------------------
enable_testing()
add_executable(StepkinEngineTests Tests/EngineTests.cpp Source/Engine/Engine.cpp)
target_include_directories(StepkinEngineTests PRIVATE Source)
add_test(NAME engine COMMAND StepkinEngineTests)

# --- Optional dev tools -------------------------------------------------------
#   StepkinSnapshot  render the panel to a PNG, or a preset to a WAV (headless)
#   StepkinRender    render a handful of patches straight from the engine
option(STEPKIN_BUILD_TOOLS "Build the snapshot and render tools" OFF)
if(STEPKIN_BUILD_TOOLS)
    juce_add_console_app(StepkinSnapshot PRODUCT_NAME "StepkinSnapshot")
    target_sources(StepkinSnapshot PRIVATE Tools/Snapshot.cpp ${STEPKIN_SOURCES})
    target_include_directories(StepkinSnapshot PRIVATE Source)
    target_compile_definitions(StepkinSnapshot PRIVATE
        JucePlugin_Name="Stepkin"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(StepkinSnapshot PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)

    add_executable(StepkinRender Tools/Render.cpp Source/Engine/Engine.cpp)
    target_include_directories(StepkinRender PRIVATE Source)
endif()
