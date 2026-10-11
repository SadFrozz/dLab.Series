#include "SaturationTube376.h"
#include <complex>
#include <iostream>
#include <stdexcept>
using namespace channel376;
int main(){try{
 SaturationTube376 tube;tube.prepare(48000,27245.130726458814);
 DynamicTube376 reference;reference.prepare(48000,27245.130726458814);
 if(std::abs(tube.smallSignalGain()/reference.smallSignalGain()-1)>1e-10)throw std::runtime_error("nominal circuit gain changed");
 for(int m=0;m<3;++m)for(double f:{100.,1000.,8000.}){
  tube.resetModel(m);std::array<std::complex<double>,3> h{};
  for(int n=0;n<48000;++n){const double ph=6.283185307179586*f*n/48000,y=tube.tick(.2*std::sin(ph));
   if(n>=24000)for(int k=0;k<3;++k)h[k]+=y*std::complex<double>(std::cos((k+1)*ph),std::sin((k+1)*ph));}
  const double measured=std::abs(h[0])*2/24000/.2/std::abs(tube.smallSignalGain());
  if(std::abs(20*std::log10(measured/tube.tonalMagnitude(f,m)))>.001)throw std::runtime_error("profile frequency response mismatch");
  if(std::hypot(std::abs(h[1]/h[0]),std::abs(h[2]/h[0]))>1e-10)throw std::runtime_error("tone profile introduces harmonic distortion");
 }
 // Regress the rejected distortion: exact amplitude homogeneity is intentional
 // for the approved clean EQ, including inputs far beyond PCM full scale.
 for(int m=0;m<3;++m){std::array<double,48000> quiet{};tube.resetModel(m);
  for(int n=0;n<48000;++n)quiet[n]=tube.tick(.03*std::sin(n*.131));
  tube.resetModel(m);for(int n=0;n<48000;++n){const double y=tube.tick(30*std::sin(n*.131));
   if(std::abs(y/1000-quiet[n])>1e-12)throw std::runtime_error("tonal branch clips or saturates");}
 }
 const double bright=tube.tonalMagnitude(8000,1)/tube.tonalMagnitude(100,1),warm=tube.tonalMagnitude(8000,0)/tube.tonalMagnitude(100,0),muted=tube.tonalMagnitude(8000,2)/tube.tonalMagnitude(100,2);
 if(!(bright>1&&warm<1&&muted<warm))throw std::runtime_error("wrong approved character order");
 for(int m=0;m<3;++m)for(int n=0;n<100;++n)if(tube.tonalMagnitude(20*std::pow(1000.,n/99.),m)>1.000001)throw std::runtime_error("headroom trim failed");
 tube.resetModel(0);for(int n=0;n<12000;++n){if(n%137==0)tube.selectModel(n%3);if(std::abs(tube.tick(0))>1e-10)throw std::runtime_error("silence switch");}
 const auto builds=DynamicTube376::mapBuildCount();tube.prepare(96000,27245.130726458814,48000);
 if(DynamicTube376::mapBuildCount()!=builds||tube.tableBuildCount()!=0)throw std::runtime_error("unused nonlinear lookup construction");
 std::cout<<"PASS approved tonal response, unchanged nominal gain, no added harmonics/clipping, profile order, headroom, silence switching, no curve banks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
