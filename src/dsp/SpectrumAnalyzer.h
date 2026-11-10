#pragma once

#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <memory>
#include <vector>

namespace defeedback
{

/** Lock-free bridge for a UI spectrum display.

    The audio thread pushes mono samples into a single-producer /
    single-consumer FIFO. The UI thread drains them and performs the FFT
    itself, so no FFT or allocation ever happens on the audio thread and the
    display never blocks audio.

    If the FIFO overflows the oldest samples are dropped; the display simply
    shows a slightly gapped spectrogram rather than stalling.
*/
class SpectrumAnalyzer
{
public:
    SpectrumAnalyzer() = default;

    void prepare (double sampleRate, int fftOrder = 11);
    void reset() noexcept;

    /** Audio thread. mono must contain numSamples valid samples. */
    void pushBlock (const float* mono, int numSamples) noexcept;

    /** UI thread. Recomputes the spectrum when new samples are available.
        Returns true and fills magDbOut (size getNumBins()) when updated. */
    bool computeSpectrum (std::vector<float>& magDbOut);

    double getSampleRate() const noexcept { return sampleRate_; }
    int getFftSize() const noexcept { return fftSize_; }
    int getNumBins() const noexcept { return numBins_; }

private:
    int fifoSize() const noexcept;

    double sampleRate_ = 48000.0;
    int    fftOrder_ = 11;
    int    fftSize_ = 2048;
    int    numBins_ = 1025;

    juce::AbstractFifo fifo_ { 1 };
    std::vector<float> fifoBuffer_;

    std::unique_ptr<juce::dsp::FFT> fft_;
    std::vector<float> window_;
    std::vector<float> fftData_;

    // UI-side accumulation.
    std::vector<float> uiRing_;
    std::vector<float> drain_;
    int uiWrite_ = 0;
    int uiFilled_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
};

} // namespace defeedback
