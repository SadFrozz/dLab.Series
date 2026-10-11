#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/Calibration.h"

juce::AudioProcessorValueTreeState::ParameterLayout Channel286Processor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto add=[&](const char* id,const char* name,float lo,float hi,float initial,const char* unit) {
        l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,
          juce::NormalisableRange<float>{lo,hi,.01f},initial,
          juce::AudioParameterFloatAttributes{}.withLabel(unit)));
    };
    // Fixed host range covers both modes; DSP/UI enforce their LINE limits.
    add("input","Line Gain",-15,45,0,"dB");
    add("drive","Drive",0,10,0,""); add("density","Density",0,10,5,"");
    l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"frequency",1},
      "De-Esser Frequency",juce::NormalisableRange<float>{800,10000,1,.4f},4000.f,
      juce::AudioParameterFloatAttributes{}.withLabel("Hz")));
    add("deThreshold","De-Esser Threshold",0,10,0,"");
    add("lf","LF Detail",0,10,0,""); add("hf","HF Detail",0,10,0,"");
    add("gateThreshold","Gate Threshold",-60,15,-15,"dB");
    add("gateRatio","Gate Ratio",1,10,1,":1"); add("output","Output Gain",-30,10,0,"dB");
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"hpf",1},"80 Hz HPF",false));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"processBypass",1},"Process Bypass",false));
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"quality",1},"Quality",
      juce::StringArray{"Standard 1x","High 2x","Ultra 4x"},0));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"modelS",1},"286s Mode",false));
    return l;
}
Channel286Processor::Channel286Processor()
  : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
      .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
    parameters(*this,nullptr,channelSuite::channel286.stateTag.data(),layout()) {
    const char* ids[]{"input","drive","density","frequency","deThreshold","lf","hf",
      "gateThreshold","gateRatio","output","hpf","processBypass","quality","modelS"};
    for(size_t i=0;i<raw.size();++i) raw[i]=parameters.getRawParameterValue(ids[i]);
}
bool Channel286Processor::isBusesLayoutSupported(const BusesLayout& b) const {
    const auto input=b.getMainInputChannelSet();
    return (input==juce::AudioChannelSet::mono() || input==juce::AudioChannelSet::stereo())
      && input==b.getMainOutputChannelSet();
}
void Channel286Processor::prepareToPlay(double rate,int blockSize) {
    maximumBlock=std::max(1,blockSize);
    const int channels=getTotalNumInputChannels();
    for(int i=0;i<3;++i) engines[i].prepare(rate*(1<<i),channels);
    for(int i=0;i<2;++i) {
        oversamplers[i]=std::make_unique<juce::dsp::Oversampling<float>>(channels,i+1,
          juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true);
        oversamplers[i]->initProcessing(static_cast<size_t>(maximumBlock));
        pathLatency[i+1]=static_cast<int>(std::lround(oversamplers[i]->getLatencyInSamples()));
    }
    // Stable host latency for every quality, including Process Bypass.
    setLatencySamples(std::max(pathLatency[1],pathLatency[2]));
    jassert(getLatencySamples()<256);
    delayMemory={}; delayPosition=0; activeQuality=-1; switchGain=1;
    for(auto& meter:meters) meter.store(0);
    inputPeak.store(0);
}
float Channel286Processor::rmsToDbu(float rms) noexcept {
    return juce::Decibels::gainToDecibels(rms,-150.f)+c286::calibration::fullScaleSineDbu+c286::calibration::rmsToPeakDb;
}
void Channel286Processor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    for(int ch=getTotalNumInputChannels();ch<buffer.getNumChannels();++ch) buffer.clear(ch,0,buffer.getNumSamples());
    c286::Parameters p;
    p.inputDb=raw[0]->load(); p.drive=raw[1]->load(); p.density=raw[2]->load();
    p.deFrequency=raw[3]->load(); p.deThreshold=raw[4]->load();
    p.lfDetail=raw[5]->load(); p.hfDetail=raw[6]->load();
    const float threshold=raw[7]->load();
    p.gateThresholdDb=threshold-c286::calibration::fullScaleSineDbu-c286::calibration::rmsToPeakDb;
    p.gateOff=threshold<=-59.99f;
    p.gateRatio=raw[8]->load(); p.outputDb=raw[9]->load();
    p.highPass=raw[10]->load()>.5f; p.processBypass=raw[11]->load()>.5f;
    p.modelS=raw[13]->load()>.5f;
    const int quality=juce::jlimit(0,2,static_cast<int>(std::lround(raw[12]->load())));
    if(quality!=activeQuality) {
        engines[quality].reset();
        if(quality>0) oversamplers[quality-1]->reset();
        delayMemory={}; delayPosition=0;
        if(activeQuality>=0) switchGain=0; // Brief fade-in after filter/detector reset.
        activeQuality=quality;
    }
    auto& engine=engines[quality]; engine.setParameters(p);
    c286::Meters m;
    const int channels=getTotalNumInputChannels();
    juce::dsp::AudioBlock<float> hostBlock(buffer);
    for(int offset=0;offset<buffer.getNumSamples();offset+=maximumBlock) {
        const int count=std::min(maximumBlock,buffer.getNumSamples()-offset);
        auto block=hostBlock.getSubsetChannelBlock(0,channels).getSubBlock(offset,count);
        std::array<float*,2> pointers{};
        c286::Meters part;
        if(quality==0) {
            for(int ch=0;ch<channels;++ch) pointers[ch]=block.getChannelPointer(ch);
            part=engine.process(pointers.data(),channels,count);
        } else {
            auto up=oversamplers[quality-1]->processSamplesUp(block);
            for(int ch=0;ch<channels;++ch) pointers[ch]=up.getChannelPointer(ch);
            part=engine.process(pointers.data(),channels,static_cast<int>(up.getNumSamples()));
            oversamplers[quality-1]->processSamplesDown(block);
        }
        m.inputRms=part.inputRms;
        m.inputPeak=std::max(m.inputPeak,part.inputPeak);
        m.outputPeak=std::max(m.outputPeak,part.outputPeak);
        m.compressorReductionDb=part.compressorReductionDb;
        m.deEsserReductionDb=part.deEsserReductionDb;
        m.gateReductionDb=part.gateReductionDb;
        const int padding=getLatencySamples()-pathLatency[quality];
        for(int i=0;i<count;++i) {
            switchGain=std::min(1.f,switchGain+static_cast<float>(1.0/(getSampleRate()*.005)));
            for(int ch=0;ch<channels;++ch) {
                auto* samples=block.getChannelPointer(ch);
                delayMemory[ch][delayPosition]=samples[i];
                samples[i]=delayMemory[ch][(delayPosition-padding+256)%256]*switchGain;
            }
            delayPosition=(delayPosition+1)%256;
        }
    }
    const float decay=static_cast<float>(std::exp(-buffer.getNumSamples()/(getSampleRate()*.3)));
    const std::array<float,5> values{m.inputRms,m.outputPeak,m.compressorReductionDb,m.deEsserReductionDb,m.gateReductionDb};
    meters[0].store(m.inputRms,std::memory_order_relaxed);
    inputPeak.store(std::max(m.inputPeak,inputPeak.load()*decay),std::memory_order_relaxed);
    for(size_t i=1;i<meters.size();++i) meters[i].store(std::max(values[i],meters[i].load()*decay),std::memory_order_relaxed);
}
void Channel286Processor::getStateInformation(juce::MemoryBlock& dest) {
    auto state=parameters.copyState();
    state.setProperty("editorWidth",editorWidth.load(),nullptr);
    state.setProperty("guideScenario",guideScenario.load(),nullptr);
    if(auto xml=state.createXml()) copyXmlToBinary(*xml,dest);
}
void Channel286Processor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size))
        if(xml->hasTagName(parameters.state.getType())) {
            auto tree=juce::ValueTree::fromXml(*xml);
            if(!tree.getChildWithProperty("id","modelS").isValid()) {
                juce::ValueTree mode("PARAM");mode.setProperty("id","modelS",nullptr);
                mode.setProperty("value",0.f,nullptr);tree.addChild(mode,-1,nullptr);
            }
            editorWidth.store(juce::jlimit(1440,2000,static_cast<int>(tree.getProperty("editorWidth",1600))));
            guideScenario.store(juce::jlimit(0,18,static_cast<int>(tree.getProperty("guideScenario",0))));
            for(auto child:tree) {
                if(auto* parameter=parameters.getParameter(child.getProperty("id").toString())) {
                    const auto range=parameter->getNormalisableRange();
                    const float value=static_cast<float>(child.getProperty("value"));
                    child.setProperty("value",std::isfinite(value)?juce::jlimit(range.start,range.end,value):parameter->convertFrom0to1(parameter->getDefaultValue()),nullptr);
                }
            }
            parameters.replaceState(tree);
        }
}
juce::AudioProcessorEditor* Channel286Processor::createEditor() { return new Channel286Editor(*this); }
#ifndef DLAB_SUITE
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Channel286Processor(); }

#endif
