#pragma once
#include "CircuitMath376.h"
namespace channel376 {
class EqCircuit376 {
public:
    static double midResistance(double nominalHz)noexcept{
        return 634+50000*(1/std::clamp(nominalHz,100.,8000.)-1/8000.)/(1/100.-1/8000.);
    }
    static std::complex<double> midResponse(double hz,double p,double nominalHz)noexcept{
        const std::complex<double> s{0,6.283185307179586*hz};
        const auto q=s*midResistance(nominalHz)*31.7e-9;
        const double a=2*10000/4220.;const auto weights=midWeights(p);
        const auto d=(1.+q+q*q)*(1.+s*24900.*10e-12);
        return -(d+a*weights[1]*q)/(d+a*weights[0]*q);
    }
    static std::complex<double> shelfResponse(double hz,double low,double high)noexcept{
        using C=std::complex<double>;const C s{0,6.283185307179586*hz};
        Matrix376<7,C> m{};Vector376<7,C> b{},x{};
        const double pl=std::clamp(1-low,1e-6,1-1e-6),ph=std::clamp(1-high,1e-6,1-1e-6);
        const C ga=1/(50000*pl)+s*39e-9,gb=1/(50000*(1-pl))+s*39e-9;
        const double ha=1/(50000*ph),hb=1/(50000*(1-ph));
        m[0][0]=1/10000.+ga;m[0][2]=-ga;b[0]=1/10000.;
        m[1][1]=1/10000.+gb;m[1][2]=-gb;m[1][6]=-1/10000.;
        m[2][2]=ga+gb+1/10000.;m[2][0]=-ga;m[2][1]=-gb;
        m[3][3]=1/3650.+ha;m[3][5]=-ha;b[3]=1/3650.;
        m[4][4]=1/3650.+hb;m[4][5]=-hb;m[4][6]=-1/3650.;
        m[5][5]=ha+hb+s*2.2e-9;m[5][3]=-ha;m[5][4]=-hb;
        m[6][2]=-1/10000.;m[6][5]=-s*2.2e-9;m[6][6]=-s*22e-12;
        return solve376<7>(m,b,x)?x[6]:C{};
    }
    void prepare(double rate)noexcept{
        fs=rate;midState={};midDerivative={};shelf.reset(rate);
        shelf.capacitor(0,2,39e-9);shelf.capacitor(1,2,39e-9);
        // Last row is summing-node KCL, rather than output-node KCL.
        // Capacitor C29 and C26 stamps below are correspondingly asymmetric.
        shelf.count=2;previousHW=previousY=previousIC29=previousIC26=0;
        low=mid=high=-1;frequency=-1;setPhysical(.5,.5,.5,1000);
    }
    void setPhysical(double l,double m,double h,double f)noexcept{
        l=std::clamp(l,1e-6,1-1e-6);m=std::clamp(m,1e-6,1-1e-6);h=std::clamp(h,1e-6,1-1e-6);
        if(m!=mid||f!=frequency){mid=m;frequency=f;compileMid();}
        if(l==low&&h==high)return;low=l;high=h;
        auto& a=shelf.conductance;a={};const double pl=1-l,ph=1-h;
        const double ga=1/(50000*pl),gb=1/(50000*(1-pl)),ha=1/(50000*ph),hb=1/(50000*(1-ph));
        a[0][0]=1/10000.+ga;a[0][2]=-ga;
        a[1][1]=1/10000.+gb;a[1][2]=-gb;a[1][6]=-1/10000.;
        a[2][2]=ga+gb+1/10000.;a[2][0]=-ga;a[2][1]=-gb;
        a[3][3]=1/3650.+ha;a[3][5]=-ha;
        a[4][4]=1/3650.+hb;a[4][5]=-hb;a[4][6]=-1/3650.;
        a[5][5]=ha+hb+2*fs*2.2e-9;a[5][3]=-ha;a[5][4]=-hb;
        a[6][2]=-1/10000.;a[6][5]=-2*fs*2.2e-9;a[6][6]=-2*fs*22e-12;
        shelf.compile();
    }
    // Endpoint preserving inverse of the actual loaded curve. Call outside the
    // sample loop on host target changes; smooth the returned physical wiper.
    static double gainPosition(double db,int band,double nominalMidHz=1000)noexcept{
        if(db==0)return .5;
        const double hz=band==0?20:band==1?1/(6.283185307179586*midResistance(nominalMidHz)*31.7e-9):20000;
        auto value=[&](double p){const auto v=band==1?midResponse(hz,p,nominalMidHz):shelfResponse(hz,band==0?p:.5,band==2?p:.5);return 20*std::log10(std::abs(v));};
        const double base=value(.5),endpoint=value(db<0?0:1),target=base+std::abs(std::clamp(db,-15.,15.))/15*(endpoint-base);
        double lo=0,hi=1;for(int it=0;it<40;++it){const double p=(lo+hi)*.5;if(value(p)<target)lo=p;else hi=p;}return (lo+hi)*.5;
    }
    double tick(double input)noexcept{
        const double w=1/(midResistance(frequency)*31.7e-9),dt=.5/fs,driver=1/(24900.*10e-12);
        const auto weights=midWeights(mid);
        Vector376<4> rhs{midState[0]+dt*midDerivative[0]+dt*w*weights[1]*input,
            midState[1]+dt*midDerivative[1],midState[2]+dt*midDerivative[2],-input};
        Vector376<4> x{};for(int i=0;i<4;++i)for(int j=0;j<4;++j)x[i]+=midInverse[i][j]*rhs[j];
        midState={x[0],x[1],x[2]};midDerivative={w*(weights[0]*x[3]+weights[1]*input-x[0]-x[1]),w*x[0],driver*(x[0]-x[2])};
        const double midOutput=x[3];
        Vector376<7> source{};source[0]=midOutput/10000;source[3]=midOutput/3650;
        const double history29=2*fs*2.2e-9*previousHW+previousIC29;
        const double history26=2*fs*22e-12*previousY+previousIC26;
        source[5]+=history29;source[6]-=history29+history26;
        const auto nodes=shelf.tick(source);
        previousIC29=2*fs*2.2e-9*(nodes[5]-previousHW)-previousIC29;
        previousIC26=2*fs*22e-12*(nodes[6]-previousY)-previousIC26;
        previousHW=nodes[5];previousY=nodes[6];return nodes[6];
    }
private:
    static std::array<double,2> midWeights(double p)noexcept{
        p=std::clamp(p,1e-6,1-1e-6);const double rt=50000*p,rb=50000*(1-p),sum=1/rt+1/rb+1/24900.;
        return {(1/rt)/sum,(1/rb)/sum};
    }
    void compileMid()noexcept{
        const double t=.5/fs,w=1/(midResistance(frequency)*31.7e-9),d=1/(24900.*10e-12);
        const auto weights=midWeights(mid);Matrix376<4> a{};
        a[0]={1+t*w,t*w,0,-t*w*weights[0]};a[1]={-t*w,1,0,0};a[2]={-t*d,0,1+t*d,0};a[3]={0,0,2*10000/4220.,1};
        for(int col=0;col<4;++col){Vector376<4> b{},x{};b[col]=1;solve376<4>(a,b,x);for(int row=0;row<4;++row)midInverse[row][col]=x[row];}
    }
    double fs=192000,low=.5,mid=.5,high=.5,frequency=1000;
    double previousHW=0,previousY=0,previousIC29=0,previousIC26=0;
    std::array<double,3> midState{},midDerivative{};Matrix376<4> midInverse{};
    LinearNetwork376<7> shelf;
};
}
