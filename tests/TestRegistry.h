#pragma once

#include <juce_core/juce_core.h>

namespace defeedback::tests
{

juce::UnitTest* biquadTests();
juce::UnitTest* detectorTests();
juce::UnitTest* notchBankTests();
juce::UnitTest* stateTests();
juce::UnitTest* regressionTests();
juce::UnitTest* processorTests();

} // namespace defeedback::tests
