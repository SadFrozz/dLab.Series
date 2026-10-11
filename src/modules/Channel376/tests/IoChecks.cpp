#include "InputCircuit376.h"
#include "OutputCircuit376.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace channel376;
void require(bool ok,const char* s){if(!ok)throw std::runtime_error(s);}
int main(int argc,char** argv){try{
    constexpr double fs=192000,pi=3.14159265358979323846;
    LineInput376 line;line.prepare(fs);LineSource376 source;source.prepare(fs);
    DriveCircuit376 drive;drive.prepare(fs);LowCutCircuit376 cut;cut.prepare(fs);OutputCircuit376 output;output.prepare(fs);
    double lineDc=0,sourceDc=0,outputDc=0;
    for(int n=0;n<30000;++n){lineDc=line.tick(1);sourceDc=source.tick(1);outputDc=output.tick(1);}
    require(std::abs(lineDc-1)<1e-3,"LINE sign/unity");require(sourceDc>0&&sourceDc<.07,"LINE source charging C40");
    require(outputDc>.9&&outputDc<1.1,"U31 signed differential gain");
    line.prepare(fs);for(int n=0;n<10000;++n)require(std::abs(line.tick(0,1))<1e-3,"LINE common-mode rejection");
    std::cout<<"LINE DC "<<lineDc<<" U31 DC "<<outputDc<<" Drive center gain "<<drive.centerGain()<<'\n';
    std::ofstream csv;if(argc>1){csv.open(argv[1]);csv<<std::setprecision(17)<<"Hz,line_real,line_imag,source_real,source_imag,drive_real,drive_imag,cut_real,cut_imag,output_real,output_imag\n";}
    for(double hz:{20.,100.,1000.,12000.,20000.}){
        line.prepare(fs);source.prepare(fs);drive.prepare(fs);cut.prepare(fs);output.prepare(fs);
        std::array<std::complex<double>,5> sum{};
        for(int n=0;n<595200;++n){const double ph=2*pi*hz*n/fs,x=.001*std::sin(ph);
            const std::array<double,5> y{line.tick(x),source.tick(x),drive.tick(x),cut.tick(x),output.tick(x)};
            if(n>=576000)for(int j=0;j<5;++j)sum[j]+=y[j]*std::complex<double>(std::sin(ph),std::cos(ph));}
        if(csv)csv<<hz;for(auto& v:sum){v*=2/(19200*.001);if(csv)csv<<','<<v.real()<<','<<v.imag();}if(csv)csv<<'\n';
        if(hz==1000){require(std::abs(std::abs(sum[2])-drive.centerGain())<.002,"Drive noninverting gain");require(std::abs(sum[3])>.99,"LOWCUT passband");}
        if(hz==20)require(std::abs(sum[3])<.1,"LOWCUT attenuation");
    }
    // Feedback-aware overload, recovery and charged pot/lowcut operation.
    line.prepare(fs);drive.prepare(fs);cut.prepare(fs);output.prepare(fs);
    for(int n=0;n<20000;++n){const double x=100*std::sin(n*.03);drive.setGain(-15+30*(n%1000)/999.);
        require(std::abs(line.tick(x))<=13.500001,"LINE rails");require(std::abs(drive.tick(x))<=13.500001,"Drive rails");
        require(std::abs(cut.tick(x))<=13.500001,"LOWCUT rails");require(std::isfinite(output.tick(x)),"U31 overloaded finite");
        require(std::abs(output.positiveLeg())<14&&std::abs(output.negativeLeg())<14,"U31 physical legs");}
    std::cout<<"Failed solves "<<line.failedSolves()<<' '<<drive.failedSolves()<<' '<<cut.failedSolves()<<' '<<output.failedSolves()<<'\n';
    require(line.failedSolves()+drive.failedSolves()+cut.failedSolves()+output.failedSolves()==0,"IO solver convergence");
    for(int n=0;n<192000;++n){line.tick(0);drive.tick(0);cut.tick(0);output.tick(0);}
    require(std::abs(output.tick(0))<1e-6,"U31 recovery");
    std::cout<<"PASS IO topology/rails/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
