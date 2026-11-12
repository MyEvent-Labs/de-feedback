juce_add_console_app(defeedback_signal_generator
    PRODUCT_NAME "De-Feedback Signal Generator")

target_sources(defeedback_signal_generator PRIVATE signal-generator/Main.cpp)

target_link_libraries(defeedback_signal_generator
    PRIVATE
        defeedback_core
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

target_compile_definitions(defeedback_signal_generator PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0)

juce_add_console_app(defeedback_benchmark
    PRODUCT_NAME "De-Feedback Benchmark")

target_sources(defeedback_benchmark PRIVATE benchmark/Main.cpp)

target_link_libraries(defeedback_benchmark
    PRIVATE
        defeedback_core
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

target_compile_definitions(defeedback_benchmark PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0)

juce_add_console_app(defeedback_analyze
    PRODUCT_NAME "De-Feedback Analyzer")

target_sources(defeedback_analyze PRIVATE analyze/Main.cpp)

target_link_libraries(defeedback_analyze
    PRIVATE
        defeedback_core
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

target_compile_definitions(defeedback_analyze PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0)

juce_add_console_app(defeedback_quality
    PRODUCT_NAME "De-Feedback Quality Harness")

target_sources(defeedback_quality PRIVATE quality/Main.cpp)

target_link_libraries(defeedback_quality
    PRIVATE
        defeedback_core
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

target_compile_definitions(defeedback_quality PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0)

juce_add_console_app(defeedback_screenshot
    PRODUCT_NAME "De-Feedback Screenshot Tool")

target_sources(defeedback_screenshot PRIVATE
    screenshot/Main.cpp
    ${CMAKE_SOURCE_DIR}/src/PluginProcessor.cpp
    ${CMAKE_SOURCE_DIR}/src/PluginEditor.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/LookAndFeel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/SpectrumView.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Meter.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/NotchDisplay.cpp)

target_include_directories(defeedback_screenshot PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(defeedback_screenshot
    PRIVATE
        defeedback_core
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

target_compile_definitions(defeedback_screenshot PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JucePlugin_Name="MyEvent Labs De-Feedback")
