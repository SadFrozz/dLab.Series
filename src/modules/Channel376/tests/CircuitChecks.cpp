#include "CompressorCircuit376.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstdlib>
using namespace channel376;
void check(bool x,const char* message){if(!x){std::cerr<<message<<'\n';std::exit(1);}}
int main(int argc,char** argv){
    std::ofstream report;if(argc>1)report.open(argv[1]);
    if(report)report<<"slow,time_s,target_V,comp_V,z_V,emitter_V,collector_V,inverting_V,gr_meter_V\n"<<std::setprecision(17);
    for(bool slow:{false,true}){
        CompressorCircuit376 c;c.prepare(48000,slow);
        std::cout<<"DC slow="<<slow<<" z="<<c.nodes().z<<" cv="<<c.output()<<" residual="<<c.maximumResidualAmps()<<'\n';
        check(c.failedSolves()==0,"DC KCL convergence");
        const double idle=c.output();
        for(int n=0;n<48000;++n){
            const double target=n<24000?-15*.0061*36500/1000:0;
            double v=c.tickTargetVoltage(target,slow);const auto& s=c.nodes();
            check(std::isfinite(v)&&std::abs(v)<=13.5,"finite physical opamp rails");
            if(n==23999)check(v<-3,"compressor demanded attenuation");
            if(report&&n%24==0)report<<slow<<','<<(n+1)/48000.<<','<<target<<','<<v<<','<<s.z<<','<<s.emitter<<','<<s.collector<<','<<s.inverting<<','<<c.meterCv()<<'\n';
        }
        std::cout<<"transient slow="<<slow<<" max residual="<<c.maximumResidualAmps()<<" failed="<<c.failedSolves()<<" idle="<<idle<<'\n';
        check(c.failedSolves()==0,"transient KCL convergence");
        for(int n=0;n<4000;++n){
            const auto previous=c.nodes();const double cv=c.tick(n<2000?60:0,!slow);
            check(std::isfinite(cv)&&std::abs(cv)<=13.5,"mode switch/overload bounded");
            if(n==0)check(std::abs(c.nodes().z-previous.z)<.3,"SLOW switch preserves capacitor charge");
        }
        check(c.failedSolves()==0,"rail/mode-switch KCL convergence");
    }
    for(double cv:{-.122,0.,.122}) {
        const double wanted=cv==0?0:(cv<0?20:-20);
        check(std::abs(20*std::log10(Blackmer376::gain(cv))-wanted)<1e-12,"manufacturer6.1mV/dB exponential VCA");
        check(Blackmer376::outputCurrent(.0001,cv)>0,"VCA delivered current sign");
    }
    std::cout<<"PASS full Q1/D8/D2/U5B/C37/C51/C52 node equations\n";
}
