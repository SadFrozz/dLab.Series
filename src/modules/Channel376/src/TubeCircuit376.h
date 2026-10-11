#pragma once
#include "CircuitMath376.h"
#include "ProcessingProfile376.h"
namespace channel376 {
// Original sch3: both sections of V1 in parallel. EH plate fit and GE grid
// collection law, exactly the declared nominal reference, not tanh waveshaping.
// Backward Euler matches calculate_tube.py's independently solved transient.
class TubeCircuit376 {
public:
    static constexpr double mu=20.459324664729085,kg=1843.191321559652,
        kp=70.40566700695378,ex=1.4185942473368784,kvb=108.89512805304192;
    static constexpr double dcPlate=58.91631146019572,dcCathode=2.549705214574776,
        dcCurrent=.000637426303643694;
    struct Point {double value=0,plateDerivative=0,gridDerivative=0;};
    static Point plateLaw(double plate,double grid)noexcept{
        if(plate<=0)return {};
        const double root=std::sqrt(kvb+plate*plate),a=kp*(1/mu+grid/root);
        const double exponential=std::exp(a>40?-a:a);
        const double soft=a>40?a:std::log1p(exponential);
        const double sigmoid=a>40?1/(1+exponential):exponential/(1+exponential);
        const double e=plate/kp*soft;if(e<=0)return {};
        const double value=2*std::pow(e,ex)/kg,d=ex*value/e;
        return {value,d*(soft/kp-plate*plate*grid*sigmoid/(root*root*root)),d*plate*sigmoid/root};
    }
    static Point gridLaw(double plate,double grid)noexcept{
        if(grid<=0)return {};
        const double positive=std::max(0.,plate),scale=1+positive/10.668016540093367;
        const double value=.0007877667974507753*std::pow(grid,1.5)/std::pow(scale,.4754832397779696);
        return {value,plate>0?-.4754832397779696*value/(10.668016540093367+plate):0,1.5*value/grid};
    }
    void prepare(double rate)noexcept{
        fs=rate;state={0,dcCathode,dcPlate,dcCurrent};q86=0;q87=dcPlate;
        failures=0;maxResidual=0;gridCurrent=0;previousOutput=0;predictorValid=false;
    }
    struct Load {double current=0,derivative=0,voltage=0;};
    double tick(double input)noexcept{
        const double y=1/(60500+1/(.33e-6*fs));
        auto load=[&](double p){const double i=y*(p-q87);return Load{i,y,60500*i};};
        const auto before=failures;const double out=tickLoaded(input,load);if(failures==before)q87=state[2]-out;return out;
    }
    template<class Port>double tickLoaded(double input,Port&& port)noexcept{
        if(!std::isfinite(input)){++failures;input=0;}
        const double gc86=22e-6*fs,yin=1/(20000+1/gc86);
        auto evaluate=[&](const Vector376<4>& x,Matrix376<4>* jac){
            const auto [g,k,p,i]=x;const double vp=p-49.9*i-k,vg=g-k;
            const auto ip=plateLaw(vp,vg),ig=gridLaw(vp,vg);
            const double t=g+1000*ig.value;const auto load=port(p);
            Vector376<4> r{yin*(t-input+q86)+t/10000+2*ig.value,
                k/2000-2*(i+ig.value),(200-p)/(332000./3)-2*i-load.current,i-ip.value};
            if(jac){
                const Vector376<4> dg{ig.gridDerivative,-ig.gridDerivative-ig.plateDerivative,ig.plateDerivative,-49.9*ig.plateDerivative};
                for(int j=0;j<4;++j){const double dt=(j==0?1.:0)+1000*dg[j];
                    (*jac)[0][j]=(yin+1/10000.)*dt+2*dg[j];
                    (*jac)[1][j]=(j==1?1/2000.:0)-(j==3?2.:0)-2*dg[j];}
                (*jac)[2]={0,0,-1/(332000./3)-load.derivative,-2};
                (*jac)[3]={-ip.gridDerivative,ip.gridDerivative+ip.plateDerivative,-ip.plateDerivative,1+49.9*ip.plateDerivative};
            }return r;
        };
        auto norm=[](const Vector376<4>& v){double r=0;for(double a:v){if(!std::isfinite(a))return HUGE_VAL;r=std::max(r,std::abs(a));}return r;};
        auto x=state;
        // With the grid below the cathode, the grid current is exactly zero.
        // Eliminate the three linear node equations, retaining the same EH
        // plate law and the loaded post-tube port. Positive grid conduction
        // still uses the full coupled solve below.
        const auto initialLoad=port(state[2]);
        const double plateAdmittance=1/(332000./3)+initialLoad.derivative;
        const double plateSource=200/(332000./3)-initialLoad.current+initialLoad.derivative*state[2];
        const double grid=yin*(input-q86)/(yin+1/10000.);
        double current=state[3];
        if(predictorValid)current+=gridSlope*(grid-lastGrid)+sourceSlope*(plateSource-lastSource);
        if(current<0||!std::isfinite(current))current=state[3];
        for(int iteration=0;iteration<8;++iteration){
#if defined(CHANNEL376_PROFILE_PROCESSING)
            ++processingProfile376.tubeScalarIterations;
#endif
            const double cathode=4000*current,plate=(plateSource-2*current)/plateAdmittance;
            if(grid>cathode||current<0)break;
            const auto law=plateLaw(plate-49.9*current-cathode,grid-cathode);
            const double error=current-law.value;
            x={grid,cathode,plate,current};
            const double slope=1+law.plateDerivative*(2/plateAdmittance+4049.9)+4000*law.gridDerivative;
            if(std::abs(error)<1e-14){
                // Check the actual port: a rail/slew transition can invalidate
                // its local affine load even with a converged scalar root.
                const auto actualLoad=port(plate);
                const double plateResidual=(200-plate)/(332000./3)-2*current-actualLoad.current;
                if(std::isfinite(plateResidual)&&std::abs(plateResidual)<1e-14){
                    state=x;gridCurrent=0;
                    maxResidual=std::max(maxResidual,std::max(std::abs(error),std::abs(plateResidual)));
                    const double currentIn=yin*(input-q86-grid),a=input-q86-currentIn/gc86;
                    q86=input-a;previousOutput=actualLoad.voltage;
                    gridSlope=law.gridDerivative/slope;sourceSlope=law.plateDerivative/(plateAdmittance*slope);
                    lastGrid=grid;lastSource=plateSource;predictorValid=true;
                    return previousOutput;
                }
                break;
            }
            const double next=current-error/slope;
            if(!std::isfinite(next)||next<0)break;
            current=next;
        }
        double residual=0;Matrix376<4> j{};auto r=evaluate(x,&j);
#if defined(CHANNEL376_PROFILE_PROCESSING)
        ++processingProfile376.tubeFullSolves;
#endif
        predictorValid=false;
        for(int it=0;it<16;++it){residual=norm(r);if(residual<1e-14)break;
#if defined(CHANNEL376_PROFILE_PROCESSING)
            ++processingProfile376.tubeFullIterations;
#endif
            for(double& v:r)v=-v;Vector376<4> delta{};if(!solve376<4>(j,r,delta))break;
            bool accepted=false;double scale=1;Vector376<4> candidate{};
            for(int bt=0;bt<16;++bt){for(int k=0;k<4;++k)candidate[k]=x[k]+scale*delta[k];
                Matrix376<4> candidateJ{};auto candidateR=evaluate(candidate,&candidateJ);
                if(norm(candidateR)<residual){j=candidateJ;r=candidateR;accepted=true;break;}scale*=.5;}
            if(!accepted)break;x=candidate;
        }
        residual=norm(r);maxResidual=std::max(maxResidual,residual);
        if(!std::isfinite(residual)||residual>1e-10){++failures;return previousOutput;}
        state=x;const auto [g,k,p,i]=state;gridCurrent=gridLaw(p-49.9*i-k,g-k).value;
        const double t=g+1000*gridCurrent,currentIn=yin*(input-q86-t);
        const double a=input-q86-currentIn/gc86;
        q86=input-a;previousOutput=port(p).voltage;
        return previousOutput;
    }
    double maximumResidualAmps()const noexcept{return maxResidual;}
    unsigned failedSolves()const noexcept{return failures;}
    double gridCurrentEach()const noexcept{return gridCurrent;}
    Vector376<4> operatingNodes()const noexcept{return state;}
private:
    double fs=192000,q86=0,q87=dcPlate,previousOutput=0,maxResidual=0,gridCurrent=0;
    Vector376<4> state{0,dcCathode,dcPlate,dcCurrent};unsigned failures=0;
    double gridSlope=0,sourceSlope=0,lastGrid=0,lastSource=0;bool predictorValid=false;
};
}
