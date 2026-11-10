#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace defeedback
{

/** Current on-disk/state schema version. */
inline constexpr int kStateVersion = 1;

/** Serialise a plugin state tree to XML text. */
juce::String stateToXml (const juce::ValueTree& state);

/** Parse XML text into a state tree.

    Fails safely (returns an invalid tree and a failed Result) on malformed
    input, a wrong root type, or a schema version newer than this build.
    Older schemas are migrated forward.
*/
juce::ValueTree stateFromXml (const juce::String& text, juce::Result& result);

/** Apply forward migrations in place. Unknown/older versions are upgraded
    to kStateVersion; the caller is responsible for validating the result. */
void migrateState (juce::ValueTree& state, int fromVersion);

} // namespace defeedback
