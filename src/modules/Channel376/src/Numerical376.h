#pragma once
#include "HalfbandCoefficients.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <limits>
namespace channel376 {
struct Fir129 {
    std::array<double,258> state{};int cursor=0;
    double tick(double input,bool outputNeeded=true) noexcept {
        state[cursor]=state[cursor+129]=input;double result=0;
        const double* history=state.data()+cursor+129;
        if(outputNeeded)for(int k=0;k<129;++k)result+=halfband376[k]*history[-k];
        cursor=(cursor+1)%129;return result;
    }
};
class Oversample4x376 {
public:
    static constexpr int latencySamples=96;
    void reset()noexcept{up1={};up2={};down2={};down1={};dry={};dryCursor=0;}
    template<class Nonlinear>double process(double input,Nonlinear&& function)noexcept {
        double result=0;
        for(int a=0;a<2;++a) {
            const double low=up1.tick(a==0?2*input:0);
            double decimated=0;
            for(int b=0;b<2;++b) {
                const double high=up2.tick(b==0?2*low:0);
                const double value=function(high);
                const double down=down2.tick(std::isfinite(value)?value:0,b==0);
                if(b==0)decimated=down;
            }
            const double output=down1.tick(decimated,a==0);if(a==0)result=output;
        }
        return result;
    }
    double delayedDry(double input)noexcept {
        const double result=dry[dryCursor];dry[dryCursor]=input;
        dryCursor=(dryCursor+1)%latencySamples;return result;
    }
private:
    Fir129 up1{},up2{},down2{},down1{};
    std::array<double,latencySamples> dry{};int dryCursor=0;
};
struct MeanSquare376 {
    double state=0,alpha=0;
    void prepare(double fs,double seconds=.0038625727518541656*(.0061/.006))noexcept {
        alpha=-std::expm1(-1/(fs*seconds));state=0;
    }
    double tick(double current)noexcept {
        state+=alpha*(current*current-state);return std::sqrt(std::max(0.,state));
    }
};
struct Meter376 {
    static constexpr double displayFloorPeak=3.1622776601683793e-8; // -150dBFS
    double inputRectified=0,grVoltage=0,eqEnvelope=0;
    double peak=0,fs=48000;
    int hold=0;
    void prepare(double rate)noexcept{*this={};fs=rate;}
    void inputTick(double rectifiedVoltage)noexcept {
        inputRectified+=(-std::expm1(-1/(fs*.001)))*(rectifiedVoltage-inputRectified);
    }
    void gainReductionTick(double voltage)noexcept {
        grVoltage+=(-std::expm1(-1/(fs*.016104)))*(voltage-grVoltage);
    }
    void eqTick(double eqVolts)noexcept {
        // Nominal emitter follower: rectifier diode .35V, VBE .65V.
        // beta=100 is a declared nominal reduction, not a measured Q4.
        const double driven=std::max(0.,eqVolts-1.);
        const double seconds=driven>eqEnvelope?20000./101*10e-6:.332;
        eqEnvelope+=(-std::expm1(-1/(fs*seconds)))*(driven-eqEnvelope);
    }
    void outputTick(double finalPCM)noexcept {
        const double p=std::abs(finalPCM);
        if(p>=peak){peak=p;hold=int(.25*fs);}
        else if(hold>0)--hold;
        else peak*=std::exp(-1/(fs*.3));
    }
    double outputDbfs()const noexcept{return peak>=displayFloorPeak?20*std::log10(peak):-std::numeric_limits<double>::infinity();}
    bool eqClip()const noexcept{return eqEnvelope>=9.109947643979057;}
};
}
