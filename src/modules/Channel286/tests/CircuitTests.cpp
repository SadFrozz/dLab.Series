#include "dsp/Engine.h"
#include "LfReference.h"
#include "DetectorReference.h"
#include "ControlReference.h"
#include "ControlTransientReference.h"
#include "dsp/ControlDevices.h"
#include <algorithm>
#include <complex>
#include <iostream>
#include <stdexcept>
#include <vector>

int main() {
    constexpr double pi=3.14159265358979323846;
    double largestError=0;
    try {
        for(const auto& ref:controlReferences) {
            const auto circuit=c286::DeCvCircuit::forModel(ref.modern);
            const double amplifier=-circuit.feedbackR*circuit.inputCurrent(ref.broad,ref.high,ref.position);
            if(std::abs(amplifier-ref.amplifier)>1.e-10 || std::abs(circuit.potVoltage(ref.position)-ref.pot)>1.e-10)
                throw std::runtime_error("Control DC voltages disagree with independent nodal solve");
            if(std::abs(circuit.outputReduction(amplifier)-ref.reduction)>1.e-7)
                throw std::runtime_error("Output diode CV disagrees with independent bisection");
        }
        std::cout<<"50 independent control DC/diode vectors passed\n";
        for(const auto& ref:controlTransientReferences) {
            const auto circuit=c286::DeCvCircuit::forModel(ref.modern);
            const double voltage=circuit.tick(ref.previous,ref.current,1/ref.rate,ref.modern);
            if(std::abs(voltage-ref.voltage)>1.e-9) {
                std::cerr<<"Feedback model="<<ref.modern<<", sr="<<ref.rate<<", prev="<<ref.previous
                         <<", current="<<ref.current<<", voltage="<<voltage<<", expected="<<ref.voltage<<"\n";
                throw std::runtime_error("Capacitor/feedback diode step disagrees with independent KCL solve");
            }
        }
        std::cout<<"48 independent capacitor/feedback diode vectors passed\n";
        for(const auto& ref:lfReferences) {
            c286::Engine engine; engine.prepare(ref.rate,1);
            c286::Parameters p; p.modelS=ref.modern; p.lfDetail=ref.detail;
            engine.setParameters(p);
            // Low-level tone: no clipping, compressor, de-esser, gate or HF Detail.
            // 1s settling then 0.25s integral periods at 80/250Hz.
            const int settle=static_cast<int>(ref.rate);
            const int count=static_cast<int>(ref.rate/4);
            std::complex<double> projection{};
            std::vector<float> buffer(256);
            for(int offset=0;offset<settle+count;) {
                const int n=std::min(256,settle+count-offset);
                for(int i=0;i<n;++i) buffer[i]=.01f*static_cast<float>(std::cos(2*pi*ref.frequency*(offset+i)/ref.rate));
                float* ptr=buffer.data(); engine.process(&ptr,1,n);
                for(int i=0;i<n;++i) if(offset+i>=settle) {
                    const double phase=-2*pi*ref.frequency*(offset+i)/ref.rate;
                    projection+=static_cast<double>(buffer[i])*std::polar(1.,phase);
                }
                offset+=n;
            }
            projection*=2/(count*.01);
            const std::complex<double> expected(ref.real,ref.imag);
            const double error=std::abs(projection-expected)/std::abs(expected);
            largestError=std::max(largestError,error);
            if(error>2.e-4) {
                std::cerr<<"Model "<<ref.modern<<", "<<ref.rate<<" Hz, tone "<<ref.frequency
                         <<", LF "<<ref.detail<<": "<<projection<<" expected "<<expected<<"\n";
                throw std::runtime_error("DSP disagrees with independent LF nodal solve");
            }
        }
        std::cout<<"32 LF complex response vectors passed; maximum relative error "<<largestError<<"\n";
        double largestDetectorError=0;
        for(const auto& ref:detectorReferences) {
            c286::Engine engine; engine.prepare(ref.rate,1);
            c286::Parameters p; p.modelS=ref.modern; p.deFrequency=4000; p.deThreshold=5;
            engine.setParameters(p);
            std::vector<float> buffer(256);
            c286::Meters meters;
            const int count=static_cast<int>(ref.rate*2);
            for(int offset=0;offset<count;) {
                const int n=std::min(256,count-offset);
                for(int i=0;i<n;++i) buffer[i]=.01f*static_cast<float>(std::cos(2*pi*ref.frequency*(offset+i)/ref.rate));
                float* ptr=buffer.data(); meters=engine.process(&ptr,1,n); offset+=n;
            }
            const double error=std::abs(meters.deEsserReductionDb-ref.reduction);
            largestDetectorError=std::max(largestDetectorError,error);
            // The analytic reference is the infinite-time RMS ratio. Finite
            // detector averaging leaves ripple; asymmetric CV following adds
            // a small positive GR bias. Bound that effect to 0.05 dB.
            if(error>.05) {
                std::cerr<<"Model "<<ref.modern<<", "<<ref.rate<<" Hz, tone "<<ref.frequency
                         <<": GR "<<meters.deEsserReductionDb<<" expected "<<ref.reduction<<"\n";
                throw std::runtime_error("De-esser response disagrees with independent detector nodal solve");
            }
        }
        std::cout<<"12 detector steady-state vectors passed; maximum GR error "<<largestDetectorError<<" dB\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<"\n"; return 1; }
}
