#pragma once
#include <cmath>

namespace c286::calibration {
// Original 286, sheet 3: +/-15 V rails, R1=R3=4.53k, R2=20k.
// Symmetric comparator reference; use this ideal clip threshold as digital peak 1.
inline constexpr double clipPeakVolts=15.0*20000.0/(20000.0+2.0*4530.0);
inline constexpr double dbuReferenceVolts=0.775;
inline const float fullScaleSineDbu=static_cast<float>(20*std::log10(clipPeakVolts/(std::sqrt(2.0)*dbuReferenceVolts)));
inline constexpr float rmsToPeakDb=3.0102999566f;
}
