#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace defeedback
{

class DeFeedbackProcessor;

/** Real-time spectrum analyser with a smoothed, log-frequency trace and active
    notch markers. */
class SpectrumView : public juce::Component,
                     private juce::Timer
{
public:
    explicit SpectrumView (DeFeedbackProcessor& processor);
    ~SpectrumView() override = default;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    DeFeedbackProcessor& proc_;
    std::vector<float> mags_;     // raw dB per FFT bin
    std::vector<float> display_;  // smoothed dB per pixel column

    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;
    static constexpr float minDb   = -84.0f;
    static constexpr float maxDb   = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumView)
};

} // namespace defeedback
