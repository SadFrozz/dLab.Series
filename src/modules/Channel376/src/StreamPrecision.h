#pragma once
#include <algorithm>
#include <cmath>
#include <limits>

namespace channel376 {
// Selected by the host callback's actual buffer type, never from WAV metadata.
enum class StreamPrecision { float32, float64 };

// Spacing of the normal IEEE bin containing the quantizer input. At an exact
// power of two, spacing toward zero is half this spacing (except subnormals).
// Therefore this is a local dither scale, not a uniform integer PCM LSB.
inline double localQuantizationStep(double quantizerInput,
                                    StreamPrecision precision) noexcept {
    if (!std::isfinite(quantizerInput)) return 0.0;
    const int digits = precision == StreamPrecision::float32 ? 24 : 53;
    const int minimumStepExponent = precision == StreamPrecision::float32 ? -149 : -1074;
    int exponent = 0;
    std::frexp(std::abs(quantizerInput), &exponent);
    if (quantizerInput == 0.0) return std::ldexp(1.0, minimumStepExponent);
    return std::ldexp(1.0, std::max(minimumStepExponent, exponent - digits));
}

inline double noiseScaleFromQuantum16(double quantizerInput,
                                     StreamPrecision precision) noexcept {
    return std::ldexp(localQuantizationStep(quantizerInput, precision), 15);
}
} // namespace channel376
