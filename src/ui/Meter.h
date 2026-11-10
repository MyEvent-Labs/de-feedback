#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace defeedback
{

class DeFeedbackProcessor;

/** Compact input/output peak meter with dB read-out. */
class Meter : public juce::Component,
              private juce::Timer
{
public:
    explicit Meter (DeFeedbackProcessor& processor);
    ~Meter() override = default;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    DeFeedbackProcessor& proc_;
    float inputDb_ = -100.0f;
    float outputDb_ = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Meter)
};

} // namespace defeedback
