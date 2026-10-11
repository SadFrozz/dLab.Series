#include "DitherShape376.h"
#include <chrono>
#include <iostream>
#include <vector>
using namespace channel376;
volatile double dotOutput=0;
int main(){std::uint64_t frames=0,mismatch=0;double maximum=0;
    std::uint64_t hash=1469598103934665603ull;
    for(double rate:{8000.,44100.,48000.,96000.,192000.,384000.})
    for(auto precision:{StreamPrecision::float32,StreamPrecision::float64})
    for(auto dither:{DitherMode::none,DitherMode::tpdf,DitherMode::snr2})
    for(auto mode:{ShapeMode::shape1,ShapeMode::shape2}){
        DitherShapeProcessor376<false> reference;DitherShapeProcessor376<false,true,true> fast;
        reference.prepare(rate,precision,77);fast.prepare(rate,precision,77);
        reference.modes(dither,mode);fast.modes(dither,mode);
        const int count=rate==48000?131072:8192;
        for(int n=0;n<count;++n){const double input=n%1024<128?0:std::ldexp(.75+.1*std::sin(n*.131),(n/1024)%8==0?-1000:(n/1024)%8==1?-126:(n/1024)%8==2?-149:-3);
            const double a=reference.process(input),b=fast.process(input);std::uint64_t bits=0;std::memcpy(&bits,&a,8);hash=(hash^bits)*1099511628211ull;
            ++frames;if(a!=b){++mismatch;maximum=std::max(maximum,std::abs(a-b));}}
    }
    std::vector<double> pcm(192000);for(int n=0;n<int(pcm.size());++n)pcm[n]=.3*std::sin(n*.131);
    DitherShapeProcessor376<false> reference;DitherShapeProcessor376<false,true,true> fast;
    reference.prepare(48000,StreamPrecision::float64);fast.prepare(48000,StreamPrecision::float64);
    reference.modes(DitherMode::tpdf,ShapeMode::shape1);fast.modes(DitherMode::tpdf,ShapeMode::shape1);
    double sum=0;auto start=std::chrono::steady_clock::now();for(double x:pcm)sum+=reference.process(x);dotOutput=sum;
    const double original=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    sum=0;start=std::chrono::steady_clock::now();for(double x:pcm)sum+=fast.process(x);dotOutput=sum;
    const double reduced=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"frames="<<frames<<" hash="<<hash<<" differing_samples="<<mismatch<<" max_difference="<<maximum<<" original_seconds="<<original<<" reduced_seconds="<<reduced<<" speedup="<<original/reduced<<'\n';
    return mismatch?1:0;
}
