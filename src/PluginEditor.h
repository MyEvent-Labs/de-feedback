#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/LookAndFeel.h"
#include "ui/Meter.h"
#include "ui/SegmentedControl.h"
#include "ui/SpectrumView.h"

#include <memory>

namespace defeedback
{

class DeFeedbackProcessor;

class DeFeedbackEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit DeFeedbackEditor (DeFeedbackProcessor& processor);
    ~DeFeedbackEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void setAdvancedVisible (bool shouldShow);
    void applySkin();
    void showAbout();
    void showMenu();
    void savePreset();
    void loadPreset();
    void configureKnob (juce::Slider& slider, juce::Label& label, const juce::String& name,
                        std::unique_ptr<SliderAttachment>& attachmentPtr, const juce::String& paramId);

    DeFeedbackProcessor& proc_;
    DeFeedbackLookAndFeel lnf_;

    // Hero + meters.
    SpectrumView spectrum_;
    Meter meter_;

    juce::TextButton menuButton_ { "MENU" };
    juce::TextButton bypassButton_ { "BYPASS" };
    juce::TextButton freezeButton_ { "FREEZE" };
    juce::TextButton clearButton_ { "CLEAR" };
    juce::TextButton advancedButton_ { "ADVANCED" };

    SegmentedControl modeControl_ { juce::StringArray { "CONSERVATIVE", "NORMAL", "AGGRESSIVE" } };

    juce::Slider strengthSlider_, sensitivitySlider_, maxCutSlider_, recoverySlider_;
    juce::Label strengthLabel_, sensitivityLabel_, maxCutLabel_, recoveryLabel_;

    juce::Slider inputTrimSlider_, outputTrimSlider_, minFreqSlider_, maxFreqSlider_, maxNotchesSlider_, qBiasSlider_;
    juce::Label inputTrimLabel_, outputTrimLabel_, minFreqLabel_, maxFreqLabel_, maxNotchesLabel_, qBiasLabel_;
    juce::ToggleButton stereoLinkButton_ { "Linked stereo" };

    // Footer.
    juce::HyperlinkButton creditLink_;
    juce::Label versionLabel_, statusLabel_;

    juce::Rectangle<int> dockBounds_, advancedBounds_, brandBounds_, chipBounds_;
    bool advancedVisible_ = false;

    std::unique_ptr<ButtonAttachment> bypassAttach_, freezeAttach_, stereoLinkAttach_;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment_;
    std::unique_ptr<SliderAttachment> strengthAttach_, sensitivityAttach_, maxCutAttach_, recoveryAttach_;
    std::unique_ptr<SliderAttachment> inputTrimAttach_, outputTrimAttach_, minFreqAttach_, maxFreqAttach_, maxNotchesAttach_, qBiasAttach_;

    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeFeedbackEditor)
};

} // namespace defeedback
