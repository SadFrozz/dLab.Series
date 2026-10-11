#include "AnalogEngine376.h"
#include <memory>
#include <iostream>
using namespace channel376;
int main(){auto engine=std::make_unique<AnalogEngine376>();AnalogParameters376 p;
    p.values={8,5,-4,1800,6,-20,.7,5000,5,-3};p.switches.overEasy=true;p.digital={DitherMode::tpdf,ShapeMode::shape1};
    engine->prepare(48000,2,StreamPrecision::float64,p);processingProfile376={};
    for(int n=0;n<48000;++n)engine->process({.15*std::sin(n*.131)+.02*std::sin(n*.76),.1*std::sin(n*.173)});
    for(int nodes=0;nodes<16;++nodes)if(processingProfile376.predictions[nodes])std::cout<<"nodes="<<nodes<<" predictions="<<processingProfile376.predictions[nodes]<<" overloads="<<processingProfile376.overloads[nodes]<<" mask_trials="<<processingProfile376.maskTrials[nodes]<<'\n';
    std::cout<<"tube_scalar_iterations="<<processingProfile376.tubeScalarIterations<<" tube_full_solves="<<processingProfile376.tubeFullSolves<<" tube_full_iterations="<<processingProfile376.tubeFullIterations<<'\n';
    return engine->failedSolves()?1:0;
}
