#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/AdaptiveNotchBank.h"
#include "dsp/FeedbackDetector.h"
#include "dsp/Parameters.h"
#include "dsp/SpectrumAnalyzer.h"

#include <atomic>
#include <memory>
#include <vector>

namespace defeedback
{

/** Probability-like snapshot for the UI. All fields are safe to read from
    the message thread; they are plain atomics, not views into DSP state. */
struct UiSnapshot
{
    static constexpr int kMaxNotchDisplay = AdaptiveNotchBank::kMaxNotchesPerLane * AdaptiveNotchBank::kMaxLanes;

    std::atomic<int>   activeNotches { 0 };
    std::atomic<float> inputLevelDb { -100.0f };
    std::atomic<float> outputLevelDb { -100.0f };
    std::atomic<float> inputRmsDb { -100.0f };
    std::atomic<float> outputRmsDb { -100.0f };
    std::atomic<float> reductionDb { 0.0f };   // deepest active notch cut
    std::atomic<float> risk { 0.0f };
    std::atomic<float> dspLoad { 0.0f };
    std::atomic<int>   overloads { 0 };

    std::atomic<float> notchFreq[kMaxNotchDisplay];
    std::atomic<float> notchGain[kMaxNotchDisplay];
    std::atomic<float> notchQ[kMaxNotchDisplay];

    UiSnapshot()
    {
        for (int i = 0; i < kMaxNotchDisplay; ++i)
        {
            notchFreq[i].store (0.0f);
            notchGain[i].store (0.0f);
            notchQ[i].store (0.0f);
        }
    }
};

class DeFeedbackProcessor : public juce::AudioProcessor
{
public:
    DeFeedbackProcessor();
    ~DeFeedbackProcessor() override = default;

    // --- AudioProcessor -----------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- De-Feedback --------------------------------------------------------
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    UiSnapshot& getUiSnapshot() noexcept { return ui_; }
    const UiSnapshot& getUiSnapshot() const noexcept { return ui_; }

    SpectrumAnalyzer& getAnalyzer() noexcept { return analyzer_; }

    /** Message-thread helper: clears all notches via the same route the UI
        uses. Safe because it only flips an atomic flag consumed on the audio
        thread. */
    void requestClearNotches() noexcept { clearRequested_.store (true); }

private:
    void updateFromParameters();
    void readMetersAndPublish (const juce::AudioBuffer<float>& buffer, double blockSeconds) noexcept;

    FeedbackDetector detector_;
    AdaptiveNotchBank bank_;
    SpectrumAnalyzer analyzer_;

    std::vector<float> dryBuffer_;
    std::vector<float> analysisMono_;
    std::vector<DetectedPeak> peaks_;
    std::atomic<bool> clearRequested_ { false };

    juce::SmoothedValue<float> inputGain_, outputGain_, bypassMix_;
    juce::AudioBuffer<float> analysisBuffer_;

    UiSnapshot ui_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeFeedbackProcessor)
};

} // namespace defeedback
