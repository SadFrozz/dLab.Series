#pragma once
#include "DynamicTube376.h"
#include <complex>
namespace channel376 {
// Legacy class/parameter identity retained. User-approved clean tonal profiles,
// not nonlinear manufacturer models. No clipping or added harmonics here.
class SaturationTube376 {
public:
    void prepare(double fs,double trim,double =0){
        gain=nominalGain(fs,trim);sampleRate=fs;
        lowPole=std::exp(-6.283185307179586*300/fs);
        highPole=std::exp(-6.283185307179586*1400/fs);
        fadeSamples=std::max(1,int(fs*.02));
        for(int m=0;m<3;++m){
            // Fixed headroom trim leaves the approved relative band shape intact.
            const double maximum=*std::max_element(bandDb[m].begin(),bandDb[m].end());
            for(int b=0;b<3;++b)tone[m][b]=std::pow(10.,(bandDb[m][b]-maximum)/20);
        }
        resetModel(0);
    }
    void resetModel(int model)noexcept{selected=std::clamp(model,0,2);weights={};weights[selected]=1;remaining=0;low=high=0;failures=0;}
    void selectModel(int model)noexcept{model=std::clamp(model,0,2);if(model==selected)return;
        selected=model;remaining=fadeSamples;for(int j=0;j<3;++j)increments[j]=((j==model?1.:0.)-weights[j])/fadeSamples;}
    double tick(double input)noexcept{
        if(!std::isfinite(input)){input=0;++failures;}
        if(remaining){for(int j=0;j<3;++j)weights[j]+=increments[j];if(--remaining==0){weights={};weights[selected]=1;}}
        low=(1-lowPole)*input+lowPole*low;high=(1-highPole)*input+highPole*high;
        double result=0;
        for(int m=0;m<3;++m)if(weights[m]>0)result+=weights[m]*(tone[m][0]*low+tone[m][1]*(high-low)+tone[m][2]*(input-high));
        return gain*result;
    }
    unsigned failedSolves()const noexcept{return failures;}
    static unsigned tableBuildCount()noexcept{return 0;}
    double smallSignalGain()const noexcept{return gain;}
    double transfer(double x,int model)const noexcept{return x*tone[std::clamp(model,0,2)][0];}
    double tonalMagnitude(double frequency,int model)const noexcept{return std::abs(response(frequency,std::clamp(model,0,2)));}
private:
    static double nominalGain(double fs,double trim){
        // Same nominal linear post gain as DynamicTube376, without constructing
        // unused nonlinear lookup banks. Only an offline small-signal solve.
        Matrix376<7> a{};auto r=[&](int i,int j,double resistance){LinearNetwork376<7>::stamp(a,i,j,1/resistance);};
        r(0,-1,121000);r(0,1,121000);r(1,2,47000+trim);r(2,3,100);r(3,4,20);r(4,5,20000);r(5,6,20000);
        constexpr double opamp=316227.7660168379;a[2]={};a[2][2]=1;a[2][1]=opamp;a[6]={};a[6][6]=1;a[6][5]=opamp;a[0]={};a[0][0]=1;
        Vector376<7> in{},out{};in[0]=1;if(!solve376<7>(a,in,out))throw std::runtime_error("tonal nominal gain");
        const double load=121000/(2-out[1]);DynamicTube376::Map m;
        m.model=0;m.g86=22e-6*fs;m.yin=1/(20000+1/m.g86);
        m.yload=1/(load+1/(.33e-6*fs));m.Y=1/(332000./3)+m.yload;m.C=1000+2/(m.yin+1/10000.);
        const double p0=(200/(332000./3)+m.yload*TubeCircuit376::dcPlate)/m.Y;
        const auto currents=m.solve(0,p0,{TubeCircuit376::dcCurrent,0,0,0});
        return out[6]*(-2/m.Y*currents.plateSlope*m.yin/(m.yin+1/10000.)*m.yload*load);
    }
    std::complex<double> response(double frequency,int model)const noexcept{
        const auto z=std::polar(1.,-6.283185307179586*frequency/sampleRate);
        const auto l=(1-lowPole)/(1.-lowPole*z),h=(1-highPole)/(1.-highPole*z);
        return tone[model][0]*l+tone[model][1]*(h-l)+tone[model][2]*(1.-h);
    }
    inline static constexpr std::array<std::array<double,3>,3> bandDb{{{{4,0,-4}},{{-4,1,6}},{{2,-2,-8}}}};
    std::array<std::array<double,3>,3> tone{};
    std::array<double,3> weights{{1,0,0}},increments{};
    double gain=1,sampleRate=48000,lowPole=0,highPole=0,low=0,high=0;
    int selected=0,remaining=0,fadeSamples=960;unsigned failures=0;
};
}
