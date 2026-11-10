#include "dsp/Biquad.h"

#include <algorithm>
#include <cmath>

namespace defeedback
{

Biquad::Coeffs Biquad::makePeak (double sampleRate, double freqHz, double q, double gainDb) noexcept
{
    Coeffs c {};

    if (! std::isfinite (sampleRate) || sampleRate <= 0.0)
        return c;

    const auto nyquist = sampleRate * 0.5;
    freqHz = std::clamp (freqHz, 10.0, std::max (10.0, nyquist * 0.98));
    q      = std::clamp (q, 0.1, 100.0);
    gainDb = std::clamp (gainDb, -80.0, 24.0);

    const auto A     = std::pow (10.0, gainDb / 40.0);
    const auto w0    = 2.0 * 3.14159265358979323846 * freqHz / sampleRate;
    const auto cosw0 = std::cos (w0);
    const auto sinw0 = std::sin (w0);
    const auto alpha = sinw0 / (2.0 * q);

    const auto a0 = 1.0 + alpha / A;
    if (! std::isfinite (a0) || a0 == 0.0)
        return c;

    c.b0 = (1.0 + alpha * A) / a0;
    c.b1 = (-2.0 * cosw0) / a0;
    c.b2 = (1.0 - alpha * A) / a0;
    c.a1 = (-2.0 * cosw0) / a0;
    c.a2 = (1.0 - alpha / A) / a0;

    if (! std::isfinite (c.b0) || ! std::isfinite (c.b1) || ! std::isfinite (c.b2)
        || ! std::isfinite (c.a1) || ! std::isfinite (c.a2))
        return Coeffs {};

    return c;
}

} // namespace defeedback
