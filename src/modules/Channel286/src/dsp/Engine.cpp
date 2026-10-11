#include "Engine.h"
#include "Calibration.h"
#include "InputGain.h"
#include "LfCircuit.h"
#include "DetectorCircuit.h"
#include "ControlDevices.h"
#include <algorithm>

namespace c286 {
namespace {
constexpr double pi=3.14159265358979323846;
float db(float x) noexcept { return 20.f*std::log10(std::max(x,1.e-10f)); }
float gain(float x) noexcept { return std::pow(10.f,x/20.f); }
float bounded(float x, float lo, float hi, float fallback) noexcept {
    return std::isfinite(x) ? std::clamp(x,lo,hi) : fallback;
}
float follow(float now, float target, float attack, float release) noexcept {
    const float c=target>now ? attack : release;
    return target+c*(now-target);
}
}
void Biquad::highPass(double rate,double hz,double q) {
    const double w=2*pi*std::min(hz,rate*.45)/rate, c=std::cos(w);
    const double alpha=std::sin(w)/(2*q), norm=1/(1+alpha);
    b0=(1+c)*.5*norm; b1=-(1+c)*norm; b2=b0;
    a1=-2*c*norm; a2=(1-alpha)*norm;
}
void Biquad::bandPass(double rate,double hz,double q) {
    const double w=2*pi*std::min(hz,rate*.45)/rate, c=std::cos(w);
    const double alpha=std::sin(w)/(2*q), norm=1/(1+alpha);
    b0=alpha*norm; b1=0; b2=-b0; a1=-2*c*norm; a2=(1-alpha)*norm;
}
void Biquad::peak(double rate,double hz,double q,double level) {
    const double w=2*pi*std::min(hz,rate*.45)/rate;
    const double a=std::pow(10.,level/40.), alpha=std::sin(w)/(2*q);
    const double norm=1/(1+alpha/a);
    b0=(1+alpha*a)*norm; b1=-2*std::cos(w)*norm; b2=(1-alpha*a)*norm;
    a1=b1; a2=(1-alpha/a)*norm;
}
void Biquad::analogue(double rate,const std::array<double,3>& n,const std::array<double,3>& d) {
    const double k=2*rate, k2=k*k;
    const double norm=1/(d[0]*k2+d[1]*k+d[2]);
    b0=(n[0]*k2+n[1]*k+n[2])*norm;
    b1=2*(n[2]-n[0]*k2)*norm;
    b2=(n[0]*k2-n[1]*k+n[2])*norm;
    a1=2*(d[2]-d[0]*k2)*norm;
    a2=(d[0]*k2-d[1]*k+d[2])*norm;
}
float Biquad::tick(float x) noexcept {
    const double y=b0*x+z1;
    z1=b1*x-a1*y+z2; z2=b2*x-a2*y;
    if(std::abs(z1)<1.e-25) z1=0;
    if(std::abs(z2)<1.e-25) z2=0;
    return static_cast<float>(y);
}
float Engine::coefficient(float seconds) const noexcept {
    return static_cast<float>(std::exp(-1/(sr*seconds)));
}
void Engine::prepare(double rate,int channels) {
    sr=std::isfinite(rate) ? std::clamp(rate,8000.,1536000.) : 48000.;
    preparedChannels=std::clamp(channels,1,2);
    smoothCoeff=coefficient(.02f); rmsCoeff=coefficient(.01f);
    // Sheet 1: C108/C109=.1uF, R108=10k, R109=40.2k, unity Sallen-Key.
    for(int model=0;model<2;++model) {
        const double r=model==0?40200.:39200.;
        const double hpHz=1/(2*pi*std::sqrt(10000.*r)*1.e-7),hpQ=.5*std::sqrt(r/10000.);
        const auto lf=LfCircuit::forModel(model==1);
        const auto detector=DetectorCircuit::forModel(model==1);
        const auto deCv=DeCvCircuit::forModel(model==1);
        profiles[model].pilot.prepare(sr*2,model==1);
        for(size_t i=0;i<profiles[model].deVoltageToDb.size();++i)
            profiles[model].deVoltageToDb[i]=static_cast<float>(deCv.outputReduction(-15.*i/4096.));
        for(auto& c:profiles[model].channels) {
            c.inputHp.highPass(sr,hpHz,hpQ);
            const double cc=model==1?1.e-6:3.3e-6,rb=model==1?49900.:47500.,rl=model==1?26040.:26700.;
            const double b2=rl*cc*rb*.047e-6;
            c.pilotDetector.analogue(sr*2,{b2,rl*cc,0},{b2,rb*.047e-6+cc*(rb+rl),1});
            c.pilotInput.analogue(sr*2,{0,20000*cc,0},{0,20000*cc,1});
            c.lf.analogue(sr,lf.numeratorA(),lf.denominatorA());
            c.lfCut.analogue(sr,lf.numeratorB(),lf.denominatorB());
            c.broadDetector.analogue(sr,detector.numerator(),detector.denominator());
            if(model==1) c.deCoupling.analogue(sr,{0,.068e-6*49900,0},{0,.068e-6*49900,1});
            else c.deCoupling.analogue(sr,{0,0,1},{0,0,1});
            // Original C140/R209; 286s C46/R105. Same ideal RC transfer.
            c.hfAudio.analogue(sr,{0,20000*2.2e-9,0},{0,20000*2.2e-9,1});
        }
    }
    reset();
    setParameters(params);
    controlsInitialised=false; // First host snapshot starts without a stale preset ramp.
}
void Engine::reset() noexcept {
    for(auto& p:profiles) {
        for(auto& c:p.channels) {
            c.inputHp.reset(); c.deHp.reset(); c.deCoupling.reset(); c.broadDetector.reset();
            c.hfAudio.reset(); c.lf.reset(); c.lfCut.reset();
            c.pilotInput.reset(); c.pilotDetector.reset(); c.previousPilotInput=0;
        }
        p.dePower=p.broadPower=p.comparisonPower=p.gatePower=p.compGr=p.deGr=p.gateGr=0;
        p.pilot.reset();
        p.deVoltage=0;
    }
    previousDeHz=-1; controlsInitialised=false;
}
void Engine::setParameters(const Parameters& p) noexcept {
    params=p;
    params.inputDb=effectiveLineGain(p.inputDb,p.modelS);
    params.drive=bounded(p.drive,0,10,0); params.density=bounded(p.density,0,10,5);
    params.deFrequency=bounded(p.deFrequency,800,10000,5000);
    params.deThreshold=bounded(p.deThreshold,0,10,0);
    params.lfDetail=bounded(p.lfDetail,0,10,0); params.hfDetail=bounded(p.hfDetail,0,10,0);
    params.gateThresholdDb=bounded(p.gateThresholdDb,-120,0,-45);
    params.gateRatio=bounded(p.gateRatio,1,10,1); params.outputDb=bounded(p.outputDb,-30,10,0);
    const std::array<float,10> values { params.inputDb,params.drive,params.density,
      params.deFrequency,params.deThreshold,params.lfDetail,params.hfDetail,
      params.gateThresholdDb,params.gateRatio,params.outputDb };
    for(size_t i=0;i<values.size();++i) {
        control[i].target=values[i];
        if(!controlsInitialised) control[i].value=values[i];
    }
    hpMix.target=params.highPass ? 1.f:0.f;
    wetMix.target=params.processBypass ? 0.f:1.f;
    modelMix.target=params.modelS ? 1.f:0.f;
    if(!controlsInitialised) { hpMix.value=hpMix.target; wetMix.value=wetMix.target; modelMix.value=modelMix.target; }
    controlsInitialised=true;
}
float Engine::railLimit(float sample) noexcept {
    // Approximate symmetric headroom from the original +/-15V supply.
    // This is a numerical overload model, not a measured SSM2017/VCA distortion curve.
    const float rail=static_cast<float>(15.0/calibration::clipPeakVolts);
    const float knee=rail*.1f, start=rail-knee;
    const float magnitude=std::abs(sample);
    if(magnitude<=start) return sample;
    const float distance=std::min(magnitude-start,2.f*knee);
    return std::copysign(start+distance-distance*distance/(4.f*knee),sample);
}
Meters Engine::process(float* const* audio,int channels,int samples) noexcept {
    Meters m;
    if(!audio || samples<=0 || channels<1 || channels>preparedChannels) return m;
    if(!controlsInitialised) setParameters(params);
    const float gateOpen=coefficient(.002f);
    for(int i=0;i<samples;++i) {
        std::array<float,10> v{};
        for(size_t k=0;k<v.size();++k) v[k]=control[k].next(smoothCoeff);
        const float hp=hpMix.next(smoothCoeff), wet=wetMix.next(smoothCoeff), blend=modelMix.next(smoothCoeff);
        if(std::abs(v[3]-previousDeHz)>1.f || previousDeHz<0) {
            for(int model=0;model<2;++model) {
                const double w=2*sr*std::tan(pi*std::min(static_cast<double>(v[3]),sr*.45)/sr);
                for(auto& c:profiles[model].channels)
                    c.deHp.analogue(sr,{1,0,0},{1,w/DetectorCircuit::deQ(model==1),w*w});
            }
            previousDeHz=v[3];
        }
        float maxPre=0;
        std::array<float,2> input{};
        const float inGain=gain(v[0]);
        for(int ch=0;ch<channels;++ch) {
            // Linear below rail knee; conservative overload approximation above it.
            float x=std::isfinite(audio[ch][i]) ? std::clamp(audio[ch][i],-64.f,64.f) : 0;
            x*=inGain;
            maxPre=std::max(maxPre,std::abs(x)); // INPUT CLIP SENSE is before switched HPF.
            x=railLimit(x);
            input[ch]=x;
        }
        std::array<std::array<float,2>,2> output{};
        for(int model=0;model<2;++model) {
        auto& p=profiles[model];
        const auto deCv=DeCvCircuit::forModel(model==1);
        const float detectorCoeff=coefficient(static_cast<float>(deCv.device.rmsSeconds));
        float broad=0,comparison=0,high=0;
        std::array<float,2> pre{};
        for(int ch=0;ch<channels;++ch) {
            auto& c=p.channels[ch];
            pre[ch]=input[ch]+hp*(c.inputHp.tick(input[ch])-input[ch]);
            const float h=c.deCoupling.tick(c.deHp.tick(pre[ch]));
            const float detector=c.broadDetector.tick(pre[ch]);
            comparison+=detector*detector;
            broad+=pre[ch]*pre[ch];high+=h*h;
        }
        broad/=static_cast<float>(channels);high/=static_cast<float>(channels);comparison/=static_cast<float>(channels);
        p.broadPower=broad+rmsCoeff*(p.broadPower-broad);
        p.dePower=high+detectorCoeff*(p.dePower-high);
        p.comparisonPower=comparison+detectorCoeff*(p.comparisonPower-comparison);
        const float relative=db(std::sqrt(p.dePower))-db(std::sqrt(p.comparisonPower));
        if(v[4]>.001f && p.broadPower>1.e-12f) {
            const auto detectorCircuit=DetectorCircuit::forModel(model==1);
            const double broadConductance=1/detectorCircuit.directR+1/detectorCircuit.branchR;
            const double highConductance=model==1?2./49900.:1./39200.;
            const double vb=deCv.device.rmsVoltage(std::sqrt(p.comparisonPower)*calibration::clipPeakVolts*broadConductance);
            const double vh=deCv.device.rmsVoltage(std::sqrt(p.dePower)*calibration::clipPeakVolts*highConductance);
            p.deVoltage=deCv.tick(p.deVoltage,deCv.inputCurrent(vb,vh,v[4]/10.),1/sr,model==1);
            const double index=std::clamp(-p.deVoltage,0.,15.)*(4096/15.);
            const auto n=static_cast<size_t>(index);
            const auto next=std::min(n+1,size_t{4096});
            p.deGr=p.deVoltageToDb[n]+static_cast<float>(index-n)*(p.deVoltageToDb[next]-p.deVoltageToDb[n]);
        } else { p.deGr=0; p.deVoltage=0; }
        p.gatePower=broad+rmsCoeff*(p.gatePower-broad);
        const float gateBelow=std::max(0.f,v[7]-db(std::sqrt(p.gatePower)));
        const float gateTarget=params.gateOff ? 0.f : std::min(80.f,gateBelow*(v[8]-1.f));
        if(gateTarget<=p.gateGr) p.gateGr=follow(p.gateGr,gateTarget,coefficient(.0792f),gateOpen);
        else if(model==1) p.gateGr=std::min(gateTarget,p.gateGr+static_cast<float>(100./sr));
        else p.gateGr=follow(p.gateGr,gateTarget,coefficient(.0792f),gateOpen);
        const bool compressionEnabled=v[1]>.001f;
        PilotCircuit::Result comp{};
        for(int sub=0;sub<2;++sub) {
            const double pilotGain=std::pow(10.,std::clamp(p.pilot.pilotGain(v[1],v[2],p.deGr),-100.,100.)/20);
            float detectorPeak=0;
            for(int ch=0;ch<channels;++ch) {
                auto& c=p.channels[ch];
                const float sample=sub==0?(c.previousPilotInput+pre[ch])*.5f:pre[ch];
                const double voltage=c.pilotInput.tick(sample)*calibration::clipPeakVolts*pilotGain;
                const float filtered=c.pilotDetector.tick(static_cast<float>(std::clamp(voltage,-13.,13.)));
                detectorPeak=std::max(detectorPeak,std::abs(filtered));
            }
            comp=p.pilot.tick(detectorPeak>15*4990/24990.,v[1],v[2]);
        }
        for(int ch=0;ch<channels;++ch) p.channels[ch].previousPilotInput=pre[ch];
        const float autoMakeup=compressionEnabled?static_cast<float>(comp.makeupDb):0.f;
        p.compGr=compressionEnabled?static_cast<float>(comp.reductionDb):0.f;
        const float commonGain=gain(v[9]+autoMakeup-p.compGr-p.deGr-p.gateGr);
        const float hfRelative=std::clamp((relative+30.f)/30.f,0.f,1.f);
        const float hfBoost=(gain(v[6]*.9f)-1.f)*hfRelative;
        const float hfBranchGain=hfBoost;
        const auto lfCircuit=LfCircuit::forModel(model==1);
        const auto lfMix=lfCircuit.mixer(v[5]/10.);
        for(int ch=0;ch<channels;++ch) {
            const float main=pre[ch]*commonGain;
            auto& c=p.channels[ch];
            const float h=c.hfAudio.tick(main);
            const float stageA=c.lf.tick(main);
            const float stageB=c.lfCut.tick(main+static_cast<float>(lfCircuit.rd/lfCircuit.ra)*stageA);
            const float lf=static_cast<float>(lfMix[0]*stageA+lfMix[1]*stageB);
            // Sum main + LF parallel branch + HF VCA difference branch.
            const float processed=main+lf+h*hfBranchGain;
            const float out=pre[ch]+wet*(processed-pre[ch]);
            output[model][ch]=out;
        }
        const float weight=model==0?1.f-blend:blend;
        if(i==samples-1) {
            m.inputRms+=weight*std::sqrt(p.broadPower);
            m.compressorReductionDb+=weight*p.compGr*wet;
            m.deEsserReductionDb+=weight*p.deGr*wet;
            m.gateReductionDb+=weight*p.gateGr*wet;
        }
        }
        for(int ch=0;ch<channels;++ch) {
            const float out=output[0][ch]+blend*(output[1][ch]-output[0][ch]);
            audio[ch][i]=railLimit(out);m.outputPeak=std::max(m.outputPeak,std::abs(out));
        }
        m.inputPeak=std::max(m.inputPeak,maxPre);
    }
    return m;
}
}
