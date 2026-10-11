#include "AnalogEngine376.h"
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace channel376;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{auto engine=std::make_unique<AnalogEngine376>();
    check(AnalogParameters376().oversampling==OversamplingMode::off,"default OFF");
    for(auto mode:{OversamplingMode::off,OversamplingMode::x2,OversamplingMode::x4}){
        AnalogParameters376 p;p.oversampling=mode;p.values[5]=-20;p.values[6]=.7;p.values[8]=4;p.switches.overEasy=true;
        const int delay=mode==OversamplingMode::off?0:mode==OversamplingMode::x2?64:96;
        for(auto precision:{StreamPrecision::float32,StreamPrecision::float64}){
            engine->prepare(48000,2,precision,p);check(engine->latency()==delay,"mode delay");
            for(int n=0;n<8192;++n){const auto y=engine->process({.1*std::sin(n*.131),0});check(std::isfinite(y[0])&&std::abs(y[1])<1e-9,"finite, isolated stereo");}
            check(engine->failedSolves()==0,"mode solver convergence");
            engine->prepare(48000,2,precision,p);engine->setBypass(true);for(int n=0;n<512;++n)engine->process({0,0});
            for(int n=0;n<128;++n){const auto y=engine->process({n==0?.5:0,0});check(y[0]==(n==delay?.5:0),"bypass delay per mode");}
        }
        for(double rate:{8000.,44100.,96000.,384000.}){
            engine->prepare(rate,1,StreamPrecision::float64,p);
            for(int n=0;n<512;++n)check(std::isfinite(engine->process({.03*std::sin(n*.131),0})[0]),"mode/rate finite");
            check(engine->failedSolves()==0,"mode/rate convergence");
        }
    }
    AnalogParameters376 p;engine->prepare(48000,2,StreamPrecision::float64,p);
    for(int n=0;n<8192;++n){if(n==1024||n==3072||n==5120){p.oversampling=n==1024?OversamplingMode::x2:n==3072?OversamplingMode::x4:OversamplingMode::off;engine->setParameters(p);}
        engine->setParameters(p);const auto y=engine->process({.03*std::sin(n*.131),.02*std::sin(n*.173)});check(std::isfinite(y[0])&&std::isfinite(y[1]),"live mode switch finite");}
    check(engine->oversamplingFactor()==1&&engine->latency()==0&&engine->failedSolves()==0,"switch back OFF");
    std::cout<<"PASS OFF/2x/4x, stereo float32/64, 0/64/96 latency, bypass and live switch\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
