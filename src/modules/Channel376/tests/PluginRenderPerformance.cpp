#include "../src/PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;Channel376Processor p;
    const double values[]{8,5,-4,1800,6,-20,.7,5000,5,-3};
    for(int j=0;j<10;++j){auto* parameter=p.parameters.getParameter(channel376::continuous[j].id.data());parameter->setValueNotifyingHost(parameter->convertTo0to1(float(values[j])));}
    if(argc>2)p.parameters.getParameter("tubeModel")->setValueNotifyingHost(p.parameters.getParameter("tubeModel")->convertTo0to1(float(std::atoi(argv[2]))));
    p.parameters.getParameter("overEasy")->setValueNotifyingHost(1);p.setProcessingPrecision(juce::AudioProcessor::doublePrecision);p.prepareToPlay(48000,256);
    constexpr int rate=48000,seconds=30,count=rate*seconds;
    std::vector<std::array<double,2>> pcm(count);for(int n=0;n<count;++n)pcm[n]={.15*std::sin(n*.131)+.02*std::sin(n*.76),.1*std::sin(n*.173)};
    juce::MidiBuffer midi;juce::AudioBuffer<double> buffer(2,256);double checksum=0;
    auto run=[&](int samples){for(int offset=0;offset<samples;offset+=256){const int size=std::min(256,samples-offset);buffer.setSize(2,size,false,false,true);
        for(int n=0;n<size;++n){buffer.setSample(0,n,pcm[offset+n][0]);buffer.setSample(1,n,pcm[offset+n][1]);}
        p.processBlock(buffer,midi);checksum+=buffer.getSample(0,size-1)+buffer.getSample(1,size-1);
    }};
    run(rate);std::array<double,3> times{};for(auto& t:times){const auto start=std::chrono::steady_clock::now();run(count);t=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();}
    const double speed=seconds/ *std::max_element(times.begin(),times.end());const bool pass=speed>=10&&std::isfinite(checksum);
    std::ofstream file;if(argc>1)file.open(argv[1]);std::ostream& out=argc>1?file:std::cout;
    out<<"{\n  \"status\": \""<<(pass?"PASS":"FAIL")<<"\",\n  \"scope\": \"native JUCE processor, input copying and block callbacks included; DAW render unverified\",\n"
        <<"  \"sampleRate\": 48000, \"channels\": 2, \"precision\": \"float64\", \"mode\": \"FAST\",\n"
        <<"  \"processing\": \"tube, EQ, OverEasy, de-esser; PCM only\",\n"
        <<"  \"oversamplingFactor\": 1, \"minimumSpeed\": 10, \"measuredMinimumSpeed\": "<<speed<<",\n"
        <<"  \"audioSecondsPerTrial\": 30, \"trialSeconds\": ["<<times[0]<<','<<times[1]<<','<<times[2]<<"],\n"
        <<"  \"latencySamples\": "<<p.getLatencySamples()<<", \"ownParameters\": "<<p.getParameters().size()<<", \"tubeModel\": "<<p.parameters.getRawParameterValue("tubeModel")->load()<<"\n}\n";
    std::cout<<(pass?"PASS":"FAIL")<<" native processor "<<speed<<"x / 10x\n";return pass?0:2;
}
