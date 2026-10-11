#pragma once
#include "TubeCircuit376.h"
#include "LoadedEq376.h"
namespace channel376 {
class LoadedTubePath376 {
public:
    void prepare(double rate,double trimOhms=27245.130726458814)noexcept{
        tube.prepare(rate);eq.prepare(rate);post.reset(rate);g87=.33e-6*rate;q87=TubeCircuit376::dcPlate;
        post.resistor(0,-1,1/g87);post.resistor(0,-1,121000);post.resistor(0,1,121000);
        post.resistor(1,2,47000+std::clamp(trimOhms,0.,50000.));post.capacitor(1,2,10e-12);
        post.resistor(2,3,100);post.capacitor(3,-1,470e-12);post.resistor(3,4,20);post.capacitor(4,-1,1000e-12);
        post.resistor(4,5,20000);post.resistor(5,6,20000);post.capacitor(5,6,47e-12);
        post.amplifier(0,2,-1,1);post.amplifier(1,6,-1,5);post.compile();
    }
    void setPhysicalEq(double l,double m,double h,double f)noexcept{eq.setPhysical(l,m,h,f);}
    double tick(double input)noexcept{
        const auto before=tube.failedSolves();
        OpampNetwork376<7,2>::Prediction base{};double basePlate=0;
        auto load=[&](double plate){Vector376<7> b{};b[0]=g87*(plate-q87);
            if(base.valid)last=post.predictChangedSource(base,b,0,g87*(plate-basePlate));
            else {last=post.predict(b);base=last;basePlate=plate;}
            if(!last.valid)return TubeCircuit376::Load{HUGE_VAL,0,0};
            return TubeCircuit376::Load{g87*(plate-last.nodes[0]-q87),g87*(1-g87*post.sensitivity(last,0,0)),last.nodes[0]};};
        tube.tickLoaded(input,load);
        if(tube.failedSolves()==before&&post.commit(last))q87=tube.operatingNodes()[2]-post.nodes[0];
        return eq.tick(post.nodes[6]);
    }
    unsigned failedSolves()const noexcept{return tube.failedSolves()+post.failures+eq.failedSolves();}
    double maximumTubeResidual()const noexcept{return tube.maximumResidualAmps();}
    std::array<double,6> opampOutputs()const noexcept{const auto e=eq.opampOutputs();return {post.nodes[2],post.nodes[6],e[0],e[1],e[2],e[3]};}
private:
    TubeCircuit376 tube;LoadedEq376 eq;OpampNetwork376<7,2> post;
    OpampNetwork376<7,2>::Prediction last{};double g87=.06336,q87=TubeCircuit376::dcPlate;
};
}
