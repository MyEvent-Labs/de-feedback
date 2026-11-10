#include "TestRegistry.h"

#include "state/StateIO.h"

#include <juce_data_structures/juce_data_structures.h>

namespace defeedback::tests
{

namespace
{
    class StateTests : public juce::UnitTest
    {
    public:
        StateTests() : juce::UnitTest ("StateIO", "defeedback") {}

        void runTest() override
        {
            beginTest ("round trip preserves parameters and version");
            {
                juce::ValueTree state ("DeFeedbackState");
                juce::ValueTree params ("Parameters");
                params.setProperty ("strength", 1.5f, nullptr);
                params.setProperty ("mode", 2, nullptr);
                state.addChild (params, -1, nullptr);

                const auto xml = stateToXml (state);
                expect (xml.isNotEmpty());

                juce::Result result = juce::Result::fail ("unset");
                auto parsed = stateFromXml (xml, result);

                expect (result.wasOk());
                expect (parsed.isValid());
                expectEquals ((int) parsed.getProperty ("stateVersion"), kStateVersion);

                const auto parsedParams = parsed.getChildWithName ("Parameters");
                expect (parsedParams.isValid());
                expectWithinAbsoluteError ((float) parsedParams.getProperty ("strength"), 1.5f, 0.001f);
                expectEquals ((int) parsedParams.getProperty ("mode"), 2);
            }

            beginTest ("malformed XML fails safely");
            {
                juce::Result result = juce::Result::ok();
                auto parsed = stateFromXml ("this is not xml ><", result);
                expect (result.failed());
                expect (! parsed.isValid());
            }

            beginTest ("wrong root type fails safely");
            {
                juce::Result result = juce::Result::ok();
                auto parsed = stateFromXml ("<SomethingElse><A/></SomethingElse>", result);
                expect (result.failed());
            }

            beginTest ("newer schema version is rejected");
            {
                // Build the XML directly: stateToXml stamps the current version,
                // which would hide the version gate under test.
                juce::ValueTree state ("DeFeedbackState");
                state.setProperty ("stateVersion", kStateVersion + 10, nullptr);

                auto xmlElement = state.createXml();
                expect (xmlElement != nullptr);
                const auto xml = xmlElement != nullptr ? xmlElement->toString() : juce::String();

                juce::Result result = juce::Result::ok();
                auto parsed = stateFromXml (xml, result);
                expect (result.failed());
                expect (! parsed.isValid());
            }

            beginTest ("missing version migrates forward");
            {
                juce::ValueTree state ("DeFeedbackState");
                const auto xml = state.toXmlString();

                juce::Result result = juce::Result::fail ("unset");
                auto parsed = stateFromXml (xml, result);

                expect (result.wasOk());
                expectEquals ((int) parsed.getProperty ("stateVersion"), kStateVersion);
            }
        }
    };
}

juce::UnitTest* stateTests()
{
    static StateTests tests;
    return &tests;
}

} // namespace defeedback::tests
