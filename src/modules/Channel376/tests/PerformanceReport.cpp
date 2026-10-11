#include "AnalogEngine376.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
int main(){using namespace channel376;auto engine=std::make_unique<AnalogEngine376>();double sum=0;
    std::cout<<"channels,precision,active,seconds_per_audio_second,max_256_sample_block_ms,failed_solves,comp_evaluations\n";
    for(int precision:{32,64})for(int channels:{1,2})for(bool active:{false,true}){AnalogParameters376 p;if(active){p.values={8,5,-4,1800,6,-20,.7,5000,5,-3};p.switches.overEasy=true;p.digital={DitherMode::tpdf,ShapeMode::shape1};}
        engine->prepare(48000,channels,precision==64?StreamPrecision::float64:StreamPrecision::float32,p);double maximum=0;const auto start=std::chrono::steady_clock::now();
        for(int offset=0;offset<48000;offset+=256){auto block=std::chrono::steady_clock::now();for(int n=offset;n<std::min(48000,offset+256);++n){auto y=engine->process({.15*std::sin(n*.131)+.02*std::sin(n*.76),channels==2?.1*std::sin(n*.173):0});sum+=y[0]+y[1];}
            maximum=std::max(maximum,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-block).count());}
        const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();std::cout<<channels<<','<<precision<<','<<active<<','<<seconds<<','<<maximum<<','<<engine->failedSolves()<<','<<engine->channelState(0).compressorEvaluations()<<'\n';if(engine->failedSolves()!=0)return 1;
    }return std::isfinite(sum)?0:1;}
