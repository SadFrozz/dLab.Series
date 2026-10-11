#include "AccurateFunctions376.h"
#include <iostream>
#include <limits>
#include <cstdint>
using namespace channel376;
int main(){double logError=0,expError=0,log1pError=0,powError=0;
    for(int n=0;n<=300000;++n){const double x=-700+1400.*n/300000;
        const double e=std::exp(x),actual=accurateExp376(x);expError=std::max(expError,std::abs(actual/e-1));
        const double l=std::log(e);logError=std::max(logError,std::abs(accurateLog376(e)-l)/std::max(1.,std::abs(l)));
        const double z=std::exp(-30+60.*n/300000),lp=std::log1p(z);
        log1pError=std::max(log1pError,std::abs(accurateLog1p376(z)-lp)/std::max(1.,std::abs(lp)));
        const double input=std::exp(-20+25.*n/300000),p=1.4185942473368784;
        powError=std::max(powError,std::abs(accuratePowPositive376(input,p)/std::pow(input,p)-1));}
    for(int bin=0;bin<64;++bin){const double edge=1+bin/64.;
        for(double x:{std::nextafter(edge,0.),edge,std::nextafter(edge,2.)})logError=std::max(logError,std::abs(accurateLog376(x)-std::log(x)));}
    const double subnormal=std::numeric_limits<double>::denorm_min();
    if(accurateLog376(subnormal)!=std::log(subnormal)||!std::isinf(accurateLog376(0))||!std::isnan(accurateLog376(-1))||!std::isnan(accurateExp376(std::numeric_limits<double>::quiet_NaN())))return 1;
    std::cout<<"log_normalized_error="<<logError<<" exp_relative_error="<<expError<<" log1p_normalized_error="<<log1pError<<" pow_relative_error="<<powError<<'\n';
    return logError<1e-14&&expError<1e-14&&log1pError<1e-14&&powError<2e-14?0:1;
}
