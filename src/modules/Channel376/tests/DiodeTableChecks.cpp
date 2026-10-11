#include "DiodeLogTable376.h"
#include <chrono>
#include <iostream>
#include <limits>
using namespace channel376;
volatile double diodeSink=0;
int main(){double maximum=0;for(int n=0;n<=500000;++n){const double x=std::pow(10.,-18+30.*n/500000);maximum=std::max(maximum,std::abs(diodeLogTable376.evaluate(x)-std::log1p(x)));}
    std::cout<<"max_log_error="<<maximum<<" max_junction_V_error="<<maximum*1.90777*.02586417*(298.15/300.15)<<'\n';
    if(maximum>5e-12)return 1;
    for(double x:{0.,1.,std::numeric_limits<double>::infinity()})if(diodeLogTable376.evaluate(x)!=std::log1p(x))return 1;
    return 0;
}
