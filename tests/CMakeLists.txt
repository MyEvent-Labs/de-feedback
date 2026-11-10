# Offline test harness. Runs headless in CI with no audio interface.
juce_add_console_app(defeedback_tests
    PRODUCT_NAME "De-Feedback Tests")

target_sources(defeedback_tests PRIVATE
    TestMain.cpp
    unit/BiquadTests.cpp
    unit/DetectorTests.cpp
    unit/NotchBankTests.cpp
    unit/StateTests.cpp
    dsp/RegressionTests.cpp)

target_include_directories(defeedback_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(defeedback_tests
    PRIVATE
        defeedback_core
        juce::juce_dsp
        juce::juce_audio_processors
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

target_compile_definitions(defeedback_tests PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0)

if(DE_FEEDBACK_ENABLE_SANITIZERS AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU"))
    target_compile_options(defeedback_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(defeedback_tests PRIVATE -fsanitize=address,undefined)
endif()

add_test(NAME defeedback_tests COMMAND defeedback_tests)
