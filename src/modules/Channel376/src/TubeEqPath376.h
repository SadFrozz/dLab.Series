#pragma once
#include "TubeCircuit376.h"
#include "EqCircuit376.h"
namespace channel376 {
// A prepared analogue subpath, MIC_PRE_OUT -> EQ_OUT. Not the LINE -> U31
// processor. Opamps here are linear reference transfers; overload/current
// limits must be integrated before the product processor may use this path.
struct RcPole376 {
    double value=0,derivative=0,fs=192000;
    void prepare(double rate)noexcept{value=derivative=0;fs=rate;}
    double tick(double target,double seconds)noexcept{
        const double g=.5/(fs*seconds),next=(value+.5/fs*derivative+g*target)/(1+g);
        derivative=(target-next)/seconds;value=next;return next;
    }
};
class TubeEqPath376 {
public:
    static constexpr double trimFeedback=47000+3006.375492949781;
    void prepare(double rate,double trimOhms=3006.375492949781)noexcept{
        feedback=47000+std::clamp(trimOhms,0.,50000.);
        tube.prepare(rate);eq.prepare(rate);trim.prepare(rate);returned.prepare(rate);
        insert.reset(rate);insert.resistor(0,-1,100);insert.resistor(0,1,20);insert.resistor(2,-1,20000);
        insert.capacitor(0,-1,470e-12);insert.capacitor(1,-1,1000e-12);insert.capacitor(1,2,22e-6);insert.compile();
    }
    void setPhysicalEq(double low,double mid,double high,double frequency)noexcept{eq.setPhysical(low,mid,high,frequency);}
    double tick(double preTubeVolts)noexcept{
        const double plate=tube.tick(preTubeVolts);
        const double send=trim.tick(-plate*feedback/121000,feedback*10e-12);
        Vector376<3> source{};source[0]=send/100;const auto nodes=insert.tick(source);
        const double preEq=returned.tick(-nodes[2],20000.*47e-12);
        return eq.tick(preEq);
    }
    unsigned failedSolves()const noexcept{return tube.failedSolves();}
    double maximumTubeResidual()const noexcept{return tube.maximumResidualAmps();}
private:
    TubeCircuit376 tube;EqCircuit376 eq;RcPole376 trim,returned;LinearNetwork376<3> insert;double feedback=trimFeedback;
};
}
