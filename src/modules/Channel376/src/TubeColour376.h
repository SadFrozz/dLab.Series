#pragma once
#include "TubeCircuit376.h"
#include "LoadedEq376.h"
#include "TubeModels376.h"
#include "DynamicTube376.h"
#include "SaturationTube376.h"
#include <stdexcept>
namespace channel376 {
// Historical static prototype, retained for research comparisons only.
// The active ColourTubePath376 below uses DynamicTube376 instead.
// Tube coloration prototype: EH/GE transfer at the original bias/load,
// folded fixed trim gain and two coupling poles. No per-sample valve Newton
// solve or simulation of U32A/U32B. The EQ remains the original loaded model.
class TubeColour376 {
public:
    struct Point {double value=0,slope=0;};
    static constexpr int intervals=4096;
    // Expanded virtual excitation domain for the audible color profiles.
    static constexpr double minimum=-128,maximum=128,step=(maximum-minimum)/intervals;
    void prepare(double fs,double trimOhms){
        if(!ready||trimOhms!=lastTrim){
            alternative=false;modelDcPlate=TubeCircuit376::dcPlate;build(trimOhms);
            const auto original=table;const double originalTau=outputTau,originalGain=table[intervals/2].slope;
            alternative=true;modelDcPlate=55.57068864681753;build(trimOhms);
            const double normalize=originalGain/table[intervals/2].slope;
            for(auto& point:table){point.value*=normalize;point.slope*=normalize;}
            alternativeTable=table;table=original;outputTau=originalTau;alternative=false;modelDcPlate=TubeCircuit376::dcPlate;
            lastTrim=trimOhms;ready=true;
        }
        fadeSamples=std::max(1,int(fs*.02));resetModel(0);
        inputPole=fs*.000022*30000/(1+fs*.000022*30000);
        outputPole=fs*outputTau/(1+fs*outputTau);
        previousInput=inputHighPass=previousColour=outputHighPass=0;
    }
    void resetModel(int index)noexcept{selected=std::clamp(index,0,2);weights={};weights[selected]=1;remaining=0;}
    void selectModel(int index)noexcept{
        index=std::clamp(index,0,2);if(index==selected)return;selected=index;remaining=fadeSamples;
        for(int j=0;j<3;++j)increments[j]=((j==selected?1.:0.)-weights[j])/remaining;
    }
    double curve(double input)const noexcept{return lookup(table,input);}
    double curveForModel(double input,int index)const noexcept{
        // User-requested audible base coloration at DRIVE0. This excitation
        // calibration is an artistic color model, not a physical376 voltage gain.
        const double excitation=index==1?12.:(index==2?2.:4.);
        const double linear=input*smallSignalGain();
        const double nonlinear=(index==1?lookup(alternativeTable,input*excitation):curve(input*excitation))/excitation;
        // Retain half the linear path: coloration without flattening voice peaks.
        // Both components share the same coupling filters and small-signal gain.
        return linear+.5*(nonlinear-linear);
    }
    double selectedCurve(double input)noexcept{
        if(remaining==0)return curveForModel(input,selected);
        for(int j=0;j<3;++j)weights[j]+=increments[j];
        if(--remaining==0){weights={};weights[selected]=1;}
        double output=0;for(int j=0;j<3;++j)if(weights[j]>0)output+=weights[j]*curveForModel(input,j);return output;
    }
    static double lookup(const std::array<Point,intervals+1>& data,double input)noexcept{return lookupDomain(data,input,minimum,step);}
    static double lookupDomain(const std::array<Point,intervals+1>& data,double input,double lower,double spacing)noexcept{
        const double position=std::clamp((input-lower)/spacing,0.,double(intervals));
        const int index=std::min(intervals-1,int(position));const double u=position-index;
        const auto& a=data[index];const auto& b=data[index+1];
        const double aa=2*a.value-2*b.value+spacing*(a.slope+b.slope),bb=-3*a.value+3*b.value-spacing*(2*a.slope+b.slope);
        return ((aa*u+bb)*u+spacing*a.slope)*u+a.value;
    }
    double tick(double input)noexcept{
        inputHighPass=inputPole*(inputHighPass+input-previousInput);previousInput=input;
        const double coloured=selectedCurve(inputHighPass);
        outputHighPass=outputPole*(outputHighPass+coloured-previousColour);previousColour=coloured;
        return outputHighPass;
    }
    double smallSignalGain()const noexcept{return table[intervals/2].slope;}
    double outputCornerHz()const noexcept{return 1/(6.283185307179586*outputTau);}
private:
    TubeCircuit376::Point plateLaw(double plate,double grid)const noexcept{
        if(!alternative)return TubeCircuit376::plateLaw(plate,grid);
        // Historic Tung-Sol family fit already calculated in Tube376 baseline report.
        constexpr double mu=22.12791799910628,kg=1225.9052298660777,kp=68.28017863331158,ex=1.3,kvb=300;
        if(plate<=0)return {};
        const double root=std::sqrt(kvb+plate*plate),a=kp*(1/mu+grid/root);
        const double exponential=std::exp(a>40?-a:a),soft=a>40?a:std::log1p(exponential);
        const double sigmoid=a>40?1/(1+exponential):exponential/(1+exponential);
        const double e=plate/kp*soft;if(e<=0)return {};
        const double value=2*std::pow(e,ex)/kg,d=ex*value/e;
        return {value,d*(soft/kp-plate*plate*grid*sigmoid/(root*root*root)),d*plate*sigmoid/root};
    }
    void build(double trimOhms){
        // Static sch3 U32A/U32B network. Derive the constant level mapping
        // and the input load; this matrix is solved only during preparation.
        Matrix376<7> m{};auto resistor=[&](int a,int b,double r){LinearNetwork376<7>::stamp(m,a,b,1/r);};
        resistor(0,-1,121000);resistor(0,1,121000);resistor(1,2,47000+trimOhms);
        resistor(2,3,100);resistor(3,4,20);resistor(4,5,20000);resistor(5,6,20000);
        constexpr double a=316227.7660168379;
        m[2]={};m[2][2]=1;m[2][1]=a;m[6]={};m[6][6]=1;m[6][5]=a;
        m[0]={};m[0][0]=1;Vector376<7> source{},post{};source[0]=1;
        if(!solve376<7>(m,source,post))throw std::runtime_error("tube colour fixed trim mapping");
        postGain=post[6];loadResistance=121000/(2-post[1]);
        auto x=alternative?Vector376<4>{0,2.610168277467153,55.57068864681753,.0006525420693667882}:Vector376<4>{0,TubeCircuit376::dcCathode,TubeCircuit376::dcPlate,TubeCircuit376::dcCurrent};
        table[intervals/2]=solvePoint(0,x);biasPlate=x[2];table[intervals/2].value=0;
        const auto bias=x;
        const double buildMinimum=alternative?-128.:minimum,buildStep=alternative?256./intervals:step;
        for(int j=intervals/2+1;j<=intervals;++j)table[j]=solvePoint(buildMinimum+j*buildStep,x);
        x=bias;for(int j=intervals/2-1;j>=0;--j)table[j]=solvePoint(buildMinimum+j*buildStep,x);
        const auto ip=plateLaw(bias[2]-49.9*bias[3]-bias[1],bias[0]-bias[1]);
        const double plateResistance=1/(1/(332000./3)+2*ip.plateDerivative/(1+4049.9*ip.plateDerivative+4000*ip.gridDerivative));
        outputTau=.33e-6*(loadResistance+plateResistance);
    }
    Point solvePoint(double input,Vector376<4>& x){
        constexpr double yin=1/20000.;
        auto evaluate=[&](const Vector376<4>& v,Matrix376<4>& jac){
            const auto [g,k,p,i]=v;const double vp=p-49.9*i-k,vg=g-k;
            const auto ip=plateLaw(vp,vg),ig=TubeCircuit376::gridLaw(vp,vg);
            const double t=g+1000*ig.value;
            const Vector376<4> dg{ig.gridDerivative,-ig.gridDerivative-ig.plateDerivative,ig.plateDerivative,-49.9*ig.plateDerivative};
            for(int j=0;j<4;++j){const double dt=(j==0?1.:0)+1000*dg[j];jac[0][j]=(yin+1/10000.)*dt+2*dg[j];jac[1][j]=(j==1?1/2000.:0)-(j==3?2.:0)-2*dg[j];}
            jac[2]={0,0,-1/(332000./3)-1/loadResistance,-2};
            jac[3]={-ip.gridDerivative,ip.gridDerivative+ip.plateDerivative,-ip.plateDerivative,1+49.9*ip.plateDerivative};
            return Vector376<4>{yin*(t-input)+t/10000+2*ig.value,k/2000-2*(i+ig.value),
                (200-p)/(332000./3)-2*i-(p-modelDcPlate)/loadResistance,i-ip.value};};
        auto norm=[](const auto& r){double result=0;for(double v:r)result=std::max(result,std::abs(v));return result;};
        Matrix376<4> jac{};auto r=evaluate(x,jac);
        for(int iteration=0;iteration<24&&norm(r)>1e-14;++iteration){Vector376<4> rhs{},delta{};for(int j=0;j<4;++j)rhs[j]=-r[j];
            if(!solve376<4>(jac,rhs,delta))break;
            double scale=1;bool accepted=false;
            for(int bt=0;bt<16;++bt){auto candidate=x;for(int j=0;j<4;++j)candidate[j]+=scale*delta[j];Matrix376<4> nextJac{};auto next=evaluate(candidate,nextJac);
                if(norm(next)<norm(r)){x=candidate;r=next;jac=nextJac;accepted=true;break;}scale*=.5;}
            if(!accepted)break;
        }
        if(norm(r)>1e-10)throw std::runtime_error("tube colour EH/GE curve convergence");
        Vector376<4> derivative{};if(!solve376<4>(jac,Vector376<4>{yin,0,0,0},derivative))throw std::runtime_error("tube colour derivative");
        return {(x[2]-biasPlate)*postGain,derivative[2]*postGain};
    }
    std::array<Point,intervals+1> table{},alternativeTable{};
    bool alternative=false;double modelDcPlate=TubeCircuit376::dcPlate;
    int selected=0,remaining=0,fadeSamples=960;std::array<double,3> weights{{1,0,0}},increments{};
    bool ready=false;double lastTrim=0,biasPlate=TubeCircuit376::dcPlate,postGain=0,loadResistance=60500,outputTau=0;
    double inputPole=0,outputPole=0,previousInput=0,inputHighPass=0,previousColour=0,outputHighPass=0;
};
class ColourTubePath376 {
public:
    void prepare(double rate,double trimOhms,double hostRate=0){colour.prepare(rate,trimOhms,hostRate);eq.prepare(rate);}
    void selectModel(int model)noexcept{colour.selectModel(model);}
    void resetModel(int model)noexcept{colour.resetModel(model);}
    void setPhysicalEq(double l,double m,double h,double f)noexcept{eq.setPhysical(l,m,h,f);}
    double tick(double input)noexcept{return eq.tick(colour.tick(input));}
    unsigned failedSolves()const noexcept{return colour.failedSolves()+eq.failedSolves();}
private:SaturationTube376 colour;LoadedEq376 eq;
};
}
