cmake_minimum_required(VERSION 3.22)

project(DeFeedback
    VERSION 0.0.1
    DESCRIPTION "MyEvent Labs De-Feedback - experimental adaptive feedback control"
    HOMEPAGE_URL "https://github.com/MyEvent-Labs/de-feedback"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
endif()

option(DE_FEEDBACK_BUILD_TESTS "Build offline unit / DSP regression tests" ON)
option(DE_FEEDBACK_BUILD_TOOLS "Build offline signal generator and benchmark tools" ON)
option(DE_FEEDBACK_ENABLE_SANITIZERS "Enable ASan/UBSan on supported toolchains" OFF)
set(DE_FEEDBACK_JUCE_PATH "" CACHE PATH "Path to a local JUCE 8 checkout (offline builds)")

include(cmake/DeFeedbackJuce.cmake)

# ---------------------------------------------------------------------------
# Shared DSP core. No GUI, no plugin-format code. Links into the plugin and
# is exercised directly by the offline test harness.
# ---------------------------------------------------------------------------
add_library(defeedback_core STATIC
    src/dsp/Biquad.cpp
    src/dsp/FeedbackDetector.cpp
    src/dsp/AdaptiveNotchBank.cpp
    src/dsp/SpectrumAnalyzer.cpp
    src/state/StateIO.cpp)

target_include_directories(defeedback_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(defeedback_core
    PUBLIC
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_audio_processors
        juce::juce_data_structures)

target_compile_definitions(defeedback_core
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0)

# ---------------------------------------------------------------------------
# Plugin targets
# ---------------------------------------------------------------------------
juce_add_plugin(DeFeedback
    COMPANY_NAME              "MyEvent Labs"
    BUNDLE_ID                 "labs.myevent.defeedback"
    PLUGIN_MANUFACTURER_CODE  MyEv
    PLUGIN_CODE               Dfdb
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "MyEvent Labs De-Feedback"
    DESCRIPTION               "Experimental adaptive feedback-control plugin (ALPHA)"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           Fx Distortion
    AU_MAIN_TYPE              kAudioUnitType_Effect
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "De-Feedback standalone uses audio input to analyse and suppress feedback.")

target_sources(DeFeedback PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/ui/LookAndFeel.cpp
    src/ui/SpectrumView.cpp
    src/ui/Meter.cpp)

target_include_directories(DeFeedback PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(DeFeedback
    PRIVATE
        defeedback_core
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

juce_generate_juce_header(DeFeedback)

# ---------------------------------------------------------------------------
# Tests and tools
# ---------------------------------------------------------------------------
if(DE_FEEDBACK_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(DE_FEEDBACK_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

message(STATUS "De-Feedback ${PROJECT_VERSION} alpha - tests=${DE_FEEDBACK_BUILD_TESTS} tools=${DE_FEEDBACK_BUILD_TOOLS}")
