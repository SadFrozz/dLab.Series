#include "TubeCircuit376.h"
#include "EqCircuit376.h"
#include "DetectorCircuit376.h"
#include "CompressorCircuit376.h"
#include "TubeEqPath376.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <vector>
using namespace channel376;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    constexpr double pi=3.14159265358979323846;
    TubeCircuit376 tube;tube.prepare(48000);
    for(int n=0;n<1000;++n)require(std::abs(tube.tick(0))<1e-10,"tube DC startup");
    require(tube.failedSolves()==0,"tube DC solver");
    if(argc>1){std::ofstream csv(argv[1]);csv<<std::setprecision(17)<<"sample,input_V,plate_output_V,grid_current_A\n";
        for(double amplitude:{.01,6.,12.,18.}){
            tube.prepare(48000);double re=0,im=0;
            for(int n=0;n<14400;++n){const double phase=2*pi*1000*n/48000,input=amplitude*std::sin(phase),v=tube.tick(input);
                require(std::isfinite(v),"tube finite");
                csv<<n<<','<<input<<','<<v<<','<<tube.gridCurrentEach()<<'\n';
                if(n>=12000){re+=v*std::cos(phase);im+=v*std::sin(phase);}}
            std::cout<<"tube amplitude "<<amplitude<<" fundamental "<<2*std::hypot(re,im)/2400<<" residual "<<tube.maximumResidualAmps()<<" failures "<<tube.failedSolves()<<'\n';
            require(tube.failedSolves()==0,"tube transient solver");
            if(amplitude==.01)require(std::abs(2*std::hypot(re,im)/2400/.01/2.434209614275318-.999944)<.002,"tube AC reference");
        }
    }
    // Independent frequency-domain admittance versus real-time state model.
    double maximumError=0;
    for(double fs:{48000.,192000.,1536000.})for(int band=0;band<3;++band)for(double p:{.001,.5,.999})for(double hz:{20.,1000.,12000.}){
        EqCircuit376 eq;eq.prepare(fs);double l=.5,m=.5,h=.5;if(band==0)l=p;if(band==1)m=p;if(band==2)h=p;eq.setPhysical(l,m,h,1000);
        std::complex<double> y{};const int count=int(fs*.2),start=int(fs*.1);
        for(int n=0;n<count;++n){const double phase=2*pi*hz*n/fs,v=eq.tick(std::sin(phase));require(std::isfinite(v),"EQ finite");if(n>=start)y+=v*std::complex<double>(std::sin(phase),std::cos(phase));}
        y*=2./(count-start);
        const double warped=fs/pi*std::tan(pi*hz/fs);
        const auto expected=EqCircuit376::midResponse(warped,m,1000)*EqCircuit376::shelfResponse(warped,l,h);
        const double error=std::abs(y-expected)/std::max(1.,std::abs(expected));maximumError=std::max(maximumError,error);
        if(error>.02)std::cerr<<"EQ mismatch "<<fs<<' '<<band<<' '<<p<<' '<<hz<<" observed "<<y<<" expected "<<expected<<" error "<<error<<'\n';
        require(error<.02,"EQ complex admittance comparison");
    }
    for(int band=0;band<3;++band){require(std::abs(EqCircuit376::gainPosition(0,band)-.5)<1e-12,"EQ center");
        require(EqCircuit376::gainPosition(-15,band)<1e-4,"EQ cut endpoint");require(EqCircuit376::gainPosition(15,band)>.9999,"EQ boost endpoint");}
    for(double feedback:{634000.,20000000.}){
        RmsDetectorCircuit376 rms;rms.prepare(192000,feedback);const double initial=rms.voltage();
        for(int n=0;n<19200;++n)require(std::abs(rms.tick(0)-initial)<1e-10,"RMS self-feedback DC state");
        std::cout<<"RMS idle voltage "<<initial<<'\n';
    }
    for(double hz:{20.,100.,1000.,12000.}){
        BroadbandCurrent376 broad;broad.prepare(192000);std::complex<double> current{};
        for(int n=0;n<38400;++n){const double phase=2*pi*hz*n/192000,v=broad.tick(std::sin(phase));if(n>=19200)current+=v*std::complex<double>(std::sin(phase),std::cos(phase));}
        current*=2./19200;const auto wanted=BroadbandCurrent376::admittance(192000/pi*std::tan(pi*hz/192000));
        require(std::abs(current-wanted)<1e-8,"Broadband capacitor network admittance");
    }
    CompressorDrive376 drive;drive.prepare();
    for(double dbu:{-40.,-30.,-20.,-10.,0.,10.,20.}){drive.setThreshold(dbu);std::cout<<"Threshold "<<dbu<<" dBu pot "<<drive.potVoltage()<<" V, requested "<<drive.requestedPotVoltage()<<" V\n";
        require(std::abs(drive.potVoltage())<=15,"Threshold physical rails");
        const double current=.775*std::pow(10.,dbu/20)*std::abs(BroadbandCurrent376::admittance(1000));
        const double at=RmsDetectorCircuit376::steadyVoltage(current,634000);
        const double target=drive.tick(at,1,false);require(std::abs(target)<.15,"Threshold nominal calibration tolerance");
        require(drive.tick(at+.01,.5,false)<0,"Signed compressor target above threshold");
        require(drive.tick(at-.01,.5,false)>0,"Signed compressor target below threshold");
    }
    for(double rate:{32000.,192000.})for(double frequency:{800.,4000.,10000.}){
        HighFrequency376 hf;hf.prepare(rate);hf.setFrequency(frequency);const double hz=rate==32000?1000:4000;std::complex<double> observed{};
        for(int n=0;n<int(rate*.1);++n){const double ph=2*pi*hz*n/rate,v=hf.tick(std::sin(ph));if(n>=int(rate*.05))observed+=v*std::complex<double>(std::sin(ph),std::cos(ph));}
        observed*=2./int(rate*.05);const std::complex<double> s{0,2*rate*std::tan(pi*hz/rate)};
        const double r=HighFrequency376::resistance(frequency);const auto expected=2.*s*s*4.7e-9*2.2e-9*r*r/(s*s*4.7e-9*2.2e-9*r*r+s*r*4.7e-9+1.);
        require(std::abs(observed-expected)<1e-8,"HF filter physical admittance");
    }
    // Reset reproducibility and chunk invariance of the prepared subpath.
    TubeEqPath376 path;path.prepare(192000);std::vector<double> reference(10000);
    for(int n=0;n<10000;++n)reference[n]=path.tick(std::sin(2*pi*1000*n/192000));
    path.prepare(192000);
    for(int block=0;block<10000;block+=37)for(int n=block;n<std::min(block+37,10000);++n)
        require(path.tick(std::sin(2*pi*1000*n/192000))==reference[n],"Analogue reset/chunk invariance");
    for(int n=0;n<3000;++n){const double p=.5+.49*std::sin(n*.01);path.setPhysicalEq(p,1-p,p,100+7900*p);
        require(std::isfinite(path.tick(10*std::sin(n*.03))),"EQ changing charged wipers");}
    require(path.failedSolves()==0,"Tube/EQ path solver");
    tube.prepare(192000);const auto start=std::chrono::steady_clock::now();
    for(int n=0;n<192000;++n)tube.tick(6*std::sin(2*pi*1000*n/192000));
    std::cout<<"Tube seconds per audio second (one channel, 4x48k): "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"\nEQ maximum normalized complex error: "<<maximumError<<'\n';
    std::cout<<"PASS analog tube/EQ\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
