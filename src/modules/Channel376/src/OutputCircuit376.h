#pragma once
#include "OpampNetwork376.h"
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_OUTPUT)
#include "ReducedLinear376.h"
#endif
namespace channel376 {
// Sch6 cross-coupled U31. P9pin2-pin3 is one balanced output, not stereo.
class OutputCircuit376 {
public:
    void prepare(double rate)noexcept{
        net.reset(rate);
        // [A-,A+,B-,B+,pin3,pin2,Aout,Bout]. Feedback R224/R225
        // is before the 60.4ohm output resistors; cross-coupling is after them.
        net.resistor(0,-1,10000);net.resistor(0,6,6040);net.resistor(1,-1,2670);
        net.resistor(1,4,4530);net.resistor(1,5,3160);
        net.resistor(2,-1,10000);net.resistor(2,7,5760);net.resistor(3,-1,3160);
        net.resistor(3,4,3240);net.resistor(3,5,4530);
        net.resistor(6,4,60.4);net.resistor(7,5,60.4);
        net.capacitor(4,-1,470e-12);net.capacitor(5,-1,470e-12);
        net.amplifier(0,6,1,0,100000,10e6,13,8e6);net.amplifier(1,7,3,2,100000,10e6,13,8e6);net.compile();
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_OUTPUT)
        reduced.compile(net,(1u<<4)|(1u<<5));
        Vector376<8> weights{};weights[0]=1/10000.;weights[3]=1/3160.;reduced.configureInput(weights);
#endif
    }
    double tick(double vcaOutput)noexcept{
        Vector376<8> b{};b[0]=vcaOutput/10000;b[3]=vcaOutput/3160;
        Vector376<8> x{};
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_OUTPUT)
        if(!reduced.tickInput(vcaOutput,x)){reduced.restore(net);x=net.tick(b);reduced.sync(net);}
#else
        x=net.tick(b);
#endif
        pin2=x[5];pin3=x[4];return pin2-pin3;
    }
    double positiveLeg()const noexcept{return pin2;}
    double negativeLeg()const noexcept{return pin3;}
    unsigned failedSolves()const noexcept{return net.failures;}
private:OpampNetwork376<8,2> net;double pin2=0,pin3=0;
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_OUTPUT)
    ReducedLinear376<8,2> reduced;
#endif
};
}
