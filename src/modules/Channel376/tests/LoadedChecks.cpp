#include "LoadedTubePath376.h"
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <iomanip>
using namespace channel376;
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(int argc,char** argv){try{
    LoadedTubePath376 tube;LoadedEq376 eq;LoadedHighFrequency376 hf;
    const double fs=192000,pi=3.14159265358979323846;
    tube.prepare(fs);eq.prepare(fs);hf.prepare(fs);
    for(int n=0;n<1000;++n){require(std::abs(tube.tick(0))<1e-9,"coupled DC silence");require(hf.tick(0)==0,"HF silence");}
    std::ofstream csv;if(argc>1){csv.open(argv[1]);csv<<std::setprecision(17)<<"Hz,eq_real,eq_imag,hf_real,hf_imag,tube_real,tube_imag\n";}
    for(double hz:{20.,100.,1000.,12000.,20000.}){
        tube.prepare(fs);eq.prepare(fs);hf.prepare(fs);std::array<std::complex<double>,3> sum{};
        for(int n=0;n<230400;++n){const double ph=2*pi*hz*n/fs,x=.001*std::sin(ph);const std::array<double,3> v{eq.tick(x),hf.tick(x),tube.tick(x)};
            if(n>=211200)for(int j=0;j<3;++j)sum[j]+=v[j]*std::complex<double>(std::sin(ph),std::cos(ph));}
        if(csv)csv<<hz;for(auto& v:sum){v*=2/(19200*.001);if(csv)csv<<','<<v.real()<<','<<v.imag();}if(csv)csv<<'\n';
        std::cout<<"Loaded "<<hz<<"Hz EQ "<<sum[0]<<" HF "<<sum[1]<<" Tube "<<sum[2]<<'\n';}
    require(tube.failedSolves()+eq.failedSolves()+hf.failedSolves()==0,"loaded small-signal convergence");
    tube.prepare(fs);eq.prepare(fs);hf.prepare(fs);
    for(int n=0;n<20000;++n){const double x=40*std::sin(n*.021);const double p=.5+.499999*std::sin(n*.003);
        if(n%100==0){eq.setPhysical(p,1-p,p,100+7900*p);tube.setPhysicalEq(p,1-p,p,100+7900*p);hf.setResistance(4750+50000*p);}
        require(std::isfinite(tube.tick(x)),"coupled overload finite");require(std::abs(eq.tick(x))<13.50001,"shelf rail");require(std::abs(hf.tick(x))<13.50001,"HF rail");
        for(auto v:tube.opampOutputs())require(std::abs(v)<=13.50001,"physical opamp rails before VCA");}
    std::cout<<"Failures tube/EQ/HF "<<tube.failedSolves()<<' '<<eq.failedSolves()<<' '<<hf.failedSolves()<<'\n';
    require(tube.failedSolves()+eq.failedSolves()+hf.failedSolves()==0,"loaded overload solver");
    for(int n=0;n<192000;++n){tube.tick(0);eq.tick(0);hf.tick(0);}
    require(std::abs(tube.tick(0))<.02&&std::abs(eq.tick(0))<.02&&std::abs(hf.tick(0))<.02,"charged overload recovery");
    std::cout<<"PASS loaded tube/trim/return/EQ/HF\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
