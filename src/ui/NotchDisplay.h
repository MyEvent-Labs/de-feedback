#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace defeedback
{

class DeFeedbackProcessor;

/** Live list of active notches: frequency, Q and reduction depth. */
class NotchDisplay : public juce::Component,
                     private juce::Timer
{
public:
    explicit NotchDisplay (DeFeedbackProcessor& processor);
    ~NotchDisplay() override = default;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    static constexpr int kMaxRows = 24;

    struct Row
    {
        float freq = 0.0f;
        float gain = 0.0f;
        float q = 0.0f;
    };

    DeFeedbackProcessor& proc_;
    std::array<Row, kMaxRows> rows_ {};
    int count_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NotchDisplay)
};

} // namespace defeedback
