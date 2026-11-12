#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/LookAndFeel.h"
#include "ui/Meter.h"
#include "ui/NotchDisplay.h"
#include "ui/SectionPanel.h"
#include "ui/SegmentedControl.h"
#include "ui/SpectrumView.h"

#include <memory>
#include <vector>

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
    void resized() override;

private:
    void timerCallback() override;
    void rebuildAdvanced();
    void showAbout();
    void showPresetMenu();
    void savePreset();
    void loadPreset();
    void configureKnob (juce::Component& parent, juce::Slider& slider, juce::Label& label, const juce::String& name,
                        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachmentPtr,
                        const juce::String& paramId);

    DeFeedbackProcessor& proc_;
    DeFeedbackLookAndFeel lnf_;

    // Hero + read-outs.
    SpectrumView spectrum_;
    Meter meter_;
    NotchDisplay notchDisplay_;

    SectionPanel spectrumPanel_ { "SPECTRUM" };
    SectionPanel levelsPanel_ { "LEVELS" };
    SectionPanel notchesPanel_ { "ACTIVE NOTCHES" };
    SectionPanel protectionPanel_ { "PROTECTION" };
    SectionPanel outputPanel_ { "OUTPUT" };
    SectionPanel actionsPanel_ { "ACTIONS" };
    SectionPanel advancedPanel_ { "ADVANCED" };

    juce::TextButton bypassButton_ { "BYPASS" };
    juce::TextButton freezeButton_ { "FREEZE" };
    juce::TextButton clearButton_ { "CLEAR ALL" };
    juce::TextButton advancedButton_ { "ADVANCED" };
    juce::TextButton presetButton_ { "PRESET" };
    juce::TextButton aboutButton_ { "ABOUT" };

    SegmentedControl modeControl_ { juce::StringArray { "CONSERVATIVE", "NORMAL", "AGGRESSIVE" } };

    juce::Slider strengthSlider_, sensitivitySlider_, maxCutSlider_, recoverySlider_;
    juce::Label strengthLabel_, sensitivityLabel_, maxCutLabel_, recoveryLabel_;

    juce::Slider inputTrimSlider_, outputTrimSlider_, minFreqSlider_, maxFreqSlider_, maxNotchesSlider_, qBiasSlider_;
    juce::Label inputTrimLabel_, outputTrimLabel_, minFreqLabel_, maxFreqLabel_, maxNotchesLabel_, qBiasLabel_;
    juce::ToggleButton stereoLinkButton_ { "Linked stereo" };

    juce::Label statusLabel_;
    bool advancedVisible_ = false;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach_, freezeAttach_, stereoLinkAttach_;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> strengthAttach_, sensitivityAttach_, maxCutAttach_, recoveryAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputTrimAttach_, outputTrimAttach_, minFreqAttach_, maxFreqAttach_, maxNotchesAttach_, qBiasAttach_;

    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeFeedbackEditor)
};

} // namespace defeedback
