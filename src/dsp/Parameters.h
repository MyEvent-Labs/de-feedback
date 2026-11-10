#pragma once

namespace defeedback::parameters
{

// Stable parameter IDs. These must not change after v0.0.1.
inline constexpr const char* bypass         = "bypass";
inline constexpr const char* mode           = "mode";
inline constexpr const char* strength       = "strength";
inline constexpr const char* sensitivity    = "sensitivity";
inline constexpr const char* maxCutDb       = "maxCutDb";
inline constexpr const char* recoverySeconds= "recoverySeconds";
inline constexpr const char* freeze         = "freeze";
inline constexpr const char* stereoLink     = "stereoLink";
inline constexpr const char* minFrequencyHz = "minFrequencyHz";
inline constexpr const char* maxFrequencyHz = "maxFrequencyHz";
inline constexpr const char* maxNotches     = "maxNotches";
inline constexpr const char* qBias          = "qBias";
inline constexpr const char* inputTrimDb    = "inputTrimDb";
inline constexpr const char* outputTrimDb   = "outputTrimDb";

enum class ProtectionMode
{
    conservative = 0,
    normal,
    aggressive
};

} // namespace defeedback::parameters
