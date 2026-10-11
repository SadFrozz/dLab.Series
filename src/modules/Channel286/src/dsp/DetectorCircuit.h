#pragma once
#include <array>
#include <cmath>

namespace c286 {
struct DetectorCircuit {
    // Normalised current through the two broadband RMS input branches.
    // The common coupling capacitor sees both branches, so they cannot be
    // modelled as two independent high-pass filters in parallel.
    double coupling, branchCap, directR, branchR;
    static DetectorCircuit forModel(bool s) noexcept {
        return s ? DetectorCircuit{.22e-6,.015e-6,75000,34630}
                 : DetectorCircuit{.22e-6,.01e-6,124000,57600};
    }
    std::array<double,3> numerator() const noexcept {
        return {coupling*branchR*branchCap,coupling*branchR/(directR+branchR),0};
    }
    std::array<double,3> denominator() const noexcept {
        return {coupling*branchR*branchCap,coupling+branchCap+branchR*branchCap/directR,1/directR};
    }
    static double deQ(bool s) noexcept {
        // Original: C132/R183 buffer followed by C133/R185 into RMS input.
        // 286s: gain-2 Sallen-Key, C110=4.7nF, C126=2.2nF, equal pot legs.
        if(s) return std::sqrt(2.2/4.7);
        const double t1=1000e-12*53600, t2=680e-12*39200;
        return std::sqrt(t1*t2)/(t1+t2);
    }
};
}
