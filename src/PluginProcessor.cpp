#include "PluginProcessor.h"

#ifndef DEFEEDBACK_HEADLESS
#include "PluginEditor.h"
#endif

#include "state/StateIO.h"

#include <cmath>

namespace defeedback
{

namespace
{
    using namespace parameters;

    struct ModeProfile
    {
        int   onFrames = 5;
        float onConfidence = 0.55f;
        float offConfidence = 0.30f;
        float baseProminenceDb = 10.0f;
        float cutScale = 1.0f;
        float attackSeconds = 0.02f;
        bool  requireGrowth = false;
    };

    ModeProfile profileFor (ProtectionMode protectionMode) noexcept
    {
        switch (protectionMode)
        {
            case ProtectionMode::conservative: return { 8, 0.62f, 0.34f, 12.0f, 0.70f, 0.05f, true };
            case ProtectionMode::aggressive:   return { 3, 0.45f, 0.28f,  8.0f, 1.00f, 0.01f, false };
            case ProtectionMode::normal:
            default:                           return { 5, 0.55f, 0.30f, 10.0f, 1.00f, 0.02f, false };
        }
    }

    void sanitiseBuffer (juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int n = 0; n < buffer.getNumSamples(); ++n)
                if (! std::isfinite (data[n]))
                    data[n] = 0.0f;
        }
    }
}

DeFeedbackProcessor::DeFeedbackProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    peaks_.reserve (64);
}

juce::AudioProcessorValueTreeState::ParameterLayout DeFeedbackProcessor::createParameterLayout()
{
    using namespace parameters;
    using FloatParam = juce::AudioParameterFloat;
    using Range = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { bypass, 1 }, "Bypass", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { mode, 1 }, "Protection Mode",
        juce::StringArray { "Conservative", "Normal", "Aggressive" }, 0));
    const auto dbAttr = juce::AudioParameterFloatAttributes().withLabel ("dB");
    const auto hzAttr = juce::AudioParameterFloatAttributes().withLabel ("Hz");
    const auto sAttr  = juce::AudioParameterFloatAttributes().withLabel ("s");

    params.push_back (std::make_unique<FloatParam> (juce::ParameterID { strength, 1 }, "Strength", Range (0.25f, 2.0f, 0.01f), 1.0f));
    params.push_back (std::make_unique<FloatParam> (juce::ParameterID { sensitivity, 1 }, "Sensitivity", Range (-6.0f, 6.0f, 0.1f), 0.0f, dbAttr));
    params.push_back (std::make_unique<FloatParam> (juce::ParameterID { maxCutDb, 1 }, "Max Cut", Range (0.0f, 18.0f, 0.1f), 9.0f, dbAttr));
    params.push_back (std::make_unique<FloatParam> (juce::ParameterID { recoverySeconds, 1 }, "Recovery",
        Range (0.5f, 30.0f, 0.1f, 0.4f), 4.0f, sAttr));
    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { freeze, 1 }, "Freeze", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { stereoLink, 1 }, "Stereo Link", true));
    params.push_back (std::make_unique<FloatParam> (juce::ParameterID { minFrequencyHz, 1 }, "Min Frequency",
        Range (20.0f, 2000.0f, 1.0f, 0.5f), 80.0f, hzAttr));
    params.push_back (std::make_unique<FloatParam> (juce::ParameterID { maxFrequencyHz, 1 }, "Max Frequency",
        Range (2000.0f, 20000.0f, 1.0f, 0.4f), 18000.0f, hzAttr));
    params.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { maxNotches, 1 }, "Max Notches", 1, 12, 12));
    params.push_back (std::make_unique<FloatParam> (juce::ParameterID { qBias, 1 }, "Q Bias", Range (0.5f, 2.0f, 0.01f), 1.0f));

    params.push_back (std::make_unique<FloatParam> (juce::ParameterID { inputTrimDb, 1 }, "Input Trim", Range (-24.0f, 24.0f, 0.1f), 0.0f, dbAttr));
    params.push_back (std::make_unique<FloatParam> (juce::ParameterID { outputTrimDb, 1 }, "Output Trim", Range (-24.0f, 24.0f, 0.1f), 0.0f, dbAttr));

    return { params.begin(), params.end() };
}

void DeFeedbackProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numCh = juce::jmax (1, getTotalNumInputChannels());

    setLatencySamples (0);

    analysisBuffer_.setSize (numCh, juce::jmax (1, samplesPerBlock), false, true, true);
    dryBuffer_.assign ((size_t) (numCh * juce::jmax (1, samplesPerBlock)), 0.0f);
    analysisMono_.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);

    detector_.prepare (sampleRate, numCh, DetectorConfig {});
    bank_.prepare (sampleRate, samplesPerBlock, numCh);
    analyzer_.prepare (sampleRate, 11);

    inputGain_.reset (sampleRate, 0.02);
    outputGain_.reset (sampleRate, 0.02);
    bypassMix_.reset (sampleRate, 0.02);
    bypassMix_.setCurrentAndTargetValue (1.0f);
}

void DeFeedbackProcessor::releaseResources()
{
    detector_.reset();
    bank_.reset();
    analyzer_.reset();
}

bool DeFeedbackProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void DeFeedbackProcessor::updateFromParameters()
{
    using namespace parameters;

    const auto sens = apvts.getRawParameterValue (sensitivity)->load();
    const auto modeIdx = (int) apvts.getRawParameterValue (mode)->load();
    const auto protectionMode = static_cast<ProtectionMode> (juce::jlimit (0, 2, modeIdx));

    const auto prof = profileFor (protectionMode);

    DetectorConfig dc;
    dc.minFrequencyHz = apvts.getRawParameterValue (minFrequencyHz)->load();
    dc.maxFrequencyHz = apvts.getRawParameterValue (maxFrequencyHz)->load();
    dc.onFrames = prof.onFrames;
    dc.onConfidence = prof.onConfidence;
    dc.offConfidence = prof.offConfidence;
    dc.requireGrowth = prof.requireGrowth;
    dc.minProminenceDb = juce::jlimit (3.0f, 30.0f, prof.baseProminenceDb - sens);
    dc.strongProminenceDb = dc.minProminenceDb + 12.0f;
    detector_.setConfig (dc);

    AdaptiveNotchBank::Config bc;
    bc.linked = apvts.getRawParameterValue (stereoLink)->load() > 0.5f;
    bc.minFrequencyHz = dc.minFrequencyHz;
    bc.maxFrequencyHz = dc.maxFrequencyHz;
    bc.maxCutDb = juce::jmin (18.0f, apvts.getRawParameterValue (maxCutDb)->load()) * prof.cutScale;
    bc.strength = apvts.getRawParameterValue (strength)->load();
    bc.qBias = apvts.getRawParameterValue (qBias)->load();
    bc.maxNotchesPerLane = (int) apvts.getRawParameterValue (maxNotches)->load();
    bc.attackSeconds = prof.attackSeconds;
    bc.releaseSeconds = juce::jmax (0.2f, apvts.getRawParameterValue (recoverySeconds)->load());
    bank_.setConfig (bc);
    bank_.setFreeze (apvts.getRawParameterValue (freeze)->load() > 0.5f);

    inputGain_.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (inputTrimDb)->load()));
    outputGain_.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (outputTrimDb)->load()));

    const bool bypassed = apvts.getRawParameterValue (bypass)->load() > 0.5f;
    bypassMix_.setTargetValue (bypassed ? 0.0f : 1.0f);
}

void DeFeedbackProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numCh <= 0 || numSamples <= 0)
        return;

    const auto startTicks = juce::Time::getHighResolutionTicks();

    if (clearRequested_.exchange (false))
        bank_.clearAll();

    updateFromParameters();

    sanitiseBuffer (buffer);

    if ((int) dryBuffer_.size() < numCh * numSamples)
        dryBuffer_.assign ((size_t) (numCh * numSamples), 0.0f);

    for (int ch = 0; ch < numCh; ++ch)
        std::copy (buffer.getReadPointer (ch), buffer.getReadPointer (ch) + numSamples,
                   dryBuffer_.begin() + (size_t) (ch * numSamples));

    // Input trim (per sample, shared across channels).
    for (int n = 0; n < numSamples; ++n)
    {
        const float g = inputGain_.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
            buffer.getWritePointer (ch)[n] *= g;
    }

    const bool linked = apvts.getRawParameterValue (parameters::stereoLink)->load() > 0.5f;

    // Analysis tap, before suppression, so the detector sees the true input.
    if (analysisBuffer_.getNumChannels() != numCh || analysisBuffer_.getNumSamples() != numSamples)
        analysisBuffer_.setSize (numCh, numSamples, false, true, true);

    for (int ch = 0; ch < numCh; ++ch)
        analysisBuffer_.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    const float* analysisPtrs[2] { nullptr, nullptr };
    if (linked)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                sum += buffer.getReadPointer (ch)[n];
            analysisMono_[(size_t) n] = sum / (float) numCh;
        }
        analysisPtrs[0] = analysisMono_.data();
        detector_.process (analysisPtrs, 1, numSamples, true);
    }
    else
    {
        for (int ch = 0; ch < juce::jmin (2, numCh); ++ch)
            analysisPtrs[ch] = analysisBuffer_.getReadPointer (ch);
        detector_.process (analysisPtrs, numCh, numSamples, false);
    }

    peaks_.clear();
    const int numPeaks = detector_.getNumPeaks();
    for (int i = 0; i < numPeaks; ++i)
        peaks_.push_back (detector_.getPeak (i));

    bank_.update (peaks_.data(), numPeaks);
    bank_.process (buffer.getArrayOfWritePointers(), numCh, numSamples);

    // Output trim + click-free bypass crossfade.
    for (int n = 0; n < numSamples; ++n)
    {
        const float og = outputGain_.getNextValue();
        const float mix = bypassMix_.getNextValue();

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            const float wet = w[n] * og;
            const float dry = dryBuffer_[(size_t) (ch * numSamples + n)];
            w[n] = dry * (1.0f - mix) + wet * mix;
        }
    }

    sanitiseBuffer (buffer);

    // Publish meters / notch display.
    {
        float dryPeak = 0.0f, wetPeak = 0.0f;
        double drySum = 0.0, wetSum = 0.0;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* d = dryBuffer_.data() + (size_t) (ch * numSamples);
            const float* w = buffer.getReadPointer (ch);

            for (int n = 0; n < numSamples; ++n)
            {
                dryPeak = juce::jmax (dryPeak, std::abs (d[n]));
                wetPeak = juce::jmax (wetPeak, std::abs (w[n]));
                drySum += (double) d[n] * d[n];
                wetSum += (double) w[n] * w[n];
            }
        }

        const double count = (double) juce::jmax (1, numCh * numSamples);
        ui_.inputLevelDb.store (juce::Decibels::gainToDecibels (dryPeak, -100.0f));
        ui_.outputLevelDb.store (juce::Decibels::gainToDecibels (wetPeak, -100.0f));
        ui_.inputRmsDb.store (juce::Decibels::gainToDecibels ((float) std::sqrt (drySum / count), -100.0f));
        ui_.outputRmsDb.store (juce::Decibels::gainToDecibels ((float) std::sqrt (wetSum / count), -100.0f));
    }

    const int active = bank_.getActiveCount();
    ui_.activeNotches.store (active);

    float risk = 0.0f;
    for (const auto& p : peaks_)
        risk = juce::jmax (risk, p.confidence);
    ui_.risk.store (risk);

    {
        NotchInfo info;
        float reduction = 0.0f;
        const int displayCount = juce::jmin (UiSnapshot::kMaxNotchDisplay, bank_.getActiveCount());
        for (int i = 0; i < displayCount; ++i)
        {
            if (bank_.getNotchInfo (i, info))
            {
                ui_.notchFreq[i].store (info.frequencyHz);
                ui_.notchGain[i].store (info.gainDb);
                ui_.notchQ[i].store (info.q);
                reduction = juce::jmax (reduction, std::abs (info.gainDb));
            }
        }

        ui_.reductionDb.store (reduction);
        for (int i = displayCount; i < UiSnapshot::kMaxNotchDisplay; ++i)
        {
            ui_.notchFreq[i].store (0.0f);
            ui_.notchGain[i].store (0.0f);
            ui_.notchQ[i].store (0.0f);
        }
    }

    analyzer_.pushBlock (buffer.getReadPointer (0), numSamples);

    const double blockSeconds = (double) numSamples / juce::jmax (1.0, getSampleRate());
    const double elapsed = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - startTicks);
    const float load = blockSeconds > 0.0 ? (float) (elapsed / blockSeconds) : 0.0f;
    ui_.dspLoad.store (ui_.dspLoad.load() * 0.9f + load * 0.1f);
    if (load > 1.0f)
        ui_.overloads.fetch_add (1);
}

juce::AudioProcessorEditor* DeFeedbackProcessor::createEditor()
{
#ifndef DEFEEDBACK_HEADLESS
    return new DeFeedbackEditor (*this);
#else
    return nullptr; // headless test build
#endif
}

void DeFeedbackProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    const auto xml = stateToXml (state);

    if (xml.isNotEmpty())
        destData.replaceAll (xml.toRawUTF8(), xml.getNumBytesAsUTF8());
}

void DeFeedbackProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const juce::String text (static_cast<const char*> (data), (size_t) sizeInBytes);

    juce::Result result = juce::Result::fail ("not parsed");
    auto tree = stateFromXml (text, result);

    if (result.failed() || ! tree.isValid())
        return; // fail safe: keep current/default state

    apvts.replaceState (tree);
}

} // namespace defeedback

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new defeedback::DeFeedbackProcessor();
}
