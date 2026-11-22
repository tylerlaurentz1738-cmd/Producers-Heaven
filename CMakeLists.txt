cmake_minimum_required(VERSION 3.22)
project(ProducersHeaven VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    # one self-contained DLL, no separate VC++ runtime needed on the user's machine
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---- JUCE: use a local copy if you have one (-DJUCE_DIR=path), otherwise download it ----
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(ProducersHeaven
    COMPANY_NAME "Producers Heaven"
    BUNDLE_ID "com.producersheaven.plugin"
    PLUGIN_MANUFACTURER_CODE Phvn
    PLUGIN_CODE Phv1
    FORMATS VST3 Standalone
    PRODUCT_NAME "Producers Heaven"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Tools)

target_sources(ProducersHeaven PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/FXRack.cpp
    Source/Core/DrumSynth.cpp
    Source/Core/BeatAnalyzer.cpp
    Source/Core/PatternEngine.cpp
    Source/UI/Widgets.cpp
    Source/UI/HomeScreen.cpp
    Source/UI/Pages.cpp)

target_include_directories(ProducersHeaven PRIVATE Source)

target_compile_definitions(ProducersHeaven PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(UNIX AND NOT APPLE)
    # Linux test builds only: avoid optional system libraries
    target_compile_definitions(ProducersHeaven PUBLIC
        JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()

target_link_libraries(ProducersHeaven
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- optional test harness (renders screenshots and exercises every page) ----
option(PH_BUILD_HARNESS "Build the UI/integration test harness" OFF)
if(PH_BUILD_HARNESS)
    juce_add_console_app(PHHarness PRODUCT_NAME "PHHarness")
    target_sources(PHHarness PRIVATE
        Tests/UIHarness.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DSP/FXRack.cpp
        Source/Core/DrumSynth.cpp
        Source/Core/BeatAnalyzer.cpp
        Source/Core/PatternEngine.cpp
        Source/UI/Widgets.cpp
        Source/UI/HomeScreen.cpp
        Source/UI/Pages.cpp)
    target_include_directories(PHHarness PRIVATE Source)
    target_compile_definitions(PHHarness PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_USE_MP3AUDIOFORMAT=1
        JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
    target_link_libraries(PHHarness PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
