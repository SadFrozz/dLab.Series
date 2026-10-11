#include "dsp/Engine.h"
#include "dsp/InputGain.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
int checks=0;
void require(bool ok,const char* text) {
    ++checks;
    if(!ok) throw std::runtime_error(text);
}
std::vector<float> tone(double sr,double hz,float amp=0.1f,double seconds=2) {
    std::vector<float> x(static_cast<size_t>(sr*seconds));
    for(size_t i=0;i<x.size();++i) x[i]=amp*static_cast<float>(std::sin(2*pi*hz*i/sr));
    return x;
}
c286::Meters run(c286::Engine& e,std::vector<float>& x,int block=256) {
    c286::Meters m;
    for(size_t offset=0;offset<x.size();) {
        const int n=std::min(block,static_cast<int>(x.size()-offset));
        float* ptr=x.data()+offset; m=e.process(&ptr,1,n); offset+=static_cast<size_t>(n);
    }
    return m;
}
float rms(const std::vector<float>& x) {
    double sum=0; const size_t begin=x.size()/2;
    for(size_t i=begin;i<x.size();++i) sum+=static_cast<double>(x[i])*x[i];
    return static_cast<float>(std::sqrt(sum/(x.size()-begin)));
}
float measured(double sr,double hz,const c286::Parameters& p,float amp=.1f) {
    c286::Engine e; e.prepare(sr,1); e.setParameters(p);
    auto x=tone(sr,hz,amp); run(e,x); return rms(x);
}
}
int main() {
  try {
    for(float x:{-100.f,-2.f,-1.f,0.f,1.f,2.f,100.f}) {
        require(std::abs(c286::Engine::railLimit(x))<1.46f,"Rail model must be bounded");
        require(c286::Engine::railLimit(x)==-c286::Engine::railLimit(-x),"Rail model symmetry");
    }
    require(c286::Engine::railLimit(.5f)==.5f,"Rail model must be linear below overload");
    c286::Parameters neutral;
    for(bool modern:{false,true})for(float requested:{-15.f,-5.f,0.f,15.f,45.f,50.f}){
        auto line=neutral;line.modelS=modern;line.inputDb=requested;line.processBypass=true;
        const float expected=.00001f/std::sqrt(2.f)*std::pow(10.f,c286::effectiveLineGain(requested,modern)/20.f);
        require(std::abs(measured(48000,1000,line,.00001f)/expected-1)<1.e-5,"LINE gain must apply mode limits in audio, including bypass");
    }
    auto modern=neutral;modern.modelS=true;
    require(std::abs(measured(48000,997,modern)-measured(48000,997,neutral))<1.e-7f,"Mode must not change neutral gain");
    auto modernLf=modern;modernLf.lfDetail=10;
    require(measured(48000,250,modernLf)<measured(48000,250,modern)*.8f,"LF Detail must include low-mid cut");
    auto originalHp=neutral;originalHp.highPass=true;
    auto modernHp=originalHp;modernHp.modelS=true;
    require(std::abs(measured(48000,80,modernHp)-measured(48000,80,originalHp))>.0001f,"Modes must use different schematic HPF values");
    auto reduction=[](bool s,float output) {
        c286::Engine engine;engine.prepare(48000,1);
        c286::Parameters settings;settings.modelS=s;settings.drive=6;settings.outputDb=output;
        engine.setParameters(settings);auto signal=tone(48000,1000,.2f);
        return run(engine,signal).compressorReductionDb;
    };
    require(std::abs(reduction(true,0)-reduction(true,10))<1.e-6f,"286s output gain must not drive pilot compression");
    require(std::abs(reduction(false,0)-reduction(false,10))<1.e-6f,"Original output gain must not drive pilot compression");
    for(bool s:{false,true}) {
        auto settings=neutral; settings.modelS=s; settings.drive=6;
        c286::Engine engine; engine.prepare(48000,1);engine.setParameters(settings);
        auto first=tone(48000,1000,.05f);const auto a=run(engine,first);
        engine.prepare(48000,1);engine.setParameters(settings);
        auto second=tone(48000,1000,.1f);const auto b=run(engine,second);
        require(b.compressorReductionDb>a.compressorReductionDb,
                "Pilot circuit must increase reduction with input level");
        require(rms(second)/rms(first)<1.95f,"Pilot circuit must compress the level increase");
        require(rms(second)>rms(first),"Steady compressor output must increase with input level");
        settings.gateThresholdDb=0;settings.gateRatio=10;
        engine.prepare(48000,1);engine.setParameters(settings);
        auto gated=tone(48000,1000,.1f);const auto c=run(engine,gated);
        require(std::abs(c.compressorReductionDb-b.compressorReductionDb)<1.e-6f,
                "Gate CV must not enter the pilot VCA in either schematic");
    }
    for(bool s:{false,true}) {
        auto settings=neutral;settings.modelS=s;settings.deThreshold=10;
        auto mix=tone(48000,8000,.2f);auto low=tone(48000,250,.05f);
        for(size_t i=0;i<mix.size();++i) mix[i]+=low[i];
        c286::Engine engine;engine.prepare(48000,1);engine.setParameters(settings);run(engine,mix);
        double amplitude=0;
        for(size_t i=mix.size()/2;i<mix.size();++i) amplitude+=mix[i]*std::sin(2*pi*250*i/48000.);
        amplitude*=4./mix.size();
        require(amplitude<.04,"De-esser must reduce LF too when sibilance triggers wideband reduction");
    }
    {
        c286::Engine engine;engine.prepare(48000,1);auto settings=neutral;
        settings.highPass=true;settings.drive=5;settings.lfDetail=4;engine.setParameters(settings);
        auto signal=tone(48000,80,.15f,.5);run(engine,signal);
        for(int i=0;i<30;++i) {
            settings.modelS=i%2;engine.setParameters(settings);
            auto chunk=tone(48000,80,.15f,.02);run(engine,chunk);
            for(float sample:chunk) require(std::isfinite(sample)&&std::abs(sample)<1.46f,"Repeated mode switches must remain finite and bounded");
        }
    }
    auto disabled=neutral; disabled.drive=8; disabled.density=0;
    require(measured(48000,1000,disabled,.5f)<measured(48000,1000,neutral,.5f)*.7f,"Minimum Density controls release; Drive remains the compression enable");
    disabled=neutral; disabled.gateRatio=10; disabled.gateThresholdDb=-80;
    disabled.gateOff=true;
    require(std::abs(measured(48000,1000,disabled,1.e-6f)/measured(48000,1000,neutral,1.e-6f)-1)<.0001f,"Gate threshold OFF must bypass expansion");
    for(double sr:{44100.,48000.,96000.,192000.}) {
        auto x=tone(sr,997); const auto original=x;
        c286::Engine e; e.prepare(sr,1); e.setParameters(neutral); run(e,x);
        require(x==original,"Neutral path must be sample-identical");
        c286::Parameters bypass=neutral; bypass.processBypass=true; bypass.inputDb=6;
        bypass.drive=10; bypass.gateRatio=10; bypass.outputDb=-30; bypass.lfDetail=10;
        e.prepare(sr,1); e.setParameters(bypass); x=original; run(e,x);
        require(std::abs(rms(x)/rms(original)-std::pow(10.f,6.f/20.f))<.0001f,
          "Process bypass must retain input gain and bypass output gain");
        c286::Parameters hp=neutral; hp.highPass=true;
        require(measured(sr,20,hp)/measured(sr,20,neutral)<.1f,"HPF must attenuate 20Hz");
        require(std::abs(measured(sr,1000,hp)/measured(sr,1000,neutral)-1)<.02f,"HPF passband");
        c286::Parameters gate=neutral; gate.gateRatio=4; gate.gateThresholdDb=-40;
        require(measured(sr,1000,gate,.001f)<measured(sr,1000,neutral,.001f)*.01f,"Gate must close on quiet tone");
        require(measured(sr,1000,gate,.2f)>measured(sr,1000,neutral,.2f)*.99f,"Gate must open above threshold");
        c286::Parameters de=neutral; de.deThreshold=10;
        require(measured(sr,8000,de)<measured(sr,8000,neutral)*.65f,"De-esser must reduce sibilant band");
        require(measured(sr,250,de)>measured(sr,250,neutral)*.95f,"De-esser must preserve LF tone");
        c286::Parameters lf=neutral; lf.lfDetail=10;
        require(measured(sr,80,lf)>measured(sr,80,neutral)*2,"LF detail boosts low band");
        c286::Parameters hf=neutral; hf.hfDetail=10;
        require(measured(sr,8000,hf)>measured(sr,8000,neutral)*1.5f,"HF detail boosts HF band");
        c286::Parameters comp=neutral; comp.drive=8;
        require(measured(sr,1000,comp,.5f)<measured(sr,1000,neutral,.5f)*.7f,"Drive compresses loud signal");
    }
    // Host partitioning must not change the sample-domain dynamics.
    c286::Parameters p; p.drive=5; p.density=9; p.deThreshold=6;
    p.gateRatio=3; p.highPass=true; p.lfDetail=2; p.hfDetail=3;
    auto a=tone(48000,6300,.3f); auto b=a;
    c286::Engine ea,eb; ea.prepare(48000,1); eb.prepare(48000,1); ea.setParameters(p); eb.setParameters(p);
    run(ea,a,1); run(eb,b,511); require(a==b,"Block-size independence");
    ea.prepare(48000,1); ea.setParameters(p); auto reset=tone(48000,6300,.3f); run(ea,reset,64);
    require(reset==a,"Prepare/reset must clear all signal history");
    // Stereo link preserves L/R ratio; sidechains do not cancel anti-phase channels.
    ea.prepare(48000,2); ea.setParameters(p); a=tone(48000,1000,.3f); b=a;
    for(auto& x:b) x=-x;
    std::array<float*,2> ptr{a.data(),b.data()}; ea.process(ptr.data(),2,static_cast<int>(a.size()));
    float maxError=0; for(size_t i=0;i<a.size();++i) maxError=std::max(maxError,std::abs(a[i]+b[i]));
    require(maxError<1.e-6f,"Linked stereo must preserve anti-phase symmetry");
    // All controls automated to random valid values; silence, full scale and invalid input.
    std::mt19937 rng(286); std::uniform_real_distribution<float> unit(0,1);
    std::vector<float> noise(257); ea.prepare(8000,1);
    for(int block=0;block<1000;++block) {
        p.inputDb=-20+80*unit(rng); p.drive=10*unit(rng); p.density=10*unit(rng);
        p.deFrequency=800+9200*unit(rng); p.deThreshold=10*unit(rng);
        p.lfDetail=10*unit(rng); p.hfDetail=10*unit(rng);
        p.gateThresholdDb=-80+80*unit(rng); p.gateRatio=1+9*unit(rng);
        p.outputDb=-30+45*unit(rng); p.highPass=unit(rng)>.5f; p.processBypass=unit(rng)>.5f;p.modelS=unit(rng)>.5f;
        ea.setParameters(p);
        for(auto& x:noise) x=block%2 ? 0.f:2*unit(rng)-1;
        noise[0]=std::numeric_limits<float>::quiet_NaN();
        float* n=noise.data(); ea.process(&n,1,static_cast<int>(noise.size()));
        require(std::all_of(noise.begin(),noise.end(),[](float x){return std::isfinite(x);}),"Automation finite output");
    }
    std::cout<<"PASS: "<<checks<<" DSP checks\n";
    return EXIT_SUCCESS;
  } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return EXIT_FAILURE; }
}
