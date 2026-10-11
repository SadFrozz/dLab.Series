#include "CompressorManifold376.h"
#include <chrono>
#include <iostream>
#include <memory>
using namespace channel376;
volatile double manifoldSum=0;
int main(){try{
    auto fast=std::make_unique<CompressorManifold376>();
    for(bool initialSlow:{false,true}){
        fast->prepare(48000,initialSlow);CompressorCircuit376 reference;reference.prepare(48000,initialSlow);
        double error=0,rmsError=0;
        for(int n=0;n<96000;++n){const bool slow=n<48000?initialSlow:!initialSlow;
            const double target=n<16000?-3.33975:n<32000?0:n<48000?-12.5:n<64000?-3-.3*std::sin(n*.071):n<80000?3:0;
            const double actual=fast->tick(target,slow),expected=reference.tickTargetVoltage(target,slow),diff=actual-expected;
            error=std::max(error,std::abs(diff));rmsError+=diff*diff;}
        std::cout<<"initialSlow="<<initialSlow<<" max_error_V="<<error<<" rms_error_V="<<std::sqrt(rmsError/96000)<<" failed="<<fast->failedSolves()<<'\n';
        if(error>.001||fast->failedSolves()||reference.failedSolves())return 1;
    }
    CompressorCircuit376 reference;reference.prepare(48000);double sum=0;
    auto start=std::chrono::steady_clock::now();for(int n=0;n<48000;++n)sum+=reference.tickTargetVoltage(-3-.03*std::sin(n*.071),false);manifoldSum=sum;
    const double original=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    fast->prepare(48000);sum=0;start=std::chrono::steady_clock::now();
    for(int n=0;n<48000;++n)sum+=fast->tick(-3-.03*std::sin(n*.071),false);manifoldSum=sum;
    const double reduced=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"original_seconds="<<original<<" manifold_seconds="<<reduced<<" speedup="<<original/reduced<<'\n';
    return fast->failedSolves()?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
