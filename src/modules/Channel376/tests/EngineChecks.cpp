#include "AnalogEngine376.h"
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <vector>
#include <memory>
using namespace channel376;
void require(bool ok,const char* s){if(!ok)throw std::runtime_error(s);}
int main(int argc,char** argv){try{
    constexpr double pi=3.14159265358979323846;
    auto storage=std::make_unique<AnalogEngine376>();auto& engine=*storage;engine.prepare(48000,1,StreamPrecision::float64);
    for(int n=0;n<2400;++n)require(std::abs(engine.process({0,0})[0])<1e-10,"silence DC startup");
    require(engine.failedSolves()==0,"silent solver");require(!std::isfinite(engine.outputDbfs()),"silent meter");
    // Whole nominal neutral chain; small signal must preserve polarity/gain.
    engine.prepare(48000,1,StreamPrecision::float64);std::complex<double> response{};
    const auto begin=std::chrono::steady_clock::now();
    for(int n=0;n<48000;++n){const double ph=2*pi*1000*n/48000,y=engine.process({.001*std::sin(ph),0})[0];
        if(n>=43200)response+=y*std::complex<double>(std::sin(ph),std::cos(ph));require(std::isfinite(y),"neutral finite");}
    response*=2/(4800*.001);
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    std::cout<<"Neutral response "<<response<<" magnitude "<<std::abs(response)<<" CPU seconds/audio-second mono48k "<<seconds<<" failures "<<engine.failedSolves()<<'\n';
#if defined(CHANNEL376_EXPERIMENTAL_TUBE_COLOUR)
    SaturationTube376 colourReference;colourReference.prepare(48000,AnalogChannel376::fullPathUnityTrim);
    const double expected=colourReference.tonalMagnitude(1000,0);
    require(std::abs(response)/expected>.97&&std::abs(response)/expected<1.03&&response.real()>.9*expected,"full chain calibrated tonal gain and polarity");
#else
    require(std::abs(response)>.97&&std::abs(response)<1.03&&response.real()>.97,"full chain signed unity");
#endif
    require(engine.failedSolves()==0,"neutral convergence");
    // Exact delayed host bypass after transition, with the wet path still running.
    AnalogParameters376 legacy;legacy.oversampling=OversamplingMode::x4;engine.prepare(48000,1,StreamPrecision::float64,legacy);engine.setBypass(true);
    for(int n=0;n<300;++n)engine.process({0,0});
    for(int n=0;n<200;++n){const double y=engine.process({n==0?1.:0.,0})[0];require(std::abs(y-(n==96?1.:0.))<1e-10,"96sample host bypass");}
    AnalogParameters376 p;p.values[5]=-20;p.values[6]=.7;p.values[8]=4;
    // Stereo maximum attenuation without mixing audio channels.
    engine.prepare(48000,2,StreamPrecision::float64,p);
    for(int n=0;n<12000;++n){const auto y=engine.process({.5*std::sin(n*.12),0});require(std::abs(y[1])<1e-9,"linked stereo channel isolation");require(std::isfinite(y[0]),"linked finite");}
    std::cout<<"Stereo failures "<<engine.failedSolves()<<'\n';for(int j=0;j<2;++j){for(auto n:engine.channelState(j).failureDetails())std::cout<<n<<' ';std::cout<<'\n';}require(engine.failedSolves()==0,"linked solver convergence");
    // Automation at exact sample positions has identical results under chunking.
    std::vector<std::array<double,2>> reference(4096);
    engine.prepare(48000,2,StreamPrecision::float64);
    auto automate=[&](int n){if(n==512){p.values[0]=8;p.values[1]=8;p.values[4]=-5;p.switches.lowCut=true;engine.setParameters(p);}
        if(n==2048){p.switches.slow=true;p.switches.overEasy=true;p.digital.dither=DitherMode::tpdf;p.digital.shape=ShapeMode::shape1;engine.setParameters(p);}};
    for(int n=0;n<4096;++n){automate(n);reference[n]=engine.process({.05*std::sin(n*.17),.03*std::sin(n*.12)});}
    require(engine.failedSolves()==0,"automation solver");
    engine.prepare(48000,2,StreamPrecision::float64);p=AnalogParameters376();p.values[5]=-20;p.values[6]=.7;p.values[8]=4;
    for(int block=0;block<4096;block+=37)for(int n=block;n<std::min(block+37,4096);++n){automate(n);require(engine.process({.05*std::sin(n*.17),.03*std::sin(n*.12)})==reference[n],"state/block invariance");}
    for(double rate:{8000.,44100.,96000.,384000.}){
        engine.prepare(rate,1,StreamPrecision::float32);for(int n=0;n<512;++n)require(std::isfinite(engine.process({.1*std::sin(n*.03),0})[0]),"whole chain rate finite");
        require(engine.failedSolves()==0,"sample rate solver");}
    engine.prepare(48000,1,StreamPrecision::float64);
    for(double invalid:{HUGE_VAL,-HUGE_VAL,std::nan("")})require(std::isfinite(engine.process({invalid,0})[0]),"invalid PCM safe");
    if(argc>1){std::ofstream csv(argv[1]);csv<<std::setprecision(17)<<"sample,input_pcm,output_pcm,input_rectifier_V,comp_meter_V,eq_V\n";
        engine.prepare(48000,1,StreamPrecision::float64,p);
        for(int n=0;n<24000;++n){const double x=.2*std::sin(2*pi*1000*n/48000),y=engine.process({x,0})[0];
            csv<<n<<','<<x<<','<<y<<','<<engine.channelState(0).inputMeterVoltage()<<','<<engine.channelState(0).gainReductionMeterVoltage()<<','<<engine.channelState(0).eqOutputVoltage()<<'\n';}}
    std::cout<<"PASS assembled nominal chain with loaded opamp headroom\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
