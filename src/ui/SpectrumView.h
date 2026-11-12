#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace defeedback
{

class DeFeedbackProcessor;

/** The hero display: a full-bleed, glowing log-frequency spectrum, the cut
    curve the notch bank is applying (with a tag per notch), and the big
    "what am I doing for you right now" read-out. */
class SpectrumView : public juce::Component,
                     private juce::Timer
{
public:
    explicit SpectrumView (DeFeedbackProcessor& processor);
    ~SpectrumView() override = default;

    /** Right edge for the read-out and notch tags, so they clear the meters. */
    void setContentRight (int x) { contentRight_ = x; }

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    struct NotchVis
    {
        float freq = 0.0f;
        float q = 8.0f;
        float gain = 0.0f;   // smoothed dB, <= 0
    };

    DeFeedbackProcessor& proc_;
    std::vector<float> mags_;       // raw dB per FFT bin
    std::vector<float> display_;    // smoothed dB per pixel column
    std::vector<NotchVis> notches_; // eased copy of the notch bank, for animation
    float heroDb_ = 0.0f;
    int contentRight_ = 0;

    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;
    static constexpr float minDb   = -84.0f;
    static constexpr float maxDb   = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumView)
};

} // namespace defeedback
