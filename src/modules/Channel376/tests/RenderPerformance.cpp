#include "AnalogEngine376.h"
#include <chrono>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>
int main(int argc,char** argv){using namespace channel376;
    constexpr int rate=48000,blockSize=256,repetitions=3;
    const int audioSeconds=argc>3?std::clamp(std::atoi(argv[3]),1,60):1;
    const double requestedMinimum=argc>4?std::atof(argv[4]):15.;
    const double minimumSpeed=std::isfinite(requestedMinimum)&&requestedMinimum>0?requestedMinimum:15.;
    const int sampleCount=rate*audioSeconds;
    std::vector<std::array<double,2>> pcm(sampleCount);
    for(int n=0;n<sampleCount;++n)pcm[n]={.15*std::sin(n*.131)+.02*std::sin(n*.76),.1*std::sin(n*.173)};
    AnalogParameters376 p;p.values={8,5,-4,1800,6,-20,.7,5000,5,-3};
    const int requestedFactor=argc>2?std::atoi(argv[2]):1;
    if(requestedFactor!=1&&requestedFactor!=2&&requestedFactor!=4)return 3;
    p.oversampling=requestedFactor==4?OversamplingMode::x4:requestedFactor==2?OversamplingMode::x2:OversamplingMode::off;
    p.switches.overEasy=true;p.switches.slow=false;p.digital={DitherMode::tpdf,ShapeMode::shape1};
    if(argc>5&&std::atoi(argv[5])==0)p.digital.shape=ShapeMode::off;
#if defined(CHANNEL376_PCM_OUTPUT_ONLY)
    p.digital={};
#endif
    auto engine=std::make_unique<AnalogEngine376>();engine->prepare(rate,2,StreamPrecision::float64,p);
    double sum=0;for(int n=0;n<rate;++n){const auto y=engine->process(pcm[n]);sum+=y[0]+y[1];}
    std::array<double,repetitions> times{};double maxBlockMs=0;
    for(int run=0;run<repetitions;++run){const auto start=std::chrono::steady_clock::now();
        for(int offset=0;offset<sampleCount;offset+=blockSize){const auto block=std::chrono::steady_clock::now();
            for(int n=offset;n<std::min(sampleCount,offset+blockSize);++n){const auto y=engine->process(pcm[n]);sum+=y[0]+y[1];}
            maxBlockMs=std::max(maxBlockMs,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-block).count());}
        times[run]=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();}
    const double worst=*std::max_element(times.begin(),times.end()),speed=audioSeconds/worst;
    const bool pass=speed>=minimumSpeed&&engine->failedSolves()==0&&std::isfinite(sum);
    std::ostream* output=&std::cout;std::ofstream file;if(argc>1){file.open(argv[1]);if(!file)return 3;output=&file;}
    *output<<"{\n  \"status\": \""<<(pass?"PASS":"FAIL")<<"\",\n"
        <<"  \"sampleRate\": 48000, \"channels\": 2, \"precision\": \"float64\", \"mode\": \"FAST\",\n"
        <<"  \"oversamplingFactor\": "<<engine->oversamplingFactor()<<", \"latencySamples\": "<<engine->latency()<<",\n"
#if defined(CHANNEL376_PCM_OUTPUT_ONLY)
        <<"  \"model\": \"current product analog core, EH coloration; PCM only\",\n"
#elif defined(CHANNEL376_EXPERIMENTAL_TUBE_COLOUR)
        <<"  \"model\": \"EXPERIMENTAL EH tube coloration, sch5 manifold and reduced output; original Shape; not installed plugin\",\n"
#elif defined(CHANNEL376_EXPERIMENTAL_MANIFOLD)
        <<"  \"model\": \"EXPERIMENTAL sch5 manifold and reduced output; original Shape; not installed plugin\",\n"
#else
        <<"  \"model\": \"default circuit core\",\n"
#endif
#if defined(CHANNEL376_BENCH_HASWELL)
        <<"  \"cpuTarget\": \"x86_64 Haswell or newer; ARM default\",\n"
#endif
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_EQ)
        <<"  \"reducedEq\": true,\n"
#endif
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_VCA)
        <<"  \"reducedVca\": true,\n"
#endif
#if defined(CHANNEL376_EXPERIMENTAL_REDUCED_FRONT)
        <<"  \"reducedFront\": true,\n"
#endif
#if defined(CHANNEL376_EXPERIMENTAL_STEREO_SHAPE)
        <<"  \"stereoShape\": true,\n"
#endif
#if defined(CHANNEL376_EXPERIMENTAL_STEREO_VCA)
        <<"  \"sharedStereoVcaControl\": true,\n"
#endif
#if defined(CHANNEL376_EXPERIMENTAL_DIODE_TABLE)
        <<"  \"diodeLogTable\": true,\n"
#endif
#if defined(CHANNEL376_EXPERIMENTAL_ROUND)
        <<"  \"fastIeeeRoundAndZeroTail\": true,\n"
#endif
#if defined(CHANNEL376_EXPERIMENTAL_WIDE_SIMD)
        <<"  \"wideSimdRequested\": true,\n"
#endif
#if defined(CHANNEL376_BENCH_PRECISE)
        <<"  \"compilerMath\": \"MSVC precise, Ob3; no fast-math\",\n"
#else
        <<"  \"compilerMath\": \"strict, no automatic FP contraction\",\n"
#endif
#if defined(CHANNEL376_PCM_OUTPUT_ONLY)
        <<"  \"processing\": \"tube, EQ, OverEasy compressor, de-esser, PCM output; no Dither/Shape\",\n"
#else
        <<"  \"processing\": \"tube, EQ, OverEasy compressor, de-esser, TPDF, "<<(p.digital.shape==ShapeMode::off?"Shape OFF":"Shape1")<<"\",\n"
#endif
        <<"  \"scope\": \"DSP core, precomputed PCM, one second warmup, three trials; DAW render unverified\",\n"
        <<"  \"audioSecondsPerTrial\": "<<audioSeconds<<",\n"
        <<"  \"minimumSpeed\": "<<minimumSpeed<<", \"measuredMinimumSpeed\": "<<speed<<",\n"
        <<"  \"secondsPerAudioSecond\": ["<<times[0]/audioSeconds<<','<<times[1]/audioSeconds<<','<<times[2]/audioSeconds<<"],\n"
        <<"  \"maximum256SampleBlockMs\": "<<maxBlockMs<<", \"failedSolves\": "<<engine->failedSolves()<<"\n}\n";
    if(argc>1)std::cout<<(pass?"PASS":"FAIL")<<" render requirement: "<<speed<<"x / "<<minimumSpeed<<"x\n";
    return pass?0:2;
}
