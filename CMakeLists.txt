cmake_minimum_required(VERSION 3.22)

project(DGUTTA_RadioReady VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE (fetched automatically on first configure)
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    juce
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.zip
)
FetchContent_MakeAvailable(juce)

# ---------------------------------------------------------------------------
# Plugin target: VST3 + AU + Standalone from the same code.
# On Linux the AU target is skipped automatically by JUCE; on macOS all
# three formats are produced.
# ---------------------------------------------------------------------------
juce_add_plugin(DGUTTA_RadioReady
    PRODUCT_NAME            "Gutta Radio Ready"
    COMPANY_NAME            "DGUTTA912"
    BUNDLE_ID               com.dgutta912.radioready
    PLUGIN_MANUFACTURER_CODE Dgt9
    PLUGIN_CODE             Dgr1
    FORMATS                 VST3 AU Standalone
    VST3_CATEGORIES         Fx Mastering Dynamics Eq
    AU_MAIN_TYPE            kAudioUnitType_Effect
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(DGUTTA_RadioReady
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

target_compile_definitions(DGUTTA_RadioReady
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(DGUTTA_RadioReady
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
