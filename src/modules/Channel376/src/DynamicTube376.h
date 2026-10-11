#pragma once
#include "TubeCircuit376.h"
#include "JjEcc82Parameters376.h"
#include <memory>
#include <mutex>
#include <vector>
#include <stdexcept>
#include <string>
#include <atomic>
namespace channel376 {
// Sch3: parallel sections, algebraic cathode bias, BE states C86/C87.
// The lookup is the solution of the loaded node equations, NOT an audio curve.
// Axes: unloaded grid voltage and unloaded plate voltage including C87 history.
class DynamicTube376 {
public:
    struct Currents {double plate=0,grid=0,plateSlope=0,gridSlope=0;};
    struct Knot {float plate,grid,plateSlope,gridSlope;};
    struct Map {
        static constexpr int nx=1024,ny=256;
        static constexpr double xmin=-12,xstep=24./nx,ymin=64,ystep=160./ny;
        double fs=0,yin=0,g86=0,yload=0,Y=0,C=0,dcPlate=0,gain=0,plateOffset=0;
        int model=0;std::vector<Knot> knots;
        TubeCircuit376::Point law(double vp,double vg)const noexcept{
            if(model==0)return TubeCircuit376::plateLaw(vp,vg);
            const double mu=model==1?22.12791799910628:jjMu376,kg=model==1?1225.9052298660777:jjKg376,
                kp=model==1?68.28017863331158:jjKp376,ex=model==1?1.3:jjEx376,kvb=model==1?300:jjKvb376;
            if(vp<=0)return {};
            const double root=std::sqrt(kvb+vp*vp),a=kp*(1/mu+vg/root);
            const double exp=std::exp(a>40?-a:a),soft=a>40?a:std::log1p(exp),sigmoid=a>40?1/(1+exp):exp/(1+exp);
            const double e=vp/kp*soft;if(e<=0)return {};
            const double value=2*std::pow(e,ex)/kg,d=ex*value/e;
            return {value,d*(soft/kp-vp*vp*vg*sigmoid/(root*root*root)),d*vp*sigmoid/root};
        }
        Currents solve(double g0,double p0,Currents initial={0,0,0,0})const {
            double ip=std::max(0.,initial.plate),ig=std::max(0.,initial.grid);
            std::array<double,4> jac{};double rp=0,rg=0;
            auto evaluate=[&](double p,double g){
                const double vp=p0-(2/Y+4049.9)*p-4000*g,vg=g0-4000*p-(C+4000)*g;
                const auto pl=law(vp,vg),gl=TubeCircuit376::gridLaw(vp,vg);
                rp=p-pl.value;rg=g-gl.value;
                jac={1+pl.plateDerivative*(2/Y+4049.9)+4000*pl.gridDerivative,
                     4000*pl.plateDerivative+(C+4000)*pl.gridDerivative,
                     gl.plateDerivative*(2/Y+4049.9)+4000*gl.gridDerivative,
                     1+4000*gl.plateDerivative+(C+4000)*gl.gridDerivative};
                return std::array<double,2>{pl.gridDerivative,gl.gridDerivative};
            };
            auto rhs=evaluate(ip,ig);
            for(int n=0;n<32&&std::max(std::abs(rp),std::abs(rg))>1e-13;++n){
                const double det=jac[0]*jac[3]-jac[1]*jac[2];
                const double dp=(rp*jac[3]-rg*jac[1])/det,dg=(rg*jac[0]-rp*jac[2])/det;
                const double before=std::max(std::abs(rp),std::abs(rg));bool accepted=false;
                for(double scale=1;scale>1e-6;scale*=.5){
                    const double p=ip-scale*dp,g=ig-scale*dg;
                    rhs=evaluate(std::max(0.,p),std::max(0.,g));if(std::max(std::abs(rp),std::abs(rg))<before){ip=std::max(0.,p);ig=std::max(0.,g);accepted=true;break;}
                }
                if(!accepted){rhs=evaluate(ip,ig);break;}
            }
            rhs=evaluate(ip,ig);
            if(!std::isfinite(rp)||std::max(std::abs(rp),std::abs(rg))>1e-10)throw std::runtime_error("dynamic tube map convergence g="+std::to_string(g0)+" p="+std::to_string(p0)+" rp="+std::to_string(rp)+" rg="+std::to_string(rg));
            const double det=jac[0]*jac[3]-jac[1]*jac[2];
            return {ip,ig,(rhs[0]*jac[3]-rhs[1]*jac[1])/det,(rhs[1]*jac[0]-rhs[0]*jac[2])/det};
        }
        void build(double rate,int index,double load){
            fs=rate;model=index;g86=22e-6*fs;yin=1/(20000+1/g86);
            yload=1/(load+1/(.33e-6*fs));Y=1/(332000./3)+yload;
            C=1000+2/(yin+1/10000.);
            double ip=TubeCircuit376::dcCurrent;
            for(int n=0;n<30;++n){const auto l=law(200-(2*(332000./3)+4049.9)*ip,-4000*ip);
                const double d=1+l.plateDerivative*(2*(332000./3)+4049.9)+4000*l.gridDerivative;
                const double delta=(ip-l.value)/d;ip-=delta;if(std::abs(delta)<1e-15)break;}
            dcPlate=200-2*(332000./3)*ip;
            knots.resize((nx+1)*(ny+1));
            for(int row=0;row<=ny;++row){Currents c{};for(int col=0;col<=nx;++col){
                c=solve(xmin+col*xstep,ymin+row*ystep,c);
                knots[row*(nx+1)+col]={float(c.plate),float(c.grid),float(c.plateSlope),float(c.gridSlope)};
            }}
            const double p0=(200/(332000./3)+yload*dcPlate)/Y;
            gain=-2/Y*solve(0,p0,{ip,0}).plateSlope*yin/(yin+1/10000.)*yload*load;
            // Anchor the interpolated map to the exact DC point: no startup
            // transient from finite knot precision when the input is silence.
            plateOffset=ip-lookup(0,p0).plate;
        }
        Currents lookup(double g0,double p0)const {
            const double x=(g0-xmin)/xstep,y=(p0-ymin)/ystep;
            // Physical fallback, never clamp the transfer into a hard table edge.
            if(x<0||x>nx||y<0||y>ny)return solve(g0,p0);
            const int col=std::min(nx-1,int(x)),row=std::min(ny-1,int(y));const double u=x-col,v=y-row;
            const auto& a=knots[row*(nx+1)+col];const auto& b=knots[row*(nx+1)+col+1];
            const auto& c=knots[(row+1)*(nx+1)+col];const auto& d=knots[(row+1)*(nx+1)+col+1];
            const double h00=(2*u-3)*u*u+1,h10=((u-2)*u+1)*u*xstep,h01=(-2*u+3)*u*u,h11=(u-1)*u*u*xstep;
            const auto blend=[&](double av,double as,double bv,double bs,double cv,double cs,double dv,double ds){
                return (1-v)*(h00*av+h10*as+h01*bv+h11*bs)+v*(h00*cv+h10*cs+h01*dv+h11*ds);};
            return {std::max(0.,plateOffset+blend(a.plate,a.plateSlope,b.plate,b.plateSlope,c.plate,c.plateSlope,d.plate,d.plateSlope)),
                    std::max(0.,blend(a.grid,a.gridSlope,b.grid,b.gridSlope,c.grid,c.gridSlope,d.grid,d.gridSlope)),0,0};
        }
    };
    void prepare(double fs,double trim,double hostRate=0){
        Matrix376<7> m{};auto r=[&](int a,int b,double v){LinearNetwork376<7>::stamp(m,a,b,1/v);};
        r(0,-1,121000);r(0,1,121000);r(1,2,47000+trim);r(2,3,100);r(3,4,20);r(4,5,20000);r(5,6,20000);
        constexpr double a=316227.7660168379;m[2]={};m[2][2]=1;m[2][1]=a;m[6]={};m[6][6]=1;m[6][5]=a;m[0]={};m[0][0]=1;
        Vector376<7> in{},out{};in[0]=1;if(!solve376<7>(m,in,out))throw std::runtime_error("dynamic post gain");
        postGain=out[6];load=121000/(2-out[1]);
        const double baseRate=hostRate>0?hostRate:fs;
        if(baseRate!=bankRate||load!=bankLoad){
            for(int mode=0;mode<3;++mode)for(int j=0;j<3;++j)bank[3*mode+j]=acquire(baseRate*(1<<mode),j,load);
            bankRate=baseRate;bankLoad=load;
        }
        const int mode=fs==baseRate?0:fs==baseRate*2?1:2;
        for(int j=0;j<3;++j)maps[j]=bank[3*mode+j];
        fadeSamples=std::max(1,int(fs*.02));failures=0;resetModel(0);
    }
    static unsigned mapBuildCount()noexcept{return builds().load(std::memory_order_relaxed);}
    void resetModel(int model)noexcept{
        selected=std::clamp(model,0,2);remaining=0;weights={};weights[selected]=1;
        for(int j=0;j<3;++j)states[j]={0,maps[j]->dcPlate,0};
    }
    void selectModel(int model)noexcept{
        model=std::clamp(model,0,2);if(model==selected)return;
        const int dominant=int(std::max_element(weights.begin(),weights.end())-weights.begin());
        if(weights[model]==0){states[model]=states[dominant];states[model].q87+=maps[model]->dcPlate-maps[dominant]->dcPlate;}
        selected=model;remaining=fadeSamples;for(int j=0;j<3;++j)increments[j]=((j==selected?1.:0.)-weights[j])/fadeSamples;
    }
    double tick(double input)noexcept{
        if(!std::isfinite(input)){++failures;input=0;}
        if(remaining==0)return branch(input,selected);
        for(int j=0;j<3;++j)weights[j]+=increments[j];
        if(--remaining==0){weights={};weights[selected]=1;}
        double result=0;for(int j=0;j<3;++j)if(weights[j]>0)result+=weights[j]*branch(input,j);return result;
    }
    double smallSignalGain()const noexcept{return maps[0]->gain*postGain;}
    double postScale()const noexcept{return postGain;}
    const Map& modelMap(int model)const noexcept{return *maps[std::clamp(model,0,2)];}
    unsigned failedSolves()const noexcept{return failures;}
    std::array<double,2> capacitorVoltages(int model=0)const noexcept{return {states[model].q86,states[model].q87};}
private:
    struct State {double q86=0,q87=0,last=0;};
    double branch(double input,int model)noexcept{
        auto& s=states[model];const auto& m=*maps[model];
        const double g0=m.yin*(input-s.q86)/(m.yin+1/10000.),p0=(200/(332000./3)+m.yload*s.q87)/m.Y;
        Currents c{};try{c=m.lookup(g0,p0);}catch(...){++failures;return s.last;}
        const double grid=g0-m.C*c.grid,plate=p0-2*c.plate/m.Y;
        const double capacitorOut=m.yload*(plate-s.q87)*load;
        s.q86+=m.yin*(input-s.q86-grid-1000*c.grid)/m.g86;
        s.q87=plate-capacitorOut;s.last=capacitorOut*postGain*maps[0]->gain/m.gain;
        return s.last;
    }
    static std::shared_ptr<const Map> acquire(double fs,int model,double load){
        struct Entry {double fs,load;int model;std::weak_ptr<const Map> map;};
        static std::mutex mutex;static std::vector<Entry> cache;
        // Bounded warm cache avoids rebuilding all maps for consecutive plugin
        // instances at the same rate. Other rates are held only by live banks.
        static std::vector<std::shared_ptr<const Map>> recent;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            for(const auto& e:cache)if(e.fs==fs&&e.load==load&&e.model==model)if(auto map=e.map.lock())return map;
        }
        // Slow construction must not hold the cache mutex and stall another
        // instance that only wants to retrieve an already-prepared map.
        auto m=std::make_shared<Map>();m->build(fs,model,load);++builds();
        const std::lock_guard<std::mutex> lock(mutex);
        for(const auto& e:cache)if(e.fs==fs&&e.load==load&&e.model==model)if(auto map=e.map.lock())return map;
        cache.erase(std::remove_if(cache.begin(),cache.end(),[](const Entry& e){return e.map.expired();}),cache.end());
        cache.push_back({fs,load,model,m});recent.push_back(m);
        if(recent.size()>9)recent.erase(recent.begin());return m;
    }
    static std::atomic<unsigned>& builds(){static std::atomic<unsigned> counter{0};return counter;}
    // Keep all OS modes alive per instance. Runtime mode changes retrieve
    // these pointers directly, without allocations, cache locks or table builds.
    std::array<std::shared_ptr<const Map>,9> bank;double bankRate=0,bankLoad=0;
    std::array<std::shared_ptr<const Map>,3> maps;std::array<State,3> states{};
    std::array<double,3> weights{{1,0,0}},increments{};int selected=0,remaining=0,fadeSamples=960;
    double postGain=0,load=60500;unsigned failures=0;
};
}
