#include "DitherShape376.h"
#include <iostream>
using namespace channel376;
int main(){std::uint64_t frames=0,mismatches=0;
    for(double rate:{8000.,44100.,48000.,96000.,192000.})for(auto precision:{StreamPrecision::float32,StreamPrecision::float64})
    for(auto dither:{DitherMode::none,DitherMode::tpdf,DitherMode::snr2})for(auto shape:{ShapeMode::off,ShapeMode::shape1,ShapeMode::shape2}){
        using Processor=DitherShapeProcessor376<false,true,true>;Processor a,b,l,r;
        a.prepare(rate,precision,11);l.prepare(rate,precision,11);b.prepare(rate,precision,72);r.prepare(rate,precision,72);
        for(auto* p:{&a,&b,&l,&r})p->modes(dither,shape);
        for(int n=0;n<8192;++n){const int e=n/1024%4==0?-1000:n/1024%4==1?-126:n/1024%4==2?-149:-3;
            std::array<double,2> input{n%1024<128?0:std::ldexp(.7+.1*std::sin(n*.131),e),n%1024<512?0:std::ldexp(.5+.2*std::sin(n*.173),e+3)};
            const auto expected=std::array<double,2>{a.process(input[0]),b.process(input[1])},actual=Processor::processPair(l,r,input);
            ++frames;for(int j=0;j<2;++j){std::uint64_t aa=0,bb=0;std::memcpy(&aa,&actual[j],8);std::memcpy(&bb,&expected[j],8);mismatches+=aa!=bb;}
        }
    }
    std::cout<<"stereo_frames="<<frames<<" bit_mismatches="<<mismatches<<'\n';return mismatches?1:0;
}
