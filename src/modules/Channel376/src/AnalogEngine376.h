#pragma once
#include "InputCircuit376.h"
#include "LoadedTubePath376.h"
#if defined(CHANNEL376_EXPERIMENTAL_TUBE_COLOUR)
#include "TubeColour376.h"
#endif
#include "DetectorCircuit376.h"
#include "CompressorCircuit376.h"
#if defined(CHANNEL376_EXPERIMENTAL_MANIFOLD)
#include "CompressorManifold376.h"
#endif
#include "MainVca376.h"
#include "OutputCircuit376.h"
#include "Numerical376.h"
#include "FastHalfband376.h"
#include "ParameterContract.h"
#include "StreamPrecision.h"
#if !defined(CHANNEL376_PCM_OUTPUT_ONLY)
#include "DitherShape376.h"
#endif
namespace channel376 {
struct AnalogParameters376 {
    std::array<double,10> values{};Switches switches{};DigitalModes digital{};OversamplingMode oversampling=OversamplingMode::off;int tubeModel=0;
    AnalogParameters376()noexcept{for(int j=0;j<10;++j)values[j]=continuous[j].initial;}
};
struct Ramp376 {
    double current=0,target=0,step=0;int remaining=0;
    void reset(double value)noexcept{current=target=value;step=0;remaining=0;}
    void set(double value,int samples)noexcept{if(value==target)return;target=value;remaining=std::max(1,samples);step=(target-current)/remaining;}
    double tick()noexcept{if(remaining>0){current+=step;if(--remaining==0)current=target;}return current;}
};
// Assembled nominal channel including loaded opamp rail/slew/GBW behaviour.
class AnalogChannel376 {
public:
    static constexpr double voltsPerPCM=8.705960659552927;
    static constexpr double fullPathUnityTrim=27245.130726458814; // calculate_io.py
    struct Controls {double comp=0,de=0;};
    void prepare(double rate,const AnalogParameters376& p,int factor=4)noexcept{
        fs=rate;line.prepare(rate);source.prepare(rate);drive.prepare(rate,p.values[0]);cut.prepare(rate);
#if defined(CHANNEL376_EXPERIMENTAL_TUBE_COLOUR)
        tubeEq.prepare(rate,fullPathUnityTrim,rate/factor);
#else
        tubeEq.prepare(rate,fullPathUnityTrim);
#endif
#if defined(CHANNEL376_EXPERIMENTAL_TUBE_COLOUR)
        tubeEq.resetModel(p.tubeModel);
#endif
        broad.prepare(rate);hf.prepare(rate);
        broadRms.prepare(rate,634000);hfRms.prepare(rate,20e6+20);hfCurrent.prepare(rate,hfRms.voltage());
        compDrive.prepare();compDrive.setThreshold(p.values[5]);
        // Audio and RMS rectification run at4x; the millisecond compressor
        // control circuit integrates at host Fs with its OWN physical dt.
        // This is not skipping RC updates at a stale4x time step.
        comp.prepare(rate/factor,p.switches.slow,compDrive.tick(broadRms.voltage(),p.values[6],p.switches.overEasy));heldComp=comp.output();de.prepare(rate);
        vca.prepare(rate);output.prepare(rate);inputMeter.prepare(rate);eqMeter.prepare(rate);
        params=p;const auto physical=physicalValues(p);for(int j=0;j<10;++j)ramps[j].reset(physical[j]);
#if defined(CHANNEL376_EXPERIMENTAL_COEFFICIENTS)
        rampSamplesRemaining=0;updateAudioControls();
#endif
        padPhase.reset((p.switches.pad20?.1:1)*(p.switches.phase?-1:1));lowCut.reset(p.switches.lowCut?1:0);
        eqVoltage=inputRectified=lastDeCv=0;
    }
    void setParameters(const AnalogParameters376& p)noexcept{
        const auto physical=physicalValues(p);params=p;
#if defined(CHANNEL376_EXPERIMENTAL_TUBE_COLOUR)
        tubeEq.selectModel(p.tubeModel);
#endif
        for(int j=0;j<10;++j)ramps[j].set(physical[j],int(fs*.02));
#if defined(CHANNEL376_EXPERIMENTAL_COEFFICIENTS)
        rampSamplesRemaining=0;for(const auto& r:ramps)rampSamplesRemaining=std::max(rampSamplesRemaining,r.remaining);
#endif
        padPhase.set((p.switches.pad20?.1:1)*(p.switches.phase?-1:1),int(fs*.005));lowCut.set(p.switches.lowCut?1:0,int(fs*.005));
    }
    void front(double pcm)noexcept{
        if(!std::isfinite(pcm))pcm=0;
        const double voltage=pcm*voltsPerPCM*padPhase.tick();
#if defined(CHANNEL376_EXPERIMENTAL_COEFFICIENTS)
        if(rampSamplesRemaining>0){for(auto& r:ramps)r.tick();--rampSamplesRemaining;updateAudioControls();}
#else
        for(auto& r:ramps)r.tick();
#endif
        const double lineOut=line.tick(voltage),selected=source.tick(lineOut);
#if !defined(CHANNEL376_EXPERIMENTAL_COEFFICIENTS)
        drive.setResistance(ramps[0].current);
#endif
        const double driven=drive.tick(selected),cutOut=cut.tick(driven),blend=lowCut.tick();
        const double preTube=driven+(cutOut-driven)*blend;inputRectified=inputMeter.tick(preTube);
        // Smooth actual R, invert the nominal-label mapping only to call the
        // existing EQ API. Capacitor states stay charged throughout automation.
#if !defined(CHANNEL376_EXPERIMENTAL_COEFFICIENTS)
        updateEqControls();
#endif
        eqVoltage=tubeEq.tick(preTube);eqMeter.eqTick(eqVoltage);
    }
    Controls controls(bool updateCompressor=true)noexcept{
        const double rms=broadRms.tick(broad.tick(eqVoltage));hf.setResistance(ramps[7].current);
        const double hfVoltage=hfRms.tickTotalCurrent(hfCurrent.tick(hf.tick(eqVoltage),hfRms.voltage()));
        if(updateCompressor){compDrive.setPotVoltage(ramps[5].current);const double target=compDrive.tick(rms,ramps[6].current,params.switches.overEasy);
            heldComp=comp.tickTargetVoltage(target,params.switches.slow);}
        lastDeCv=de.tick(rms,hfVoltage,ramps[8].current);return {heldComp,lastDeCv};
    }
    double back(Controls linked)noexcept{return output.tick(vca.tick(eqVoltage,ramps[9].current,linked.de,linked.comp))/voltsPerPCM;}
    double backStereo(Controls linked,const AnalogChannel376* first)noexcept{
#if defined(CHANNEL376_EXPERIMENTAL_STEREO_VCA)
        if(first)return output.tick(vca.tickShared(eqVoltage,first->vca.linearGain(),first->vca.controlVoltage()))/voltsPerPCM;
#endif
        return back(linked);
    }
    double inputMeterVoltage()const noexcept{return inputRectified;}
    double gainReductionMeterVoltage()const noexcept{return comp.meterCv();}
    double eqOutputVoltage()const noexcept{return eqVoltage;}
    bool eqClip()const noexcept{return eqMeter.eqClip();}
    double deCvVoltage()const noexcept{return lastDeCv;}
    double thresholdComparatorVoltage()const noexcept{return compDrive.comparator();}
    unsigned long long compressorEvaluations()const noexcept{return comp.evaluationCount();}
    unsigned failedSolves()const noexcept{return line.failedSolves()+source.failedSolves()+drive.failedSolves()+cut.failedSolves()+tubeEq.failedSolves()+hf.failedSolves()+comp.failedSolves()+vca.failedSolves()+output.failedSolves();}
    std::array<unsigned,8> failureDetails()const noexcept{return {line.failedSolves(),source.failedSolves(),drive.failedSolves(),cut.failedSolves(),tubeEq.failedSolves(),comp.failedSolves(),vca.failedSolves(),output.failedSolves()};}
private:
    void updateAudioControls()noexcept{
        drive.setResistance(ramps[0].current);
        updateEqControls();
    }
    void updateEqControls()noexcept{
        const double f=1/(1/8000.+(ramps[3].current-634)/50000*(1/100.-1/8000.));
        tubeEq.setPhysicalEq(ramps[1].current,ramps[2].current,ramps[4].current,f);
    }
    static std::array<double,10> physicalValues(const AnalogParameters376& p)noexcept{
        CompressorDrive376 threshold;threshold.prepare();threshold.setThreshold(p.values[5]);
        return {DriveCircuit376::resistance(p.values[0]),EqCircuit376::gainPosition(p.values[1],0),
            EqCircuit376::gainPosition(p.values[2],1,p.values[3]),EqCircuit376::midResistance(p.values[3]),
            EqCircuit376::gainPosition(p.values[4],2),threshold.potVoltage(),std::clamp(p.values[6],0.,1.),
            HighFrequency376::resistance(p.values[7]),std::clamp(p.values[8],0.,10.)/10,outputPot376(p.values[9])};
    }
    double fs=192000,eqVoltage=0,inputRectified=0,lastDeCv=0,heldComp=0;AnalogParameters376 params;std::array<Ramp376,10> ramps{};Ramp376 padPhase,lowCut;
    int rampSamplesRemaining=0;
    LineInput376 line;LineSource376 source;DriveCircuit376 drive;LowCutCircuit376 cut;
#if defined(CHANNEL376_EXPERIMENTAL_TUBE_COLOUR)
    ColourTubePath376 tubeEq;
#else
    LoadedTubePath376 tubeEq;
#endif
    BroadbandCurrent376 broad;LoadedHighFrequency376 hf;RmsDetectorCircuit376 broadRms,hfRms;HighFrequencyCurrent376 hfCurrent;
    CompressorDrive376 compDrive;
#if defined(CHANNEL376_EXPERIMENTAL_MANIFOLD)
    CompressorManifold376 comp;
#else
    CompressorCircuit376 comp;
#endif
    DeCv376 de;MainVca376 vca;OutputCircuit376 output;
    InputMeter376 inputMeter;Meter376 eqMeter;
};
class AnalogEngine376 {
public:
    static constexpr int maximumLatencySamples=96;
    void prepare(double rate,int channels,StreamPrecision precision,const AnalogParameters376& p=AnalogParameters376()){
        const auto safe=sanitize(p);
        fs=std::isfinite(rate)&&rate>=8000&&rate<=384000?rate:48000;count=channels==2?2:1;
        factor=1<<int(safe.oversampling);
        outputPrecision=precision;
        for(int j=0;j<2;++j){channel[j].prepare(factor*fs,safe,factor);
#if !defined(CHANNEL376_PCM_OUTPUT_ONLY)
            digital[j].prepare(fs,precision,0x091110+0x10101*j);digital[j].modes(safe.digital.dither,safe.digital.shape);
#endif
        }
        params=safe;up1={};up2={};down2={};down1={};dry={};cursor=0;bypass.reset(0);modeFade.reset(1);meter.prepare(fs);
    }
    void setParameters(AnalogParameters376 p)noexcept{
        p=sanitize(p);
        if(p.oversampling!=params.oversampling){
            factor=1<<int(p.oversampling);
            for(int j=0;j<2;++j){channel[j].prepare(factor*fs,p,factor);
#if !defined(CHANNEL376_PCM_OUTPUT_ONLY)
                digital[j].reset(0x091110+0x10101*j);
#endif
            }
            up1={};up2={};down2={};down1={};dry={};cursor=0;
            modeFade.reset(0);modeFade.set(1,int(fs*.005));
        }
        if(p.tubeModel==params.tubeModel&&p.oversampling==params.oversampling&&p.values==params.values&&p.switches.pad20==params.switches.pad20&&p.switches.phase==params.switches.phase&&p.switches.lowCut==params.switches.lowCut&&p.switches.overEasy==params.switches.overEasy&&p.switches.slow==params.switches.slow&&p.digital.dither==params.digital.dither&&p.digital.shape==params.digital.shape)return;
        params=p;for(int j=0;j<2;++j){channel[j].setParameters(p);
#if !defined(CHANNEL376_PCM_OUTPUT_ONLY)
            digital[j].modes(p.digital.dither,p.digital.shape);
#endif
        }
    }
    void setBypass(bool value)noexcept{bypass.set(value?1:0,int(fs*.005));}
    void streamPrecision(StreamPrecision value)noexcept{outputPrecision=value;
#if !defined(CHANNEL376_PCM_OUTPUT_ONLY)
        for(auto& d:digital)d.streamPrecision(value);
#endif
    }
    std::array<double,2> process(std::array<double,2> input)noexcept{
        for(auto& v:input)if(!std::isfinite(v))v=0;
        std::array<double,2> result{};
        if(factor<4){
            std::array<std::array<double,2>,2> phases{};
            for(int j=0;j<count;++j)phases[j]=factor==1?std::array<double,2>{input[j],0}:up1[j].tick(input[j]);
            for(int phase=0;phase<factor;++phase){std::array<AnalogChannel376::Controls,2> controls{};
                for(int j=0;j<count;++j){channel[j].front(phases[j][phase]);controls[j]=channel[j].controls(phase==0);}
                auto linked=controls[0];if(count==2){linked.comp=std::min(linked.comp,controls[1].comp);linked.de=std::min(linked.de,controls[1].de);}
                for(int j=0;j<count;++j){const double v=channel[j].backStereo(linked,j?&channel[0]:nullptr);
                    if(factor==1)result[j]=v;else if(phase==0)result[j]=down1[j].even(v);else down1[j].odd(v);}}
        }else{
        std::array<std::array<double,2>,2> low{};
        for(int j=0;j<count;++j)low[j]=up1[j].tick(input[j]);
        for(int a=0;a<2;++a){std::array<double,2> decimated{};std::array<std::array<double,2>,2> high{};
            for(int j=0;j<count;++j)high[j]=up2[j].tick(low[j][a]);
            for(int b=0;b<2;++b){std::array<AnalogChannel376::Controls,2> controls{};
                for(int j=0;j<count;++j){channel[j].front(high[j][b]);controls[j]=channel[j].controls(a==0&&b==0);}
                auto linked=controls[0];if(count==2){linked.comp=std::min(linked.comp,controls[1].comp);linked.de=std::min(linked.de,controls[1].de);}
                for(int j=0;j<count;++j){const double v=channel[j].backStereo(linked,j?&channel[0]:nullptr);if(b==0)decimated[j]=down2[j].even(v);else down2[j].odd(v);}}
            for(int j=0;j<count;++j){if(a==0)result[j]=down1[j].even(decimated[j]);else down1[j].odd(decimated[j]);}}
        }
        const double blend=bypass.tick(),fade=modeFade.tick();double finalPeak=0;
        std::array<double,2> wetSamples{};
#if defined(CHANNEL376_PCM_OUTPUT_ONLY)
        for(int j=0;j<count;++j){const double wet=result[j]*fade;wetSamples[j]=outputPrecision==StreamPrecision::float32?double(float(wet)):wet;}
#elif defined(CHANNEL376_EXPERIMENTAL_STEREO_SHAPE)
        if(count==2)wetSamples=DitherShape376::processPair(digital[0],digital[1],{result[0]*fade,result[1]*fade});
        else wetSamples[0]=digital[0].process(result[0]*fade);
#else
        for(int j=0;j<count;++j)wetSamples[j]=digital[j].process(result[j]*fade);
#endif
        for(int j=0;j<count;++j){const double clean=factor==1?input[j]:dry[j][cursor];dry[j][cursor]=input[j];const double wet=wetSamples[j];result[j]=wet+(clean-wet)*blend;finalPeak=std::max(finalPeak,std::abs(result[j]));}
        cursor=(cursor+1)%std::max(1,latency());meter.outputTick(finalPeak);return result;
    }
    double outputDbfs()const noexcept{return meter.outputDbfs();}
    int latency()const noexcept{return factor==1?0:factor==2?64:96;}
    int oversamplingFactor()const noexcept{return factor;}
    unsigned failedSolves()const noexcept{unsigned n=0;for(int j=0;j<count;++j)n+=channel[j].failedSolves();return n;}
    const AnalogChannel376& channelState(int index)const noexcept{return channel[std::clamp(index,0,count-1)];}
private:
    static AnalogParameters376 sanitize(AnalogParameters376 p)noexcept{
#if defined(CHANNEL376_PCM_OUTPUT_ONLY)
        p.digital={};
#endif
        for(int j=0;j<10;++j)p.values[j]=std::isfinite(p.values[j])?std::clamp(p.values[j],continuous[j].minimum,continuous[j].maximum):continuous[j].initial;
        if(int(p.digital.dither)<0||int(p.digital.dither)>2)p.digital.dither=DitherMode::none;
        if(int(p.digital.shape)<0||int(p.digital.shape)>2)p.digital.shape=ShapeMode::off;
        if(int(p.oversampling)<0||int(p.oversampling)>2)p.oversampling=OversamplingMode::off;p.tubeModel=std::clamp(p.tubeModel,0,2);return p;
    }
    double fs=48000;int count=1,cursor=0,factor=1;AnalogParameters376 params;std::array<AnalogChannel376,2> channel{};
#if !defined(CHANNEL376_PCM_OUTPUT_ONLY)
    std::array<DitherShape376,2> digital{};
#endif
    StreamPrecision outputPrecision=StreamPrecision::float64;
    std::array<Interpolate2x376,2> up1{},up2{};std::array<Decimate2x376,2> down2{},down1{};
    std::array<std::array<double,96>,2> dry{};Ramp376 bypass,modeFade;Meter376 meter;
};
}
