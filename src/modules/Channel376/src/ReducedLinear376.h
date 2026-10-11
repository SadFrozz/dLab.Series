#pragma once
#include "OpampNetwork376.h"
#include "IeeeMath376.h"
namespace channel376 {
// Compiled linear companion-state map. Same trapezoidal capacitors and
// finite opamp poles as MNA. No history-vector reconstruction per sample.
// A rejected rail/slew candidate never advances state; restore() supplies
// physical charges to the original active-set solver for overload handling.
template<int N,int Amps,int Caps=16> class ReducedLinear376 {
    static constexpr int capacity=Caps+Amps;
    using Network=OpampNetwork376<N,Amps,Caps>;
public:
    bool compile(const Network& net,unsigned observedMask)noexcept{
        fs=net.fs;capCount=net.capacitorCount;count=capCount+Amps;
        caps=net.capacitors;amps=net.amplifiers;observed=observedMask;historyMap={};
        auto matrix=net.conductance;
        for(int j=0;j<capCount;++j){const auto& c=caps[j];const double g=2*fs*c.farads;
            LinearNetwork376<N>::stamp(matrix,c.a,c.b,g);
            if(c.a>=0)observed|=1u<<c.a;if(c.b>=0)observed|=1u<<c.b;}
        for(const auto& a:amps){observed|=1u<<a.output;auto& row=matrix[a.output];row={};
            const double tau=a.gain/(6.283185307179586*a.gbw);
            row[a.output]=1+2*fs*tau;if(a.positive>=0)row[a.positive]-=a.gain;if(a.negative>=0)row[a.negative]+=a.gain;}
        inverse={};
        for(int col=0;col<N;++col){Vector376<N> rhs{},x{};rhs[col]=1;
            if(!solve376<N>(matrix,rhs,x)){valid=false;return false;}
            for(int row=0;row<N;++row)inverse[col][row]=x[row];}
        for(int col=0;col<count;++col){Vector376<N> rhs{};
            if(col<capCount){const auto& c=caps[col];const double g=2*fs*c.farads;
                if(c.a>=0)rhs[c.a]+=g;if(c.b>=0)rhs[c.b]-=g;
                for(const auto& a:amps)rhs[a.output]=0;
            }else{const auto& a=amps[col-capCount];rhs[a.output]=2*fs*a.gain/(6.283185307179586*a.gbw);}
            for(int row=0;row<N;++row)for(int k=0;k<N;++k)historyMap[col][row]+=inverse[k][row]*rhs[k];}
        sync(net);valid=true;return true;
    }
    void sync(const Network& net)noexcept{
        for(int j=0;j<capCount;++j){const auto& c=net.capacitors[j];state[j]=c.voltage+c.current/(2*fs*c.farads);}
        for(int j=0;j<Amps;++j){const auto& a=net.amplifiers[j];state[capCount+j]=a.previous+a.derivative/(2*fs);previousAmp[j]=a.previous;}
        lastNodes=net.nodes;
        for(int j=0;j<capCount;++j){const auto& c=net.capacitors[j];lastCapVoltage[j]=c.voltage;}
    }
    bool tick(Vector376<N> source,Vector376<N>& result,
              std::array<double,Amps> externalPositive={},int knownSourceCap=-1,double knownInput=0)noexcept{
        for(int j=0;j<Amps;++j)source[amps[j].output]=amps[j].gain*externalPositive[j];
        return tickImpl<false>(source,result,knownSourceCap,knownInput,0);
    }
    void configureInput(Vector376<N> weights,std::array<double,Amps> externalPositive={})noexcept{
        for(int j=0;j<Amps;++j)weights[amps[j].output]=amps[j].gain*externalPositive[j];
        inputMap={};for(int j=0;j<N;++j)if(weights[j]!=0)for(int row=0;row<N;++row)inputMap[row]+=inverse[j][row]*weights[j];
    }
    bool tickInput(double input,Vector376<N>& result,int knownSourceCap=-1)noexcept{
        return tickImpl<true>({},result,knownSourceCap,input,input);
    }
private:
    template<bool singleInput> bool tickImpl(Vector376<N> source,Vector376<N>& result,int knownSourceCap,double knownInput,double input)noexcept{
        if(!valid)return false;
        result={};
        for(int row=0;row<N;++row)if(observed&(1u<<row)){
#if defined(CHANNEL376_EXPERIMENTAL_AVX2) && defined(_MSC_VER) && defined(_M_X64)
            if(avx2376&&row+3<N&&((observed>>row)&15)==15){
                auto value=_mm256_setzero_pd();
                for(int j=0;j<count;++j)value=_mm256_add_pd(value,_mm256_mul_pd(_mm256_loadu_pd(historyMap[j].data()+row),_mm256_set1_pd(state[j])));
                if constexpr(singleInput)value=_mm256_add_pd(value,_mm256_mul_pd(_mm256_loadu_pd(inputMap.data()+row),_mm256_set1_pd(input)));
                else for(int j=0;j<N;++j)if(source[j]!=0)value=_mm256_add_pd(value,_mm256_mul_pd(_mm256_loadu_pd(inverse[j].data()+row),_mm256_set1_pd(source[j])));
                _mm256_storeu_pd(result.data()+row,value);
                for(int j=0;j<4;++j)if(!std::isfinite(result[row+j]))return false;
                row+=3;continue;
            }
#endif
#if defined(__SSE2__) || defined(_M_X64)
            if(row+1<N&&(observed&(1u<<(row+1)))){
                auto value=_mm_setzero_pd();
                for(int j=0;j<count;++j)value=_mm_add_pd(value,_mm_mul_pd(_mm_loadu_pd(historyMap[j].data()+row),_mm_set1_pd(state[j])));
                if constexpr(singleInput)value=_mm_add_pd(value,_mm_mul_pd(_mm_loadu_pd(inputMap.data()+row),_mm_set1_pd(input)));
                else for(int j=0;j<N;++j)if(source[j]!=0)value=_mm_add_pd(value,_mm_mul_pd(_mm_loadu_pd(inverse[j].data()+row),_mm_set1_pd(source[j])));
                _mm_storeu_pd(result.data()+row,value);
                if(!std::isfinite(result[row])||!std::isfinite(result[row+1]))return false;
                ++row;continue;
            }
#endif
            double value=0;for(int j=0;j<count;++j)value+=historyMap[j][row]*state[j];
            if constexpr(singleInput)value+=inputMap[row]*input;
            else for(int j=0;j<N;++j)if(source[j]!=0)value+=inverse[j][row]*source[j];
            if(!std::isfinite(value))return false;result[row]=value;
        }
        for(int j=0;j<Amps;++j){const auto& a=amps[j];const double lo=std::max(-a.swing,previousAmp[j]-a.slew/fs),hi=std::min(a.swing,previousAmp[j]+a.slew/fs);
            if(result[a.output]<lo||result[a.output]>hi)return false;}
        for(int j=0;j<capCount;++j){const auto& c=caps[j];const double v=(c.a>=0?result[c.a]:0)-(c.b>=0?result[c.b]:0)-(j==knownSourceCap?knownInput:0);
            state[j]=2*v-state[j];lastCapVoltage[j]=v;}
        for(int j=0;j<Amps;++j){const double v=result[amps[j].output];state[capCount+j]=2*v-state[capCount+j];previousAmp[j]=v;}
        lastNodes=result;return true;
    }
public:
    void restore(Network& net)const noexcept{
        for(int j=0;j<capCount;++j){auto& c=net.capacitors[j];c.voltage=lastCapVoltage[j];c.current=2*fs*c.farads*(state[j]-c.voltage);}
        for(int j=0;j<Amps;++j){auto& a=net.amplifiers[j];a.previous=previousAmp[j];a.derivative=2*fs*(state[capCount+j]-a.previous);}
        net.nodes=lastNodes;net.invalidateHistory();
    }
private:
    double fs=192000;int capCount=0,count=0;unsigned observed=0;bool valid=false;
    Matrix376<N> inverse{};std::array<Vector376<N>,capacity> historyMap{};
    std::array<double,capacity> state{};std::array<double,Amps> previousAmp{};
    std::array<double,Caps> lastCapVoltage{};
    std::array<typename Network::Capacitor,Caps> caps{};std::array<typename Network::Amplifier,Amps> amps{};
    Vector376<N> lastNodes{},inputMap{};
};
}
