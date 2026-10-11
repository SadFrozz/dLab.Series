#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include "ControlReference376.h"

namespace channel376 {
// Sch5: C37, C51/C52, D8, Q1, R29/R32, D2 and U5B solved together.
// Nominal Keysight ADS 2N4125 Is/Bf; Br=1 is the stated reverse-gain
// assumption. No capacitor reset or prescribed attack/release time constants.
class CompressorCircuit376 {
public:
    static constexpr double is=2.61651e-15,bf=101.481617,br=1;
    static constexpr double vt=8.617333262145e-5*298.15;
    static constexpr double opampGain=200000,opampSwing=13.5;
    struct State {double z=0,emitter=.62,collector=.3,inverting=0,target=0;};
    void prepare(double rate,bool slow=false,double initialTarget=0)noexcept {
        fs=rate;state={};side1=side2=meter=0;maxResidual=0;failures=0;evaluations=0;lastDrive=initialTarget;diodeGuesses={};
        meterAlpha=-std::expm1(-1/(fs*.016104));
        gc=.22e-6*fs;const double gs=2.2e-6*fs,g1=1/20000.,g2=1/39200.;
        sidePole1=gs/(gs+g1);sidePole2=gs/(gs+g2);sideDrive1=g1/(gs+g1);sideDrive2=g2/(gs+g2);
        sideHistory1=g1*gs/(gs+g1);sideHistory2=g2*gs/(gs+g2);
        passive=gc+g1*(1-g1/(gs+g1))+g2*(1-g2/(gs+g2));
        solve(initialTarget,slow,true);previousNewton=state;lastSlow=slow;side1=side2=state.z;meter=output();
        steadyTarget=initialTarget;steadySlow=slow;isSteady=failures==0;
    }
    double tickTargetVoltage(double target,bool slow)noexcept {
        if(isSteady&&target==steadyTarget&&slow==steadySlow)return meter;
        isSteady=false;
        solve(target,slow,false);
        side1=sidePole1*side1+sideDrive1*state.z;
        side2=sidePole2*side2+sideDrive2*state.z;
        const double cv=output();
        meter+=meterAlpha*(cv-meter);
        return cv;
    }
    double tick(double requestedDb,bool slow)noexcept {
        return tickTargetVoltage(-Blackmer376::compVoltsPerDb*std::clamp(requestedDb,0.,60.),slow);
    }
    double output()const noexcept {
        return std::clamp(opampGain*(state.z-state.inverting),-opampSwing,opampSwing);
    }
    double meterCv()const noexcept{return meter;}
    const State& nodes()const noexcept{return state;}
    double maximumResidualAmps()const noexcept{return maxResidual;}
    unsigned failedSolves()const noexcept{return failures;}
    unsigned long long evaluationCount()const noexcept{return evaluations;}
private:
    using Vector=std::array<double,5>;
    using Matrix=std::array<Vector,5>;
    struct Evaluation {Vector residual{};Matrix jacobian{};};
    Evaluation evaluate(const Vector& x,double target,bool slow,bool dc)const noexcept {
        ++evaluations;
        const auto [z,e,c,n,t]=x;
        const double resistor=slow?1e6:1/(1/1e6+1/200000.);
        const double attack=slow?20000.:1/(1/20000.+1/10000.);
        const double feedback=slow?39200.:1/(1/39200.+1/10000.);
        const double ef=std::exp(std::clamp(e/vt,-600.,40.));
        const double er=std::exp(std::clamp(c/vt,-600.,40.));
        const double forward=is*(ef-1),reverse=is*(er-1);
        const double df=is*ef/vt,dr=is*er/vt;
        const double ie=(1+1/bf)*forward-reverse;
        const double ic=forward-(1+1/br)*reverse;
        const double a=opampGain*(z-n),out=std::clamp(a,-opampSwing,opampSwing);
        const double active=std::abs(a)<opampSwing?opampGain:0;
        const double preRaw=opampGain*(target-t)/18.4;
        const double pre=std::clamp(preRaw,-opampSwing,opampSwing);
        const double preActive=std::abs(preRaw)<opampSwing?opampGain/18.4:0;
        const double d3=Diode376::current(t-pre,0,diodeGuesses[0]);diodeGuesses[0]=d3;
        const double gd3=t>pre?1/Diode376::derivative(d3):0;
        const double d8=Diode376::current(z-t,attack,diodeGuesses[1]);diodeGuesses[1]=d8;
        const double gd8=z>t?1/(Diode376::derivative(d8)+attack):0;
        const double d2=Diode376::current(n-out,feedback,diodeGuesses[2]);diodeGuesses[2]=d2;
        const double gd2=n>out?1/(Diode376::derivative(d2)+feedback):0;
        const double dynamicConductance=dc?0:passive;
        Evaluation r;
        r.residual={dynamicConductance*z-(dc?0:solveHistory)+d8-(c-z)/10000,
            (15-e)/resistor-ie,(c-z)/10000+(c-n)/10000-ic,(c-n)/10000-d2,
            t/18400+d3-d8};
        r.jacobian={Vector{dynamicConductance+gd8+1/10000.,0,-1/10000.,0,-gd8},
            Vector{0,-1/resistor-(1+1/bf)*df,dr,0,0},
            Vector{-1/10000.,-df,2/10000.+(1+1/br)*dr,-1/10000.,0},
            Vector{gd2*active,0,1/10000.,-1/10000.-gd2*(1+active),0},
            Vector{-gd8,0,0,0,1/18400.+gd3*(1+preActive)+gd8}};
        return r;
    }
    static double norm(const Vector& a)noexcept{
        double m=0;for(double v:a){if(!std::isfinite(v))return HUGE_VAL;m=std::max(m,std::abs(v));}return m;
    }
    static bool linear(Matrix a,Vector b,Vector& result)noexcept {
        // Eliminate E, N and T analytically. The sch5 Jacobian has only a
        // coupled Z/C block after these three one-node substitutions.
        const double ee=a[1][1],nn=a[3][3],tt=a[4][4];
        if(std::abs(ee)>1e-25&&std::abs(nn)>1e-25&&std::abs(tt)>1e-25){
            const double zz=a[0][0]-a[0][4]*a[4][0]/tt;
            const double zc=a[0][2];
            const double cz=a[2][0]-a[2][3]*a[3][0]/nn;
            const double cc=a[2][2]-a[2][1]*a[1][2]/ee-a[2][3]*a[3][2]/nn;
            const double rz=b[0]-a[0][4]*b[4]/tt;
            const double rc=b[2]-a[2][1]*b[1]/ee-a[2][3]*b[3]/nn;
            const double determinant=zz*cc-zc*cz;
            if(std::isfinite(determinant)&&std::abs(determinant)>1e-25){
                result[0]=(rz*cc-zc*rc)/determinant;
                result[2]=(zz*rc-cz*rz)/determinant;
                result[1]=(b[1]-a[1][2]*result[2])/ee;
                result[3]=(b[3]-a[3][0]*result[0]-a[3][2]*result[2])/nn;
                result[4]=(b[4]-a[4][0]*result[0])/tt;
                return true;
            }
        }
        for(int k=0;k<5;++k){
            int p=k;for(int n=k+1;n<5;++n)if(std::abs(a[n][k])>std::abs(a[p][k]))p=n;
            if(std::abs(a[p][k])<1e-25)return false;
            std::swap(a[p],a[k]);std::swap(b[p],b[k]);
            for(int n=k+1;n<5;++n){const double f=a[n][k]/a[k][k];
                for(int j=k;j<5;++j)a[n][j]-=f*a[k][j];b[n]-=f*b[k];}
        }
        for(int k=4;k>=0;--k){double v=b[k];for(int j=k+1;j<5;++j)v-=a[k][j]*result[j];result[k]=v/a[k][k];}
        return true;
    }
    void solve(double target,bool slow,bool dc)noexcept {
        solveHistory=gc*state.z+sideHistory1*side1+sideHistory2*side2;
        Vector x{state.z,state.emitter,state.collector,state.inverting,state.target};
        // U5A closed-loop T follows the requested drive while D3 conducts.
        // Predict only the Newton starting point, NEVER capacitor charge.
        // Starting at previous T falsely rails the high-AOL amplifier for
        // every tiny changing RMS target and triggers repeated backtracking.
        if(!dc&&target<0&&lastDrive<0&&state.target<0){
            x[4]+=target-lastDrive;
            if(slow==lastSlow&&std::abs(target-lastDrive)<.05){
                x[0]+=state.z-previousNewton.z;x[1]+=state.emitter-previousNewton.emitter;
                x[2]+=state.collector-previousNewton.collector;x[3]+=state.inverting-previousNewton.inverting;
            }
        }
        double residual=0;
        auto evaluation=evaluate(x,target,slow,dc);
        for(int iteration=0;iteration<32;++iteration){
            auto r=evaluation;residual=norm(r.residual);
            // Runtime convergence stays below the existing 1e-10 A
            // acceptance bound. For finite-AOL opamp differences, demanding
            // 1e-12 A can sit below double cancellation noise and exhaust
            // backtracking every sample. DC setup retains its stricter bound.
            if(residual<(dc?1e-12:5e-11))break;
            Vector delta{};for(double& v:r.residual)v=-v;
            if(!linear(r.jacobian,r.residual,delta))break;
            double step=1.;Vector trial=x;bool accepted=false;
            for(int backtrack=0;backtrack<20;++backtrack){
                for(int j=0;j<5;++j)trial[j]=x[j]+step*delta[j];
                if(trial[1]>-.2&&trial[1]<1&&trial[2]<1){
                    auto candidate=evaluate(trial,target,slow,dc);
                    if(norm(candidate.residual)<residual){evaluation=candidate;accepted=true;break;}
                }
                step*=.5;
            }
            if(!accepted)break;x=trial;
        }
        residual=norm(evaluation.residual);
        maxResidual=std::max(maxResidual,residual);if(residual>=1e-10){++failures;return;}
        previousNewton=state;state={x[0],x[1],x[2],x[3],x[4]};
        lastDrive=target;lastSlow=slow;
    }
    double fs=48000,side1=0,side2=0,meter=0,maxResidual=0,meterAlpha=0;
    unsigned failures=0;State state{};double steadyTarget=0;bool steadySlow=false,isSteady=false;
    mutable unsigned long long evaluations=0;
    double lastDrive=0;
    mutable std::array<double,3> diodeGuesses{};
    State previousNewton{};bool lastSlow=false;
    double gc=0,passive=0,sidePole1=0,sidePole2=0,sideDrive1=0,sideDrive2=0,sideHistory1=0,sideHistory2=0,solveHistory=0;
};
}
