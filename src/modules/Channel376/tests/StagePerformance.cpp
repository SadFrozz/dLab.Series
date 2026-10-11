#include "AnalogEngine376.h"
#include <chrono>
#include <iostream>
#include <vector>
using namespace channel376;
volatile double sink=0;
template<class F>void bench(const char* name,int count,F&& f){const auto start=std::chrono::steady_clock::now();double sum=0;for(int n=0;n<count;++n)sum+=f(n);sink=sum;std::cout<<name<<','<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<'\n';}
int main(int argc,char** argv){const double fs=argc>1?std::atof(argv[1]):48000;const int n=int(fs);
    std::vector<double> pcm(std::max(n,48000)),compPcm(48000),digitalPcm(48000);
    for(int j=0;j<int(pcm.size());++j)pcm[j]=.3*std::sin(j*.031);
    for(int j=0;j<48000;++j){compPcm[j]=std::sin(j*.071);digitalPcm[j]=.3*std::sin(j*.131);}
    LineInput376 line;line.prepare(fs);bench("LINE",n,[&](int i){return line.tick(pcm[i]);});
    LineSource376 source;source.prepare(fs);bench("source",n,[&](int i){return source.tick(pcm[i]);});
    DriveCircuit376 drive;drive.prepare(fs,8);bench("drive",n,[&](int i){return drive.tick(pcm[i]);});
    LowCutCircuit376 cut;cut.prepare(fs);bench("lowCut",n,[&](int i){return cut.tick(pcm[i]);});
    TubeCircuit376 tube;tube.prepare(fs);bench("tube",n,[&](int i){return tube.tick(pcm[i]);});
#if defined(CHANNEL376_EXPERIMENTAL_TUBE_COLOUR)
    ColourTubePath376 path;path.prepare(fs,AnalogChannel376::fullPathUnityTrim);
#else
    LoadedTubePath376 path;path.prepare(fs);
#endif
    path.setPhysicalEq(.7,.3,.8,1800);bench("tube+trim+EQ",n,[&](int i){return path.tick(pcm[i]);});
    LoadedEq376 eq;eq.prepare(fs);eq.setPhysical(.7,.3,.8,1800);bench("EQ",n,[&](int i){return eq.tick(pcm[i]);});
    LoadedHighFrequency376 hf;hf.prepare(fs);bench("HF",n,[&](int i){return hf.tick(pcm[i]);});
    InputMeter376 meter;meter.prepare(fs);bench("inputMeter",n,[&](int i){return meter.tick(pcm[i]);});
    BroadbandCurrent376 broad;broad.prepare(fs);RmsDetectorCircuit376 rms;rms.prepare(fs,634000);bench("broad+RMS",n,[&](int i){return rms.tick(broad.tick(pcm[i]));});
    DeCv376 de;de.prepare(fs);bench("deCV",n,[&](int i){return de.tick(.1,.1+.05*pcm[i]/.3,.7);});
#if defined(CHANNEL376_EXPERIMENTAL_MANIFOLD)
    CompressorManifold376 comp;
#else
    CompressorCircuit376 comp;
#endif
    comp.prepare(48000);bench("compressor",48000,[&](int i){return comp.tickTargetVoltage(-3-.03*compPcm[i],false);});
    MainVca376 vca;vca.prepare(fs);bench("VCA",n,[&](int i){return vca.tick(pcm[i],0,-1,-3);});
    OutputCircuit376 out;out.prepare(fs);bench("output",n,[&](int i){return out.tick(pcm[i]);});
    Oversample4x376 fir;bench("SRC",48000,[&](int i){return fir.process(digitalPcm[i],[](double x){return x;});});
    for(bool shape:{false,true}){DitherShape376 d;d.prepare(48000,StreamPrecision::float64);d.modes(DitherMode::tpdf,shape?ShapeMode::shape1:ShapeMode::off);bench(shape?"DitherShape64":"Dither64",48000,[&](int i){return d.process(digitalPcm[i]);});}
}
