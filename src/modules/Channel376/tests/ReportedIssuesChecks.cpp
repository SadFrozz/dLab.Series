#include "AnalogEngine376.h"
#include "DitherShape376.h" // Historical standalone algorithm reference only.
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace channel376;
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
struct Result{std::complex<double> response;double gr=0,de=0,seconds=0;};
Result measure(double hz,AnalogParameters376 p){auto e=std::make_unique<AnalogEngine376>();e->prepare(48000,1,StreamPrecision::float32,p);Result r;auto start=std::chrono::steady_clock::now();
    for(int n=0;n<48000;++n){double phase=6.283185307179586*hz*n/48000;double y=e->process({.03*std::sin(phase),0})[0];if(n>=43200)r.response+=y*std::complex<double>(std::sin(phase),std::cos(phase));}
    r.response*=2./(4800*.03);r.gr=e->channelState(0).gainReductionMeterVoltage();r.de=e->channelState(0).deCvVoltage();r.seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();check(e->failedSolves()==0,"solver convergence");return r;
}
int main(){try{AnalogParameters376 p;
    auto normal=measure(1000,p);p.switches.phase=true;auto inverse=measure(1000,p);double phaseError=std::abs(normal.response+inverse.response)/std::abs(normal.response);std::cout<<"Phase fundamental null relative "<<phaseError<<'\n';check(phaseError<.001,"PHASE not inverting full audio path");
    p={};auto low=measure(20,p);p.switches.lowCut=true;auto cut=measure(20,p);double lowDb=20*std::log10(std::abs(cut.response/low.response));std::cout<<"Low Cut at20Hz dB "<<lowDb<<'\n';check(lowDb<-15,"LOW CUT not removing low frequency");
    // Integer-cycle 75/150 Hz acquisition starts at 0.9 s; use 80/160 for
    // spectral quadrature here, with exact75Hz verified by direct AC test.
    for(double hz:{40.,80.,160.}){p={};auto unfiltered=measure(hz,p);p.switches.lowCut=true;auto filtered=measure(hz,p);double db=20*std::log10(std::abs(filtered.response/unfiltered.response));std::cout<<"Low Cut "<<hz<<"Hz dB "<<db<<'\n';if(hz==80)check(db>-3&&db< -1,"75Hz circuit nominal region");}
    p={};p.values[5]=-30;p.values[6]=.7;auto hard=measure(1000,p);p.switches.overEasy=true;auto soft=measure(1000,p);std::cout<<"Compressor hard/OverEasy GR voltages "<<hard.gr<<' '<<soft.gr<<" time "<<hard.seconds<<' '<<soft.seconds<<'\n';check(hard.gr<-.5&&soft.gr<-.5,"OverEasy compressor attenuation missing above knee");
    p={};auto deOff=measure(8000,p);p.values[8]=7;auto deOn=measure(8000,p);double deDb=20*std::log10(std::abs(deOn.response/deOff.response));std::cout<<"De-esser8kHz attenuation dB "<<deDb<<" CV "<<deOn.de<<" time "<<deOn.seconds<<'\n';check(deOn.de<-.3&&deDb<-1,"De-esser not reducing HF-rich signal");
    // Compare exact, same-seed host-float32 outputs. Dither noise is a local
    // floating-point LSB, not an audible 16-bit integer noise floor.
    DitherShape376 none,noise,shape;none.prepare(48000,StreamPrecision::float32,42);noise.prepare(48000,StreamPrecision::float32,42);shape.prepare(48000,StreamPrecision::float32,42);noise.modes(DitherMode::tpdf,ShapeMode::off);shape.modes(DitherMode::tpdf,ShapeMode::shape1);int changedNoise=0,changedShape=0;double error=0;
    for(int n=0;n<16384;++n){const double x=.375+.01*std::sin(n*.13),a=none.process(x),b=noise.process(x),c=shape.process(x);changedNoise+=a!=b;changedShape+=b!=c;error+=(b-a)*(b-a);}
    std::cout<<"Dither changed samples "<<changedNoise<<" Shape changed samples "<<changedShape<<" difference RMS dBFS "<<20*std::log10(std::sqrt(error/16384))<<'\n';check(changedNoise>1000&&changedShape>1000,"Dither/Shape unchanged output");
    for(double input:{.03,.125})for(double drive:{0.,10.,15.}){
        auto engine=std::make_unique<AnalogEngine376>();AnalogParameters376 params;params.values[0]=drive;params.values[9]=-15;engine->prepare(48000,1,StreamPrecision::float64,params);
        std::array<std::complex<double>,10> harmonics{};bool eqClip=false;
        for(int n=0;n<48000;++n){double phase=6.283185307179586*1000*n/48000,y=engine->process({input*std::sin(phase),0})[0];if(n>=38400){for(int h=1;h<=10;++h)harmonics[h-1]+=y*std::complex<double>(std::cos(h*phase),std::sin(h*phase));eqClip|=engine->channelState(0).eqClip();}}
        double power=0;for(int h=1;h<10;++h)power+=std::norm(harmonics[h]);double thd=100*std::sqrt(power)/std::abs(harmonics[0]);
        std::cout<<"Harmonics PCMpeak="<<input<<" Drive="<<drive<<" THD%="<<thd<<" H2dBc="<<20*std::log10(std::abs(harmonics[1]/harmonics[0]))<<" EQclip="<<eqClip<<'\n';
#if defined(CHANNEL376_EXPERIMENTAL_TUBE_COLOUR)
        // The user approved clean tonal profiles and rejected generated
        // distortion. Keep the original nonlinear test for circuit research.
        check(std::isfinite(thd),"nonfinite clean-profile spectrum");
        if(drive==0)check(thd<.01,"clean character adds audible-range harmonic distortion at nominal level");
#else
        check(thd>1e-4,"nonlinear harmonic generation absent");
#endif
    }
    std::cout<<"PASS user-reported controls and loaded OverEasy equation\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
