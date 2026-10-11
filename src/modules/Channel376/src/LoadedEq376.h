#pragma once
#include "EqCircuit376.h"
#include "OpampNetwork376.h"
#include "DetectorCircuit376.h"
#include "ReducedLinear376.h"
namespace channel376 {
// Original sch3 including C21 after U32B, gyrator U1A/U1B and LF353 U8B/A.
template<bool reducedEQ=false> class LoadedEqProcessor376 {
public:
    void prepare(double rate)noexcept{
        fs=rate;mid.reset(rate);shelves.reset(rate);inputVoltage=inputCurrent=0;
        midReady=shelvesReady=false;
        mid.capacitor(0,2,31.7e-9);mid.capacitor(3,4,31.7e-9);mid.capacitor(4,5,10e-12);
        mid.capacitor(9,-1,22e-6); // C21 to known U32B output, corrected at commit.
        mid.amplifier(0,2,1,0);mid.amplifier(1,4,-1,5);mid.amplifier(2,8,-1,7,100000,3e6,13.5,13e6);
        shelves.capacitor(0,2,39e-9);shelves.capacitor(1,2,39e-9);shelves.capacitor(5,6,2.2e-9);shelves.capacitor(6,7,22e-12);
        shelves.amplifier(0,7,-1,6,100000,3e6,13.5,13e6);
        low=midPosition=high=frequency=HUGE_VAL;setPhysical(.5,.5,.5,1000);
    }
    void setPhysical(double l,double m,double h,double f)noexcept{
        l=std::clamp(l,1e-6,1-1e-6);m=std::clamp(m,1e-6,1-1e-6);h=std::clamp(h,1e-6,1-1e-6);
        if(m!=midPosition||f!=frequency){
            if constexpr(reducedEQ)if(midReady)reducedMid.restore(mid);
            midPosition=m;frequency=f;mid.conductance={};
            const double r=EqCircuit376::midResistance(f);
            mid.resistor(0,2,r);mid.resistor(0,3,r);mid.resistor(1,-1,10000);mid.resistor(1,2,10000);
            mid.resistor(4,5,24900);mid.resistor(6,5,24900);mid.resistor(6,8,50000*m);mid.resistor(6,9,50000*(1-m));
            mid.resistor(9,7,10000);mid.resistor(8,7,10000);mid.resistor(2,7,4220);mid.compile();
            if constexpr(reducedEQ){midReady=reducedMid.compile(mid,1u<<8);Vector376<10> weights{};weights[9]=2*fs*22e-6;reducedMid.configureInput(weights);}
        }
        if(l==low&&h==high)return;
        if constexpr(reducedEQ)if(shelvesReady)reducedShelves.restore(shelves);
        low=l;high=h;shelves.conductance={};
        shelves.resistor(0,-1,10000);shelves.resistor(1,7,10000);shelves.resistor(0,2,50000*(1-l));shelves.resistor(1,2,50000*l);
        shelves.resistor(2,6,10000);shelves.resistor(3,-1,3650);shelves.resistor(4,7,3650);
        shelves.resistor(3,5,50000*(1-h));shelves.resistor(4,5,50000*h);shelves.compile();
        if constexpr(reducedEQ){shelvesReady=reducedShelves.compile(shelves,1u<<7);Vector376<8> weights{};weights[0]=1/10000.;weights[3]=1/3650.;reducedShelves.configureInput(weights);}
    }
    double tick(double input)noexcept{
        Vector376<10> b{};b[9]=2*fs*22e-6*input;
        Vector376<10> x{};bool fast=false;
        if constexpr(reducedEQ){
            fast=midReady&&reducedMid.tickInput(input,x,3);
            if(fast)mid.nodes=x;
            else {reducedMid.restore(mid);inputVoltage=mid.capacitors[3].voltage;inputCurrent=mid.capacitors[3].current;}
        }
        if(!fast){const auto prediction=mid.predict(b);if(mid.commit(prediction)){
            const double v=mid.nodes[9]-input;inputCurrent=2*fs*22e-6*(v-inputVoltage)-inputCurrent;inputVoltage=v;
            mid.capacitors[3].voltage=v;mid.capacitors[3].current=inputCurrent;mid.invalidateHistory();}
            if constexpr(reducedEQ)reducedMid.sync(mid);
        }
        Vector376<8> source{};source[0]=mid.nodes[8]/10000;source[3]=mid.nodes[8]/3650;
        if constexpr(reducedEQ){Vector376<8> y{};
            if(!shelvesReady||!reducedShelves.tickInput(mid.nodes[8],y)){reducedShelves.restore(shelves);y=shelves.tick(source);reducedShelves.sync(shelves);}
            shelves.nodes=y;return y[7];
        }else return shelves.tick(source)[7];
    }
    unsigned failedSolves()const noexcept{return mid.failures+shelves.failures;}
    std::array<double,4> opampOutputs()const noexcept{return {mid.nodes[2],mid.nodes[4],mid.nodes[8],shelves.nodes[7]};}
private:
    OpampNetwork376<10,3> mid;OpampNetwork376<8,1> shelves;
    ReducedLinear376<10,3> reducedMid;ReducedLinear376<8,1> reducedShelves;
    bool midReady=false,shelvesReady=false;
    double fs=192000,low=.5,midPosition=.5,high=.5,frequency=1000,inputVoltage=0,inputCurrent=0;
};
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_EQ)
using LoadedEq376=LoadedEqProcessor376<true>;
#else
using LoadedEq376=LoadedEqProcessor376<false>;
#endif
class LoadedHighFrequency376 {
public:
    void prepare(double rate)noexcept{
        fs=rate;voltageA=currentA=voltageB=currentB=0;net.reset(rate);
        net.capacitor(0,-1,4.7e-9);net.capacitor(0,1,2.2e-9);net.resistor(2,-1,10000);net.resistor(2,3,10000);
        net.amplifier(0,3,1,2,200000,3e6,13.5,13e6);r=HUGE_VAL;setResistance(HighFrequency376::resistance(4000));
    }
    void setResistance(double value)noexcept{
        value=std::clamp(value,4750.,54750.);if(value==r)return;r=value;net.conductance={};
        net.resistor(0,3,r);net.resistor(1,-1,r);net.resistor(2,-1,10000);net.resistor(2,3,10000);net.compile();
    }
    double tick(double input)noexcept{
        // C8 connects source to node0; C9 connects node0 to node1.
        Vector376<4> source{};source[0]=2*fs*4.7e-9*input;
        const auto p=net.predict(source);if(net.commit(p)){
            const double v=net.nodes[0]-input;currentA=2*fs*4.7e-9*(v-voltageA)-currentA;voltageA=v;
            net.capacitors[0].voltage=v;net.capacitors[0].current=currentA;net.invalidateHistory();}
        return net.nodes[3];
    }
    unsigned failedSolves()const noexcept{return net.failures;}
private:OpampNetwork376<4,1> net;double fs=192000,r=4750,voltageA=0,currentA=0,voltageB=0,currentB=0;
};
}
