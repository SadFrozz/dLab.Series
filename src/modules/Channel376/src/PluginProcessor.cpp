#include "PluginProcessor.h"
#include "PluginEditor.h"
namespace {
juce::NormalisableRange<float> frequencyRange(const std::array<float,5>& knots){
    auto from=[knots](float,float,float p){p=juce::jlimit(0.f,1.f,p);const int j=std::min(3,int(p*4));const float t=p*4-j;return std::exp(std::log(knots[j])*(1-t)+std::log(knots[j+1])*t);};
    auto to=[knots](float,float,float v){v=juce::jlimit(knots[0],knots[4],v);int j=0;while(j<3&&v>knots[j+1])++j;return (j+(std::log(v)-std::log(knots[j]))/(std::log(knots[j+1])-std::log(knots[j])))/4;};
    return {knots[0],knots[4],from,to};
}
}
const std::array<const char*,5> Channel376Processor::switchIDs{{"pad20","phase","lowCut","overEasy","slow"}};
juce::AudioProcessorValueTreeState::ParameterLayout Channel376Processor::layout(){
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    const char* names[]{"Drive","Low","Mid","Mid Frequency","High","Threshold","Ratio","De-esser Frequency","De-esser Amount","Output Level"};
    for(int j=0;j<10;++j){const auto& p=channel376::continuous[j];juce::NormalisableRange<float> r{float(p.minimum),float(p.maximum)};
        if(j==3)r=frequencyRange({100,200,1000,4000,8000});if(j==7)r=frequencyRange({800,1000,4000,8000,10000});
        const juce::String unit=j==3||j==7?"Hz":j==5?"dBu":j==6||j==8?"":"dB";
        l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{p.id.data(),1},names[j],r,float(p.initial),juce::AudioParameterFloatAttributes{}.withLabel(unit)));}
    const char* switchNames[]{"20dB PAD","Phase","Low Cut","OverEasy","Slow"};
    for(int j=0;j<5;++j)l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{switchIDs[j],1},switchNames[j],false));
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"oversampling",1},"Oversampling",juce::StringArray{"Off","2x","4x"},0));
    juce::StringArray tubes;for(const auto& t:channel376::tubeModels376)tubes.add(juce::String::fromUTF8(t.name.data()));
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"tubeModel",2},"Tube Model",tubes,0));return l;
}
Channel376Processor::Channel376Processor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
    parameters(*this,nullptr,"Channel376State",layout()),engine(std::make_unique<channel376::AnalogEngine376>()){
    for(int j=0;j<10;++j)raw[j]=parameters.getRawParameterValue(channel376::continuous[j].id.data());
    for(int j=0;j<5;++j)raw[10+j]=parameters.getRawParameterValue(switchIDs[j]);raw[15]=parameters.getRawParameterValue("oversampling");raw[16]=parameters.getRawParameterValue("tubeModel");
    meters[0].store(-std::numeric_limits<double>::infinity());
}
bool Channel376Processor::isBusesLayoutSupported(const BusesLayout& b)const{
    const auto s=b.getMainInputChannelSet();return (s==juce::AudioChannelSet::mono()||s==juce::AudioChannelSet::stereo())&&s==b.getMainOutputChannelSet();
}
channel376::AnalogParameters376 Channel376Processor::snapshot()const noexcept{
    channel376::AnalogParameters376 p;for(int j=0;j<10;++j)p.values[j]=raw[j]->load(std::memory_order_relaxed);
    p.switches={raw[10]->load()>.5f,raw[11]->load()>.5f,raw[12]->load()>.5f,raw[13]->load()>.5f,raw[14]->load()>.5f};
    p.digital={};p.oversampling=channel376::OversamplingMode(int(raw[15]->load()));p.tubeModel=int(raw[16]->load());return p;
}
void Channel376Processor::prepareToPlay(double rate,int){juce::ScopedNoDenormals noDenormals;engine->prepare(rate,getTotalNumInputChannels(),getProcessingPrecision()==doublePrecision?channel376::StreamPrecision::float64:channel376::StreamPrecision::float32,snapshot());setLatencySamples(engine->latency());}
template<class Sample>void Channel376Processor::render(juce::AudioBuffer<Sample>& b,bool bypass){
    juce::ScopedNoDenormals noDenormals;engine->streamPrecision(std::is_same_v<Sample,double>?channel376::StreamPrecision::float64:channel376::StreamPrecision::float32);
    engine->setParameters(snapshot());if(getLatencySamples()!=engine->latency())setLatencySamples(engine->latency());engine->setBypass(bypass);const int channels=std::min(getTotalNumInputChannels(),b.getNumChannels());
    for(int j=channels;j<b.getNumChannels();++j)b.clear(j,0,b.getNumSamples());if(channels<1)return;
    auto* left=b.getWritePointer(0);auto* right=channels>1?b.getWritePointer(1):nullptr;
    for(int n=0;n<b.getNumSamples();++n){const auto y=engine->process({double(left[n]),right?double(right[n]):0});left[n]=Sample(y[0]);if(right)right[n]=Sample(y[1]);}
    double input=0,gr=0,de=0,compare=std::numeric_limits<double>::infinity();bool clip=false;
    for(int j=0;j<channels;++j){const auto& c=engine->channelState(j);input=std::max(input,c.inputMeterVoltage());gr=std::min(gr,c.gainReductionMeterVoltage());de=std::min(de,c.deCvVoltage());compare=std::min(compare,c.thresholdComparatorVoltage());clip|=c.eqClip();}
    const std::array<double,6> m{engine->outputDbfs(),input,gr,clip?1.:0.,de,compare};for(int j=0;j<6;++j)meters[j].store(m[j],std::memory_order_relaxed);
}
void Channel376Processor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&){render(b,false);}
void Channel376Processor::processBlock(juce::AudioBuffer<double>& b,juce::MidiBuffer&){render(b,false);}
void Channel376Processor::processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer&){render(b,true);}
void Channel376Processor::processBlockBypassed(juce::AudioBuffer<double>& b,juce::MidiBuffer&){render(b,true);}
void Channel376Processor::getStateInformation(juce::MemoryBlock& dest){auto state=parameters.copyState();state.setProperty("version",1,nullptr);state.setProperty("tubeModelRevision",2,nullptr);if(auto xml=state.createXml())copyXmlToBinary(*xml,dest);}
void Channel376Processor::setStateInformation(const void* data,int bytes){if(bytes<=0||bytes>1048576)return;if(auto xml=getXmlFromBinary(data,bytes))if(xml->hasTagName("Channel376State")&&xml->getIntAttribute("version",1)==1){
    auto state=juce::ValueTree::fromXml(*xml);
    // The retired descriptive Sylvania choice must not silently become JJ.
    if(xml->getIntAttribute("tubeModelRevision",1)<2)for(auto child:state)
        if(child.getProperty("id").toString()=="tubeModel"&&int(child.getProperty("value"))==2)child.setProperty("value",0.f,nullptr);
    for(int i=state.getNumChildren()-1;i>=0;--i){const auto id=state.getChild(i).getProperty("id").toString();if(id=="dither"||id=="shape")state.removeChild(i,nullptr);}
    for(auto child:state){const auto id=child.getProperty("id").toString();if(auto* p=parameters.getParameter(id)){
        double v=double(child.getProperty("value"));if(!std::isfinite(v))v=p->convertFrom0to1(p->getDefaultValue());child.setProperty("value",p->convertFrom0to1(juce::jlimit(0.f,1.f,p->convertTo0to1(float(v)))),nullptr);}}
    bool hasOversampling=false;for(auto child:state)hasOversampling|=child.getProperty("id").toString()=="oversampling";
    if(!hasOversampling){juce::ValueTree child("PARAM");child.setProperty("id","oversampling",nullptr);child.setProperty("value",0.f,nullptr);state.addChild(child,-1,nullptr);}
    bool hasTube=false;for(auto child:state)hasTube|=child.getProperty("id").toString()=="tubeModel";
    if(!hasTube){juce::ValueTree child("PARAM");child.setProperty("id","tubeModel",nullptr);child.setProperty("value",0.f,nullptr);state.addChild(child,-1,nullptr);}
    parameters.replaceState(state);}}
juce::AudioProcessorEditor* Channel376Processor::createEditor(){return new Channel376Editor(*this);}
#ifndef DLAB_SUITE
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Channel376Processor;}

#endif
