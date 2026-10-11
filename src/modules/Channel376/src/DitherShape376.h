#pragma once
#include "ParameterContract.h"
#include "ShapeTables.h"
#include "StreamPrecision.h"
#include "IeeeMath376.h"
#include <array>
#include <complex>
#include <cstring>
#include <cstdint>
#include <limits>
#include <stdexcept>
#if (defined(CHANNEL376_EXPERIMENTAL_WIDE_SIMD) || defined(CHANNEL376_EXPERIMENTAL_STEREO_SHAPE)) && (defined(__SSE2__) || defined(_M_X64))
#include <emmintrin.h>
#endif

namespace channel376 {
// Non-overlapping expansion. Windows long double is deliberately not used.
struct Wide {
    double hi=0,lo=0;
    static Wide sum(double a,double b) noexcept {
        const double s=a+b,z=s-a;
        return {s,(a-(s-z))+(b-z)};
    }
    Wide operator+(Wide b) const noexcept {
#if defined(CHANNEL376_EXPERIMENTAL_WIDE_SIMD) && (defined(__SSE2__) || defined(_M_X64))
        // The two independent TwoSum operations retain their original
        // operation order within each lane; only hi/lo run in parallel.
        const auto a2=_mm_set_pd(lo,hi),b2=_mm_set_pd(b.lo,b.hi);
        const auto result=_mm_add_pd(a2,b2),z=_mm_sub_pd(result,a2);
        const auto error=_mm_add_pd(_mm_sub_pd(a2,_mm_sub_pd(result,z)),_mm_sub_pd(b2,z));
        double sums[2],remainders[2];_mm_storeu_pd(sums,result);_mm_storeu_pd(remainders,error);
        Wide s{sums[0],remainders[0]},t{sums[1],remainders[1]};
#else
        auto s=sum(hi,b.hi),t=sum(lo,b.lo);
#endif
        auto u=sum(s.lo,t.hi),v=sum(s.hi,u.hi);
        return sum(v.hi,v.lo+u.lo+t.lo);
    }
    Wide operator-() const noexcept {return {-hi,-lo};}
    Wide operator-(Wide b) const noexcept {return *this+-b;}
    Wide times(double b) const noexcept {
        const double p=hi*b;
        return sum(p,exactFma376(hi,b,-p)+lo*b);
    }
    Wide shifted(int e) const noexcept {return {exactScale376(hi,e),exactScale376(lo,e)};}
};
struct ScaledError {Wide value;int exponent=0;};

inline bool evenDouble(double x) noexcept {
    std::uint64_t bits=0;std::memcpy(&bits,&x,sizeof x);return (bits&1)==0;
}
inline bool evenFloat(float x) noexcept {
    std::uint32_t bits=0;std::memcpy(&bits,&x,sizeof x);return (bits&1)==0;
}
inline Wide magnitude(Wide x) noexcept {return x.hi<0||(x.hi==0&&x.lo<0)?-x:x;}
inline bool smaller(Wide a,Wide b) noexcept {
    const auto d=a-b;return d.hi<0||(d.hi==0&&d.lo<0);
}
// Compare actual neighboring IEEE values in a scaled domain, including
// subnormals and power-of-two bin boundaries. Ties select the even mantissa.
inline double roundScaled(Wide input,int exponent,StreamPrecision precision) noexcept {
    input=Wide::sum(input.hi,input.lo);
    double initial=exactScale376(input.hi,exponent);
    if(precision==StreamPrecision::float32) initial=static_cast<float>(initial);
    if(!std::isfinite(initial)) initial=std::copysign(precision==StreamPrecision::float32?
        double(std::numeric_limits<float>::max()):std::numeric_limits<double>::max(),initial);
    auto distance=[&](double x){return magnitude(input-Wide{exactScale376(x,-exponent),0});};
    double result=initial;auto best=distance(result);
    for(int direction:{-1,1}) {
        const double infinity=direction<0?-std::numeric_limits<double>::infinity():
                                                 std::numeric_limits<double>::infinity();
        const double neighbor=precision==StreamPrecision::float32?
            double(std::nextafter(static_cast<float>(initial),static_cast<float>(infinity))):
            std::nextafter(initial,infinity);
        if(!std::isfinite(neighbor)) continue;
        const auto error=distance(neighbor);
        const bool even=precision==StreamPrecision::float32?evenFloat(float(neighbor)):evenDouble(neighbor);
        if(smaller(error,best)||(!smaller(best,error)&&even)) {result=neighbor;best=error;}
    }
    return result;
}

inline double roundScaledFast376(Wide input,int exponent,StreamPrecision precision)noexcept {
    // TwoSum chooses the correctly rounded high part of hi+lo. Scaling a
    // normal binary64 by an exact power of two preserves that choice except
    // at the normal/subnormal boundary, where the full neighbor check stays.
    if(precision==StreamPrecision::float64){
        const auto normalized=Wide::sum(input.hi,input.lo);
        std::uint64_t bits=0;std::memcpy(&bits,&normalized.hi,8);
        const int encoded=int((bits>>52)&2047),scaled=encoded+exponent;
        if(encoded>0&&encoded<2047&&scaled>1&&scaled<2047)
            return exactScale376(normalized.hi,exponent);
    }
    return roundScaled(input,exponent,precision);
}

template<bool compensatedDot=false,bool fastRound=false,bool skipZeroTaps=false> class DitherShapeProcessor376 {
public:
    static constexpr int maximumTaps=256;
    void prepare(double sampleRate,StreamPrecision format,std::uint32_t seed=0x091110) {
        if(!(sampleRate>=8000&&sampleRate<=384000)) throw std::invalid_argument("sample rate outside 8..384kHz");
        rate=sampleRate;precision=format;
        for(int shape=1;shape<=2;++shape) design(shape,coefficients[shape-1]);
        for(int s=0;s<2;++s){feedbackTaps[s]=taps;while(feedbackTaps[s]>0&&coefficients[s][feedbackTaps[s]-1]==0)--feedbackTaps[s];}
        reset(seed);
    }
    void reset(std::uint32_t seed=0x091110) noexcept {
        random1=seed&0xffffff;random2=(seed^0x5a3b17)&0xffffff;
        previousUniform=signedUnit(random1);filters={};errors={};cursor=0;queueHead=queueTail=0;sequence=0;
    }
    void modes(DitherMode d,ShapeMode s) noexcept {dither=d;shape=s;}
    void streamPrecision(StreamPrecision value)noexcept{precision=value;}
    double process(double input,const Wide* suppliedFeedback=nullptr) noexcept {
        if(!std::isfinite(input)) {reset();return 0;}
        if(dither==DitherMode::none&&shape==ShapeMode::off)
            return precision==StreamPrecision::float32?double(float(input)):input;
        int anchor=exponentOf(input);
        while(queueHead!=queueTail&&sequence-peakQueue[queueHead].sequence>std::uint64_t(taps))queueHead=(queueHead+1)&511;
        if(shape!=ShapeMode::off) {
            if(queueHead!=queueTail)anchor=std::max(anchor,peakQueue[queueHead].exponent);
        }
        if(anchor<-1022) anchor=-1022;
        Wide signal{exactScale376(input,-anchor),0};
        if(shape!=ShapeMode::off) {
            const auto& h=coefficients[int(shape)-1];
            // The original 24-sample anchor history remains unchanged.
            const int activeTaps=skipZeroTaps?feedbackTaps[int(shape)-1]:taps;
            Wide feedback;
            if(suppliedFeedback)feedback=*suppliedFeedback;
            else {
            // Keep the original expansion when error feedback can dominate
            // the new input. Cancellation in silence/extreme level changes
            // needs its extra low-part accuracy; normal PCM can use Dot2.
            const bool safeDot=compensatedDot&&(queueHead==queueTail||exponentOf(input)>=peakQueue[queueHead].exponent+8);
            if(safeDot){
                double sum=0,correction=0;
                for(int k=0;k<activeTaps;++k){const auto& e=errors[(cursor+maximumTaps-1-k)%maximumTaps];
                    const auto v=e.value.shifted(e.exponent-anchor);const double product=v.hi*h[k];
                    const double remainder=exactFma376(v.hi,h[k],-product)+v.lo*h[k];
                    const auto added=Wide::sum(sum,product);sum=added.hi;correction+=added.lo+remainder;}
                feedback=Wide::sum(sum,correction);
            }else{
            for(int k=0;k<activeTaps;++k) {
                const auto& e=errors[(cursor+maximumTaps-1-k)%maximumTaps];
                feedback=feedback+e.value.shifted(e.exponent-anchor).times(h[k]);
            }
            }
            }
            signal=signal-feedback;
        }
        // Reanchor after cancellation, so silence uses a representable scaled
        // dither even when the actual float64 LSB is 2^-1074.
        const double lead=signal.hi!=0?signal.hi:signal.lo;
        const int desired=lead==0?(precision==StreamPrecision::float32?-126:-1022):
            std::max(precision==StreamPrecision::float32?-126:-1022,anchor+exponentOf(lead));
        signal=signal.shifted(anchor-desired);anchor=desired;
        const int digits=precision==StreamPrecision::float32?24:53;
        const int minimum=precision==StreamPrecision::float32?-149:-1074;
        const int signalExponent=lead==0?minimum:anchor+exponentOf(signal.hi!=0?signal.hi:signal.lo);
        const int stepExponent=std::max(minimum,signalExponent-(digits-1));
        const double unit=unitNoise(dither);
        const auto noisy=signal+Wide{exactScale376(unit,stepExponent-anchor),0};
        const double result=fastRound?roundScaledFast376(noisy,anchor,precision):roundScaled(noisy,anchor,precision);
        const Wide error=Wide{exactScale376(result,-anchor),0}-signal;
        if(error.hi!=0){const int exponent=anchor+exponentOf(error.hi);
            while(queueHead!=queueTail&&peakQueue[(queueTail+511)&511].exponent<=exponent)queueTail=(queueTail+511)&511;
            peakQueue[queueTail]={sequence,exponent};queueTail=(queueTail+1)&511;
        }
        ++sequence;
        errors[cursor]={error,anchor};cursor=(cursor+1)%maximumTaps;
        return result;
    }
    static std::array<double,2> processPair(DitherShapeProcessor376& left,DitherShapeProcessor376& right,std::array<double,2> input)noexcept{
#if defined(CHANNEL376_EXPERIMENTAL_STEREO_SHAPE) && (defined(__SSE2__) || defined(_M_X64))
        if(!compensatedDot&&left.shape!=ShapeMode::off&&left.shape==right.shape&&left.rate==right.rate&&std::isfinite(input[0])&&std::isfinite(input[1])){
            const int al=left.feedbackAnchor(input[0]),ar=right.feedbackAnchor(input[1]);
            const int active=skipZeroTaps?left.feedbackTaps[int(left.shape)-1]:left.taps;
            const auto& h=left.coefficients[int(left.shape)-1];
            StereoWide feedback{_mm_setzero_pd(),_mm_setzero_pd()};
            for(int k=0;k<active;++k){
                const auto& el=left.errors[(left.cursor+maximumTaps-1-k)%maximumTaps];
                const auto& er=right.errors[(right.cursor+maximumTaps-1-k)%maximumTaps];
                const auto l=el.value.shifted(el.exponent-al),r=er.value.shifted(er.exponent-ar);
                const StereoWide value{_mm_set_pd(r.hi,l.hi),_mm_set_pd(r.lo,l.lo)};
                feedback=feedback.plus(value.times(h[k]));
            }
            double hi[2],lo[2];_mm_storeu_pd(hi,feedback.hi);_mm_storeu_pd(lo,feedback.lo);
            const Wide fl{hi[0],lo[0]},fr{hi[1],lo[1]};
            return {left.process(input[0],&fl),right.process(input[1],&fr)};
        }
#endif
        return {left.process(input[0]),right.process(input[1])};
    }
    double unitNoise(DitherMode mode) noexcept {
        if(mode==DitherMode::none)return 0;
        const double previous=signedUnit(random1);
        random1=(12389*random1+3063)&0xffffff;
        const double u=signedUnit(random1);
        if(mode==DitherMode::tpdf) {
            random2=(12389*random2+3063)&0xffffff;
            return (u+signedUnit(random2))*.5;
        }
        double value=q23(u-previous);previousUniform=u;
        for(auto& f:filters) value=f.tick(value);
        return value*11/16;
    }
    template<class Sample>void processBlock(Sample* samples,int count)noexcept {
        for(int n=0;n<count;++n)samples[n]=static_cast<Sample>(process(double(samples[n])));
    }
    const std::array<double,maximumTaps>& table(ShapeMode s)const noexcept {return coefficients[int(s)-1];}
    int tapCount()const noexcept{return taps;}
    bool usesOriginalTable()const noexcept{return original;}
private:
#if defined(CHANNEL376_EXPERIMENTAL_STEREO_SHAPE) && (defined(__SSE2__) || defined(_M_X64))
    struct StereoWide {
        __m128d hi,lo;
        static StereoWide sum(__m128d a,__m128d b)noexcept{const auto s=_mm_add_pd(a,b),z=_mm_sub_pd(s,a);return {s,_mm_add_pd(_mm_sub_pd(a,_mm_sub_pd(s,z)),_mm_sub_pd(b,z))};}
        static StereoWide orderedSum(__m128d a,__m128d b)noexcept{
#if defined(CHANNEL376_EXPERIMENTAL_ORDERED_SUM)
            const auto sign=_mm_set1_pd(-0.0);
            if(_mm_movemask_pd(_mm_cmpge_pd(_mm_andnot_pd(sign,a),_mm_andnot_pd(sign,b)))==3){
                const auto s=_mm_add_pd(a,b),error=_mm_sub_pd(b,_mm_sub_pd(s,a));
                // Match TwoSum's canonical +0 residual, including silence.
                return {s,_mm_andnot_pd(_mm_cmpeq_pd(error,_mm_setzero_pd()),error)};
            }
#endif
            return sum(a,b);
        }
        StereoWide plus(StereoWide b)const noexcept{
            const auto s=sum(hi,b.hi),t=sum(lo,b.lo),u=sum(s.lo,t.hi),v=orderedSum(s.hi,u.hi);
            return orderedSum(v.hi,_mm_add_pd(_mm_add_pd(v.lo,u.lo),t.lo));
        }
        StereoWide times(double coefficient)const noexcept{
            const auto b=_mm_set1_pd(coefficient),p=_mm_mul_pd(hi,b);__m128d remainder;
#if defined(_MSC_VER) && defined(_M_X64)
            if(fma376)remainder=_mm_fmadd_pd(hi,b,_mm_xor_pd(p,_mm_set1_pd(-0.0)));
            else
#elif defined(__APPLE__) && defined(__x86_64__)
            if(fma376)remainder=hardwareFmaPair376(hi,b,_mm_xor_pd(p,_mm_set1_pd(-0.0)));
            else
#endif
            {double h[2],products[2];_mm_storeu_pd(h,hi);_mm_storeu_pd(products,p);remainder=_mm_set_pd(exactFma376(h[1],coefficient,-products[1]),exactFma376(h[0],coefficient,-products[0]));}
            return orderedSum(p,_mm_add_pd(remainder,_mm_mul_pd(lo,b)));
        }
    };
    int feedbackAnchor(double input)noexcept{
        int anchor=exponentOf(input);
        while(queueHead!=queueTail&&sequence-peakQueue[queueHead].sequence>std::uint64_t(taps))queueHead=(queueHead+1)&511;
        if(queueHead!=queueTail)anchor=std::max(anchor,peakQueue[queueHead].exponent);
        return std::max(-1022,anchor);
    }
#endif
    static int exponentOf(double x)noexcept {
        return exactExponent376(x);
    }
    static double signedUnit(std::uint32_t r)noexcept {
        return (r<0x800000?double(r):double(r)-16777216.)/8388608.;
    }
    static double q23(double x)noexcept {
        return std::clamp(std::floor(x*8388608.),-8388608.,8388607.)/8388608.;
    }
    struct Filter {
        std::array<double,3> x{},y{};
        double tick(double v)noexcept {
            const double result=q23(-1.459028959274292*y[0]-.9103689193725586*y[1]
                -.19782495498657227*y[2]+.03168916702270508*v
                -.09506797790527344*x[0]+.09506797790527344*x[1]-.03168916702270508*x[2]);
            x={v,x[0],x[1]};y={result,y[0],y[1]};return result;
        }
    };
    void design(int mode,std::array<double,maximumTaps>& out) {
        out.fill(0);int chosen=0;
        for(int r=1;r<4;++r)if(std::abs(std::log(rate/shapeRates[r]))<
                std::abs(std::log(rate/shapeRates[chosen])))chosen=r;
        original=rate==shapeRates[chosen];const auto& source=shapeTables[2*chosen+mode-1];
        if(original){taps=24;std::copy(source.begin(),source.end(),out.begin());return;}
        // Minimum-phase NTF from physical-frequency log magnitude, normalized
        // to monic NTF[0]=1. Resampling H's phase directly loses noise shaping
        // at low rates because fractional delays require noncausal feedback.
        // Below the smallest source rate, preserve normalized spectral shape:
        // truncating its low-frequency slice leaves no band for displaced noise.
        const double ratio=rate<shapeRates[0]?1.:rate/shapeRates[chosen];
        taps=std::min(maximumTaps,std::max(192,int(std::ceil(24*ratio))+32));
        constexpr int bins=2048;constexpr double pi=3.14159265358979323846;
        std::array<double,maximumTaps+1> cepstrum{},ntf{};
        for(int b=0;b<=bins;++b) {
            const double omega=pi*b/bins,base=omega*ratio;
            std::complex<double> target{};
            if(base<=pi)for(int k=0;k<24;++k)target+=source[k]*std::polar(1.,-base*(k+1));
            double logMagnitude=std::log(std::max(1e-8,std::abs(1.-target)));
            if(ratio>1&&base>.95*pi&&base<pi)
                logMagnitude*=.5*(1+std::cos(pi*(base-.95*pi)/(.05*pi)));
            const double weight=(b==0||b==bins)?.5:1.;
            for(int k=0;k<=taps;++k)cepstrum[k]+=weight*logMagnitude*std::cos(omega*k)/bins;
        }
        ntf[0]=1.; // c[0] removed: causal error feedback has no zero-delay tap.
        for(int n=1;n<=taps;++n){
            for(int k=1;k<=n;++k)ntf[n]+=2*k*cepstrum[k]*ntf[n-k]/n;
            out[n-1]=-ntf[n];
        }
        double bound=0;for(double h:out)bound+=std::abs(h);
        if(!std::isfinite(bound)||bound>64)throw std::runtime_error("unbounded shaping response");
    }
    double rate=48000;StreamPrecision precision=StreamPrecision::float32;
    DitherMode dither=DitherMode::none;ShapeMode shape=ShapeMode::off;
    std::uint32_t random1=0x091110,random2=0x532a07;double previousUniform=0;
    std::array<Filter,2> filters{};
    std::array<ScaledError,maximumTaps> errors{};int cursor=0,taps=24;bool original=true;
    std::array<std::array<double,maximumTaps>,2> coefficients{};
    std::array<int,2> feedbackTaps{};
    struct Peak {std::uint64_t sequence=0;int exponent=0;};
    std::array<Peak,512> peakQueue{};int queueHead=0,queueTail=0;std::uint64_t sequence=0;
};
#if defined(CHANNEL376_EXPERIMENTAL_DOT2)
using DitherShape376=DitherShapeProcessor376<true>;
#elif defined(CHANNEL376_EXPERIMENTAL_ROUND)
using DitherShape376=DitherShapeProcessor376<false,true,true>;
#else
using DitherShape376=DitherShapeProcessor376<false>;
#endif
}
