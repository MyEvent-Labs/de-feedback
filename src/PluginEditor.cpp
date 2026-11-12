#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "state/StateIO.h"

#include <cmath>

namespace defeedback
{

DeFeedbackEditor::DeFeedbackEditor (DeFeedbackProcessor& processor)
    : juce::AudioProcessorEditor (&processor),
      proc_ (processor),
      spectrum_ (processor),
      meter_ (processor),
      notchDisplay_ (processor)
{
    setLookAndFeel (&lnf_);
    setResizable (true, true);
    setResizeLimits (900, 600, 1600, 1000);
    setSize (1080, 700);

    // Panels.
    for (auto* p : { &spectrumPanel_, &levelsPanel_, &notchesPanel_, &protectionPanel_,
                     &outputPanel_, &actionsPanel_, &advancedPanel_ })
        addAndMakeVisible (*p);

    spectrumPanel_.addAndMakeVisible (spectrum_);
    levelsPanel_.addAndMakeVisible (meter_);
    notchesPanel_.addAndMakeVisible (notchDisplay_);

    // Header actions.
    bypassAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc_.apvts, parameters::bypass, bypassButton_);
    bypassButton_.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton_);

    presetButton_.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetButton_);

    aboutButton_.onClick = [this] { showAbout(); };
    addAndMakeVisible (aboutButton_);

    // Protection mode.
    addAndMakeVisible (modeControl_);
    if (auto* modeParam = proc_.apvts.getParameter (parameters::mode))
    {
        modeAttachment_ = std::make_unique<juce::ParameterAttachment> (*modeParam,
            [this] (float v) { modeControl_.setSelectedIndex ((int) std::lround (v), juce::dontSendNotification); });
        modeAttachment_->sendInitialUpdate();
        modeControl_.onChange = [this] (int idx) { modeAttachment_->setValueAsCompleteGesture ((float) idx); };
    }

    // Protection knobs.
    configureKnob (strengthSlider_, strengthLabel_, "STRENGTH", strengthAttach_, parameters::strength);
    configureKnob (sensitivitySlider_, sensitivityLabel_, "SENSITIVITY", sensitivityAttach_, parameters::sensitivity);
    configureKnob (maxCutSlider_, maxCutLabel_, "MAX CUT", maxCutAttach_, parameters::maxCutDb);
    configureKnob (recoverySlider_, recoveryLabel_, "RECOVERY", recoveryAttach_, parameters::recoverySeconds);

    // Output knobs.
    configureKnob (inputTrimSlider_, inputTrimLabel_, "IN TRIM", inputTrimAttach_, parameters::inputTrimDb);
    configureKnob (outputTrimSlider_, outputTrimLabel_, "OUT TRIM", outputTrimAttach_, parameters::outputTrimDb);

    stereoLinkAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc_.apvts, parameters::stereoLink, stereoLinkButton_);
    stereoLinkButton_.setClickingTogglesState (true);
    addAndMakeVisible (stereoLinkButton_);

    // Advanced knobs.
    configureKnob (minFreqSlider_, minFreqLabel_, "MIN FREQ", minFreqAttach_, parameters::minFrequencyHz);
    configureKnob (maxFreqSlider_, maxFreqLabel_, "MAX FREQ", maxFreqAttach_, parameters::maxFrequencyHz);
    configureKnob (maxNotchesSlider_, maxNotchesLabel_, "NOTCHES", maxNotchesAttach_, parameters::maxNotches);
    configureKnob (qBiasSlider_, qBiasLabel_, "Q BIAS", qBiasAttach_, parameters::qBias);

    // Actions.
    freezeAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc_.apvts, parameters::freeze, freezeButton_);
    freezeButton_.setClickingTogglesState (true);
    addAndMakeVisible (freezeButton_);

    clearButton_.onClick = [this] { proc_.requestClearNotches(); };
    addAndMakeVisible (clearButton_);

    advancedButton_.setClickingTogglesState (true);
    advancedButton_.onClick = [this] { advancedVisible_ = advancedButton_.getToggleState(); rebuildAdvanced(); resized(); };
    addAndMakeVisible (advancedButton_);

    statusLabel_.setFont (DeFeedbackLookAndFeel::typeface (11.0f));
    statusLabel_.setColour (juce::Label::textColourId, Palette::textDim);
    statusLabel_.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (statusLabel_);

    rebuildAdvanced();
    startTimerHz (12);
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
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 15);
    slider.setColour (juce::Slider::textBoxTextColourId, Palette::text);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (DeFeedbackLookAndFeel::typeface (10.0f, true));
    label.setColour (juce::Label::textColourId, Palette::textDim);
    addAndMakeVisible (label);

    attachmentPtr = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc_.apvts, paramId, slider);
}

void DeFeedbackEditor::rebuildAdvanced()
{
    advancedPanel_.setVisible (advancedVisible_);

    for (auto* c : { (juce::Component*) &minFreqSlider_, (juce::Component*) &maxFreqSlider_,
                     (juce::Component*) &maxNotchesSlider_, (juce::Component*) &qBiasSlider_,
                     (juce::Component*) &minFreqLabel_, (juce::Component*) &maxFreqLabel_,
                     (juce::Component*) &maxNotchesLabel_, (juce::Component*) &qBiasLabel_ })
        c->setVisible (advancedVisible_);
}

void DeFeedbackEditor::timerCallback()
{
    const auto& snap = proc_.getUiSnapshot();

    juce::String status;
    status << juce::String (juce::roundToInt (proc_.getSampleRate() / 1000.0)) << " kHz   "
           << proc_.getBlockSize() << " smp   "
           << "lat " << proc_.getLatencySamples() << "   "
           << "notches " << snap.activeNotches.load() << "   "
           << "load " << juce::String (snap.dspLoad.load() * 100.0f, 1) << "%";

    if (snap.overloads.load() > 0)
        status << "   OVERLOAD";

    statusLabel_.setText (status, juce::dontSendNotification);
}

void DeFeedbackEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    // Header band.
    auto header = getLocalBounds().removeFromTop (56);
    g.setColour (Palette::surface);
    g.fillRect (header);
    g.setColour (Palette::outline);
    g.drawHorizontalLine (header.getBottom() - 1, 0.0f, (float) getWidth());

    g.setColour (Palette::accent);
    g.setFont (DeFeedbackLookAndFeel::typeface (15.0f, true));
    g.drawText ("MyEvent Labs", header.reduced (16, 0).withWidth (150).withTrimmedTop (-1),
                juce::Justification::centredLeft, false);

    g.setColour (Palette::text);
    g.setFont (DeFeedbackLookAndFeel::typeface (15.0f, true));
    g.drawText ("De-Feedback", header.withTrimmedLeft (168).withWidth (150),
                juce::Justification::centredLeft, false);

    // ALPHA chip.
    const juce::Rectangle<int> chip (330, 19, 62, 18);
    g.setColour (Palette::warning.withAlpha (0.16f));
    g.fillRoundedRectangle (chip.toFloat(), 4.0f);
    g.setColour (Palette::warning);
    g.setFont (DeFeedbackLookAndFeel::typeface (10.0f, true));
    g.drawText ("ALPHA", chip, juce::Justification::centred, false);

    // Footer separator.
    g.setColour (Palette::outline);
    g.drawHorizontalLine (getHeight() - 27, 0.0f, (float) getWidth());
}

void DeFeedbackEditor::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (56);
    header.removeFromRight (14);
    bypassButton_.setBounds (header.removeFromRight (96).reduced (0, 14));
    header.removeFromRight (8);
    presetButton_.setBounds (header.removeFromRight (84).reduced (0, 14));
    header.removeFromRight (8);
    aboutButton_.setBounds (header.removeFromRight (68).reduced (0, 14));

    auto footer = area.removeFromBottom (28);
    statusLabel_.setBounds (footer.reduced (16, 0));

    auto body = area.reduced (16, 14);

    const int advH = advancedVisible_ ? 122 : 0;
    const int deckH = 156;

    if (advancedVisible_)
        advancedPanel_.setBounds (body.removeFromBottom (advH).withTrimmedBottom (0));

    auto deck = body.removeFromBottom (deckH);
    body.removeFromBottom (12);

    // Top row: spectrum (hero) + read-out sidebar.
    auto sidebar = body.removeFromRight (300);
    body.removeFromRight (12);

    spectrumPanel_.setBounds (body);
    spectrum_.setBounds (spectrumPanel_.getContentBounds());

    levelsPanel_.setBounds (sidebar.removeFromTop (juce::roundToInt ((float) sidebar.getHeight() * 0.56f)));
    sidebar.removeFromTop (12);
    notchesPanel_.setBounds (sidebar);
    meter_.setBounds (levelsPanel_.getContentBounds());
    notchDisplay_.setBounds (notchesPanel_.getContentBounds());

    // Control deck.
    const int outW = 226, actW = 176;
    protectionPanel_.setBounds (deck.removeFromLeft (deck.getWidth() - outW - actW - 24));
    deck.removeFromLeft (12);
    outputPanel_.setBounds (deck.removeFromLeft (outW));
    deck.removeFromLeft (12);
    actionsPanel_.setBounds (deck);

    {
        auto pc = protectionPanel_.getContentBounds();
        modeControl_.setBounds (pc.removeFromTop (26).withWidth (juce::jmin (348, pc.getWidth())));
        pc.removeFromTop (10);
        auto row = pc;
        const int kw = juce::jmax (54, row.getWidth() / 4);
        auto place = [&] (juce::Slider& s, juce::Label& l)
        {
            auto col = row.removeFromLeft (kw);
            l.setBounds (col.removeFromTop (14));
            s.setBounds (col);
        };
        place (strengthSlider_, strengthLabel_);
        place (sensitivitySlider_, sensitivityLabel_);
        place (maxCutSlider_, maxCutLabel_);
        place (recoverySlider_, recoveryLabel_);
    }

    {
        auto oc = outputPanel_.getContentBounds();
        stereoLinkButton_.setBounds (oc.removeFromBottom (20));
        oc.removeFromBottom (4);
        auto row = oc;
        const int ow = juce::jmax (54, row.getWidth() / 2);
        auto place = [&] (juce::Slider& s, juce::Label& l)
        {
            auto col = row.removeFromLeft (ow);
            l.setBounds (col.removeFromTop (14));
            s.setBounds (col);
        };
        place (inputTrimSlider_, inputTrimLabel_);
        place (outputTrimSlider_, outputTrimLabel_);
    }

    {
        auto ac = actionsPanel_.getContentBounds();
        freezeButton_.setBounds (ac.removeFromTop (32));
        ac.removeFromTop (8);
        clearButton_.setBounds (ac.removeFromTop (32));
        ac.removeFromTop (8);
        advancedButton_.setBounds (ac.removeFromTop (32));
    }

    if (advancedVisible_)
    {
        auto ac = advancedPanel_.getContentBounds();
        const int aw = juce::jmax (54, ac.getWidth() / 4);
        auto place = [&] (juce::Slider& s, juce::Label& l)
        {
            auto col = ac.removeFromLeft (aw);
            l.setBounds (col.removeFromTop (14));
            s.setBounds (col);
        };
        place (minFreqSlider_, minFreqLabel_);
        place (maxFreqSlider_, maxFreqLabel_);
        place (maxNotchesSlider_, maxNotchesLabel_);
        place (qBiasSlider_, qBiasLabel_);
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
                                for (auto* p : proc_.getParameters())
                                    p->setValueNotifyingHost (p->getDefaultValue());
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
