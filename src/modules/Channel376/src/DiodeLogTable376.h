#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
namespace channel376 {
// log(1+x), x>=1e-3: binary range reduction and cubic Hermite on [1,2].
// Preparation-only table generation. The small-x branch keeps log1p so
// cancellation near zero does not discard the diode's junction slope.
struct DiodeLogTable376 {
    static constexpr int count=256;
    std::array<std::array<double,4>,count> coefficients{};
    DiodeLogTable376(){for(int j=0;j<count;++j){
        const double a=1+double(j)/count,b=1+double(j+1)/count;
        const double y0=std::log(a),y1=std::log(b),d0=1/(count*a),d1=1/(count*b);
        coefficients[j]={y0,d0,-3*y0+3*y1-2*d0-d1,2*y0-2*y1+d0+d1};
    }}
    double evaluate(double x)const noexcept {
        if(x<1e-3||!std::isfinite(x))return std::log1p(x);
        return logPositive(1+x);
    }
    double logPositive(double y)const noexcept{
        if(!(y>=std::numeric_limits<double>::min()&&std::isfinite(y)))return std::log(y);
        std::uint64_t bits=0;std::memcpy(&bits,&y,8);
        const int exponent=int((bits>>52)&2047)-1023;
        const auto mantissa=bits&((1ull<<52)-1);const int index=int(mantissa>>(52-8));
        const double u=double(mantissa&((1ull<<(52-8))-1))*0x1p-44;
        const auto& c=coefficients[index];
        return exponent*0.69314718055994530942+((c[3]*u+c[2])*u+c[1])*u+c[0];
    }
};
inline const DiodeLogTable376 diodeLogTable376{};
}
