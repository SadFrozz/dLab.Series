#include "TubeColour376.h"
#include "LoadedTubePath376.h"
#include <complex>
#include <iostream>
#include <memory>
using namespace channel376;
template<class T> std::array<double,3> spectrum(T& path,double amplitude,double hz,double fs){
    std::array<std::complex<double>,3> h{};const int count=int(fs);
    for(int n=0;n<count;++n){double phase=6.283185307179586*hz*n/fs,y=path.tick(amplitude*std::sin(phase));
        if(!std::isfinite(y))throw std::runtime_error("nonfinite tube colour output");
        if(n>=count/2)for(int j=0;j<3;++j)h[j]+=y*std::complex<double>(std::cos((j+1)*phase),std::sin((j+1)*phase));}
    return {std::abs(h[0])*4/count, std::abs(h[1]/h[0]),std::abs(h[2]/h[0])};
}
int main(){try{
    constexpr double trim=27245.130726458814;
    auto colour=std::make_unique<TubeColour376>();colour->prepare(48000,trim);
    std::cout<<"Small signal gain "<<colour->smallSignalGain()<<" coupling corner Hz "<<colour->outputCornerHz()<<'\n';
    if(!std::isfinite(colour->smallSignalGain())||colour->smallSignalGain()>=0)throw std::runtime_error("tube gain sign");
    std::cout<<"rate,hz,inputV,referenceFundamental,colourFundamental,referenceH2,colourH2,referenceH3,colourH3\n";
    for(double rate:{32000.,48000.,96000.})for(double hz:{100.,1000.,4000.})for(double amplitude:{.03,.3,1.,3.}){
        auto reference=std::make_unique<LoadedTubePath376>();auto fast=std::make_unique<ColourTubePath376>();
        reference->prepare(rate,trim);fast->prepare(rate,trim);reference->setPhysicalEq(.5,.5,.5,1000);fast->setPhysicalEq(.5,.5,.5,1000);
        auto a=spectrum(*reference,amplitude,hz,rate),b=spectrum(*fast,amplitude,hz,rate);
        std::cout<<rate<<','<<hz<<','<<amplitude<<','<<a[0]<<','<<b[0]<<','<<a[1]<<','<<b[1]<<','<<a[2]<<','<<b[2]<<'\n';
        if(reference->failedSolves()||fast->failedSolves())throw std::runtime_error("tube path convergence");
        if(hz==1000&&amplitude<=.3&&std::abs(b[0]/a[0]-1)>.03)throw std::runtime_error("small signal gain drift");
        if(hz==1000&&amplitude==1&&b[1]<1e-5)throw std::runtime_error("tube asymmetry lost");
    }
    std::cout<<"PASS coloration prototype; spectra are evidence, not an equivalence assertion\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
