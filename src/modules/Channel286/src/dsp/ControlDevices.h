#pragma once
#include <algorithm>
#include <cmath>

namespace c286 {
struct ControlDevice {
    double rmsVoltsPerDb, vcaVoltsPerDb, rmsSeconds, rmsReferenceCurrent;
    static ControlDevice forModel(bool modern) noexcept {
        if(modern) return {.0062,.0062,.003467,7.5e-6};
        // NEC uPC1252HA2 / uPC1253H2: nominal scale at 25C is 5.9mV/dB.
        // Original 286: R170/R186=560k; R172/R189=2.2M; CT=1.5uF.
        // Blackmer log-domain reduction uses nominal VBE=.7V (CT=2*VBE).
        // The level-match relation is from THAT2252 theory, not a claim that
        // every NEC offset/linearity characteristic is identical to THAT2252.
        const double bias=(15-3*.7)/560000;
        const double timing=(15+2*.7)/2200000;
        const double thermal=.0059*10/std::log(10.);
        return {.0059,.0059,thermal*1.5e-6/timing,std::sqrt(bias*timing)/2.9};
    }
    double rmsVoltage(double rmsCurrent) const noexcept {
        return rmsVoltsPerDb*20*std::log10(std::max(rmsCurrent,1.e-12)/rmsReferenceCurrent);
    }
};

struct SwitchingDiode {
    // Static reduction of Vishay 1N4148 model, doc 85547 rev1.1, 27-Nov-03.
    // Includes its current-dependent series element; charge storage/capacitance
    // are excluded here because this helper is used for slow control voltages.
    static constexpr double saturation=5.94633e-9, ideality=1.90777;
    static constexpr double thermal=.02586417*(298.15/300.15);
    static double forwardVoltage(double current) noexcept {
        current=std::max(0.,current);
        return ideality*thermal*std::log1p(current/saturation)
             +.560464*current+600*current/(5000*current/thermal+1);
    }
    static double forwardDerivative(double current) noexcept {
        current=std::max(0.,current);
        const double t=5000*current/thermal+1;
        return ideality*thermal/(current+saturation)+.560464+600/(t*t);
    }
};

struct DeCvCircuit {
    double compareFeedback, inputR, feedbackR, mainR, pilotR;
    ControlDevice device;
    static DeCvCircuit forModel(bool modern) noexcept {
        return modern ? DeCvCircuit{260200,260200,475000,250000,124010,ControlDevice::forModel(true)}
                      : DeCvCircuit{261000,261000,487000,249000,124000,ControlDevice::forModel(false)};
    }
    double potVoltage(double position) const noexcept {
        // Original VR108=5K402084; modern VR8=5K-C, not the 50k offset trim.
        // position denotes electrical wiper fraction. The C-taper angular law
        // is not specified by the available schematics and remains an extension.
        return 15*position/(1+5000*position*(1-position)/1000000);
    }
    double inputCurrent(double broadbandVoltage,double highVoltage,double position) const noexcept {
        const double ratio=compareFeedback/10000;
        const double comparison=(1+ratio)*highVoltage-ratio*broadbandVoltage;
        return comparison/inputR+potVoltage(position)/1000000;
    }
    double timeConstant() const noexcept { return feedbackR*3.3e-9; }
    double tick(double previousVoltage,double current,double seconds,bool modern) const noexcept {
        // Backward-Euler KCL at the inverting amplifier's virtual-zero node.
        // The feedback capacitor and diode(s) participate in the same equation.
        const double g=1/feedbackR+3.3e-9/seconds;
        const double source=3.3e-9/seconds*previousVoltage-current;
        const double unconstrained=source/g;
        if(source<=0) return std::max(-15.,unconstrained);
        const int count=modern?2:1;
        double diodeCurrent=source;
        if(unconstrained<1.) diodeCurrent=std::min(source,SwitchingDiode::saturation
            *std::expm1(unconstrained/(count*SwitchingDiode::ideality*SwitchingDiode::thermal)));
        for(int i=0;i<12;++i) {
            const double residual=diodeCurrent/g+count*SwitchingDiode::forwardVoltage(diodeCurrent)-unconstrained;
            if(std::abs(residual)<1.e-12) break;
            diodeCurrent=std::clamp(diodeCurrent-residual/(1/g+count*SwitchingDiode::forwardDerivative(diodeCurrent)),0.,source);
        }
        return std::min(15.,count*SwitchingDiode::forwardVoltage(diodeCurrent));
    }
    double outputReduction(double amplifierVoltage) const noexcept {
        if(amplifierVoltage>=0) return 0;
        // Output series diode drives both CV summers at virtual zero.
        // Solve -Vamp = I*(Rmain || Rpilot)+Vdiode(I), fixed bounded Newton.
        const double load=1/(1/mainR+1/pilotR);
        double current=-amplifierVoltage/load;
        for(int i=0;i<12;++i) {
            const double residual=load*current+SwitchingDiode::forwardVoltage(current)+amplifierVoltage;
            if(std::abs(residual)<1.e-12) break;
            current=std::clamp(current-residual/(load+SwitchingDiode::forwardDerivative(current)),0.,-amplifierVoltage/load);
        }
        // Nominal 4.7V shunt zener; its measured low-current knee is not fitted.
        const double cvMagnitude=std::min(4.7,load*current);
        return cvMagnitude*(4990/mainR)/device.vcaVoltsPerDb;
    }
};
}
