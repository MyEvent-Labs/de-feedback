#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace defeedback
{

class DeFeedbackProcessor;

/** Real-time spectrum with active notch markers. */
class SpectrumView : public juce::Component,
                     private juce::Timer
{
public:
    explicit SpectrumView (DeFeedbackProcessor& processor);
    ~SpectrumView() override = default;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    DeFeedbackProcessor& proc_;
    std::vector<float> mags_;

    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;
    static constexpr float minDb = -90.0f;
    static constexpr float maxDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumView)
};

} // namespace defeedback
