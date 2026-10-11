#include "LoadedEq376.h"
#include <memory>
#include <chrono>
#include <iostream>
#include <vector>
using namespace channel376;
volatile double eqSink=0;
int main(){
    for(double fs:{8000.,32000.,48000.,192000.,1536000.}){
        auto ref=std::make_unique<LoadedEqProcessor376<false>>();auto fast=std::make_unique<LoadedEqProcessor376<true>>();ref->prepare(fs);fast->prepare(fs);
        double maximum=0;for(int n=0;n<64000;++n){
            if(n%4096==0){const double l=.1+.8*((n/4096)%3)/2.,m=.2+.6*((n/4096)%4)/3.,h=1-l,f=100+7900*((n/4096)%5)/4.;ref->setPhysical(l,m,h,f);fast->setPhysical(l,m,h,f);}
            const double x=n<16000?.3*std::sin(n*.131):n<32000?100*std::sin(n*.47):n<48000?.3*std::sin(n*.031):0;
            const double a=ref->tick(x),b=fast->tick(x);if(!std::isfinite(b))return 1;maximum=std::max(maximum,std::abs(a-b));
        }
        std::cout<<"rate="<<fs<<" maximum_error_V="<<maximum<<" failures="<<ref->failedSolves()<<','<<fast->failedSolves()<<'\n';
        if(maximum>1e-6||ref->failedSolves()||fast->failedSolves())return 1;
    }
    std::vector<double> pcm(192000);for(int n=0;n<int(pcm.size());++n)pcm[n]=.3*std::sin(n*.131);
    auto ref=std::make_unique<LoadedEqProcessor376<false>>();auto fast=std::make_unique<LoadedEqProcessor376<true>>();ref->prepare(48000);fast->prepare(48000);
    auto start=std::chrono::steady_clock::now();double sum=0;for(double x:pcm)sum+=ref->tick(x);eqSink=sum;
    const double original=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    start=std::chrono::steady_clock::now();sum=0;for(double x:pcm)sum+=fast->tick(x);eqSink=sum;
    const double reduced=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"original_seconds="<<original<<" reduced_seconds="<<reduced<<" speedup="<<original/reduced<<'\n';
}
