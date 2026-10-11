#pragma once
#include "CircuitMath376.h"
#include "ProcessingProfile376.h"
#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#endif
namespace channel376 {
// Finite-AOL/GBW MNA. Predict/commit separation couples the tube load without
// advancing capacitor charge during Newton iterations. No allocations.
template<int N,int Amps,int Caps=16> class OpampNetwork376 {
    static constexpr int masks(){int n=1;for(int j=0;j<Amps;++j)n*=3;return n;}
public:
    struct Amplifier {int output=-1,positive=-1,negative=-1;double gain=316227.7660168379,gbw=15e6,swing=13.5,slew=5e6,previous=0,derivative=0;};
    struct Capacitor {int a=-1,b=-1;double farads=0,voltage=0,current=0;};
    struct Prediction {Vector376<N> nodes{};int mask=0;bool valid=false;};
    Matrix376<N> conductance{};Vector376<N> nodes{};
    std::array<Amplifier,Amps> amplifiers{};std::array<Capacitor,Caps> capacitors{};
    int capacitorCount=0;double fs=192000;unsigned failures=0;
    void reset(double rate)noexcept{*this={};fs=rate;}
    void resistor(int a,int b,double r)noexcept{LinearNetwork376<N>::stamp(conductance,a,b,1/r);}
    void capacitor(int a,int b,double c)noexcept{capacitors[capacitorCount++]={a,b,c,0,0};}
    void amplifier(int index,int out,int plus,int minus,double gain=316227.7660168379,double gbw=15e6,double swing=13.5,double slew=5e6)noexcept{
        amplifiers[index]={out,plus,minus,gain,gbw,swing,slew,0,0};
    }
    bool compile()noexcept{
        matrix=conductance;cached={};historyValid=false;
        for(int i=0;i<capacitorCount;++i){const auto& c=capacitors[i];LinearNetwork376<N>::stamp(matrix,c.a,c.b,2*fs*c.farads);}
        for(const auto& a:amplifiers){auto& row=matrix[a.output];row={};const double tau=a.gain/(6.283185307179586*a.gbw);
            row[a.output]=1+2*fs*tau;if(a.positive>=0)row[a.positive]-=a.gain;if(a.negative>=0)row[a.negative]+=a.gain;}
        return buildInverse(0);
    }
    Prediction predict(Vector376<N> source,std::array<double,Amps> externalPositive={})noexcept{
#if defined(CHANNEL376_PROFILE_PROCESSING)
        ++processingProfile376.predictions[N];
#endif
        if(!cached[0])return {nodes,0,false};ensureHistory();
        for(int k=0;k<N;++k)source[k]+=history[k];
        for(int j=0;j<Amps;++j){const auto& a=amplifiers[j];source[a.output]=history[a.output]+a.gain*externalPositive[j];}
        auto candidate=multiply(inverses[0],source);std::array<double,Amps> lo{},hi{};bool overload=false;
        for(int j=0;j<Amps;++j){const auto& a=amplifiers[j];lo[j]=std::max(-a.swing,a.previous-a.slew/fs);hi[j]=std::min(a.swing,a.previous+a.slew/fs);
            overload|=candidate[a.output]<lo[j]||candidate[a.output]>hi[j];}
        int selected=0;
        if(overload){bool found=false;
#if defined(CHANNEL376_PROFILE_PROCESSING)
            ++processingProfile376.overloads[N];
#endif
            for(int mask=1;mask<masks()&&!found;++mask){auto b=source;int code=mask;std::array<int,Amps> active{};
#if defined(CHANNEL376_PROFILE_PROCESSING)
                ++processingProfile376.maskTrials[N];
#endif
                for(int j=0;j<Amps;++j){active[j]=code%3;code/=3;if(active[j])b[amplifiers[j].output]=active[j]==1?lo[j]:hi[j];}
                if(!cached[mask]&&!buildInverse(mask))continue;const auto x=multiply(inverses[mask],b);bool consistent=true;
                for(int j=0;j<Amps;++j){const int out=amplifiers[j].output;double residual=-source[out];for(int k=0;k<N;++k)residual+=matrix[out][k]*x[k];
                    if(active[j]==0)consistent&=x[out]>=lo[j]-1e-9&&x[out]<=hi[j]+1e-9;
                    else if(active[j]==1)consistent&=residual>=-1e-5;else consistent&=residual<=1e-5;}
                if(consistent){candidate=x;selected=mask;found=true;}}
            if(!found)return {nodes,0,false};}
        for(double v:candidate)if(!std::isfinite(v))return {nodes,0,false};return {candidate,selected,true};
    }
    double sensitivity(const Prediction& p,int node,int sourceNode)const noexcept{return p.valid?inverses[p.mask][sourceNode][node]:0;}
    Prediction predictChangedSource(const Prediction& base,Vector376<N> source,
                                     int changedNode,double delta)noexcept{
        if(base.valid&&base.mask==0){
            auto candidate=base.nodes;bool overload=false;
            for(int node=0;node<N;++node)candidate[node]+=inverses[0][changedNode][node]*delta;
            for(const auto& a:amplifiers){
                const double lo=std::max(-a.swing,a.previous-a.slew/fs),hi=std::min(a.swing,a.previous+a.slew/fs);
                overload|=candidate[a.output]<lo||candidate[a.output]>hi;
            }
            if(!overload)return {candidate,0,true};
        }
        return predict(source);
    }
    bool commit(const Prediction& p)noexcept{
        if(!p.valid){++failures;return false;}
        for(int j=0;j<capacitorCount;++j){auto& c=capacitors[j];const double v=(c.a>=0?p.nodes[c.a]:0)-(c.b>=0?p.nodes[c.b]:0);
            c.current=2*fs*c.farads*(v-c.voltage)-c.current;c.voltage=v;}
        for(auto& a:amplifiers){const double v=p.nodes[a.output];a.derivative=2*fs*(v-a.previous)-a.derivative;a.previous=v;}
        nodes=p.nodes;historyValid=false;return true;
    }
    Vector376<N> tick(Vector376<N> source,std::array<double,Amps> externalPositive={})noexcept{const auto p=predict(source,externalPositive);commit(p);return nodes;}
    void invalidateHistory()noexcept{historyValid=false;}
private:
    static Vector376<N> multiply(const Matrix376<N>& m,const Vector376<N>& b)noexcept{
        // Inverses are column-major. SIMD operates across independent node
        // equations and keeps every dot product's original addition order.
        Vector376<N> x{};int row=0;
#if defined(__SSE2__) || defined(_M_X64)
        for(;row+1<N;row+=2){auto sum=_mm_setzero_pd();
            for(int col=0;col<N;++col)sum=_mm_add_pd(sum,_mm_mul_pd(_mm_loadu_pd(m[col].data()+row),_mm_set1_pd(b[col])));
            _mm_storeu_pd(x.data()+row,sum);}
#endif
        for(;row<N;++row)for(int col=0;col<N;++col)x[row]+=m[col][row]*b[col];return x;
    }
    bool buildInverse(int mask)noexcept{
        auto m=matrix;int code=mask;for(int j=0;j<Amps;++j){const int active=code%3;code/=3;
            if(active){const int out=amplifiers[j].output;m[out]={};m[out][out]=1;}}
        for(int col=0;col<N;++col){Vector376<N> b{},x{};b[col]=1;if(!solve376<N>(m,b,x))return false;
            for(int row=0;row<N;++row)inverses[mask][col][row]=x[row];}cached[mask]=true;return true;
    }
    void ensureHistory()noexcept{
        if(historyValid)return;history={};
        for(int j=0;j<capacitorCount;++j){const auto& c=capacitors[j];const double h=2*fs*c.farads*c.voltage+c.current;
            if(c.a>=0)history[c.a]+=h;if(c.b>=0)history[c.b]-=h;}
        for(const auto& a:amplifiers){const double tau=a.gain/(6.283185307179586*a.gbw);history[a.output]=2*fs*tau*a.previous+tau*a.derivative;}historyValid=true;
    }
    Matrix376<N> matrix{};std::array<Matrix376<N>,masks()> inverses{};std::array<bool,masks()> cached{};
    Vector376<N> history{};bool historyValid=false;
};
}
