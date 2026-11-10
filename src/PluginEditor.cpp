#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "state/StateIO.h"

namespace defeedback
{

namespace
{
    juce::Font editorFont (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }
}

DeFeedbackEditor::DeFeedbackEditor (DeFeedbackProcessor& processor)
    : juce::AudioProcessorEditor (&processor),
      proc_ (processor),
      spectrum_ (processor),
      meter_ (processor)
{
    setLookAndFeel (&lnf_);
    setResizable (true, true);
    setResizeLimits (560, 420, 1400, 900);
    setSize (720, 520);

    juce::ignoreUnused (minFreqLabel_, maxFreqLabel_, maxNotchesLabel_, qBiasLabel_, inputTrimLabel_, outputTrimLabel_);

    addAndMakeVisible (spectrum_);
    addAndMakeVisible (meter_);

    // Header.
    bypassAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc_.apvts, parameters::bypass, bypassButton_);
    bypassButton_.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton_);

    freezeAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc_.apvts, parameters::freeze, freezeButton_);
    freezeButton_.setClickingTogglesState (true);
    addAndMakeVisible (freezeButton_);

    clearButton_.onClick = [this] { proc_.requestClearNotches(); };
    addAndMakeVisible (clearButton_);

    advancedButton_.setClickingTogglesState (true);
    advancedButton_.onClick = [this] { advancedVisible_ = advancedButton_.getToggleState(); rebuildAdvanced(); resized(); };
    addAndMakeVisible (advancedButton_);

    presetButton_.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetButton_);

    aboutButton_.onClick = [this] { showAbout(); };
    addAndMakeVisible (aboutButton_);

    // Mode.
    modeAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc_.apvts, parameters::mode, modeBox_);
    modeBox_.addItemList ({ "Conservative", "Normal", "Aggressive" }, 1);
    addAndMakeVisible (modeBox_);
    modeLabel_.setText ("PROTECTION", juce::dontSendNotification);
    modeLabel_.setJustificationType (juce::Justification::centred);
    modeLabel_.setFont (editorFont (10.0f));
    addAndMakeVisible (modeLabel_);

    // Knobs.
    configureKnob (strengthSlider_, strengthLabel_, "STRENGTH", strengthAttach_, parameters::strength);
    configureKnob (sensitivitySlider_, sensitivityLabel_, "SENSITIVITY", sensitivityAttach_, parameters::sensitivity);
    configureKnob (maxCutSlider_, maxCutLabel_, "MAX CUT", maxCutAttach_, parameters::maxCutDb);
    configureKnob (recoverySlider_, recoveryLabel_, "RECOVERY", recoveryAttach_, parameters::recoverySeconds);

    // Advanced controls.
    configureKnob (minFreqSlider_, minFreqLabel_, "MIN FREQ", minFreqAttach_, parameters::minFrequencyHz);
    configureKnob (maxFreqSlider_, maxFreqLabel_, "MAX FREQ", maxFreqAttach_, parameters::maxFrequencyHz);
    configureKnob (maxNotchesSlider_, maxNotchesLabel_, "NOTCHES", maxNotchesAttach_, parameters::maxNotches);
    configureKnob (qBiasSlider_, qBiasLabel_, "Q BIAS", qBiasAttach_, parameters::qBias);
    configureKnob (inputTrimSlider_, inputTrimLabel_, "IN TRIM", inputTrimAttach_, parameters::inputTrimDb);
    configureKnob (outputTrimSlider_, outputTrimLabel_, "OUT TRIM", outputTrimAttach_, parameters::outputTrimDb);

    stereoLinkAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc_.apvts, parameters::stereoLink, stereoLinkButton_);
    stereoLinkButton_.setClickingTogglesState (true);
    addAndMakeVisible (stereoLinkButton_);

    statusLabel_.setFont (editorFont (11.0f));
    statusLabel_.setColour (juce::Label::textColourId, Palette::textDim);
    statusLabel_.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (statusLabel_);

    rebuildAdvanced();
    startTimerHz (8);
}

DeFeedbackEditor::~DeFeedbackEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DeFeedbackEditor::configureKnob (juce::Slider& slider, juce::Label& label, const juce::String& name,
                                      std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachmentPtr,
                                      const juce::String& paramId)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 66, 16);
    slider.setColour (juce::Slider::textBoxTextColourId, Palette::text);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (editorFont (10.0f));
    label.setColour (juce::Label::textColourId, Palette::textDim);
    addAndMakeVisible (label);

    attachmentPtr = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc_.apvts, paramId, slider);
}

void DeFeedbackEditor::rebuildAdvanced()
{
    const bool show = advancedVisible_;

    for (auto* c : { (juce::Component*) &minFreqSlider_, (juce::Component*) &maxFreqSlider_, (juce::Component*) &maxNotchesSlider_,
                     (juce::Component*) &qBiasSlider_, (juce::Component*) &inputTrimSlider_, (juce::Component*) &outputTrimSlider_,
                     (juce::Component*) &minFreqLabel_, (juce::Component*) &maxFreqLabel_, (juce::Component*) &maxNotchesLabel_,
                     (juce::Component*) &qBiasLabel_, (juce::Component*) &inputTrimLabel_, (juce::Component*) &outputTrimLabel_,
                     (juce::Component*) &stereoLinkButton_ })
        c->setVisible (show);
}

void DeFeedbackEditor::timerCallback()
{
    const auto& snap = proc_.getUiSnapshot();

    juce::String status;
    status << juce::String (juce::roundToInt (proc_.getSampleRate() / 1000.0)) << " kHz"
           << "  |  " << proc_.getBlockSize() << " samp"
           << "  |  lat " << proc_.getLatencySamples()
           << "  |  notches " << snap.activeNotches.load()
           << "  |  load " << juce::String (snap.dspLoad.load() * 100.0f, 1) << "%";

    if (snap.overloads.load() > 0)
        status << "  |  OVERLOAD";

    statusLabel_.setText (status, juce::dontSendNotification);
}

void DeFeedbackEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    auto header = getLocalBounds().removeFromTop (46).toFloat();

    g.setColour (Palette::panel);
    g.fillRect (header);

    g.setColour (Palette::accent);
    g.setFont (editorFont (15.0f, true));
    g.drawText ("MyEvent Labs", header.reduced (14.0f, 0.0f).withWidth (140.0f), juce::Justification::centredLeft, false);

    g.setColour (Palette::text);
    g.setFont (editorFont (15.0f, true));
    g.drawText ("De-Feedback", header.withTrimmedLeft (150.0f).withWidth (150.0f), juce::Justification::centredLeft, false);

    g.setColour (Palette::warning);
    g.setFont (editorFont (10.0f, true));
    g.drawText ("v0.0.1 ALPHA", header.withTrimmedLeft (300.0f).withWidth (110.0f), juce::Justification::centredLeft, false);

    // Footer line.
    g.setColour (Palette::outline);
    g.drawHorizontalLine (getHeight() - 40, 0.0f, (float) getWidth());
}

void DeFeedbackEditor::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (46);
    header.removeFromLeft (430);
    aboutButton_.setBounds (header.removeFromLeft (62).reduced (3, 10));
    presetButton_.setBounds (header.removeFromLeft (78).reduced (3, 10));
    bypassButton_.setBounds (header.removeFromRight (84).reduced (3, 10));

    auto footer = area.removeFromBottom (40);
    statusLabel_.setBounds (footer.removeFromRight (footer.getWidth() - 220).reduced (10, 6));
    meter_.setBounds (footer.reduced (8, 5));

    auto gap = 8;
    auto content = area.reduced (gap);

    spectrum_.setBounds (content.removeFromTop (advancedVisible_ ? content.getHeight() - 250 : content.getHeight() - 132));

    content.removeFromTop (6);
    auto controls = content.removeFromTop (advancedVisible_ ? 128 : 126);

    auto modeArea = controls.removeFromLeft (110);
    modeLabel_.setBounds (modeArea.removeFromTop (16));
    modeBox_.setBounds (modeArea.removeFromTop (30));

    auto buttonArea = controls.removeFromRight (110);
    freezeButton_.setBounds (buttonArea.removeFromTop (36).reduced (0, 3));
    clearButton_.setBounds (buttonArea.removeFromTop (36).reduced (0, 3));
    advancedButton_.setBounds (buttonArea.removeFromTop (36).reduced (0, 3));

    auto knobRow = controls.reduced (10, 0);
    const int knobW = juce::jmax (64, knobRow.getWidth() / 4);

    auto placeKnob = [&] (juce::Slider& slider, juce::Label& label)
    {
        auto col = knobRow.removeFromLeft (knobW);
        label.setBounds (col.removeFromTop (14));
        slider.setBounds (col);
    };

    placeKnob (strengthSlider_, strengthLabel_);
    placeKnob (sensitivitySlider_, sensitivityLabel_);
    placeKnob (maxCutSlider_, maxCutLabel_);
    placeKnob (recoverySlider_, recoveryLabel_);

    if (advancedVisible_)
    {
        content.removeFromTop (6);
        auto adv = content.removeFromTop (116);

        auto row = adv;
        auto left = row.removeFromLeft (row.getWidth() / 2);
        auto right = row;

        auto placeAdv = [] (juce::Rectangle<int>& r, juce::Slider& slider, juce::Label& label, int w)
        {
            auto col = r.removeFromLeft (w);
            label.setBounds (col.removeFromTop (14));
            slider.setBounds (col.reduced (2));
        };

        placeAdv (left, minFreqSlider_, minFreqLabel_, left.getWidth() / 3);
        placeAdv (left, maxFreqSlider_, maxFreqLabel_, left.getWidth() / 2);
        placeAdv (left, maxNotchesSlider_, maxNotchesLabel_, left.getWidth());

        placeAdv (right, qBiasSlider_, qBiasLabel_, right.getWidth() / 3);
        placeAdv (right, inputTrimSlider_, inputTrimLabel_, right.getWidth() / 2);
        placeAdv (right, outputTrimSlider_, outputTrimLabel_, right.getWidth());

        stereoLinkButton_.setBounds (adv.getX() + 4, adv.getBottom() - 10, 140, 20);
    }
}

void DeFeedbackEditor::showAbout()
{
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::WarningIcon,
        "MyEvent Labs De-Feedback v0.0.1 ALPHA",
        "Experimental adaptive feedback-control plugin.\n\n"
        "ALPHA SOFTWARE: Test under controlled conditions before any live event. "
        "This release is not validated for show-critical use and does not replace "
        "correct loudspeaker placement, gain structure, system tuning or a competent "
        "live-sound engineer.\n\n"
        "This build is a deterministic signal processor. It is not AI-powered and makes "
        "no guaranteed gain-before-feedback improvement claim. v0.0.1 introduces no "
        "intentional lookahead.\n\n"
        "Free software, distributed under GPLv3 (see THIRD_PARTY_NOTICES.md).");
}

void DeFeedbackEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Save Preset...");
    menu.addItem (2, "Load Preset...");
    menu.addSeparator();
    menu.addItem (3, "Reset to Defaults");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetButton_),
                        [this] (int result)
                        {
                            if (result == 1) savePreset();
                            else if (result == 2) loadPreset();
                            else if (result == 3)
                            {
                                for (auto* p : proc_.getParameters())
                                    p->setValueNotifyingHost (p->getDefaultValue());
                            }
                        });
}

void DeFeedbackEditor::savePreset()
{
    fileChooser_ = std::make_unique<juce::FileChooser> ("Save De-Feedback preset",
                                                        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                        "*.defeedback.xml");

    fileChooser_->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                               [this] (const juce::FileChooser& fc)
                               {
                                   const auto file = fc.getResult();
                                   if (file == juce::File())
                                       return;

                                   const auto xml = stateToXml (proc_.apvts.copyState());
                                   if (xml.isNotEmpty())
                                       file.withFileExtension ("defeedback.xml").replaceWithText (xml);
                               });
}

void DeFeedbackEditor::loadPreset()
{
    fileChooser_ = std::make_unique<juce::FileChooser> ("Load De-Feedback preset",
                                                        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                        "*.defeedback.xml");

    fileChooser_->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                               [this] (const juce::FileChooser& fc)
                               {
                                   const auto file = fc.getResult();
                                   if (! file.existsAsFile())
                                       return;

                                   juce::Result result = juce::Result::fail ("not parsed");
                                   auto tree = stateFromXml (file.loadFileAsString(), result);
                                   if (result.wasOk() && tree.isValid())
                                       proc_.apvts.replaceState (tree);
                               });
}

} // namespace defeedback
