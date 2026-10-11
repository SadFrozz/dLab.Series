#include "../src/PluginProcessor.h"
#include "../src/PluginEditor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <stdexcept>
void require(bool condition,const char* text){if(!condition)throw std::runtime_error(text);}
int main(int argc,char** argv){try{
    juce::ScopedJuceInitialiser_GUI gui;
    Channel376Processor p;require(p.getName()=="dLab 376"&&p.parameters.state.getType().toString()=="Channel376State","Identity");
    require(p.getParameters().size()==17,"Parameter count");
    require(p.parameters.getParameter("dither")==nullptr&&p.parameters.getParameter("shape")==nullptr,"Removed digital parameters");
    require(p.parameters.getRawParameterValue("oversampling")->load()==0,"Oversampling defaults OFF");
    for(int i=0;i<10;++i){auto* q=dynamic_cast<juce::AudioProcessorParameterWithID*>(p.getParameters()[i]);require(q&&q->paramID==channel376::continuous[i].id.data()&&q->getVersionHint()==1,"Parameter contract");}
    for(const char* id:{"eqMidFrequency","deFrequency"}){auto* q=p.parameters.getParameter(id);const std::array<float,5> expected=juce::String(id)=="deFrequency"?std::array<float,5>{800,1000,4000,8000,10000}:std::array<float,5>{100,200,1000,4000,8000};for(int i=0;i<5;++i)require(std::abs(q->convertFrom0to1(i*.25f)-expected[i])<.02f,"Frequency landmarks");}
    auto* drive=p.parameters.getParameter("drive");drive->setValueNotifyingHost(drive->convertTo0to1(7));juce::MemoryBlock state;p.getStateInformation(state);drive->setValueNotifyingHost(.5f);p.setStateInformation(state.getData(),int(state.getSize()));require(std::abs(p.parameters.getRawParameterValue("drive")->load()-7)<1e-5,"State roundtrip");
    const char wrong[]{"Channel286State"};p.setStateInformation(wrong,sizeof(wrong));require(std::abs(p.parameters.getRawParameterValue("drive")->load()-7)<1e-5,"Foreign state accepted");drive->setValueNotifyingHost(.5f);
    auto* tube=p.parameters.getParameter("tubeModel");require(tube&&tube->getVersionHint()==2,"Appended tube parameter version");
    tube->setValueNotifyingHost(1);p.getStateInformation(state);tube->setValueNotifyingHost(0);p.setStateInformation(state.getData(),int(state.getSize()));require(p.parameters.getRawParameterValue("tubeModel")->load()==2,"Tube state roundtrip");
    auto oldSylvania=p.parameters.copyState();oldSylvania.removeProperty("tubeModelRevision",nullptr);oldSylvania.setProperty("version",1,nullptr);
    juce::MemoryBlock sylvaniaState;juce::AudioProcessor::copyXmlToBinary(*oldSylvania.createXml(),sylvaniaState);
    p.setStateInformation(sylvaniaState.getData(),int(sylvaniaState.getSize()));require(p.parameters.getRawParameterValue("tubeModel")->load()==0,"Retired Sylvania migrates to EH rather than JJ");
    auto* oversampling=p.parameters.getParameter("oversampling");
    oversampling->setValueNotifyingHost(1);p.getStateInformation(state);oversampling->setValueNotifyingHost(0);p.setStateInformation(state.getData(),int(state.getSize()));
    require(p.parameters.getRawParameterValue("oversampling")->load()==2,"Oversampling state roundtrip");
    auto legacy=p.parameters.copyState();for(int i=legacy.getNumChildren()-1;i>=0;--i)if(legacy.getChild(i).getProperty("id").toString()=="oversampling"||legacy.getChild(i).getProperty("id").toString()=="tubeModel")legacy.removeChild(i,nullptr);
    legacy.setProperty("version",1,nullptr);juce::MemoryBlock oldState;
    for(const char* id:{"dither","shape"}){juce::ValueTree child("PARAM");child.setProperty("id",id,nullptr);child.setProperty("value",2.f,nullptr);legacy.addChild(child,-1,nullptr);}
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),oldState);
    p.setStateInformation(oldState.getData(),int(oldState.getSize()));require(p.parameters.getRawParameterValue("oversampling")->load()==0,"Old state uses OFF");require(p.parameters.getRawParameterValue("tubeModel")->load()==0,"Old state uses EH even after other selection");
    for(auto child:p.parameters.copyState())require(child.getProperty("id").toString()!="dither"&&child.getProperty("id").toString()!="shape","Retired state values discarded");
    for(int channels:{1,2})for(bool doubles:{false,true}){auto layout=p.getBusesLayout();layout.inputBuses.set(0,juce::AudioChannelSet::canonicalChannelSet(channels));layout.outputBuses.set(0,juce::AudioChannelSet::canonicalChannelSet(channels));require(p.setBusesLayout(layout),"Bus layout");p.setProcessingPrecision(doubles?juce::AudioProcessor::doublePrecision:juce::AudioProcessor::singlePrecision);p.prepareToPlay(48000,256);require(p.getLatencySamples()==0,"Default latency");juce::MidiBuffer midi;
        if(doubles){juce::AudioBuffer<double> b(channels,4097);for(int c=0;c<channels;++c)for(int i=0;i<4097;++i)b.setSample(c,i,.03*std::sin(i*.13));p.processBlock(b,midi);for(int c=0;c<channels;++c)for(int i=0;i<4097;++i)require(std::isfinite(b.getSample(c,i)),"Double render");p.processBlockBypassed(b,midi);}
        else{juce::AudioBuffer<float> b(channels,4097);for(int c=0;c<channels;++c)for(int i=0;i<4097;++i)b.setSample(c,i,.03f*std::sin(i*.13f));p.processBlock(b,midi);for(int c=0;c<channels;++c)for(int i=0;i<4097;++i)require(std::isfinite(b.getSample(c,i)),"Float render");p.processBlockBypassed(b,midi);}
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());require(editor->getWidth()==1460&&editor->getHeight()==717,"Approved panel dimensions");
    if(argc>1){auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds(),true);juce::File file(argv[1]);file.deleteFile();if(auto stream=file.createOutputStream()){juce::PNGImageFormat png;require(png.writeImageToStream(snapshot,*stream),"PNG write");}else require(false,"PNG open");}
    for(int model:{1,2,0}){tube->setValueNotifyingHost(tube->convertTo0to1(float(model)));juce::AudioBuffer<float> b(p.getTotalNumInputChannels(),1024);b.clear();juce::MidiBuffer midi;p.processBlock(b,midi);for(int c=0;c<b.getNumChannels();++c)for(int n=0;n<b.getNumSamples();++n)require(std::isfinite(b.getSample(c,n)),"Tube selection render");}
      const auto mapsBeforeOversampling=channel376::DynamicTube376::mapBuildCount();
      for(int mode:{1,2,0}){oversampling->setValueNotifyingHost(oversampling->convertTo0to1(float(mode)));juce::AudioBuffer<float> buffer(p.getTotalNumInputChannels(),32);buffer.clear();juce::MidiBuffer midi;p.processBlock(buffer,midi);require(p.getLatencySamples()==(mode==0?0:mode==1?64:96),"Host latency follows runtime mode");}
      require(channel376::DynamicTube376::mapBuildCount()==mapsBeforeOversampling,"No dynamic map construction in audio callback on OS change");
    std::cout<<"PASS: separate identity, 17 parameters, retired digital state ignored, mono/stereo float32/64, optional oversampling, bypass, panel snapshot\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
