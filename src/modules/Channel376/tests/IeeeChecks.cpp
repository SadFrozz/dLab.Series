#include "IeeeMath376.h"
#include <iostream>
#include <limits>
using namespace channel376;
bool equal(double a,double b){if(std::isnan(a)&&std::isnan(b))return true;std::uint64_t x=0,y=0;std::memcpy(&x,&a,8);std::memcpy(&y,&b,8);return x==y;}
int main(){std::uint64_t errors=0;
    for(int n=0;n<200000;++n){const double a=std::ldexp(std::sin(n*.173),(n%2040)-1020),b=std::ldexp(std::cos(n*.137),(n*17%2000)-1000),c=std::ldexp(std::sin(n*.371),(n*31%2000)-1000);
        errors+=!equal(exactFma376(a,b,c),std::fma(a,b,c));
        const int shift=n%4000-2000;errors+=!equal(exactScale376(a,shift),std::ldexp(a,shift));
    }
    for(double a:{0.,-0.,std::numeric_limits<double>::denorm_min(),-std::numeric_limits<double>::denorm_min(),std::numeric_limits<double>::max()})for(int e:{-2048,-1,0,1,2048})errors+=!equal(exactScale376(a,e),std::ldexp(a,e));
    std::cout<<"IEEE exact helper mismatches="<<errors<<'\n';return errors?1:0;
}
