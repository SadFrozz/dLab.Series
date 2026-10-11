#include "AnalogEngine376.h"
#include <fstream>
#include <iomanip>
#include <memory>
using namespace channel376;
int main(int argc,char** argv){if(argc!=2)return 2;std::ofstream out(argv[1]);if(!out)return 3;
    out<<"case,sample,left,right,comp,de,clip,failed\n"<<std::setprecision(17);
    auto engine=std::make_unique<AnalogEngine376>();
    for(int scenario=0;scenario<6;++scenario){AnalogParameters376 p;
        p.values={8,5,-4,1800,6,-20,.7,5000,5,-3};p.switches.overEasy=scenario%2==0;p.switches.slow=scenario>=3;
        if(scenario==2||scenario==5){p.values[0]=15;p.values[5]=-40;p.values[6]=1;p.values[9]=10;}
        p.digital={DitherMode::tpdf,ShapeMode::shape1};engine->prepare(48000,2,StreamPrecision::float64,p);
        for(int n=0;n<16384;++n){if(n==8192){p.switches.slow=!p.switches.slow;p.switches.phase=true;p.switches.lowCut=true;p.values[2]=4;engine->setParameters(p);}
            const double amplitude=n<6000?.15:n<11000?0:.3;
            const auto y=engine->process({amplitude*(std::sin(n*.131)+.15*std::sin(n*.76)),amplitude*.7*std::sin(n*.173)});
            const auto& channel=engine->channelState(0);
            out<<scenario<<','<<n<<','<<y[0]<<','<<y[1]<<','<<channel.gainReductionMeterVoltage()<<','<<channel.deCvVoltage()<<','<<channel.eqClip()<<','<<engine->failedSolves()<<'\n';}
    }
}
