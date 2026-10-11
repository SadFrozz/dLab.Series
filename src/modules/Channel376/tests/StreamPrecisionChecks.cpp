#include "StreamPrecision.h"
#include <cfenv>
#include <iostream>
#include <limits>

int main() {
    using namespace channel376;
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits==24);
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits==53);
    if(std::fegetround()!=FE_TONEAREST) return 1;
    const auto fp32=StreamPrecision::float32, fp64=StreamPrecision::float64;
    struct Case {double sample;int exponent;};
    for(const Case test: {Case{0.75,-24},Case{1.0,-23},Case{-0.75,-24},
                         Case{2.0,-22},Case{0.001,-33},Case{0.000001,-43}}) {
        const double step=localQuantizationStep(test.sample,fp32);
        if(step!=std::ldexp(1.0,test.exponent)) return 2;
        const float sample=static_cast<float>(test.sample);
        const float outward=std::nextafter(sample,sample>0 ? std::numeric_limits<float>::infinity():-std::numeric_limits<float>::infinity());
        if(std::abs(static_cast<double>(outward)-sample)!=step) return 3;
        if(noiseScaleFromQuantum16(test.sample,fp32)!=std::ldexp(1.0,test.exponent+15)) return 4;
        if(localQuantizationStep(test.sample,fp64)!=std::ldexp(1.0,test.exponent-29)) return 5;
    }
    if(localQuantizationStep(0.0,fp32)!=std::ldexp(1.0,-149)) return 6;
    if(localQuantizationStep(0.0,fp64)!=std::numeric_limits<double>::denorm_min()) return 7;
    if(localQuantizationStep(std::ldexp(1.0,-140),fp32)!=std::ldexp(1.0,-149)) return 8;
    // The boundary is asymmetric; do not claim a globally uniform quantizer.
    if(1.0-static_cast<double>(std::nextafter(1.0f,0.0f))!=std::ldexp(1.0,-24)) return 9;
    const double half=std::ldexp(1.0,-25);
    if(static_cast<float>(0.75+half)!=0.75f) return 10; // nearest-even midpoint
    if(static_cast<float>(0.75+3*half)!=0.75f+std::ldexp(1.0f,-23)) return 11;
    std::cout<<"Host float32/64 spacing, Quantum16 scale, exponent boundaries and rounding verified\n";
}
