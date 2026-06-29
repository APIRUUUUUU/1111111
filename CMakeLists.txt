cmake_minimum_required(VERSION 3.22)

project(CrystalVoice VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.12
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

juce_add_plugin(CrystalVoice
    COMPANY_NAME "VJIDAI"
    BUNDLE_ID "com.vjidai.crystalvoice"

    PLUGIN_MANUFACTURER_CODE VjdI
    PLUGIN_CODE CrVo

    FORMATS VST3 Standalone

    PRODUCT_NAME "Crystal Voice"
    PLUGIN_NAME "Crystal Voice"
    PLUGIN_DESC "Low-latency vocal enhancement and harmony prototype"
    PLUGIN_MANUFACTURER "VJIDAI"
    PLUGIN_VERSION "0.1.0"

    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE

    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE

    VST3_CATEGORIES Fx Dynamics Reverb
)

juce_generate_juce_header(CrystalVoice)

target_sources(CrystalVoice
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

target_compile_definitions(CrystalVoice
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(CrystalVoice
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
