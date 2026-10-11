#include "ControlReference376.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <cstdlib>
using namespace channel376;
void check(bool b,const char* s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
int main(int argc,char**argv){
    for(double voltage:{.01,.2,1.,3.,13.5})for(double series:{0.,10.,20000.}){
        const double i=Diode376::current(voltage,series);
        check(std::abs(Diode376::voltage(i)+series*i-voltage)<1e-8,"diode inverse across amplifier rail and series bounds");}
    for(double d:{-20.,-10.,0.,10.,20.}) {
        double cv=cv376(outputPot376(d),0,0);
        check(std::abs(20*std::log10(gain376(cv))-d)<1e-10,"loaded output gain calibration");
    }
    check(loadedRatio376(0).compression==0&&loadedRatio376(1).compression==1,"ratio endpoints");
    check(std::abs(1/(1-loadedRatio376(.5).compression)-4.13108009299)<1e-8,"loaded center ratio");
    for(double p:{0.,.25,.5,1.})for(double l:{-60.,-20.,0.,20.}) {
        const double x=requestedReduction376(l,-10,p,true);
        check(x>=0&&x<=60&&std::isfinite(x),"diode soft knee bounded");
    }
    Rms376 rms;rms.prepare(48000);
    for(int n=0;n<24000;++n)rms.tick(Rms376::referenceCurrent*std::sqrt(2.)*std::sin(n*2*3.141592653589793/48),1e30);
    check(std::abs(rms.voltage())<.0003,"RMS volts-per-db level match including ripple");
    InputMeter376 input;input.prepare(48000);double im=0;
    for(int n=0;n<20000;++n)im=input.tick(-1);
    check(std::abs(im-1)<1e-10,"negative precision rectifier DC");
    for(int n=0;n<20000;++n)im=input.tick(1);
    check(im>.5&&im<.9,"positive rectifier contains loaded diode drop");
    for(int n=0;n<50000;++n)im=input.tick(0);
    check(im<1e-10,"meter off branch discharges through 21k");
    DeCv376 de;de.prepare(48000);double v=0;
    for(int n=0;n<10000;++n)v=de.tick(.1,.1,.5);
    check(v<0&&v>-8,"de CV negative, rail-limited and finite");
    const double expected=-523000*(.1/280000+(15*.5/(1+1250/1e6))/1e6);
    check(std::abs(v-expected)<1e-8,"feedback taken after D44/D45, no 5.6V CV threshold");
    for(int n=0;n<1000;++n)v=de.tick(.1,.02,0);
    check(std::abs(v)<1e-10,"de passive discharge through R57 parallel R221");
    for(int n=0;n<10000;++n)v=de.tick(0,.5,1);
    check(v>=-8&&de.amplifierVoltage()>=-13.5,"de actual amplifier rail bound");
    std::ofstream output;if(argc>1)output.open(argv[1]);
    if(output)output<<"slow,time_s,request_db,comp_cv_V,gr_meter_V\n"<<std::setprecision(17);
    for(bool slow:{false,true}) {
        CompressorMemory376 c;c.prepare(48000);double previous=0;
        for(int n=0;n<36000;++n){const double demand=n<24000?15:0;
            const double cv=c.tick(demand,slow);
            check(std::isfinite(cv)&&cv<=0&&cv>=-15,"coupled compressor transient bounded");
            if(n==23999)check(std::abs(cv+.219*15)<.2,"compressor steady CV source network");
            if(n==35999)check(std::abs(cv)<std::abs(previous),"release and program memory");
            if(n==24000)previous=cv;
            if(output&&n%24==0)output<<slow<<','<<double(n)/48000<<','<<demand<<','<<cv<<','<<c.meterCv()<<'\n';
        }
    }
    std::cout<<"PASS RMS reference, output trim, ratio, diode knee, multi-cap memory and de-CV feedback\n";
}
