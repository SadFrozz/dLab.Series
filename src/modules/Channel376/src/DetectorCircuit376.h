#pragma once
#include "ControlReference376.h"
#include "CircuitMath376.h"
namespace channel376 {
// Sch4: two paths after C66, not a single generic high-pass biquad.
class BroadbandCurrent376 {
public:
    void prepare(double rate)noexcept{fs=rate;voltageA=voltageB=currentA=currentB=0;
        ca=2*fs*.33e-6;cb=2*fs*47e-9;cd0=ca+cb+1/27000.;cd1=cb+1/15000.;inverseDet=1/(cd0*cd1-cb*cb);inverseD1=1/cd1;
    }
    double tick(double input)noexcept{
#if defined(CHANNEL376_EXPERIMENTAL_COEFFICIENTS)
        const double a=ca,b=cb;
#else
        const double a=2*fs*.33e-6,b=2*fs*47e-9;
#endif
        const double historyA=a*voltageA+currentA,historyB=b*voltageB+currentB;
#if defined(CHANNEL376_EXPERIMENTAL_COEFFICIENTS)
        const double n0=((a*input+historyA+historyB)*cd1-b*historyB)*inverseDet;
        const double n1=(b*n0-historyB)*inverseD1;
#else
        const double d0=a+b+1/27000.,d1=b+1/15000.;
        const double n0=((a*input+historyA+historyB)*d1-b*historyB)/(d0*d1-b*b);
        const double n1=(b*n0-historyB)/d1;
#endif
        const double va=n0-input,vb=n0-n1;
        currentA=a*(va-voltageA)-currentA;voltageA=va;
        currentB=b*(vb-voltageB)-currentB;voltageB=vb;
        return n0/27000+n1/15000;
    }
    static std::complex<double> admittance(double hz)noexcept{
        const std::complex<double> s{0,6.283185307179586*hz};
        const auto y=1/27000.+1./(15000.+1./(s*47e-9));return 1./(1./y+1./(s*.33e-6));
    }
private:double fs=192000,voltageA=0,voltageB=0,currentA=0,currentB=0,ca=0,cb=0,cd0=0,cd1=0,inverseDet=0,inverseD1=0;
};
// High-pass Sallen-Key U7D. R40+VR1A and R17+VR1B are matched;
// resistor changes retain integrator voltages, not DF coefficient history.
class HighFrequency376 {
public:
    void prepare(double rate)noexcept{fs=rate;one=two=dOne=dTwo=0;setFrequency(4000);}
    static double resistance(double hz)noexcept{
        return 4750+50000*(1/std::clamp(hz,800.,10000.)-1/10000.)/(1/800.-1/10000.);
    }
    void setFrequency(double hz)noexcept{r=resistance(hz);}
    void setResistance(double value)noexcept{r=std::clamp(value,4750.,54750.);}
    double tick(double input)noexcept{
        const double w=1/(r*std::sqrt(4.7e-9*2.2e-9)),q=std::sqrt(4.7e-9*2.2e-9)/4.7e-9,t=.5/fs,g=t*w;
        const double a=one+t*dOne+g*input,b=two+t*dTwo;
        const double nextOne=(a-g*b)/(1+g/q+g*g),nextTwo=b+g*nextOne;
        dOne=w*(input-nextOne/q-nextTwo);dTwo=w*nextOne;one=nextOne;two=nextTwo;
        return 2*(input-one/q-two);
    }
private:double fs=192000,r=4750,one=0,two=0,dOne=0,dTwo=0;
};
class RmsDetectorCircuit376 {
public:
    static double steadyVoltage(double acCurrentRms,double feedbackOhms)noexcept{
        const double reference=Rms376::referenceCurrent,k=Blackmer376::voltsPerDb;
        auto residual=[&](double v){const double current=std::hypot(acCurrentRms,v/feedbackOhms);
            return v-k*20*std::log10(std::max(current,1e-20)/reference);};
        double lo=-1,hi=1;
        for(int it=0;it<64;++it){const double m=(lo+hi)*.5;if(residual(m)>0)hi=m;else lo=m;}
        return (lo+hi)*.5;
    }
    void prepare(double rate,double feedbackOhms)noexcept{
        feedback=feedbackOhms;rms.prepare(rate);
        const double dc=steadyVoltage(0,feedback);
        rms.meanSquare=dc*dc/(feedback*feedback);
    }
    double tick(double current)noexcept{return rms.tick(current,feedback);}
    double tickTotalCurrent(double current)noexcept{return rms.tick(current,HUGE_VAL);}
    double voltage()const noexcept{return rms.voltage();}
private:Rms376 rms;double feedback=634000;
};
// Original U5D/R12/R26/R11/R19/R258 and loaded VR5/D5 network.
// This supplies signed voltage to the five-node compressor, not a pre-clipped
// generic gain-reduction demand. Soft knee is the physical loaded diode law.
class CompressorDrive376 {
public:
    void prepare()noexcept{bias=overEasyBias376();diodeEstimate=0;previousRatio=HUGE_VAL;setThreshold(-10);}
    void setThreshold(double dbu)noexcept{
        const double inputCurrent=.775*std::pow(10.,std::clamp(dbu,-40.,20.)/20)*std::abs(BroadbandCurrent376::admittance(1000));
        const double voltage=RmsDetectorCircuit376::steadyVoltage(inputCurrent,634000);
        unboundedPot=(15*10000/(280000.+562000)-10000/4990.*voltage)/(10000/412000.);
        thresholdPot=std::clamp(unboundedPot,-15.,15.);
    }
    double tick(double rmsVoltage,double ratioPosition,bool overEasy)noexcept{
        const double u5d=std::clamp(-10000/4990.*rmsVoltage-10000/412000.*thresholdPot+15*10000/(280000.+562000),-13.5,13.5);
        comparatorVoltage=u5d;
        if(ratioPosition!=previousRatio){previousRatio=ratioPosition;ratio=loadedRatio376(ratioPosition);}
        double v=u5d;
        if(overEasy){
            // D1 is a grounded bias diode (the open triangle is GROUND,
            // not a connection to D5). R20 feeds this bias into U5D's
            // inverting summer: actual U5D = hard U5D - bias.
            // D5 then rectifies that voltage into the loaded VR5 network.
            comparatorVoltage=u5d-bias;
            diodeEstimate=ratio.load>0?Diode376::current(bias-u5d,ratio.load,diodeEstimate):0;
            v=-ratio.load*diodeEstimate;
        }
        return 18.4*ratio.wiper*v;
    }
    double potVoltage()const noexcept{return thresholdPot;}
    void setPotVoltage(double v)noexcept{thresholdPot=std::clamp(v,-15.,15.);}
    double requestedPotVoltage()const noexcept{return unboundedPot;}
    double comparator()const noexcept{return comparatorVoltage;}
private:double thresholdPot=0,unboundedPot=0,bias=0,comparatorVoltage=0;
    double diodeEstimate=0,previousRatio=HUGE_VAL;Ratio376 ratio{};
};
}
