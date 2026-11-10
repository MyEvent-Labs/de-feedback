#include "TestRegistry.h"

#include <juce_core/juce_core.h>

#include <cstdio>

int main()
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);

    juce::Array<juce::UnitTest*> tests
    {
        defeedback::tests::biquadTests(),
        defeedback::tests::detectorTests(),
        defeedback::tests::notchBankTests(),
        defeedback::tests::stateTests(),
        defeedback::tests::regressionTests()
    };

    runner.runTests (tests);

    int failures = 0;
    int total = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        const auto* result = runner.getResult (i);
        if (result == nullptr)
            continue;

        failures += result->failures;
        total += result->passes;

        std::printf ("%s: %d passed, %d failed\n",
                     result->unitTestName.toRawUTF8(), result->passes, result->failures);

        if (result->subcategoryName.isNotEmpty())
            std::printf ("    in: %s\n", result->subcategoryName.toRawUTF8());

        for (const auto& message : result->messages)
            std::printf ("    %s\n", message.toRawUTF8());
    }

    std::printf ("\nDe-Feedback tests: %d passed, %d failed\n", total, failures);
    return failures == 0 ? 0 : 1;
}
