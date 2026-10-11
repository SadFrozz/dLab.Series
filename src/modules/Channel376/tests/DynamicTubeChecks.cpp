#include "DynamicTube376.h"
#include <iostream>
#include <chrono>
using namespace channel376;
int main(){try{
 std::cout<<"rate,case,reference_rms,error_rms_relative,max_error_V\n";
 for(double fs:{32000.,48000.,96000.}){
  DynamicTube376 fast;fast.prepare(fs,27245.130726458814);
  for(int mode=0;mode<4;++mode){fast.resetModel(0);TubeCircuit376 ref;ref.prepare(fs);
   double power=0,error=0,maxError=0,maxGrid=0;const int count=int(fs*.3);
   for(int n=0;n<count;++n){double x=mode==0?.01*std::sin(n*6.283185307179586*1000/fs):
     mode==1?3*std::sin(n*6.283185307179586*1000/fs):
     mode==2?(n<count/3?30*std::sin(n*6.283185307179586*75/fs):0):
     5*std::sin(n*6.283185307179586*73/fs)+5*std::sin(n*6.283185307179586*997/fs);
    double a=ref.tick(x)*fast.postScale(),b=fast.tick(x),d=b-a;power+=a*a;error+=d*d;maxError=std::max(maxError,std::abs(d));
    maxGrid=std::max(maxGrid,ref.gridCurrentEach());
   }
   double relative=std::sqrt(error/std::max(power,1e-30));
   std::cout<<fs<<','<<mode<<','<<std::sqrt(power/count)<<','<<relative<<','<<maxError<<'\n';
   if(relative>.00002||ref.failedSolves()||fast.failedSolves())throw std::runtime_error("dynamic tube reference mismatch");
   if(mode==2&&maxGrid<1e-7)throw std::runtime_error("overload test failed to exercise grid conduction");
  }
 }
 DynamicTube376 fast;fast.prepare(48000,27245.130726458814);
 double nodeResidual=0;
 for(int model=0;model<3;++model){const auto& map=fast.modelMap(model);
  for(double g0:{-15.,-3.4,-.37,1.234,4.135,7.11,15.})for(double p0:{20.,53.125,108.531,161.7,240.}){
   const auto c=map.lookup(g0,p0);const double vp=p0-(2/map.Y+4049.9)*c.plate-4000*c.grid,vg=g0-4000*c.plate-(map.C+4000)*c.grid;
   const double residual=std::max(std::abs(c.plate-map.law(vp,vg).value),std::abs(c.grid-TubeCircuit376::gridLaw(vp,vg).value));
   if(residual>nodeResidual){nodeResidual=residual;std::cout<<"Nodal peak "<<model<<','<<g0<<','<<p0<<','<<residual<<'\n';}
  }}
 // 0.03uA is far below the resolution of the manufacturer curve fit.
 if(nodeResidual>3e-8)throw std::runtime_error("interpolated nodal current residual");
 std::cout<<"Maximum nodal current residual A "<<nodeResidual<<'\n';
 for(int n=0;n<10000;++n){if(n%71==0)fast.selectModel(n%3);if(std::abs(fast.tick(0))>1e-5)throw std::runtime_error("DC during model switch");}
 const auto start=std::chrono::steady_clock::now();double sum=0;
 for(int n=0;n<480000;++n)sum+=fast.tick(3*std::sin(n*.131));
 std::cout<<"PASS dynamic states vs full solver; tube-only speed "<<10/std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"x checksum "<<sum<<'\n';
 return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
