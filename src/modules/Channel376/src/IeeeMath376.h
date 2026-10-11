#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#if defined(_MSC_VER) && defined(_M_X64)
#include <immintrin.h>
#include <intrin.h>
#elif defined(__APPLE__) && defined(__x86_64__)
#include <immintrin.h>
#include <cpuid.h>
#endif
namespace channel376 {
#if defined(_MSC_VER) && defined(_M_X64)
inline bool detectFma376()noexcept{int c[4]{};__cpuid(c,1);return (c[2]&(1<<12))&&(c[2]&(1<<27))&&(c[2]&(1<<28))&&(_xgetbv(0)&6)==6;}
inline const bool fma376=detectFma376();
inline bool detectAvx2376()noexcept{int c[4]{};__cpuid(c,0);if(c[0]<7||!fma376)return false;__cpuidex(c,7,0);return (c[1]&(1<<5))!=0;}
inline const bool avx2376=detectAvx2376();
#elif defined(__APPLE__) && defined(__x86_64__)
inline bool detectFma376()noexcept{
    unsigned a=0,b=0,c=0,d=0;if(!__get_cpuid(1,&a,&b,&c,&d))return false;
    if(!(c&(1u<<12))||!(c&(1u<<27))||!(c&(1u<<28)))return false;
    unsigned stateLow=0,stateHigh=0;__asm__("xgetbv":"=a"(stateLow),"=d"(stateHigh):"c"(0));return (stateLow&6)==6;
}
inline const bool fma376=detectFma376();
__attribute__((target("avx,fma"))) inline double hardwareFma376(double a,double b,double c)noexcept{
    return _mm_cvtsd_f64(_mm_fmadd_sd(_mm_set_sd(a),_mm_set_sd(b),_mm_set_sd(c)));
}
__attribute__((target("avx,fma"))) inline __m128d hardwareFmaPair376(__m128d a,__m128d b,__m128d c)noexcept{
    return _mm_fmadd_pd(a,b,c);
}
#endif
inline double exactFma376(double a,double b,double c)noexcept{
#if defined(_MSC_VER) && defined(_M_X64)
    if(fma376)return _mm_cvtsd_f64(_mm_fmadd_sd(_mm_set_sd(a),_mm_set_sd(b),_mm_set_sd(c)));
#elif defined(__APPLE__) && defined(__x86_64__)
    if(fma376)return hardwareFma376(a,b,c);
#endif
    return std::fma(a,b,c);
}
inline double exactScale376(double x,int shift)noexcept{
    std::uint64_t bits;std::memcpy(&bits,&x,8);const int e=int((bits>>52)&2047),next=e+shift;
    if((bits&0x7fffffffffffffffull)==0)return x;
    if(e>0&&e<2047&&next>0&&next<2047){bits+=std::uint64_t(std::int64_t(shift))*(1ull<<52);std::memcpy(&x,&bits,8);return x;}
    return std::ldexp(x,shift);
}
inline int exactExponent376(double x)noexcept{
    std::uint64_t bits;std::memcpy(&bits,&x,8);const int e=int((bits>>52)&2047);if(e>0)return e-1023;
    bits&=(1ull<<52)-1;if(!bits)return -1074;int position=0;
    while(bits>>1){bits>>=1;++position;}return position-1074;
}
}
