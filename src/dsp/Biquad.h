#pragma once

#include <cmath>

namespace defeedback
{

/** Peaking (bell) EQ biquad used as a controllable-depth notch.

    A true "brick-wall" notch cannot be tuned in depth, so De-Feedback models
    each notch as a second-order peaking filter with a negative gain. This
    keeps the filter unconditionally stable, gives the user a predictable
    maximum-cut control and avoids the extremely high Q needed for a classic
    notch to reach the same depth.

    Coefficients follow the RBJ Audio EQ Cookbook and are always clamped to a
    stable region (see makePeak). Processing is Transposed Direct Form II in
    double precision; the audio path may convert to float at the edges.
*/
class Biquad
{
public:
    struct Coeffs
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    };

    /** Build peaking coefficients. freqHz is clamped to [10, 0.49*fs],
        q to [0.1, 100] and gainDb to [-80, +24]. */
    static Coeffs makePeak (double sampleRate, double freqHz, double q, double gainDb) noexcept;

    static constexpr Coeffs makeIdentity() noexcept { return {}; }

    void setCoeffs (const Coeffs& c) noexcept { coeffs = c; }
    const Coeffs& getCoeffs() const noexcept { return coeffs; }

    void reset() noexcept { z1 = 0.0; z2 = 0.0; }

    /** Replace non-finite state with zero (denormal/inf recovery). */
    void sanitiseState() noexcept
    {
        if (! std::isfinite (z1)) z1 = 0.0;
        if (! std::isfinite (z2)) z2 = 0.0;
    }

    /** One sample, Transposed Direct Form II. */
    inline double processSample (double x) noexcept
    {
        const auto y = coeffs.b0 * x + z1;
        z1 = coeffs.b1 * x - coeffs.a1 * y + z2;
        z2 = coeffs.b2 * x - coeffs.a2 * y;
        return y;
    }

private:
    Coeffs coeffs {};
    double z1 = 0.0, z2 = 0.0;
};

} // namespace defeedback
