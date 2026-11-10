#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace defeedback
{

class DeFeedbackProcessor;

/** Input / output level meters with peak, RMS, peak-hold and a dedicated
    gain-reduction meter. */
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

    float inPeak_ = -100.0f, inRms_ = -100.0f, inHold_ = -100.0f;
    float outPeak_ = -100.0f, outRms_ = -100.0f, outHold_ = -100.0f;
    float gr_ = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Meter)
};

} // namespace defeedback
