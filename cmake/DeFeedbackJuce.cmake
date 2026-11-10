# Pins the JUCE dependency for De-Feedback.
#
# Strategy: FetchContent pinned to an immutable JUCE 8 release, with an
# escape hatch for offline / air-gapped builds:
#
#   cmake -S . -B build -DDE_FEEDBACK_JUCE_PATH=/path/to/JUCE
#
# The default pin is JUCE 8.0.17-security (commit e548e97a54a083acbfc330d584419fd074409045).
# Do not float this to a branch or "master".
include_guard(GLOBAL)

set(DE_FEEDBACK_JUCE_TAG "8.0.17-security" CACHE STRING "Pinned JUCE git tag")

if(DE_FEEDBACK_JUCE_PATH)
    message(STATUS "De-Feedback: using local JUCE at ${DE_FEEDBACK_JUCE_PATH}")
    add_subdirectory("${DE_FEEDBACK_JUCE_PATH}" "${CMAKE_BINARY_DIR}/_juce" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    message(STATUS "De-Feedback: fetching JUCE ${DE_FEEDBACK_JUCE_TAG}")
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${DE_FEEDBACK_JUCE_TAG}
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()
