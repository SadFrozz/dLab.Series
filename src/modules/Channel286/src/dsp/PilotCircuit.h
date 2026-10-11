#pragma once
#include "ControlDevices.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <memory>

namespace c286 {
// US5444788 pilot topology, component values from 056163A / 050257D.
// Reduced active-device model: nominal VBE=.7, infinite beta, ideal LM339;
// storage capacitors and nonlinear control network retain independent states.
class PilotCircuit {
public:
    struct Result { double makeupDb, reductionDb; };
    void prepare(double rate,bool s) {
        modern=s; scale=s?.0062:.0059; dt=1/rate;
        tables=std::make_unique<Tables>();
        auto& driveMain=tables->driveMain; auto& drivePilot=tables->drivePilot;
        auto& phi=tables->phi; auto& schottky=tables->schottky; auto& bridge=tables->bridge;
        cap=s?2.e-6:2.2e-6; rm1=s?22100.:20000.; rm2=s?44200.:39200.;
        leak=s?20.e6:22.e6; postCap=s?1.e-6:1.5e-6;
        a1=cap/(cap+dt/rm1); a2=cap/(cap+dt/rm2);
        g1=cap/dt*(1-a1); g2=cap/dt*(1-a2);
        af=.22e-6/dt+g1+g2+1/leak; ar=.047e-6/dt;
        for(size_t i=0;i<driveMain.size();++i) {
            const double p=static_cast<double>(i)/(driveMain.size()-1);
            double w=15*p;
            if(i && i+1<driveMain.size()) {
                double lo=0,hi=15;
                for(int k=0;k<45;++k) {
                    w=(lo+hi)*.5;
                    const double diode=loadedDiode(w*31600/311600.,1/(1/280000.+1/31600.),1);
                    const double node=(w/280000.-diode)/(1/280000.+1/31600.);
                    const double residual=(w-15)*(1/(s?13300.:12700.)+1/(50000*(1-p)))
                        +w/(50000*p)+w/100000+(w-node)/280000;
                    if(residual>0) hi=w; else lo=w;
                }
            }
            drivePilot[i]=2490*w/100000/scale;
            driveMain[i]=4990*loadedDiode(w*31600/311600.,1/(1/280000.+1/31600.),1)/scale;
        }
        for(size_t i=0;i<phi.size();++i) {
            const double z=-15.*i/(phi.size()-1);
            const double plus=z*200000/10200000.;
            const double load=1/(1/100000.+1/10000.);
            const double open=(15/100000.+plus/10000.)*load;
            const double reference=open-load*loadedDiode(open,load,1);
            const double amp=std::clamp(2.01*plus-reference+.15,-15.,15.);
            phi[i]=4990*loadedDiode(-amp,10000,1)/scale;
        }
        const double baseline=phi[0];
        for(auto& value:phi) value-=baseline;
        // BAT54S manufacturer forward model. Original 1N5711 forward curve
        // is an explicit nominal approximation within its datasheet bounds.
        for(size_t i=0;i<schottky.size();++i) {
            const double v=static_cast<double>(i)/(schottky.size()-1);
            schottky[i]=shockleyLoaded(v,s?34.9e-6:.2e-6,s?2.28:1.5,s?.210:15.);
        }
        // Eliminate the 499-ohm resistor and the antiparallel bridge node.
        // The table maps r-z to net current, including the 1M resistor.
        for(size_t i=0;i<bridge.size();++i) {
            const double v=30.*i/(bridge.size()-1);
            double lo=0,hi=v;
            for(int k=0;k<55;++k) {
                const double drop=(lo+hi)*.5;
                const double current=(v-drop)/499.;
                const double di=current-drop/1.e6;
                if(di<0 || 2*SwitchingDiode::forwardVoltage(di)<drop) hi=drop;
                else lo=drop;
            }
            bridge[i]=(v-(lo+hi)*.5)/499.;
        }
        reset();
    }
    void reset() noexcept { fast=raw=post=0; memory1=memory2=-15; }
    double pilotGain(double drive,double density,double deMainDb) const noexcept {
        const double h=densityVoltage(density);
        const double offset=-15*2490/(modern?596000.:604000.)/scale;
        const double feedback=2490/(modern?124010.:124000.)/scale;
        const double deRatio=(2490/(modern?124010.:124000.))/(4990/(modern?250000.:249000.));
        return lookup(tables->drivePilot,drive/10)+2490*h/1.e6/scale+offset+feedback*raw-deRatio*deMainDb;
    }
    Result tick(bool attack,double drive,double density) noexcept {
        const double h=densityVoltage(density);
        const double rb=modern?66410.:66500.;
        const double base=(h/rb-13.6/10000)/(1/rb+1/10000.);
        const double ia=attack?std::max(0.,(base+13.6)/(modern?1681.:1690.)):0.;
        const double ir=attack?0.:std::max(0.,(h-.7)/100000.);
        const double sf=(.22e-6/dt+g1+g2)*fast-g1*(memory1+15)-g2*(memory2+15);
        const double sr=ar*raw;
        const double sum=sf+sr+ir-ia;
        double delta=fast-raw;
        // Eliminate memory nodes and the common attack collector. Exponential
        // forward junctions sharing Ia yield a logistic current division.
        for(int k=0;k<12;++k) {
            const auto sch=tableValueSlope(tables->schottky,std::max(0.,delta),1.);
            const double w=1/(1+std::exp(std::clamp(-delta/(SwitchingDiode::ideality*SwitchingDiode::thermal),-60.,60.)));
            const double r=(sum-af*delta)/(af+ar);
            const double residual=ar*r-delta/1.e7-(delta>0?sch[0]:0)+ia*(1-w)-sr;
            const double derivative=-ar*af/(af+ar)-1.e-7-(delta>0?sch[1]:0)
                -ia*w*(1-w)/(SwitchingDiode::ideality*SwitchingDiode::thermal);
            const double step=residual/derivative;
            delta-=std::clamp(step,-.1,.1);
            if(std::abs(step)<1.e-10) break;
        }
        const double nextRaw=(sum-af*delta)/(af+ar);
        // Reduced transistor compliance: release cannot charge beyond zero;
        // lower bound is the -15V supply, not an artificial dB ceiling.
        const double nextFast=std::clamp(nextRaw+delta,-15.,0.);
        memory1=a1*(memory1+nextFast-fast)-15*(1-a1);
        memory2=a2*(memory2+nextFast-fast)-15*(1-a2);
        fast=nextFast; raw=std::clamp(nextRaw,-15.,0.);
        const double g=postCap/dt+1/10200000.;
        const double source=postCap/dt*post;
        double z=post;
        for(int k=0;k<12;++k) {
            const double difference=raw-z;
            const auto current=tableValueSlope(tables->bridge,std::abs(difference),30.);
            const double residual=g*z-source-std::copysign(current[0],difference);
            const double step=residual/(g+current[1]);
            z-=std::clamp(step,-.25,.25);
            if(std::abs(step)<1.e-10) break;
        }
        post=std::clamp(z,-15.,0.);
        return {lookup(tables->driveMain,drive/10),lookup(tables->phi,-post/15)};
    }
    double rawVoltage() const noexcept { return raw; }
    double postVoltage() const noexcept { return post; }
    Result driveContribution(double drive) const noexcept {
        return {lookup(tables->driveMain,drive/10),lookup(tables->drivePilot,drive/10)};
    }
    std::array<double,4> timingVoltages() const noexcept { return {fast,memory1,memory2,raw}; }
    static double densityVoltage(double angle) noexcept {
        // Conventional 10% midpoint A-taper assumption, exact OEM taper unknown.
        const double q=std::expm1(std::log(81.)*std::clamp(angle/10,0.,1.))/80.;
        const double bottom=(.7+499*15/50000.)/(1+499/50000.);
        return bottom+(15-bottom)*q;
    }
private:
    bool modern=false;
    double scale=.0059,dt=1/48000.,cap=2.2e-6,rm1=20000,rm2=39200,leak=22.e6,postCap=1.5e-6;
    double a1=0,a2=0,g1=0,g2=0,af=0,ar=0,fast=0,raw=0,post=0,memory1=-15,memory2=-15;
    struct Tables {
        std::array<double,1025> driveMain{},drivePilot{};
        std::array<double,4097> phi{},schottky{},bridge{};
    };
    std::unique_ptr<Tables> tables;
    template<size_t N> static double lookup(const std::array<double,N>& table,double fraction) noexcept {
        const double x=std::clamp(fraction,0.,1.)*(N-1);
        const size_t n=std::min(static_cast<size_t>(x),N-2);
        return table[n]+(x-n)*(table[n+1]-table[n]);
    }
    template<size_t N> static std::array<double,2> tableValueSlope(const std::array<double,N>& table,double v,double range) noexcept {
        const double x=std::clamp(v/range,0.,1.)*(N-1);
        const size_t n=std::min(static_cast<size_t>(x),N-2);
        const double slope=(table[n+1]-table[n])*(N-1)/range;
        return {table[n]+(x-n)*(table[n+1]-table[n]),slope};
    }
    static double loadedDiode(double voltage,double resistance,int count) noexcept {
        if(voltage<=0) return 0;
        double lo=0,hi=voltage/resistance;
        for(int k=0;k<45;++k) {
            const double i=(lo+hi)*.5;
            if(resistance*i+count*SwitchingDiode::forwardVoltage(i)>voltage) hi=i; else lo=i;
        }
        return (lo+hi)*.5;
    }
    static double shockleyLoaded(double v,double is,double n,double resistance) noexcept {
        double lo=0,hi=v/resistance;
        for(int k=0;k<55;++k) {
            const double i=(lo+hi)*.5;
            if(n*SwitchingDiode::thermal*std::log1p(i/is)+resistance*i>v) hi=i; else lo=i;
        }
        return (lo+hi)*.5;
    }
};
}
