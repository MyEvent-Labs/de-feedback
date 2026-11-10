#include "dsp/SpectrumAnalyzer.h"

#include <algorithm>
#include <cmath>

namespace defeedback
{

void SpectrumAnalyzer::prepare (double sampleRate, int fftOrder)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    fftOrder_   = juce::jlimit (8, 14, fftOrder);
    fftSize_    = 1 << fftOrder_;
    numBins_    = fftSize_ / 2 + 1;

    fft_ = std::make_unique<juce::dsp::FFT> (fftOrder_);

    fifoBuffer_.assign ((size_t) fifoSize(), 0.0f);
    fifo_.setTotalSize (fifoSize());

    window_.resize ((size_t) fftSize_);
    for (int n = 0; n < fftSize_; ++n)
        window_[(size_t) n] = (float) (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * (double) n / (double) (fftSize_ - 1)));

    fftData_.assign ((size_t) (fftSize_ * 2), 0.0f);

    uiRing_.assign ((size_t) fftSize_, 0.0f);
    drain_.assign ((size_t) fifoSize(), 0.0f);

    reset();
}

int SpectrumAnalyzer::fifoSize() const noexcept
{
    return fftSize_ * 2;
}

void SpectrumAnalyzer::reset() noexcept
{
    fifo_.reset();
    std::fill (fifoBuffer_.begin(), fifoBuffer_.end(), 0.0f);
    std::fill (uiRing_.begin(), uiRing_.end(), 0.0f);
    uiWrite_  = 0;
    uiFilled_ = 0;
}

void SpectrumAnalyzer::pushBlock (const float* mono, int numSamples) noexcept
{
    if (mono == nullptr || numSamples <= 0)
        return;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo_.prepareToWrite (numSamples, start1, size1, start2, size2);

    int written = 0;
    for (int i = 0; i < size1; ++i)
    {
        const float x = mono[written];
        fifoBuffer_[(size_t) (start1 + i)] = std::isfinite (x) ? x : 0.0f;
        ++written;
    }

    for (int i = 0; i < size2; ++i)
    {
        const float x = mono[written];
        fifoBuffer_[(size_t) (start2 + i)] = std::isfinite (x) ? x : 0.0f;
        ++written;
    }

    fifo_.finishedWrite (size1 + size2);
}

bool SpectrumAnalyzer::computeSpectrum (std::vector<float>& magDbOut)
{
    if (fft_ == nullptr)
        return false;

    const int ready = fifo_.getNumReady();
    if (ready <= 0)
        return false;

    const int toRead = juce::jmin (ready, (int) drain_.size());

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo_.prepareToRead (toRead, start1, size1, start2, size2);

    for (int i = 0; i < size1; ++i)
    {
        uiRing_[(size_t) uiWrite_] = fifoBuffer_[(size_t) (start1 + i)];
        uiWrite_ = (uiWrite_ + 1) % fftSize_;
        uiFilled_ = juce::jmin (uiFilled_ + 1, fftSize_);
    }

    for (int i = 0; i < size2; ++i)
    {
        uiRing_[(size_t) uiWrite_] = fifoBuffer_[(size_t) (start2 + i)];
        uiWrite_ = (uiWrite_ + 1) % fftSize_;
        uiFilled_ = juce::jmin (uiFilled_ + 1, fftSize_);
    }

    fifo_.finishedRead (size1 + size2);

    if (uiFilled_ < fftSize_)
        return false;

    const int start = uiWrite_; // oldest sample
    for (int n = 0; n < fftSize_; ++n)
        fftData_[(size_t) n] = uiRing_[(size_t) ((start + n) % fftSize_)] * window_[(size_t) n];

    for (int n = fftSize_; n < fftSize_ * 2; ++n)
        fftData_[(size_t) n] = 0.0f;

    fft_->performRealOnlyForwardTransform (fftData_.data(), true);

    magDbOut.resize ((size_t) numBins_);

    const float norm = 2.0f / (float) fftSize_;
    for (int b = 0; b < numBins_; ++b)
    {
        const float re = fftData_[(size_t) (2 * b)];
        const float im = fftData_[(size_t) (2 * b + 1)];
        const float mag = std::sqrt (re * re + im * im) * norm;
        magDbOut[(size_t) b] = juce::Decibels::gainToDecibels (mag, -140.0f);
    }

    uiFilled_ = fftSize_; // keep full; future reads append
    return true;
}

} // namespace defeedback
