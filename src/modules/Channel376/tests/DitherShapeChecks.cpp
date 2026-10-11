#include "DitherShape376.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <cstdlib>
using namespace channel376;
static void require(bool value,const char* what){if(!value){std::cerr<<what<<'\n';std::exit(1);}}
int main(int argc,char** argv) {
    require(roundScaled({1,std::ldexp(1.,-53)},0,StreamPrecision::float64)==1,"double even tie");
    require(roundScaled({1,std::ldexp(3.,-53)},0,StreamPrecision::float64)==
        std::nextafter(std::nextafter(1.,2.),2.),"double odd tie");
    require(roundScaled({0,std::ldexp(1.,-53)},-1022,StreamPrecision::float64)==0,"subnormal tie");
    require(roundScaled({0,std::ldexp(.75,-52)},-1022,StreamPrecision::float64)==
        std::numeric_limits<double>::denorm_min(),"subnormal increment");
    if(argc>1) {
        std::ifstream file(argv[1]);std::string line;DitherShape376 generator;
        generator.prepare(48000,StreamPrecision::float64,0x091110);int count=0;
        while(std::getline(file,line)) {
            std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);
            double index,random,ignored,capture;row>>index>>random>>ignored>>capture;
            if(capture>=std::ldexp(1.,55))capture-=std::ldexp(1.,56);
            const double expected=capture/std::ldexp(1.,47)*std::ldexp(1.,15);
            require(generator.unitNoise(DitherMode::snr2)==expected,"SNR2 firmware capture mismatch");++count;
        }
        require(count==256,"256 firmware samples");
    }
    int cases=0;bool changed64=false;
    for(double rate:{8000.,32000.,44100.,48000.,88200.,96000.,192000.,384000.})
        for(auto precision:{StreamPrecision::float32,StreamPrecision::float64})
            for(int d=0;d<3;++d)for(int s=0;s<3;++s) {
                DitherShape376 a,b;a.prepare(rate,precision);b.prepare(rate,precision);
                a.modes(DitherMode(d),ShapeMode(s));b.modes(DitherMode(d),ShapeMode(s));
                for(int n=0;n<8192;++n) {
                    double input=n<4096?.125+.0625*std::sin(n*.13):0;
                    if(n==1024)input=2.;if(n==1025)input=-2.;
                    if(n==2048)input=std::numeric_limits<double>::denorm_min();
                    const double x=a.process(input),y=b.process(input);
                    require(std::isfinite(x)&&x==y,"finite deterministic sample processing");
                    if(d==0&&s==0)require(x==(precision==StreamPrecision::float32?double(float(input)):input),"off bypass");
                    if(precision==StreamPrecision::float64&&d!=0&&n<4096&&x!=input)changed64=true;
                    require(std::abs(x-input)<1e-4,"bounded float feedback/noise");
                }
                if(rate==44100||rate==48000||rate==88200||rate==96000) {
                    require(a.usesOriginalTable()&&a.tapCount()==24,"original rates retained");
                    for(int mode=1;mode<=2;++mode) {
                        int r=0;while(shapeRates[r]!=rate)++r;
                        for(int k=0;k<24;++k)require(a.table(ShapeMode(mode))[k]==shapeTables[2*r+mode-1][k],"table exact");
                    }
                }
                ++cases;
            }
    require(changed64,"float64 dither survives compensated rounding");
    // Block size and independent channel seeds.
    DitherShape376 a,b,c;a.prepare(48000,StreamPrecision::float32,17);
    b.prepare(48000,StreamPrecision::float32,17);c.prepare(48000,StreamPrecision::float32,31);
    a.modes(DitherMode::tpdf,ShapeMode::shape2);b.modes(DitherMode::tpdf,ShapeMode::shape2);
    c.modes(DitherMode::tpdf,ShapeMode::shape2);
    std::vector<float> x(8192,.25f),y=x,z=x;
    a.processBlock(x.data(),int(x.size()));
    for(int offset=0;offset<int(y.size());){const int count=std::min(127,int(y.size())-offset);b.processBlock(y.data()+offset,count);offset+=count;}
    c.processBlock(z.data(),int(z.size()));require(x==y&&x!=z,"block independence / independent channels");
    std::cout<<"PASS "<<cases<<" mode/rate/precision cases, firmware capture, wide rounding, blocks and stereo seeds\n";
}
