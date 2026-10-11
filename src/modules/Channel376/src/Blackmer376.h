#pragma once
#include <cmath>
#include <algorithm>
#if defined(CHANNEL376_EXPERIMENTAL_EXP_TABLE)
#include "ExponentialTable376.h"
#endif
namespace channel376 {
// 2150-class current-in/current-out transfer, control pin3.
// 6.1mV/dB: THAT2150/2252 typical at TA25C, self-heated die ~35C.
// Jands 72-0194 cross-reference supports the 2150 family, not die identity.
struct Blackmer376 {
    static constexpr double voltsPerDb=.0061;
    static constexpr double log10=2.302585092994045684;
    static constexpr double twoThermalVolts=20*voltsPerDb/log10;
    static constexpr double compVoltsPerDb=voltsPerDb*36500/1000;
    static double gain(double cv)noexcept {
#if defined(CHANNEL376_EXPERIMENTAL_EXP_TABLE)
        return exponentialTable376.evaluate(std::clamp(-cv/twoThermalVolts,-600.,600.));
#else
        return std::exp(std::clamp(-cv/twoThermalVolts,-600.,600.));
#endif
    }
    static double outputCurrent(double inputCurrent,double cv)noexcept {
        return inputCurrent*gain(cv); // Delivered into U30B; transimpedance inverts.
    }
};
}
