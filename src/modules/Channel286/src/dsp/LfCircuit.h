#pragma once
#include <array>

namespace c286 {
// Ideal op-amp reduction of the two LF RC stages and loaded 50k linear pot.
// Values are nominal schematic components, not a fit to measured hardware.
struct LfCircuit {
    double ri, rsh, ca, cb, rfb, rload, amplifierGain;
    double rd, ra, cf, ci, rf;
    double mixA, mixB, shunt, sum;
    static LfCircuit forModel(bool s) noexcept {
        if(s) return {412000,867000,10e-9,10e-9,280000,280000,1+10000./7142.,
                      133100,47000,10e-9,3.3e-9,356900,499,1332,13300,22599};
        // Original VR205 is a 10k trim. Nominal reduction assumes 10k;
        // its actual factory setting cannot be recovered from the schematic.
        return {412000,866000,10e-9,10e-9,280000,280000,1+10000./7150.,
                133000,46400,10e-9,3.3e-9,357000,499,1370,12700,22600};
    }
    std::array<double,3> numeratorA() const noexcept { return {0,amplifierGain*cb/ri,0}; }
    std::array<double,3> denominatorA() const noexcept {
        const double y=1/ri+1/rsh+1/rfb;
        return {ca*cb,y*cb+(ca+cb)/rload-amplifierGain*cb/rfb,y/rload};
    }
    std::array<double,3> numeratorB() const noexcept { return {0,-rf*ci/rd,0}; }
    std::array<double,3> denominatorB() const noexcept { return {rf*cf*ci,cf+ci,1/rd+1/ra}; }
    std::array<double,2> mixer(double position) const noexcept {
        const double bottom=50000*position;
        const double loadedBottom=bottom*sum/(bottom+sum);
        const double load=50000*(1-position)+loadedBottom;
        const double scale=100000/sum*(loadedBottom/load)
            /(1/mixA+1/mixB+1/shunt+1/load);
        return {scale/mixA,scale/mixB};
    }
};
}
