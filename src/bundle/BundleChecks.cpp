#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <stdexcept>
static void require(bool b,const char* msg) { if(!b) throw std::runtime_error(msg); }
int main(int argc,char** argv) {
 try {
  juce::ScopedJuceInitialiser_GUI gui;
  require(argc==3,"Expected bundle and preview directory");
  juce::VST3PluginFormat format;
  juce::OwnedArray<juce::PluginDescription> types;
  format.findAllTypesForFile(types,juce::File(argv[1]).getFullPathName());
  std::cout << "Scanned " << juce::File(argv[1]).getFullPathName() << ": " << types.size() << " effects\n";
  require(types.size()==2,"The single VST3 bundle must expose two effects");
  juce::StringArray names;
  for(auto* desc:types) {
   names.add(desc->name);
   juce::String error;
   auto instance=format.createInstanceFromDescription(*desc,48000,512,error);
   require(instance!=nullptr,error.toRawUTF8());
   require(instance->getName()==desc->name,"Wrong processor behind model CID");
   std::cout << desc->name << ": " << instance->getParameters().size() << " host parameters\n";
   require(instance->getParameters().size()==(desc->name=="dLab 376"?18:15),"Models alias parameter layouts");
   auto independent=format.createInstanceFromDescription(*desc,48000,512,error);
   require(independent!=nullptr,"Second instance failed");
   auto* gain=instance->getParameters()[0];
   const auto before=independent->getParameters()[0]->getValue();
   gain->setValueNotifyingHost(.65f);
   require(std::abs(independent->getParameters()[0]->getValue()-before)<1e-6f,"Instances share parameters");
   juce::MemoryBlock state;instance->getStateInformation(state);
   gain->setValueNotifyingHost(.1f);instance->setStateInformation(state.getData(),int(state.getSize()));
   require(std::abs(gain->getValue()-.65f)<1e-5f,"State recall failed");
   instance->prepareToPlay(48000,512);
   juce::AudioBuffer<float> buffer(2,512);juce::MidiBuffer midi;
   double energy=0;
   for(int block=0;block<4;++block) {
    for(int c=0;c<2;++c)for(int i=0;i<512;++i) buffer.setSample(c,i,float(.01*std::sin((block*512+i)*.09)));
    instance->processBlock(buffer,midi);
    for(int c=0;c<2;++c)for(int i=0;i<512;++i) {auto v=buffer.getSample(c,i);require(std::isfinite(v),"Nonfinite render");energy+=v*v;}
   }
   require(energy>1e-8,"Silent model");
   instance->releaseResources();
   auto editor=std::unique_ptr<juce::AudioProcessorEditor>(instance->createEditorIfNeeded());
   require(editor!=nullptr,"Missing editor");
   auto image=editor->createComponentSnapshot(editor->getLocalBounds());
   auto output=juce::File(argv[2]).getChildFile(desc->name+".png").createOutputStream();
   require(output!=nullptr,"Preview output");if(output){output->setPosition(0);output->truncate();}
   juce::PNGImageFormat png;require(png.writeImageToStream(image,*output),"Preview encode");
   instance->editorBeingDeleted(editor.get());
  }
  require(names.contains("dLab 286")&&names.contains("dLab 376"),"Display names");
  std::cout<<"PASS: one bundle, two independent models, state recall, native renders and both editors\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
