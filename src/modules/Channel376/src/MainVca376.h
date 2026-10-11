#pragma once
#include "Blackmer376.h"
#include "OpampNetwork376.h"
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_VCA)
#include "ReducedLinear376.h"
#endif
namespace channel376 {
class SeriesCurrent376 {
public:
    void prepare(double rate,double resistance,double capacitance)noexcept{fs=rate;r=resistance;c=capacitance;voltage=current=0;inverseG=1/(2*fs*c);admittance=1/(r+inverseG);}
    double tick(double input)noexcept{
#if defined(CHANNEL376_EXPERIMENTAL_COEFFICIENTS)
        const double i=(input-voltage-current*inverseG)*admittance;
#else
        const double g=2*fs*c,i=(input-voltage-current/g)/(r+1/g);
#endif
        voltage=input-r*i;current=i;return i;
    }
private:double fs=192000,r=20000,c=22.1e-6,voltage=0,current=0,inverseG=0,admittance=0;
};
class MainVca376 {
public:
    void prepare(double rate)noexcept{
        input.prepare(rate,20000,22.1e-6);
        cv.reset(rate);cv.resistor(0,-1,121000);cv.resistor(0,-1,49900);cv.resistor(0,-1,36500);cv.resistor(0,1,1000);
        cv.amplifier(0,1,-1,0);cv.compile();
        audio.reset(rate);audio.resistor(0,1,20000);audio.capacitor(0,1,47e-12);audio.amplifier(0,1,-1,0);audio.compile();
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_VCA)
        reducedCv.compile(cv,1u<<1);Vector376<2> weights{1,0};reducedCv.configureInput(weights);
        reducedAudio.compile(audio,1u<<1);reducedAudio.configureInput(weights);
#endif
    }
    double tick(double eqVoltage,double outputPotVoltage,double deCv,double compCv)noexcept{
        Vector376<2> b{};b[0]=outputPotVoltage/121000+deCv/49900+compCv/36500;
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_VCA)
        Vector376<2> x{};if(!reducedCv.tickInput(b[0],x)){reducedCv.restore(cv);x=cv.tick(b);reducedCv.sync(cv);}
        control=x[1];lastGain=Blackmer376::gain(control);return tickShared(eqVoltage,lastGain,control);
#else
        control=cv.tick(b)[1];lastGain=Blackmer376::gain(control);return tickShared(eqVoltage,lastGain,control);
#endif
    }
    double tickShared(double eqVoltage,double sharedGain,double sharedControl)noexcept{
        control=sharedControl;lastGain=sharedGain;Vector376<2> b{};b[0]=input.tick(eqVoltage)*sharedGain;
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_VCA)
        Vector376<2> x{};
        if(!reducedAudio.tickInput(b[0],x)){reducedAudio.restore(audio);x=audio.tick(b);reducedAudio.sync(audio);}
        return x[1];
#else
        return audio.tick(b)[1];
#endif
    }
    double controlVoltage()const noexcept{return control;}
    double linearGain()const noexcept{return lastGain;}
    unsigned failedSolves()const noexcept{return audio.failures+cv.failures;}
private:SeriesCurrent376 input;OpampNetwork376<2,1> cv,audio;double control=0,lastGain=1;
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_VCA)
    ReducedLinear376<2,1> reducedCv,reducedAudio;
#endif
};
// U16 input: C34/R132 followed by the20ohm R130, with R128 feedback
// into the loaded node. No duplicate RMS feedback term is added elsewhere.
class HighFrequencyCurrent376 {
public:
    void prepare(double rate,double dcRmsVoltage)noexcept{fs=rate;capacitor=-20*dcRmsVoltage/(20e6+20);current=0;inverseG=1/(2*fs*47e-9);y=1/(75000+inverseG);inverseDenominator=1/(y+1/20e6+1/20.);}
    double tick(double input,double feedback)noexcept{
#if defined(CHANNEL376_EXPERIMENTAL_COEFFICIENTS)
        const double drive=input-capacitor-current*inverseG;
        const double node=(y*drive+feedback/20e6)*inverseDenominator;
#else
        const double g=2*fs*47e-9,y=1/(75000+1/g),drive=input-capacitor-current/g;
        const double node=(y*drive+feedback/20e6)/(y+1/20e6+1/20.);
#endif
        current=y*(drive-node);capacitor=input-node-75000*current;return node/20;
    }
private:double fs=192000,capacitor=0,current=0,inverseG=0,y=0,inverseDenominator=0;
};
}
