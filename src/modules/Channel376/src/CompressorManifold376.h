#pragma once
#include "CompressorCircuit376.h"
#include "CircuitMath376.h"
#include <stdexcept>
namespace channel376 {
// Research prototype: sch5 memoryless E/C/N manifold + two dynamic Z/T
// equations. C37/C51/C52 and finite U5A/D3/D8 retain their original dt/laws.
// Not selected by the plugin until interpolation and overload qualification.
class CompressorManifold376 {
public:
    struct Curve {double release=0,releaseSlope=0,cv=0,cvSlope=0;};
    static constexpr int intervals=8192;
    static constexpr double low=-15,high=.8,step=(high-low)/intervals;
    void prepare(double rate,bool slow=false,double initialTarget=0){
        fs=rate;alpha=-std::expm1(-1/(fs*.016104));failures=0;evaluations=0;guess3=guess8=0;lastSlow=slow;lastScalar=false;lastScalarHistory=0;
        releaseIndex=cvIndex=-1;
        cachedGc=.22e-6*fs;cachedGs=2.2e-6*fs;inverseSide1=1/(cachedGs+1/20000.);inverseSide2=1/(cachedGs+1/39200.);
        historySide1=(1/20000.)*cachedGs*inverseSide1;historySide2=(1/39200.)*cachedGs*inverseSide2;
        cachedPassive=cachedGc+historySide1+historySide2;
        if(!tablesReady)for(int mode=0;mode<2;++mode){Vector376<3> x{mode?.58:.62,low+.4,low};
            for(int j=0;j<=intervals;++j){if(j){x[1]+=step;x[2]+=step;}
                tables[mode][j]=staticPoint(low+j*step,mode!=0,x);}}
        tablesReady=true;
        CompressorCircuit376 reference;reference.prepare(fs,slow,initialTarget);
        z=side1=side2=reference.nodes().z;t=reference.nodes().target;meter=reference.output();lastTarget=initialTarget;
        steadyTarget=initialTarget;steadySlow=slow;steadyOutput=reference.output();isSteady=reference.failedSolves()==0;
        tFloor=-18400*Diode376::current(13.5,18400);
    }
    Curve lookup(double value,bool slow)const noexcept{
        const double position=std::clamp((value-low)/step,0.,double(intervals));
        const int index=std::min(intervals-1,int(position));const double u=position-index;
        const auto& a=tables[slow][index];const auto& b=tables[slow][index+1];
        auto interpolate=[&](double v0,double d0,double v1,double d1){
            const double aa=2*v0-2*v1+step*(d0+d1),bb=-3*v0+3*v1-step*(2*d0+d1),cc=step*d0;
            return std::array<double,2>{((aa*u+bb)*u+cc)*u+v0,(3*aa*u*u+2*bb*u+cc)/step};};
        const auto release=interpolate(a.release,a.releaseSlope,b.release,b.releaseSlope);
        const auto cv=interpolate(a.cv,a.cvSlope,b.cv,b.cvSlope);
        return {release[0],release[1],std::clamp(cv[0],-13.5,13.5),cv[1]};
    }
    Curve lookupRelease(double value,bool slow)const noexcept{
        const double position=std::clamp((value-low)/step,0.,double(intervals));
        const int index=std::min(intervals-1,int(position));const double u=position-index;
        if(index!=releaseIndex||slow!=releaseSlow){
            const auto& a=tables[slow][index];const auto& b=tables[slow][index+1];
            releasePolynomial={a.release,step*a.releaseSlope,
                -3*a.release+3*b.release-step*(2*a.releaseSlope+b.releaseSlope),
                2*a.release-2*b.release+step*(a.releaseSlope+b.releaseSlope)};
            releaseIndex=index;releaseSlow=slow;
        }
        const auto& p=releasePolynomial;
        return {((p[3]*u+p[2])*u+p[1])*u+p[0],(3*p[3]*u*u+2*p[2]*u+p[1])/step,0,0};
    }
    double lookupCv(double value,bool slow)const noexcept{
        const double position=std::clamp((value-low)/step,0.,double(intervals));
        const int index=std::min(intervals-1,int(position));const double u=position-index;
        if(index!=cvIndex||slow!=cvSlow){const auto& a=tables[slow][index];const auto& b=tables[slow][index+1];
            cvPolynomial={a.cv,step*a.cvSlope,-3*a.cv+3*b.cv-step*(2*a.cvSlope+b.cvSlope),2*a.cv-2*b.cv+step*(a.cvSlope+b.cvSlope)};
            cvIndex=index;cvSlow=slow;
        }
        const auto& p=cvPolynomial;return std::clamp(((p[3]*u+p[2])*u+p[1])*u+p[0],-13.5,13.5);
    }
    double tick(double target,bool slow)noexcept{
        if(isSteady&&target==steadyTarget&&slow==steadySlow)return steadyOutput;
        isSteady=false;
        if(z<0&&z<=target&&releaseStep(target,slow)){lastScalar=false;return commitControl(target,slow);}
        if(target<-.2&&z<-.1&&scalarAttack(target,slow))return commitControl(target,slow);
        lastScalar=false;
        const double gc=.22e-6*fs,gs=2.2e-6*fs,g1=1/20000.,g2=1/39200.;
        const double passive=gc+g1*(1-g1/(gs+g1))+g2*(1-g2/(gs+g2));
        const double attack=slow?20000.:1/(1/20000.+1/10000.);
        auto evaluate=[&](double zz,double tt){
            ++evaluations;
            const auto curve=lookupRelease(zz,slow);
            const double raw=CompressorCircuit376::opampGain*(target-tt)/18.4,pre=std::clamp(raw,-13.5,13.5);
            const double active=std::abs(raw)<13.5?CompressorCircuit376::opampGain/18.4:0;
            const double d3=Diode376::current(tt-pre,0,guess3),d8=Diode376::current(zz-tt,attack,guess8);guess3=d3;guess8=d8;
            const double gd3=tt>pre?1/Diode376::derivative(d3):0,gd8=zz>tt?1/(Diode376::derivative(d8)+attack):0;
            const double s1=(gs*side1+g1*zz)/(gs+g1),s2=(gs*side2+g2*zz)/(gs+g2);
            return std::array<double,6>{gc*(zz-z)+g1*(zz-s1)+g2*(zz-s2)+d8-curve.release,
                tt/18400+d3-d8,passive+gd8-curve.releaseSlope,-gd8,-gd8,1/18400.+gd3*(1+active)+gd8};};
        double zz=z,tt=t;
        if(target<0&&lastTarget<0&&t<0)tt+=target-lastTarget;
        auto r=evaluate(zz,tt);
        auto norm=[](const auto& a){return std::max(std::abs(a[0]),std::abs(a[1]));};
        for(int it=0;it<24&&norm(r)>5e-11;++it){
            const double det=r[2]*r[5]-r[3]*r[4];
            const double dz=(-r[0]*r[5]+r[3]*r[1])/det,dt=(-r[2]*r[1]+r[4]*r[0])/det;
            double scale=1;bool accepted=false;
            for(int bt=0;bt<20;++bt){const double nz=zz+scale*dz,nt=tt+scale*dt;auto next=evaluate(nz,nt);
                if(nz>=low&&nz<=high&&norm(next)<norm(r)){zz=nz;tt=nt;r=next;accepted=true;break;}scale*=.5;}
            if(!accepted)break;
        }
        if(!std::isfinite(norm(r))||norm(r)>=1e-10){++failures;return lookupCv(z,slow);}
        z=zz;t=tt;return commitControl(target,slow);
    }
    unsigned failedSolves()const noexcept{return failures;}
    double tickTargetVoltage(double target,bool slow)noexcept{return tick(target,slow);}
    double output()const noexcept{return isSteady?steadyOutput:lookupCv(z,lastSlow);}
    double meterCv()const noexcept{return meter;}
    unsigned long long evaluationCount()const noexcept{return evaluations;}
private:
    bool releaseStep(double target,bool slow)noexcept{
        // With D8 off, Z's physical capacitor equation does not depend on
        // the threshold driver T. Solve Z alone, then U5A/D3 without an
        // inverse diode function. Accept only when the solved Z <= T.
        const double gc=.22e-6*fs,gs=2.2e-6*fs,g1=1/20000.,g2=1/39200.;
        const double passive=cachedPassive;
        const double history=cachedGc*z+historySide1*side1+historySide2*side2;
        double zz=z;
        for(int iteration=0;iteration<8;++iteration){++evaluations;
            if(zz<low||zz>high)return false;const auto curve=lookupRelease(zz,slow);
            const double r=passive*zz-history-curve.release;
            if(std::abs(r)<2.5e-11)break;
            zz-=r/(passive-curve.releaseSlope);
        }
        if(zz<low||zz>0||std::abs(passive*zz-history-lookupRelease(zz,slow).release)>2.5e-11)return false;
        const double a=CompressorCircuit376::opampGain/18.4;
        double tt=0;
        if(target<0){
            if(target<=tFloor-13.5/a)tt=tFloor;
            else {tt=std::clamp(t+target-lastTarget,tFloor,0.);
                bool converged=false;
                for(int iteration=0;iteration<8;++iteration){++evaluations;
                    const double q=-tt/18400,slope=Diode376::derivative(q);
                    const double r=tt-a*(target-tt)-Diode376::voltage(q);
                    if(std::abs(r)/slope<2.5e-11){converged=true;break;}
                    tt=std::clamp(tt-r/(1+a+slope/18400),tFloor,0.);}
                if(!converged)return false;
            }
        }
        if(zz>tt)return false;
        z=zz;t=tt;guess8=0;guess3=std::max(0.,-tt/18400);return true;
    }
    double commitControl(double target,bool slow)noexcept{
        const double gs=2.2e-6*fs;
        side1=(gs*side1+z/20000)*inverseSide1;side2=(gs*side2+z/39200)*inverseSide2;lastTarget=target;
        lastSlow=slow;const double cv=lookupCv(z,slow);meter+=alpha*(cv-meter);return cv;
    }
    bool scalarAttack(double target,bool slow)noexcept{
        // Both D3 and D8 conduct in this common region. Use D8 current as
        // the unknown, not its repeatedly inverted voltage. Derive Z from
        // C37/side branches and T directly from U5A + D3's forward law.
        const double gc=.22e-6*fs,gs=2.2e-6*fs,g1=1/20000.,g2=1/39200.;
        const double passive=cachedPassive;
        const double history=cachedGc*z+historySide1*side1+historySide2*side2;
        const double attack=slow?20000.:1/(1/20000.+1/10000.);
        const double a=CompressorCircuit376::opampGain/18.4;
        double current=guess8,zz=z,tt=target,q=0,finalError=HUGE_VAL;
        if(!(current>0))return false;
        if(lastScalar&&slow==lastSlow&&lastTarget<0){
            const auto curve=lookupRelease(z,slow);const double slope3=Diode376::derivative(guess3);
            const double dt=slope3/(a+1+slope3/18400);
            const double slope=1/(passive-curve.releaseSlope)+attack+Diode376::derivative(current)+dt;
            current+=((history-lastScalarHistory)/passive-(target-lastTarget))/slope;
        }else if(lastTarget<0)current-= (target-lastTarget)/(1/passive+attack+Diode376::derivative(current));
        for(int iteration=0;iteration<12;++iteration){
            ++evaluations;
            if(!(current>0&&std::isfinite(current)))return false;
            Curve curve{};
            for(int j=0;j<5;++j){if(zz<low||zz>-.01)return false;curve=lookupRelease(zz,slow);
                const double error=passive*zz+current-curve.release-history;
                if(std::abs(error)<1e-13)break;zz-=error/(passive-curve.releaseSlope);}
            for(int j=0;j<5;++j){q=current-tt/18400;if(!(q>0))return false;
                const double next=(a*target+Diode376::voltage(q))/(a+1);
                // After applying this fixed-point update, the remaining
                // junction mismatch comes only from dq=(tt-next)/18400.
                // Far from zero the diode slope ratio is bounded tightly;
                // <1e-12 A leaves ample margin below the original 5e-11 A
                // circuit tolerance. Keep the original voltage rule near 0.
                const double nextQ=current-next/18400;
                if(std::min(q,nextQ)>100*Diode376::saturation&&std::abs(next-tt)/18400<1e-12){tt=next;break;}
                if(std::abs(next-tt)<1e-14){tt=next;break;}tt=next;}
            q=current-tt/18400;const double pre=tt-Diode376::voltage(q);
            if(pre<=-13.5||pre>=13.5)return false;
            finalError=zz-tt-attack*current-Diode376::voltage(current);
            // F is in volts, while the original solver stops on amps.
            // Convert the D8 mismatch to a current bound, leaving half the
            // original 5e-11 A runtime tolerance for the other equations.
            if(std::abs(finalError)/(attack+Diode376::derivative(current))<2.5e-11){
                if(std::abs(passive*zz+current-lookupRelease(zz,slow).release-history)>5e-11)return false;
                z=zz;t=tt;guess8=current;guess3=q;lastScalar=true;lastScalarHistory=history;return true;
            }
            const double slope3=Diode376::derivative(q);
            const double dt=slope3/(a+1+slope3/18400);
            const double dz=-1/(passive-curve.releaseSlope);
            const double next=current-finalError/(dz-dt-attack-Diode376::derivative(current));
            current=next>0?next:current*.5;
        }
        return false;
    }
    static Curve staticPoint(double z,bool slow,Vector376<3>& x){
        const double resistance=slow?1e6:1/(1/1e6+1/200000.),feedback=slow?39200.:1/(1/39200.+1/10000.);
        auto evaluate=[&](const Vector376<3>& v,Matrix376<3>& jac,double* zDerivative=nullptr){
            const auto [e,c,n]=v;
            const double ef=std::exp(std::clamp(e/CompressorCircuit376::vt,-600.,40.)),er=std::exp(std::clamp(c/CompressorCircuit376::vt,-600.,40.));
            const double forward=CompressorCircuit376::is*(ef-1),reverse=CompressorCircuit376::is*(er-1);
            const double df=CompressorCircuit376::is*ef/CompressorCircuit376::vt,dr=CompressorCircuit376::is*er/CompressorCircuit376::vt;
            const double raw=CompressorCircuit376::opampGain*(z-n),out=std::clamp(raw,-13.5,13.5),active=std::abs(raw)<13.5?CompressorCircuit376::opampGain:0;
            const double d2=Diode376::current(n-out,feedback),gd2=n>out?1/(Diode376::derivative(d2)+feedback):0;
            jac={Vector376<3>{-1/resistance-(1+1/CompressorCircuit376::bf)*df,dr,0},
                Vector376<3>{-df,2/10000.+(1+1/CompressorCircuit376::br)*dr,-1/10000.},
                Vector376<3>{0,1/10000.,-1/10000.-gd2*(1+active)}};
            if(zDerivative)*zDerivative=gd2*active;
            return Vector376<3>{(15-e)/resistance-(1+1/CompressorCircuit376::bf)*forward+reverse,
                (c-z)/10000+(c-n)/10000-forward+(1+1/CompressorCircuit376::br)*reverse,(c-n)/10000-d2};};
        auto norm=[](const auto& r){double m=0;for(double value:r)m=std::max(m,std::abs(value));return m;};
        Matrix376<3> jac{};auto r=evaluate(x,jac);
        for(int iteration=0;iteration<40&&norm(r)>1e-12;++iteration){Vector376<3> rhs{},delta{};
            for(int j=0;j<3;++j)rhs[j]=-r[j];if(!solve376<3>(jac,rhs,delta))break;
            double scale=1;bool accepted=false;
            for(int bt=0;bt<24;++bt){auto trial=x;for(int j=0;j<3;++j)trial[j]+=scale*delta[j];Matrix376<3> nextJac{};auto next=evaluate(trial,nextJac);
                if(norm(next)<norm(r)){x=trial;r=next;jac=nextJac;accepted=true;break;}scale*=.5;}
            if(!accepted)break;}
        if(norm(r)>1e-10)throw std::runtime_error("sch5 static manifold did not converge");
        double zd=0;evaluate(x,jac,&zd);Vector376<3> derivative{};
        if(!solve376<3>(jac,Vector376<3>{0,1/10000.,-zd},derivative))throw std::runtime_error("sch5 manifold derivative");
        const double raw=CompressorCircuit376::opampGain*(z-x[2]),active=std::abs(raw)<13.5?CompressorCircuit376::opampGain:0;
        return {(x[1]-z)/10000,(derivative[1]-1)/10000,std::clamp(raw,-13.5,13.5),active*(1-derivative[2])};
    }
    std::array<std::array<Curve,intervals+1>,2> tables{};
    double fs=48000,z=0,t=0,side1=0,side2=0,meter=0,alpha=0,lastTarget=0,guess3=0,guess8=0;unsigned failures=0;
    unsigned long long evaluations=0;bool lastSlow=false;
    bool lastScalar=false,tablesReady=false;double lastScalarHistory=0;
    bool isSteady=false,steadySlow=false;double steadyOutput=0,steadyTarget=0,tFloor=0;
    double cachedGc=0,cachedGs=0,inverseSide1=0,inverseSide2=0,historySide1=0,historySide2=0,cachedPassive=0;
    mutable int releaseIndex=-1;mutable bool releaseSlow=false;mutable std::array<double,4> releasePolynomial{};
    mutable int cvIndex=-1;mutable bool cvSlow=false;mutable std::array<double,4> cvPolynomial{};
};
}
