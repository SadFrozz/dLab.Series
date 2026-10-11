#include "Numerical376.h"
#include "FastHalfband376.h"
#include "ControlReference376.h"
#include <iostream>
#include <cstdlib>
using namespace channel376;
void check(bool b,const char* s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
int main(){
    for(double fs:{44100.,192000.,768000.}){InputMeter376 fast;fast.prepare(fs);double cap=0,error=0;
        for(int n=0;n<12000;++n){const double volts=8*std::sin(n*.071)+.1*std::cos(n*.613),gc=1e-6*fs,g=.001;
            if(volts<0)cap=(gc*cap-g*volts)/(gc+g);
            else {const double previous=cap,off=previous/(1+1/(fs*1e-6*21000)),rectOff=off*20000/21000;
                if(volts<=rectOff)cap=off;
                else {double lo=rectOff,hi=volts;
                    for(int iteration=0;iteration<40;++iteration){const double rect=(lo+hi)*.5,c=(gc*previous+g*rect)/(gc+g),i=std::max(0.,rect/20000+(rect-c)/1000);
                        if(rect+Diode376::voltage(i)>volts)hi=rect;else lo=rect;}
                    cap=(gc*previous+g*(lo+hi)*.5)/(gc+g);}}
            error=std::max(error,std::abs(cap-fast.tick(volts)));}
        std::cout<<"Meter Thevenin max error "<<error<<'\n';check(error<1e-10,"meter elimination preserves original diode/capacitor solution");}
    for(bool nonlinear:{false,true}){Oversample4x376 reference;FastOversample4x376 polyphase;double error=0;
        for(int n=0;n<16384;++n){const double input=n==0?1:std::sin(n*.031)*.5+std::cos(n*.217)*.2;auto f=[=](double x){return nonlinear?x*x*x:x;};error=std::max(error,std::abs(reference.process(input,f)-polyphase.process(input,f)));}
        std::cout<<"Polyphase max error "<<error<<'\n';check(error<1e-14,"polyphase must preserve 129-tap linear/nonlinear SRC within floating-point accumulation error");}
    Fir129 optimized;std::array<double,129> original{};int cursor=0;
    Fir129 allOutputs,decimated;
    for(int n=0;n<4096;++n){const double x=std::sin(n*.031)+std::cos(n*.21);const bool needed=n%2==0;const double reference=allOutputs.tick(x),actual=decimated.tick(x,needed);
        if(needed)check(actual==reference,"unused FIR outputs may be skipped without losing charge/history or changing retained samples");}
    for(int n=0;n<4096;++n){const double input=std::sin(n*.031)+std::cos(n*.21);original[cursor]=input;double expected=0;
        for(int k=0;k<129;++k)expected+=halfband376[k]*original[(cursor+129-k)%129];
        check(optimized.tick(input)==expected,"contiguous FIR preserves original tap arithmetic exactly");cursor=(cursor+1)%129;}
    Oversample4x376 core;double maximum=0;int position=0;
    for(int n=0;n<512;++n){double y=core.process(n==0?1:0,[](double x){return x;});
        if(std::abs(y)>maximum){maximum=std::abs(y);position=n;}
        check(core.delayedDry(n==0?1:0)==(n==96?1:0),"dry latency");}
    check(position==96,"4x linear-phase group delay 96 samples");
    core.reset();double y=0;for(int n=0;n<4096;++n)y=core.process(.25,[](double x){return x;});
    check(std::abs(y-.25)<1e-5,"unity DC");
    core.reset();for(int n=0;n<4096;++n)check(core.process(0,[](double x){return x*x*x;})==0,"silent oversampling");
    core.reset();double real=0,imaginary=0;
    constexpr double pi=3.14159265358979323846;
    for(int n=0;n<48256;++n){const double input=.5*std::cos(2*pi*13000*n/48000);
        double out=core.process(input,[](double x){return x*x*x;});
        if(n>=256){real+=out*std::cos(2*pi*9000*n/48000);imaginary+=out*std::sin(2*pi*9000*n/48000);}}
    const double aliasedAmplitude=2*std::hypot(real,imaginary)/48000;
    check(aliasedAmplitude<1e-5,"4x suppression of cubic 39kHz folded to 9kHz");
    for(double fs:{44100.,48000.,96000.,192000.,384000.}){
        MeanSquare376 rms;rms.prepare(fs);
        for(int n=0;n<int(fs*.2);++n)rms.tick(1);
        check(std::abs(rms.state-1)<1e-8,"physical RMS convergence");
        Meter376 meter;meter.prepare(fs);
        check(std::isinf(meter.outputDbfs())&&meter.outputDbfs()<0,"silent meter matches -infinity contract");
        meter.outputTick(Meter376::displayFloorPeak*.5);
        check(std::isinf(meter.outputDbfs()),"below -150dBFS display floor");
        check(meter.peak>0,"display floor preserves the measured signal");
        meter.outputTick(Meter376::displayFloorPeak);
        check(std::abs(meter.outputDbfs()+150)<1e-10,"exactly -150dBFS remains numeric");
        meter.outputTick(.5);
        check(std::abs(meter.outputDbfs()+6.020599913279624)<1e-10,"final PCM dBFS meter");
    }
    std::cout<<"PASS latency96, dry alignment, unity, silence, RMS and PCM meters; cubic alias="<<aliasedAmplitude<<'\n';
}
