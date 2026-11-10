#include "state/StateIO.h"

namespace defeedback
{

namespace
{
    const juce::Identifier rootType { "DeFeedbackState" };
    const juce::Identifier versionProp { "stateVersion" };
}

juce::String stateToXml (const juce::ValueTree& state)
{
    auto copy = state.createCopy();
    copy.setProperty (versionProp, kStateVersion, nullptr);

    if (auto xml = copy.createXml())
        return xml->toString();

    return {};
}

void migrateState (juce::ValueTree& state, int fromVersion)
{
    // v0 -> v1: introduce the explicit schema version marker. Parameter
    // defaults are supplied by the AudioProcessorValueTreeState, which
    // substitutes any missing parameter children with their defaults.
    juce::ignoreUnused (fromVersion);

    state.setProperty (versionProp, kStateVersion, nullptr);
}

juce::ValueTree stateFromXml (const juce::String& text, juce::Result& result)
{
    if (text.isEmpty())
    {
        result = juce::Result::fail ("State is empty");
        return {};
    }

    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (text));

    if (xml == nullptr)
    {
        result = juce::Result::fail ("State is not valid XML");
        return {};
    }

    if (! xml->hasTagName (rootType.toString()))
    {
        result = juce::Result::fail ("State root element is not DeFeedbackState");
        return {};
    }

    auto tree = juce::ValueTree::fromXml (*xml);

    if (! tree.isValid())
    {
        result = juce::Result::fail ("State tree is invalid");
        return {};
    }

    const int version = (int) tree.getProperty (versionProp, 0);

    if (version > kStateVersion)
    {
        result = juce::Result::fail ("State was written by a newer De-Feedback version");
        return {};
    }

    migrateState (tree, version);

    result = juce::Result::ok();
    return tree;
}

} // namespace defeedback
