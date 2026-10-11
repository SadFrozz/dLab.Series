#pragma once
#include "OpampNetwork376.h"
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_FRONT)
#include "ReducedLinear376.h"
#endif
namespace channel376 {
class LineInput376 {
public:
    void prepare(double rate)noexcept{
        net.reset(rate);
        // J4 pin1 TIP -> R243/159 -> positive; pin2 RING -> R244/150 -> negative.
        net.resistor(0,-1,10000);net.resistor(0,2,10000);net.resistor(1,-1,10000);net.resistor(1,3,10000);
        net.resistor(2,4,20000);net.resistor(3,-1,20000);
        net.capacitor(0,-1,47e-12);net.capacitor(1,-1,47e-12);net.capacitor(3,-1,47e-12);net.capacitor(2,4,47e-12);
        net.amplifier(0,4,3,2);net.compile();
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_FRONT)
        reduced.compile(net,1u<<4);Vector376<5> weights{};weights[0]=-.5/10000;weights[1]=.5/10000;reduced.configureInput(weights);
#endif
    }
    double tick(double tipMinusRing,double commonMode=0)noexcept{
        Vector376<5> b{};b[0]=(commonMode-.5*tipMinusRing)/10000;b[1]=(commonMode+.5*tipMinusRing)/10000;
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_FRONT)
        Vector376<5> x{};if(commonMode!=0||!reduced.tickInput(tipMinusRing,x)){reduced.restore(net);x=net.tick(b);reduced.sync(net);}return x[4];
#else
        return net.tick(b)[4];
#endif
    }
    unsigned failedSolves()const noexcept{return net.failures;}
private:OpampNetwork376<5,1> net;
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_FRONT)
    ReducedLinear376<5,1> reduced;
#endif
};
class LineSource376 {
public:
    void prepare(double rate)noexcept{
        net.reset(rate);
        // J1 normalled contacts4->1. No instrument connected. Ferrite L1
        // is a DC short in this audio-band nominal model (RF impedance unknown).
        net.resistor(0,-1,10000);net.capacitor(0,-1,240e-12);net.capacitor(0,1,.33e-6);net.resistor(1,-1,475000);
        net.resistor(2,3,10000);net.capacitor(2,3,100e-12);net.amplifier(0,3,1,2);
        net.resistor(3,4,15000);net.resistor(4,-1,1000);net.resistor(4,-1,100000);net.compile();
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_FRONT)
        reduced.compile(net,1u<<4);Vector376<5> weights{};weights[0]=1/10000.;reduced.configureInput(weights);
#endif
    }
    double tick(double lineVoltage)noexcept{Vector376<5> b{};b[0]=lineVoltage/10000;
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_FRONT)
        Vector376<5> x{};if(!reduced.tickInput(lineVoltage,x)){reduced.restore(net);x=net.tick(b);reduced.sync(net);}return x[4];
#else
        return net.tick(b)[4];
#endif
    }
    unsigned failedSolves()const noexcept{return net.failures;}
private:OpampNetwork376<5,1> net;
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_FRONT)
    ReducedLinear376<5,1> reduced;
#endif
};
class DriveCircuit376 {
public:
    static constexpr double minimumResistance=80.4,maximumResistance=5080.4;
    static double gainAtResistance(double r)noexcept{return 1+4990/r;}
    static double centerGain()noexcept{return std::sqrt(gainAtResistance(minimumResistance)*gainAtResistance(maximumResistance));}
    static double resistance(double nominalDb)noexcept{
        // Nominal +/-15 labels interpolate log gain between actual endpoints.
        const double p=(std::clamp(nominalDb,-15.,15.)+15)/30;
        const double gain=std::exp(std::log(gainAtResistance(maximumResistance))*(1-p)+std::log(gainAtResistance(minimumResistance))*p);
        return 4990/(gain-1);
    }
    void prepare(double rate,double db=0)noexcept{
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_FRONT)
        ready=false;
#endif
        net.reset(rate);net.capacitor(0,-1,1000e-6);net.capacitor(1,2,47e-12);
        net.amplifier(0,2,-1,1);previousResistance=HUGE_VAL;setGain(db);
    }
    void setGain(double db)noexcept{
        setResistance(resistance(db));
    }
    void setResistance(double r)noexcept{
        r=std::clamp(r,minimumResistance,maximumResistance);if(r==previousResistance)return;previousResistance=r;
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_FRONT)
        if(ready)reduced.restore(net);
#endif
        net.conductance={};net.resistor(0,1,r);net.resistor(1,2,4990);net.compile();
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_FRONT)
        ready=reduced.compile(net,1u<<2);reduced.configureInput({}, {1});
#endif
    }
    double tick(double input)noexcept{
        // U6A positive input is the known voltage of the selected source.
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_FRONT)
        Vector376<3> x{};if(!ready||!reduced.tickInput(input,x)){reduced.restore(net);x=net.tick({}, {input});reduced.sync(net);}return x[2];
#else
        return net.tick(Vector376<3>{},{input})[2];
#endif
    }
    unsigned failedSolves()const noexcept{return net.failures;}
private:
    OpampNetwork376<3,1> net;double previousResistance=0;
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_FRONT)
    ReducedLinear376<3,1> reduced;bool ready=false;
#endif
};
class LowCutCircuit376 {
public:
    void prepare(double rate)noexcept{
        fs=rate;net.reset(rate);net.capacitor(0,-1,.33e-6);net.capacitor(0,1,.33e-6);
        net.resistor(0,2,4530);net.resistor(1,-1,10000);net.amplifier(0,2,1,2);net.compile();previousInput=current=0;
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_FRONT)
        reduced.compile(net,1u<<2);Vector376<3> weights{};weights[0]=2*fs*.33e-6;reduced.configureInput(weights);
#endif
    }
    double tick(double input)noexcept{
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_FRONT)
        Vector376<3> result{};if(reduced.tickInput(input,result,0))return result[2];
        reduced.restore(net);previousInput=net.capacitors[0].voltage;current=net.capacitors[0].current;
#endif
        // C14 node0->known Drive output: voltage=node0-input.
        auto& c=net.capacitors[0];c.voltage=previousInput;c.current=current;
        Vector376<3> b{};b[0]=2*fs*.33e-6*input;
        const auto x=net.tick(b);const double v=x[0]-input;
        current=2*fs*.33e-6*(v-previousInput)-current;previousInput=v;
        c.voltage=v;c.current=current;
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_FRONT)
        reduced.sync(net);
#endif
        return x[2];
    }
    unsigned failedSolves()const noexcept{return net.failures;}
private:OpampNetwork376<3,1> net;double fs=192000,previousInput=0,current=0;
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_FRONT)
    ReducedLinear376<3,1> reduced;
#endif
};
}
