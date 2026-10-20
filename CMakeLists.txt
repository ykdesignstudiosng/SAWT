cmake_minimum_required(VERSION 3.22)

# =====================================================================
#  SAWT - The Voice-Only Synthesizer
#  Every sound in this plugin is derived from the human voice.
#  No instrumental oscillator is ever used as a sound source.
# =====================================================================
project(SAWT VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(SAWT_BUILD_CLAP     "Build the CLAP format (fetches clap-juce-extensions)" ON)
option(SAWT_FETCH_JUCE     "Download JUCE automatically when lib/JUCE is absent"  ON)
option(SAWT_ENABLE_LTO     "Enable link-time-optimisation (slower link, smaller binary)" OFF)

# ---------------------------------------------------------------------
# Locate ALSA so we can decide whether a Standalone target is possible
# (Linux plugin builds do not need ALSA; the host provides the audio).
# ---------------------------------------------------------------------
include(CheckIncludeFileCXX)
set(ALSA_AVAILABLE FALSE)
if (UNIX AND NOT APPLE)
    if (EXISTS "/usr/include/alsa/asoundlib.h")
        set(ALSA_AVAILABLE TRUE)
    endif()
else()
    set(ALSA_AVAILABLE TRUE) # Windows / macOS always OK
endif()

# ---------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------
include(FetchContent)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lib/JUCE/CMakeLists.txt")
    add_subdirectory(lib/JUCE EXCLUDE_FROM_ALL)
elseif (SAWT_FETCH_JUCE)
    message(STATUS "SAWT: fetching JUCE 8.0.15 (first configure may take a while)...")
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
else()
    message(FATAL_ERROR "JUCE not found. Place JUCE at lib/JUCE or set SAWT_FETCH_JUCE=ON.")
endif()

# ---------------------------------------------------------------------
# CLAP support (optional)
# ---------------------------------------------------------------------
if (SAWT_BUILD_CLAP)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lib/clap-juce-extensions/CMakeLists.txt")
        add_subdirectory(lib/clap-juce-extensions EXCLUDE_FROM_ALL)
    else()
        message(STATUS "SAWT: fetching clap-juce-extensions...")
        FetchContent_Declare(clap-juce-extensions
            GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
            GIT_TAG        origin/main
            GIT_SHALLOW    TRUE)
        FetchContent_MakeAvailable(clap-juce-extensions)
    endif()
endif()

# ---------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------
set(SAWT_FORMATS VST3)
if (ALSA_AVAILABLE)
    list(APPEND SAWT_FORMATS Standalone)
endif()
if (APPLE)
    list(APPEND SAWT_FORMATS AU)
endif()
if (NOT SAWT_BUILD_CLAP)
    message(STATUS "SAWT: CLAP disabled, formats: ${SAWT_FORMATS}")
endif()

juce_add_plugin(SAWT
    COMPANY_NAME                "SAWT Audio"
    COMPANY_WEBSITE             "https://sawtaudio.example"
    PLUGIN_MANUFACTURER_CODE    Sawt
    PLUGIN_CODE                 Swt1
    FORMATS                     ${SAWT_FORMATS}
    PRODUCT_NAME                "SAWT"
    DESCRIPTION                 "The Voice-Only Synthesizer - every sound derived from the human voice"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN                     kAudioUnitType_MusicDevice)

target_sources(SAWT PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params.cpp
    Source/Core/Analyzers.cpp
    Source/Core/Slicer.cpp
    Source/Core/PresetManager.cpp
    Source/DSP/PSOLA.cpp
    Source/DSP/FormantShifter.cpp
    Source/DSP/SamplerEngine.cpp
    Source/DSP/GranularEngine.cpp
    Source/DSP/FormantEngine.cpp
    Source/DSP/DrumSynth.cpp
    Source/DSP/DrumEngine.cpp
    Source/DSP/EffectsRack.cpp
    Source/UI/LookAndFeel.cpp
    Source/UI/Widgets.cpp
    Source/UI/WaveformView.cpp
    Source/UI/XYPad.cpp
    Source/UI/SpectrumView.cpp
    Source/UI/DrumPanel.cpp)

target_compile_definitions(SAWT PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_ALSA=1
    JUCE_JACK=0)

target_link_libraries(SAWT
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_data_structures
        juce::juce_events
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if (SAWT_ENABLE_LTO)
    target_link_libraries(SAWT PUBLIC juce::juce_recommended_lto_flags)
endif()

# Standalone target additionally needs audio-device support
if (ALSA_AVAILABLE)
    target_link_libraries(SAWT PRIVATE juce::juce_audio_devices juce::juce_audio_utils)
endif()

# ---------------------------------------------------------------------
# CLAP format
# ---------------------------------------------------------------------
if (SAWT_BUILD_CLAP)
    clap_juce_extensions_plugin(TARGET SAWT
        CLAP_ID       "com-sawtaudio-sawt"
        CLAP_FEATURES instrument synthesizer)
endif()
