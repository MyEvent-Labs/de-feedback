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
