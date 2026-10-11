#include "DitherShape376.h"
#include <fstream>
#include <iomanip>
#include <iostream>
using namespace channel376;
int main(int argc,char**argv){if(argc!=2)return 1;
    const std::string directory=argv[1];
    std::ofstream fir(directory+"/adapted-shapes.csv");fir<<"rate,shape,index,coefficient\n"<<std::setprecision(17);
    for(double rate:{8000.,16000.,22050.,32000.,44100.,48000.,88200.,96000.,192000.,384000.}) {
        DitherShape376 model;model.prepare(rate,StreamPrecision::float64);
        for(int s=1;s<=2;++s)for(int k=0;k<model.tapCount();++k)
            fir<<rate<<','<<s<<','<<k<<','<<model.table(ShapeMode(s))[k]<<'\n';
    }
    std::ofstream noise(directory+"/noise-statistics.csv");noise<<"mode,mean,variance,min,max\n"<<std::setprecision(17);
    for(auto mode:{DitherMode::tpdf,DitherMode::snr2}) {
        DitherShape376 model;model.prepare(48000,StreamPrecision::float64);
        double sum=0,squares=0,minimum=1,maximum=-1;constexpr int count=262144;
        for(int n=0;n<count;++n){double x=model.unitNoise(mode);sum+=x;squares+=x*x;minimum=std::min(minimum,x);maximum=std::max(maximum,x);}
        noise<<int(mode)<<','<<sum/count<<','<<squares/count-(sum/count)*(sum/count)<<','<<minimum<<','<<maximum<<'\n';
    }
    std::ofstream rounded(directory+"/rounded-noise.csv");rounded<<"precision,shape,index,error_LSB\n"<<std::setprecision(17);
    for(auto p:{StreamPrecision::float32,StreamPrecision::float64})for(int s=0;s<3;++s) {
        DitherShape376 model;model.prepare(48000,p);model.modes(DitherMode::tpdf,ShapeMode(s));
        const double step=localQuantizationStep(.375,p);
        for(int n=0;n<65536;++n){double y=model.process(.375);rounded<<(p==StreamPrecision::float32?32:64)<<','<<s<<','<<n<<','<<(y-.375)/step<<'\n';}
    }
    std::cout<<"Digital FIR/noise/rounded-output evidence exported\n";
}
