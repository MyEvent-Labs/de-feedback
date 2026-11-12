#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "state/StateIO.h"

#include <cmath>

namespace defeedback
{

namespace
{
    constexpr int kMargin = 16;
    constexpr int kFooterH = 26;
    constexpr int kDockH = 124;
    constexpr int kAdvancedH = 108;
    constexpr int kMeterW = 98;

    juce::Font brandFont() { return DeFeedbackLookAndFeel::typeface (15.0f, true).withExtraKerningFactor (0.26f); }
    juce::Font linkFont()  { return DeFeedbackLookAndFeel::typeface (13.5f); }
}

DeFeedbackEditor::DeFeedbackEditor (DeFeedbackProcessor& processor)
    : juce::AudioProcessorEditor (&processor),
      proc_ (processor),
      spectrum_ (processor),
      meter_ (processor)
{
    Palette::loadSavedSkin();
    setLookAndFeel (&lnf_);

    addAndMakeVisible (spectrum_);
    addAndMakeVisible (meter_);

    menuButton_.onClick = [this] { showMenu(); };
    addAndMakeVisible (menuButton_);

    // Protection mode.
    addAndMakeVisible (modeControl_);
    if (auto* modeParam = proc_.apvts.getParameter (parameters::mode))
    {
        modeAttachment_ = std::make_unique<juce::ParameterAttachment> (*modeParam,
            [this] (float v) { modeControl_.setSelectedIndex ((int) std::lround (v), juce::dontSendNotification); });
        modeAttachment_->sendInitialUpdate();
        modeControl_.onChange = [this] (int idx) { modeAttachment_->setValueAsCompleteGesture ((float) idx); };
    }

    // Dock knobs.
    configureKnob (strengthSlider_, strengthLabel_, "STRENGTH", strengthAttach_, parameters::strength);
    configureKnob (sensitivitySlider_, sensitivityLabel_, "SENSITIVITY", sensitivityAttach_, parameters::sensitivity);
    configureKnob (maxCutSlider_, maxCutLabel_, "MAX CUT", maxCutAttach_, parameters::maxCutDb);
    configureKnob (recoverySlider_, recoveryLabel_, "RECOVERY", recoveryAttach_, parameters::recoverySeconds);
    configureKnob (inputTrimSlider_, inputTrimLabel_, "IN TRIM", inputTrimAttach_, parameters::inputTrimDb);
    configureKnob (outputTrimSlider_, outputTrimLabel_, "OUT TRIM", outputTrimAttach_, parameters::outputTrimDb);

    // Advanced drawer.
    configureKnob (minFreqSlider_, minFreqLabel_, "MIN FREQ", minFreqAttach_, parameters::minFrequencyHz);
    configureKnob (maxFreqSlider_, maxFreqLabel_, "MAX FREQ", maxFreqAttach_, parameters::maxFrequencyHz);
    configureKnob (maxNotchesSlider_, maxNotchesLabel_, "NOTCHES", maxNotchesAttach_, parameters::maxNotches);
    configureKnob (qBiasSlider_, qBiasLabel_, "Q BIAS", qBiasAttach_, parameters::qBias);

    stereoLinkAttach_ = std::make_unique<ButtonAttachment> (proc_.apvts, parameters::stereoLink, stereoLinkButton_);
    stereoLinkButton_.setClickingTogglesState (true);
    addAndMakeVisible (stereoLinkButton_);

    // Actions.
    freezeAttach_ = std::make_unique<ButtonAttachment> (proc_.apvts, parameters::freeze, freezeButton_);
    freezeButton_.setClickingTogglesState (true);
    addAndMakeVisible (freezeButton_);

    clearButton_.onClick = [this] { proc_.requestClearNotches(); };
    addAndMakeVisible (clearButton_);

    advancedButton_.setClickingTogglesState (true);
    advancedButton_.onClick = [this] { setAdvancedVisible (advancedButton_.getToggleState()); };
    addAndMakeVisible (advancedButton_);

    bypassAttach_ = std::make_unique<ButtonAttachment> (proc_.apvts, parameters::bypass, bypassButton_);
    bypassButton_.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton_);

    // Footer: credit link, version, engine status.
    creditLink_.setButtonText ("Developed by MyEvent Labs");
    creditLink_.setURL (juce::URL (DEFEEDBACK_HOMEPAGE));
    creditLink_.setFont (linkFont(), false, juce::Justification::centredLeft);
    addAndMakeVisible (creditLink_);

    versionLabel_.setText ("v" DEFEEDBACK_VERSION " ALPHA", juce::dontSendNotification);
    versionLabel_.setFont (DeFeedbackLookAndFeel::mono (10.5f));
    addAndMakeVisible (versionLabel_);

    statusLabel_.setFont (DeFeedbackLookAndFeel::mono (10.5f));
    statusLabel_.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (statusLabel_);

    applySkin();
    setAdvancedVisible (false);

    setResizable (true, true);
    setResizeLimits (900, 600, 1600, 1000);
    setSize (1080, 700);

    startTimerHz (12);
}

DeFeedbackEditor::~DeFeedbackEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DeFeedbackEditor::configureKnob (juce::Slider& slider, juce::Label& label, const juce::String& name,
                                      std::unique_ptr<SliderAttachment>& attachmentPtr, const juce::String& paramId)
{
    // The value is drawn inside the ring by the look-and-feel, so no text box.
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRepaintsOnMouseActivity (true);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (DeFeedbackLookAndFeel::caps (10.5f));
    addAndMakeVisible (label);

    attachmentPtr = std::make_unique<SliderAttachment> (proc_.apvts, paramId, slider);

    // Show the parameter's own formatted text (with unit), e.g. "9.0 dB".
    if (auto* param = proc_.apvts.getParameter (paramId))
    {
        const auto unit = param->getLabel();
        slider.textFromValueFunction = [param, unit] (double v)
        {
            const auto value = param->getText (param->convertTo0to1 ((float) v), 0);
            return unit.isEmpty() ? value : value + " " + unit;
        };
    }
}

void DeFeedbackEditor::setAdvancedVisible (bool shouldShow)
{
    advancedVisible_ = shouldShow;

    for (auto* c : std::initializer_list<juce::Component*> { &minFreqSlider_, &maxFreqSlider_, &maxNotchesSlider_, &qBiasSlider_,
                                                             &minFreqLabel_, &maxFreqLabel_, &maxNotchesLabel_, &qBiasLabel_,
                                                             &stereoLinkButton_ })
        c->setVisible (shouldShow);

    resized();
    repaint();
}

void DeFeedbackEditor::applySkin()
{
    // Colours that components cache, as opposed to reading the palette while painting.
    lnf_.refreshColours();
    bypassButton_.setColour (juce::TextButton::buttonOnColourId, Palette::danger);

    for (auto* child : getChildren())
        if (auto* label = dynamic_cast<juce::Label*> (child))
            label->setColour (juce::Label::textColourId, Palette::textDim);

    sendLookAndFeelChange();
    repaint();
}

void DeFeedbackEditor::timerCallback()
{
    const auto& snap = proc_.getUiSnapshot();
    const auto sep = "  " + middleDot() + "  ";

    juce::String status;
    status << juce::String (juce::roundToInt (proc_.getSampleRate() / 1000.0)) << " kHz" << sep
           << proc_.getBlockSize() << " smp" << sep
           << "latency " << proc_.getLatencySamples() << sep
           << "load " << juce::String (snap.dspLoad.load() * 100.0f, 1) << "%";

    if (snap.overloads.load() > 0)
        status << sep << "OVERLOAD";

    statusLabel_.setText (status, juce::dontSendNotification);
}

void DeFeedbackEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    const auto view = spectrum_.getBounds().toFloat();
    g.setColour (Palette::screen);
    g.fillRect (view);

    if (Palette::glow)
    {
        // A faint bloom in the signal colour behind the analyser.
        const auto cx = view.getCentreX();
        const auto cy = view.getY() + view.getHeight() * 0.72f;
        g.setGradientFill (juce::ColourGradient (Palette::accent.withAlpha (0.075f), cx, cy,
                                                 Palette::accent.withAlpha (0.0f), cx + view.getWidth() * 0.55f, cy, true));
        g.fillRect (view);
    }

    auto drawPanel = [&] (juce::Rectangle<int> r)
    {
        g.setColour (Palette::surface);
        g.fillRoundedRectangle (r.toFloat(), Palette::radiusPanel);
        g.setColour (Palette::outline);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), Palette::radiusPanel, 1.0f);
    };

    drawPanel (dockBounds_);
    if (advancedVisible_)
        drawPanel (advancedBounds_);
}

void DeFeedbackEditor::paintOverChildren (juce::Graphics& g)
{
    g.setColour (Palette::text);
    g.setFont (brandFont());
    g.drawText ("DE-FEEDBACK", brandBounds_, juce::Justification::centredLeft, false);

    g.setColour (Palette::warning.withAlpha (0.16f));
    g.fillRoundedRectangle (chipBounds_.toFloat(), 4.0f);
    g.setColour (Palette::warning);
    g.setFont (DeFeedbackLookAndFeel::caps (10.0f));
    g.drawText ("ALPHA", chipBounds_, juce::Justification::centred, false);
}

void DeFeedbackEditor::resized()
{
    auto area = getLocalBounds();

    // Footer.
    auto footer = area.removeFromBottom (kFooterH).reduced (kMargin + 6, 0).withTrimmedBottom (4);
    creditLink_.setBounds (footer.removeFromLeft (juce::GlyphArrangement::getStringWidthInt (linkFont(), creditLink_.getButtonText()) + 8));
    footer.removeFromLeft (10);
    versionLabel_.setBounds (footer.removeFromLeft (120));
    statusLabel_.setBounds (footer);

    // Dock and (optional) advanced drawer.
    area.removeFromBottom (4);
    dockBounds_ = area.removeFromBottom (kDockH).reduced (kMargin, 0);

    if (advancedVisible_)
    {
        area.removeFromBottom (8);
        advancedBounds_ = area.removeFromBottom (kAdvancedH).reduced (kMargin, 0);
    }

    area.removeFromBottom (6);

    // Analyser fills everything else; meters and brand float over it.
    spectrum_.setBounds (area);
    meter_.setBounds (area.getRight() - kMargin - kMeterW, area.getY() + 16, kMeterW, area.getHeight() - 44);
    spectrum_.setContentRight (area.getWidth() - kMargin - kMeterW - 18);

    brandBounds_ = { 22, 16, juce::GlyphArrangement::getStringWidthInt (brandFont(), "DE-FEEDBACK") + 4, 24 };
    chipBounds_ = { brandBounds_.getRight() + 10, 19, 50, 18 };
    menuButton_.setBounds (chipBounds_.getRight() + 10, 16, 62, 24);

    auto placeKnob = [] (juce::Rectangle<int> column, juce::Slider& s, juce::Label& l, float scale)
    {
        l.setBounds (column.removeFromBottom (16));
        s.setBounds (column.withSizeKeepingCentre (column.getWidth(), juce::roundToInt ((float) column.getHeight() * scale)));
    };

    // Dock: mode | protection knobs | trims | actions.
    {
        auto dock = dockBounds_.reduced (16, 10);

        modeControl_.setBounds (dock.removeFromLeft (132));
        dock.removeFromLeft (14);

        auto buttons = dock.removeFromRight (176).withSizeKeepingCentre (176, 76);
        dock.removeFromRight (10);

        auto top = buttons.removeFromTop (34);
        buttons.removeFromTop (8);
        freezeButton_.setBounds (top.removeFromLeft (84));
        clearButton_.setBounds (top.withTrimmedLeft (8));
        advancedButton_.setBounds (buttons.removeFromLeft (84));
        bypassButton_.setBounds (buttons.withTrimmedLeft (8));

        const float unit = juce::jmin (132.0f, (float) dock.getWidth() / 5.6f);
        auto column = [&] (float weight) { return dock.removeFromLeft (juce::roundToInt (unit * weight)); };

        placeKnob (column (1.0f), strengthSlider_, strengthLabel_, 1.0f);
        placeKnob (column (1.0f), sensitivitySlider_, sensitivityLabel_, 1.0f);
        placeKnob (column (1.0f), maxCutSlider_, maxCutLabel_, 1.0f);
        placeKnob (column (1.0f), recoverySlider_, recoveryLabel_, 1.0f);
        placeKnob (column (0.8f), inputTrimSlider_, inputTrimLabel_, 0.8f);
        placeKnob (column (0.8f), outputTrimSlider_, outputTrimLabel_, 0.8f);
    }

    if (advancedVisible_)
    {
        auto drawer = advancedBounds_.reduced (16, 8);
        stereoLinkButton_.setBounds (drawer.removeFromRight (150).withSizeKeepingCentre (150, 22));

        const int w = juce::jmin (132, drawer.getWidth() / 4);
        placeKnob (drawer.removeFromLeft (w), minFreqSlider_, minFreqLabel_, 1.0f);
        placeKnob (drawer.removeFromLeft (w), maxFreqSlider_, maxFreqLabel_, 1.0f);
        placeKnob (drawer.removeFromLeft (w), maxNotchesSlider_, maxNotchesLabel_, 1.0f);
        placeKnob (drawer.removeFromLeft (w), qBiasSlider_, qBiasLabel_, 1.0f);
    }
}

void DeFeedbackEditor::showAbout()
{
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::WarningIcon,
        "MyEvent Labs De-Feedback v" DEFEEDBACK_VERSION " ALPHA",
        "Experimental adaptive feedback-control plugin.\n\n"
        "ALPHA SOFTWARE: Test under controlled conditions before any live event. "
        "This release is not validated for show-critical use and does not replace "
        "correct loudspeaker placement, gain structure, system tuning or a competent "
        "live-sound engineer.\n\n"
        "This build is a deterministic signal processor. It is not AI-powered and makes "
        "no guaranteed gain-before-feedback improvement claim. v" DEFEEDBACK_VERSION " introduces no "
        "intentional lookahead.\n\n"
        "Free software, distributed under GPLv3 (see THIRD_PARTY_NOTICES.md).\n\n"
        DEFEEDBACK_HOMEPAGE);
}

void DeFeedbackEditor::showMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Save Preset...");
    menu.addItem (2, "Load Preset...");
    menu.addSeparator();
    menu.addItem (3, "Reset to Defaults");
    menu.addSeparator();

    juce::PopupMenu skins;
    int skinId = 10;
    for (auto* name : { "Scope", "Console", "Instrument" })
    {
        skins.addItem (skinId, name, true, (int) Palette::skin() == skinId - 10);
        ++skinId;
    }
    menu.addSubMenu ("Skin", skins);

    menu.addSeparator();
    menu.addItem (4, "About De-Feedback");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuButton_),
                        [this] (int result)
                        {
                            if (result == 1) savePreset();
                            else if (result == 2) loadPreset();
                            else if (result == 3)
                                for (auto* p : proc_.getParameters())
                                    p->setValueNotifyingHost (p->getDefaultValue());
                            else if (result == 4) showAbout();
                            else if (result >= 10)
                            {
                                Palette::setSkin ((Skin) (result - 10), true);
                                applySkin();
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
