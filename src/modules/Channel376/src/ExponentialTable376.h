#pragma once
#include <algorithm>
#include "IeeeMath376.h"
#include <array>
namespace channel376 {
// Preparation-only table for the exact exponential gain law; cubic Hermite
// on one binary octave and exact power-of-two scaling at runtime.
struct ExponentialTable376 {
    static constexpr int count=256;
    std::array<std::array<double,4>,count> coefficients{};
    ExponentialTable376(){for(int j=0;j<count;++j){
        constexpr double ln2=.69314718055994530942;
        const double a=std::exp2(double(j)/count),b=std::exp2(double(j+1)/count),da=ln2*a/count,db=ln2*b/count;
        coefficients[j]={a,da,-3*a+3*b-2*da-db,2*a-2*b+da+db};
    }}
    double evaluate(double x)const noexcept{
        if(!std::isfinite(x)||x< -600||x>600)return std::exp(x);
        const double binary=x/0.69314718055994530942;int exponent=int(binary);if(binary<exponent)--exponent;
        const double position=(binary-exponent)*count;const int index=std::min(count-1,int(position));const double u=position-index;
        const auto& c=coefficients[index];return exactScale376(((c[3]*u+c[2])*u+c[1])*u+c[0],exponent);
    }
};
inline const ExponentialTable376 exponentialTable376{};
}
