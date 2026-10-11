#include "../src/PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <complex>
#include <fstream>
#include <iostream>
#include <vector>
using namespace channel376;
#include "PluginVoiceCompare376.h"
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;if(argc>3&&std::string(argv[1])=="--voice")return compareVoice376(argv[2],argv[3],argc>4?std::atof(argv[4]):0);std::ofstream file;if(argc>1)file.open(argv[1]);std::ostream& out=argc>1?file:std::cout;
 constexpr int fs=48000,count=48000;const double pi=6.283185307179586;
 out<<"drive,input_peak_dBFS,active,model,output_rms_dBFS,H2_percent,H3_percent,matched_delta_dBr,EQ_clip,raw_choice\n";
 for(double drive:{0.,15.})for(double level:{-36.,-24.,-12.})for(bool active:{false,true}){
  std::vector<double> baseline;double baseEnergy=0;
  for(int m=0;m<3;++m){Channel376Processor p;p.setProcessingPrecision(juce::AudioProcessor::doublePrecision);
   auto set=[&](const char* id,float v){auto* q=p.parameters.getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(v));};
   set("drive",float(drive));set("output",-15);if(active){set("ratio",.7f);set("threshold",-20);set("overEasy",1);set("deAmount",5);}
   p.prepareToPlay(fs,256);set("tubeModel",float(m));juce::MidiBuffer midi;juce::AudioBuffer<double> b(2,256);std::vector<double> output;
   for(int n=0;n<count*2;n+=256){int size=std::min(256,count*2-n);b.setSize(2,size,false,false,true);
    for(int j=0;j<size;++j){const double x=std::pow(10.,level/20)*std::sin(pi*1000*(n+j)/fs);b.setSample(0,j,x);b.setSample(1,j,x);}
    p.processBlock(b,midi);for(int j=0;j<size;++j)if(n+j>=count)output.push_back(b.getSample(0,j));
   }
   double e=0,dot=0;std::array<std::complex<double>,3> h{};
   for(int n=0;n<count;++n){e+=output[n]*output[n];if(m)dot+=output[n]*baseline[n];for(int j=0;j<3;++j){double phase=pi*1000*n*(j+1)/fs;h[j]+=output[n]*std::complex<double>(std::cos(phase),std::sin(phase));}}
   double delta=0;if(m){const double match=dot/e;for(int n=0;n<count;++n)delta+=std::pow(output[n]*match-baseline[n],2);}else{baseline=output;baseEnergy=e;}
   if(p.parameters.getRawParameterValue("tubeModel")->load()!=m||!std::isfinite(e))return 2;
   // Approved tonal characters must produce different, finite audio responses.
   if(drive==0&&level==-12&&!active&&m>0&&delta/baseEnergy<1e-12)return 4;
   if(!std::isfinite(std::abs(h[1]/h[0]))||!std::isfinite(std::abs(h[2]/h[0])))return 5;
   out<<drive<<','<<level<<','<<active<<','<<m<<','<<10*std::log10(e/count)<<','<<100*std::abs(h[1]/h[0])<<','<<100*std::abs(h[2]/h[0])<<','<<(delta>0?10*std::log10(delta/baseEnergy):-150)<<','<<p.meters[3].load()<<','<<p.parameters.getRawParameterValue("tubeModel")->load()<<'\n';
  }
 }
 return 0;}
