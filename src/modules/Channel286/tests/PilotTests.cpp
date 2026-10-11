#include "dsp/Engine.h"
#include "PilotTransientReference.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>

int main(int argc,char** argv) {
    try {
        std::ofstream evidence;
        if(argc>1) {evidence.open(argv[1]);if(!evidence) throw std::runtime_error("Cannot write pilot evidence");}
        if(evidence) evidence<<"model,input_peak_dBFS,output_peak_equivalent_dBFS,GR_dB\n";
        double maxError=0,maxPostError=0;
        for(bool modern:{false,true}) for(double rate:{48000.,96000.}) for(double density:{0.,5.,10.}) {
            c286::PilotCircuit circuit;circuit.prepare(rate,modern);
            for(int step=1;step<=384;++step) {
                circuit.tick((step-1)%32<8,5,density);
                for(const auto& ref:pilotReferences) if(ref.modern==modern && ref.rate==rate && ref.density==density && ref.step==step) {
                    const auto nodes=circuit.timingVoltages();
                    for(size_t n=0;n<4;++n) maxError=std::max(maxError,std::abs(nodes[n]-ref.node[n]));
                    maxPostError=std::max(maxPostError,std::abs(circuit.postVoltage()-ref.post));
                }
            }
            const auto drive=circuit.driveContribution(5);
            if(std::abs(drive.makeupDb-(modern?17.949254373202:19.136631356286))>1.e-6)
                throw std::runtime_error("Drive disagrees with independent SPICE-checked reference");
        }
        std::cout<<"60 independent four-node timing vectors, max error V="<<maxError<<'\n';
        if(maxError>2.e-5) throw std::runtime_error("Timing states disagree with dense nodal solve");
        std::cout<<"60 independent nonlinear post-CV vectors, max error V="<<maxPostError<<'\n';
        if(maxPostError>2.e-4) throw std::runtime_error("Post-CV disagrees with independent bridge/capacitor solve");
        constexpr double pi=3.14159265358979323846;
        for(bool modern:{false,true}) {
            double previousOutput=-200;
            for(double inputDb:{-60.,-40.,-20.,-6.}) {
                c286::Engine engine;engine.prepare(48000,1);
                c286::Parameters p;p.modelS=modern;p.drive=5;p.density=5;engine.setParameters(p);
                const double amplitude=std::pow(10.,inputDb/20);
                std::vector<float> block(256);double power=0;int measured=0;
                c286::Meters meter;
                for(int offset=0;offset<144000;offset+=256) {
                    const int count=std::min(256,144000-offset);
                    for(int i=0;i<count;++i) block[i]=static_cast<float>(amplitude*std::sin(2*pi*1000*(offset+i)/48000));
                    float* ptr=block.data();meter=engine.process(&ptr,1,count);
                    if(offset>=96000) for(int i=0;i<count;++i) {power+=block[i]*block[i];++measured;}
                }
                const double outputDb=20*std::log10(std::sqrt(2*power/measured));
                std::cout<<(modern?"286s":"286")<<" input="<<inputDb<<" output="<<outputDb<<" GR="<<meter.compressorReductionDb<<'\n';
                if(evidence) evidence<<(modern?"286s":"286")<<','<<inputDb<<','<<outputDb<<','<<meter.compressorReductionDb<<'\n';
                if(outputDb<=previousOutput) throw std::runtime_error("Output curve must remain monotonic");
                if(inputDb==-60 && std::abs(outputDb-inputDb-(modern?17.949254:19.136631))>.05)
                    throw std::runtime_error("Quiet gain must follow the loaded Drive law");
                previousOutput=outputDb;
            }
            auto transient=[&](double rate,double density) {
                c286::Engine engine;engine.prepare(rate,1);
                c286::Parameters p;p.modelS=modern;p.drive=5;p.density=static_cast<float>(density);engine.setParameters(p);
                std::vector<float> block(256);c286::Meters meter;std::array<float,3> gr{};
                const int toneCount=static_cast<int>(2*rate),end=toneCount+static_cast<int>(2*rate);
                bool captured=false;
                for(int offset=0;offset<end;) {
                    int count=std::min(256,end-offset);
                    if(offset<toneCount) count=std::min(count,toneCount-offset);
                    for(int i=0;i<count;++i) block[i]=offset<toneCount?.3f*static_cast<float>(std::sin(2*pi*997*(offset+i)/rate)):0.f;
                    float* ptr=block.data();meter=engine.process(&ptr,1,count);offset+=count;
                    if(offset==toneCount) gr[0]=meter.compressorReductionDb;
                    if(!captured && offset>=toneCount+static_cast<int>(.2*rate)) {gr[1]=meter.compressorReductionDb;captured=true;}
                }
                gr[2]=meter.compressorReductionDb;
                return gr;
            };
            const auto slow=transient(48000,1),fast=transient(48000,9);
            std::cout<<(modern?"286s":"286")<<" release GR slow="<<slow[0]<<','<<slow[1]<<','<<slow[2]
                <<" fast="<<fast[0]<<','<<fast[1]<<','<<fast[2]<<'\n';
            if(fast[1]/fast[0]>=slow[1]/slow[0] || fast[2]>slow[2])
                throw std::runtime_error("Density must accelerate recovery after the tone");
            const auto lowRate=transient(44100,5),highRate=transient(96000,5);
            std::cout<<"Rate convergence GR difference="<<std::abs(lowRate[0]-highRate[0])<<'\n';
            if(std::abs(lowRate[0]-highRate[0])>.3f || std::abs(lowRate[1]-highRate[1])>.3f)
                throw std::runtime_error("Pilot dynamics must converge across sample rates");
        }
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
