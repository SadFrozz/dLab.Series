#pragma once
#if defined(CHANNEL376_EXPERIMENTAL_DIODE_TABLE) || defined(CHANNEL376_EXPERIMENTAL_RMS_LOG)
#include "DiodeLogTable376.h"
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include "Blackmer376.h"
namespace channel376 {
struct Diode376 {
    static constexpr double saturation=5.94633e-9,ideality=1.90777;
    static constexpr double thermal=.02586417*(298.15/300.15);
    static double voltage(double i)noexcept {
        i=std::max(0.,i);
#if defined(CHANNEL376_EXPERIMENTAL_DIODE_TABLE)
        const double junction=diodeLogTable376.evaluate(i/saturation);
#else
        const double junction=std::log1p(i/saturation);
#endif
        return ideality*thermal*junction+.560464*i+600*i/(5000*i/thermal+1);
    }
    static double current(double volts,double seriesResistance=0,double estimate=-1)noexcept {
        if(volts<=0)return 0;
        // Both the ideal-junction inverse and the diode's own positive series
        // resistance bound the root. A fixed1A cap breaks the derivative when
        // a rail-saturated amplifier momentarily forward-biases D3 strongly.
        double low=0,high=volts/(seriesResistance+.560464),i=estimate;
        // A previous positive root already gives the Newton starting point.
        // The positive series resistance bounds the new root independently;
        // do not calculate an unused exponential inverse on this warm path.
        if(!(estimate>0&&estimate<high)){
            high=std::min(high,saturation*std::expm1(std::min(40.,volts/(ideality*thermal))));i=high;
        }
        for(int n=0;n<20;++n){
            const double f=voltage(i)+seriesResistance*i-volts;
            if(std::abs(f)<1e-12)return i;
            if(f>0)high=i;else low=i;
            const double trial=i-f/(derivative(i)+seriesResistance);
            i=trial>low&&trial<high?trial:(low+high)*.5;
        }
        return i;
    }
    static double derivative(double i)noexcept {
        const double t=1+5000*i/thermal;
        return ideality*thermal/(i+saturation)+.560464+600/(t*t);
    }
};
struct Rms376 {
    static constexpr double voltsPerDb=Blackmer376::voltsPerDb;
    static constexpr double referenceCurrent=4.500434928509843e-6;
    static constexpr double seconds=.0038625727518541656*(.0061/.006);
    double meanSquare=referenceCurrent*referenceCurrent,alpha=0;
    mutable double cachedSquare=referenceCurrent*referenceCurrent,cachedVoltage=0;
    void prepare(double fs)noexcept{alpha=-std::expm1(-1/(fs*seconds));meanSquare=referenceCurrent*referenceCurrent;cachedSquare=meanSquare;cachedVoltage=0;}
    double voltage()const noexcept{
        if(meanSquare!=cachedSquare){cachedSquare=meanSquare;
            const double normalized=std::max(meanSquare,1e-24)/(referenceCurrent*referenceCurrent);
#if defined(CHANNEL376_EXPERIMENTAL_RMS_LOG)
            cachedVoltage=voltsPerDb*10*(diodeLogTable376.logPositive(normalized)/Blackmer376::log10);
#else
            cachedVoltage=voltsPerDb*10*std::log10(normalized);
#endif
        }
        return cachedVoltage;
    }
    double tick(double inputCurrent,double inputFeedbackOhm)noexcept {
        const double current=inputCurrent+voltage()/inputFeedbackOhm;
        meanSquare+=alpha*(current*current-meanSquare);return voltage();
    }
};
inline double cv376(double outputPotVoltage,double deCv,double compressorCv)noexcept {
    return -1000*(outputPotVoltage/121000+deCv/49900+compressorCv/36500);
}
inline double gain376(double cv)noexcept{return Blackmer376::gain(cv);}
inline double outputPot376(double gainDb)noexcept{
    // Host knob commands nominal dB. Solve the loaded actual wiper voltage.
    const double wanted=121000*Blackmer376::voltsPerDb*std::clamp(gainDb,-20.,20.)/1000;
    double lo=0,hi=1;
    for(int n=0;n<56;++n){double p=(lo+hi)*.5;double v=(30*p-15)/(1+20000*p*(1-p)/121000);
        if(v<wanted)lo=p;else hi=p;}
    const double p=(lo+hi)*.5;return (30*p-15)/(1+20000*p*(1-p)/121000);
}
struct Ratio376 {double wiper=0,compression=0,load=0;};
inline Ratio376 loadedRatio376(double position)noexcept{
    const double p=std::clamp(position,0.,1.);
    const double top=20000*(1-p),bottom=20000*p,parallel=top*4990/(top+4990);
    const double w=bottom/(bottom+parallel);
    return {w,std::min(1.,(1000/36500.)*18.4*(10000/4990.)*w),bottom+parallel};
}
inline double overEasyBias376()noexcept {
    // D1 at the top of R20: one active diode, +15 via R27=280k.
    double lo=0,hi=.8;
    for(int n=0;n<48;++n){const double x=(lo+hi)*.5;
        if(Diode376::current(x)+x/10000+x/332000>(15-x)/280000)hi=x;else lo=x;}
    return (lo+hi)*.5;
}
inline double requestedReduction376(double levelDb,double thresholdDb,double ratioPosition,bool soft)noexcept{
    const auto ratio=loadedRatio376(ratioPosition);
    if(!soft)return std::clamp(ratio.compression*(levelDb-thresholdDb),0.,60.);
    const double amplifier=-Blackmer376::voltsPerDb*(10000/4990.)*(levelDb-thresholdDb)-overEasyBias376();
    // D5 rectified, loaded node. Nominal threshold is the manual-calibrated
    // operating point; it is not a claimed factory offset measurement.
    double lo=0,hi=std::max(0.,-amplifier);
    for(int n=0;n<48;++n){const double magnitude=(lo+hi)*.5;
        if(magnitude+Diode376::voltage(magnitude/ratio.load)>-amplifier)hi=magnitude;else lo=magnitude;}
    const double pre=(lo+hi)*.5;
    return std::clamp(pre*18.4*ratio.wiper*(1000/36500.)/Blackmer376::voltsPerDb,0.,60.);
}
// Historical reduced model, retained to reproduce stage1 research only.
// Production integration must use CompressorCircuit376 instead.
class CompressorMemory376 {
public:
    void prepare(double rate)noexcept{fs=rate;z=side1=side2=meter=0;}
    double tick(double requestedDb,bool slow)noexcept {
        const double target=-.219*std::clamp(requestedDb,0.,60.);
        const double attack=slow?20000.:1/(1/20000.+1/10000.);
        const double source=.5*(15-.65)/(slow?1e6:1/(1/1e6+1/200000.));
        const double gc=.22e-6*fs,g1=1/20000.,g2=1/39200.,gs=2.2e-6*fs;
        auto side=[&](double x,double prev,double g){return (gs*prev+g*x)/(gs+g);};
        auto residual=[&](double x){
            const double q=std::min(source,std::max(0.,-x/10000));
            return gc*(x-z)+g1*(x-side(x,side1,g1))+g2*(x-side(x,side2,g2))
                +Diode376::current(x-target,attack)-q;
        };
        double low=-15,high=.8,x=z;
        const double passiveDerivative=gc+g1*(1-g1/(gs+g1))+g2*(1-g2/(gs+g2));
        for(int n=0;n<20;++n){
            const double f=residual(x);if(std::abs(f)<1e-12)break;
            if(f>0)high=x;else low=x;
            const double current=Diode376::current(x-target,attack);
            const double derivative=passiveDerivative+(x>target?1/(Diode376::derivative(current)+attack):0)
                +(x<0&&-x/10000<source?1/10000.:0);
            const double trial=x-f/derivative;x=trial>low&&trial<high?trial:(low+high)*.5;
        }
        z=x;side1=side(z,side1,g1);side2=side(z,side2,g2);
        const double q=std::min(source,std::max(0.,-z/10000));
        const double feedback=slow?39200.:1/(1/39200.+1/10000.);
        const double cv=std::min(0.,z-Diode376::voltage(q)-feedback*q);
        meter+=(-std::expm1(-1/(fs*.016104)))*(cv-meter);
        return cv;
    }
    double meterCv()const noexcept{return meter;}
private:double fs=48000,z=0,side1=0,side2=0,meter=0;
};
class DeCv376 {
public:
    void prepare(double rate)noexcept{fs=rate;cv=0;amplifier=0;}
    double tick(double broadbandRmsVoltage,double hfRmsVoltage,double amountElectrical,
                double offsetTrimVoltage=0)noexcept{
        const double p=std::clamp(amountElectrical,0.,1.);
        const double amount=15*p/(1+5000*p*(1-p)/1e6);
        const double comparison=(1+280000/10700.)*hfRmsVoltage
            -(280000/10700.)*broadbandRmsVoltage-(280000/2210000.)*offsetTrimVoltage;
        const double input=comparison/280000+amount/1e6;
        const double capacitor=3.3e-9*fs;
        double next=(capacitor*cv-input)/(capacitor+1/523000.);
        double drive=input-next/49900;
        if(drive<=0||next>0) {
            // When D45 is off, both R57 and the CV summer R221 discharge C33.
            next=capacitor*cv/(capacitor+1/523000.+1/49900.);drive=0;
        }
        const double required=next-5.6-Diode376::voltage(std::max(0.,drive));
        if(drive>0&&required<-13.5){
            // x-5.6-Vd(input-x/R)=-13.5 => Vd(i)+R*i=7.9+R*input.
            // Algebraically identical rail equation, without 48 bisections.
            next=49900*(input-Diode376::current(7.9+49900*input,49900));
        }
        cv=std::min(0.,next);amplifier=drive>0?std::max(-13.5,required):0;
        return cv;
    }
    double amplifierVoltage()const noexcept{return amplifier;}
private:double fs=48000,cv=0,amplifier=0;
};
// Sch8 U13A/D12/D15: negative half is precision rectified; positive
// half passes one loaded diode. This is not an RMS meter.
class InputMeter376 {
public:
    void prepare(double rate)noexcept{fs=rate;capacitor=0;diodeEstimate=0;
        gcCached=1e-6*fs;inverseDen=1/(gcCached+1/1000.);offDecay=1/(1+1/(fs*1e-6*21000));
        inverseAdmittance=1/(1/20000.+(1/1000.)*gcCached*inverseDen);
    }
    double tick(double volts)noexcept{
#if defined(CHANNEL376_EXPERIMENTAL_COEFFICIENTS)
        constexpr double g=1/1000.;
        if(volts<0){capacitor=(gcCached*capacitor-g*volts)*inverseDen;return capacitor;}
        const double previous=capacitor,offCap=previous*offDecay,offRect=offCap*20000/21000;
        if(volts<=offRect){capacitor=offCap;return capacitor;}
        diodeEstimate=Diode376::current(volts-offRect,inverseAdmittance,diodeEstimate);
        const double rect=offRect+diodeEstimate*inverseAdmittance;
        capacitor=(gcCached*previous+g*rect)*inverseDen;return capacitor;
#else
        const double gc=1e-6*fs,g=1/1000.;
        if(volts<0){capacitor=(gc*capacitor-g*volts)/(gc+g);return capacitor;}
        const double previous=capacitor;
        const double offCap=previous/(1+1/(fs*1e-6*21000));
        const double offRect=offCap*20000/21000;
        if(volts<=offRect){capacitor=offCap;return capacitor;}
        // Eliminate the capacitor companion and R209/R210 into the
        // diode's Thevenin load. Same implicit equation as the bisection.
        const double admittance=1/20000.+g*gc/(gc+g);
        diodeEstimate=Diode376::current(volts-offRect,1/admittance,diodeEstimate);
        const double rect=offRect+diodeEstimate/admittance;
        capacitor=(gc*previous+g*rect)/(gc+g);return capacitor;
#endif
    }
    static constexpr std::array<double,4> thresholds{{.08115253739381625,.24749687874856177,
        .7744375627310338,6.921466865773948}};
private:double fs=48000,capacitor=0,diodeEstimate=0,gcCached=0,inverseDen=0,offDecay=0,inverseAdmittance=0;
};
}
