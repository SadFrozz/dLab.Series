#pragma once
#include "IeeeMath376.h"
#include <array>
namespace channel376 {
// Small-range polynomials after tabulated range reduction. These accelerate
// elementary functions, not the device characteristic or state equations.
// IEEE exceptional/subnormal cases retain the standard-library fallback.
inline constexpr double functionLn2Hi376=0.69314670562744141,functionLn2Lo376=4.7493250390316726e-07;
inline constexpr std::array<double,64> logarithmCenters376{{
    0.007782140442054949,
    0.023167059281534379,
    0.038318864302136602,
    0.053244514518812285,
    0.067950661908507751,
    0.082443669211074586,
    0.096729626458551113,
    0.11081436634029011,
    0.12470347850095724,
    0.13840232285911913,
    0.15191604202584197,
    0.16524957289530717,
    0.17840765747281831,
    0.19139485299962947,
    0.20421554142869089,
    0.21687393830061436,
    0.22937410106484582,
    0.24171993688714516,
    0.25391520998096345,
    0.26596354849713794,
    0.27786845100345631,
    0.28963329258304266,
    0.30126133057816179,
    0.3127557100038969,
    0.32411946865421198,
    0.33535554192113781,
    0.34646676734620857,
    0.3574558889218038,
    0.36832556115870763,
    0.37907835293496944,
    0.38971675114002519,
    0.40024316412701272,
    0.41065992498526838,
    0.42096929464412963,
    0.43117346481837132,
    0.4412745608048752,
    0.45127464413945856,
    0.46117571512217015,
    0.47097971521879101,
    0.4806885293457519,
    0.49030398804519382,
    0.49982786955644931,
    0.5092619017898079,
    0.51860776420804566,
    0.52786708962084239,
    0.53704146589688362,
    0.54613243759813568,
    0.55514150754050162,
    0.56407013828480301,
    0.57291975356178548,
    0.58169173963462251,
    0.59038744660217635,
    0.59900818964608338,
    0.60755525022454182,
    0.61602987721551405,
    0.62443328801189346,
    0.63276666957103778,
    0.64103117942093124,
    0.64922794662510985,
    0.65735807270836,
    0.66542263254509049,
    0.6734226752121667,
    0.68135922480790312,
    0.689233281238809
}};
inline constexpr std::array<double,32> exponentialCenters376{{
    1,
    1.0218971486541166,
    1.0442737824274138,
    1.0671404006768237,
    1.0905077326652577,
    1.1143867425958924,
    1.1387886347566916,
    1.1637248587775775,
    1.189207115002721,
    1.215247359980469,
    1.241857812073484,
    1.2690509571917332,
    1.2968395546510096,
    1.3252366431597413,
    1.3542555469368927,
    1.383909881963832,
    1.4142135623730951,
    1.4451808069770467,
    1.4768261459394993,
    1.5091644275934228,
    1.5422108254079407,
    1.5759808451078865,
    1.6104903319492543,
    1.6457554781539649,
    1.681792830507429,
    1.7186192981224779,
    1.7562521603732995,
    1.7947090750031072,
    1.8340080864093424,
    1.8741676341103,
    1.9152065613971474,
    1.9571441241754002
}};
inline double accurateLog376(double x)noexcept{
    std::uint64_t bits=0;std::memcpy(&bits,&x,8);
    const int rawExponent=int((bits>>52)&2047);
    if(!(x>0)||rawExponent==0||rawExponent==2047)return std::log(x);
    const int exponent=rawExponent-1023,index=int((bits>>46)&63);
    bits=(bits&((1ull<<52)-1))|(1023ull<<52);double mantissa=0;std::memcpy(&mantissa,&bits,8);
    const double center=1+(index+.5)/64,r=(mantissa-center)/(mantissa+center),r2=r*r;
    const double polynomial=r*(2+r2*(2./3+r2*(2./5+r2*(2./7))));
    return ((exponent*functionLn2Hi376+logarithmCenters376[index])+polynomial)+exponent*functionLn2Lo376;
}
inline double accurateLog1p376(double x)noexcept{
    if(!(x>=1e-4&&std::isfinite(x)))return std::log1p(x);
    const double sum=1+x,error=(sum-1)-x;
    return accurateLog376(sum)-error/sum;
}
inline double accurateExp376(double x)noexcept{
    if(!(x>=-700&&x<=700))return std::exp(x);
    const int k=int(x*(32/0.693147180559945309417232121458176568)+(x<0?-.5:.5));
    int exponent=k/32,index=k-exponent*32;if(index<0){index+=32;--exponent;}
    const double r=(x-k*(functionLn2Hi376/32))-k*(functionLn2Lo376/32);
    const double polynomial=1+r*(1+r*(.5+r*(1./6+r*(1./24+r*(1./120+r*(1./720+r*(1./5040+r*(1./40320))))))));
    return exactScale376(exponentialCenters376[index]*polynomial,exponent);
}
inline double accuratePowPositive376(double x,double power)noexcept{
    return x>0&&std::isfinite(x)?accurateExp376(power*accurateLog376(x)):std::pow(x,power);
}
}
