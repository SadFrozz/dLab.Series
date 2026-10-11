#include "DynamicTube376.h"
#include <complex>
#include <iostream>
#include <stdexcept>
using namespace channel376;
int main(){try{
 DynamicTube376 tube;tube.prepare(48000,27245.130726458814);
 std::cout<<"model,input_V,fundamental,H2_ratio,H3_ratio\n";
 std::array<double,3> h2{},h3{},fundamental{};
 for(int m=0;m<3;++m)for(double amplitude:{.01,3.,12.}){
  tube.resetModel(m);std::array<std::complex<double>,3> h{};
  for(int n=0;n<24000;++n){double phase=6.283185307179586*1000*n/48000,y=tube.tick(amplitude*std::sin(phase));
   if(!std::isfinite(y))throw std::runtime_error("nonfinite tube output");
   if(n>=12000)for(int j=0;j<3;++j)h[j]+=y*std::complex<double>(std::cos((j+1)*phase),std::sin((j+1)*phase));}
  std::cout<<m<<','<<amplitude<<','<<std::abs(h[0])*2/12000<<','<<std::abs(h[1]/h[0])<<','<<std::abs(h[2]/h[0])<<'\n';
  if(amplitude==.01)fundamental[m]=std::abs(h[0]);
  if(amplitude==3){h2[m]=std::abs(h[1]/h[0]);h3[m]=std::abs(h[2]/h[0]);}
 }
 for(int m=1;m<3;++m){if(std::abs(fundamental[m]/fundamental[0]-1)>.001)throw std::runtime_error("small signal normalization");
  if(std::abs(h2[m]-h2[0])+std::abs(h3[m]-h3[0])<1e-6)throw std::runtime_error("manufacturer curves identical");}
 tube.resetModel(0);for(int n=0;n<10000;++n){if(n%73==0)tube.selectModel(n%3);if(std::abs(tube.tick(0))>1e-10)throw std::runtime_error("silence DC during selection");}
 tube.resetModel(0);double previous=0,maxStep=0;
 for(int n=0;n<24000;++n){const double y=tube.tick(3);if(n>20000){if(n%137==0)tube.selectModel((n/137)%3);maxStep=std::max(maxStep,std::abs(y-previous));}previous=y;}
 if(maxStep>.02||tube.failedSolves())throw std::runtime_error("interrupted model transition");
 std::cout<<"PASS three manufacturer fits, gain normalization, silence and interrupted20ms transitions; max step "<<maxStep<<" V\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
