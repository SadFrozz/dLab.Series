// Minimal VST2 interoperability bridge using the public VeSTige ABI header
// shipped by OBS. No proprietary Steinberg VST2 SDK is required.
#include "Processors.h"
#include "../../deps/obs-vst-abi/aeffectx.h"
#include <cstring>
#include <cmath>
#if JUCE_WINDOWS
#define NOMINMAX
#include <windows.h>
#endif
namespace {
struct Effect : AEffect { void (*processDoubleReplacing)(AEffect*,double**,double**,int); char future[56]; };
static_assert(sizeof(AEffect)==128,"Unexpected 64-bit VST2 ABI");
static_assert(sizeof(Effect)==192,"Unexpected extended VST2 ABI");
struct Rect { short top=0,left=0,bottom=0,right=0; };
struct Bridge final : juce::AudioProcessorListener, juce::ComponentListener {
 juce::ScopedJuceInitialiser_GUI gui;
 Effect effect{};
 audioMasterCallback host;
 std::unique_ptr<juce::AudioProcessor> processor;
 std::unique_ptr<juce::AudioProcessorEditor> editor;
 juce::AudioBuffer<float> floats;
 juce::AudioBuffer<double> doubles;
 juce::MemoryBlock chunk;
 juce::MidiBuffer midi;
 Rect rect;
 double rate=48000;
 int block=2048;
 bool running=false;
 inline static thread_local Bridge* settingFromHost=nullptr;
 explicit Bridge(audioMasterCallback h):host(h),processor(dlab::createProcessor(DLAB_VST2_MODEL,juce::AudioProcessor::wrapperType_VST)) {
  effect.magic=CCONST('V','s','t','P');effect.ptr3=this;
  effect.dispatcher=dispatch;effect.setParameter=set;effect.getParameter=get;
  effect.process=render<float>;effect.processReplacing=render<float>;effect.processDoubleReplacing=render<double>;
  effect.numPrograms=1;effect.numParams=processor->getParameters().size();effect.numInputs=effect.numOutputs=2;
  effect.flags=effFlagsHasEditor|effFlagsCanReplacing|effFlagsProgramChunks|(1<<12);
  effect.uniqueID=DLAB_VST2_MODEL==376?CCONST('C','3','7','6'):CCONST('C','2','8','6');effect.version=10000;
  processor->addListener(this);
 }
 ~Bridge()override {closeEditor();stop();processor->removeListener(this);}
 static Bridge& self(AEffect* e){return *static_cast<Bridge*>(e->ptr3);}
 static void copy(void* p,const juce::String& text,int size){if(p){std::strncpy(static_cast<char*>(p),text.toRawUTF8(),size-1);static_cast<char*>(p)[size-1]=0;}}
 static void set(AEffect* e,int i,float value){auto& b=self(e);const juce::ScopedValueSetter<Bridge*> guard(settingFromHost,&b);dlab::setParameter(*b.processor,i,value);}
 static float get(AEffect* e,int i){auto& p=self(e).processor->getParameters();return juce::isPositiveAndBelow(i,p.size())?p[i]->getValue():0.f;}
 void start() {
  stop();const auto capacity=std::max(block,8192);
  floats.setSize(2,capacity);doubles.setSize(2,capacity);
  processor->setRateAndBufferSizeDetails(rate,block);processor->prepareToPlay(rate,capacity);
  effect.initialDelay=processor->getLatencySamples();running=true;
 }
 void stop(){if(running){processor->releaseResources();running=false;}}
 template<typename T> static void render(AEffect* e,T** in,T** out,int count) {
  auto& b=self(e);if(count<=0)return;
  if(!b.running){for(int c=0;c<2;++c)std::copy(in[c],in[c]+count,out[c]);return;}
  // Hosts may deliver larger blocks; process bounded slices without allocating.
  juce::ScopedNoDenormals guard;
  auto& storage=[&]() -> juce::AudioBuffer<T>& {if constexpr(std::is_same_v<T,float>)return b.floats;else return b.doubles;}();
  for(int offset=0;offset<count;) {
   const int n=std::min(storage.getNumSamples(),count-offset);
   for(int c=0;c<2;++c)std::copy(in[c]+offset,in[c]+offset+n,storage.getWritePointer(c));
   juce::AudioBuffer<T> audio(storage.getArrayOfWritePointers(),2,n);
   b.midi.clear();b.processor->processBlock(audio,b.midi);
   for(int c=0;c<2;++c)std::copy(audio.getReadPointer(c),audio.getReadPointer(c)+n,out[c]+offset);
   offset+=n;
  }
 }
 void closeEditor(){if(editor){editor->removeComponentListener(this);editor->removeFromDesktop();processor->editorBeingDeleted(editor.get());editor.reset();}}
 static intptr_t dispatch(AEffect* e,int op,int index,intptr_t value,void* ptr,float opt) {
  auto& b=self(e);
  switch(op) {
   case 0:return 1; // open
   case 1:delete &b;return 1;
   case 2:return value==0;case 3:return 0;
   case 4:return 1;case 5:copy(ptr,"Default",24);return 1;
   case 6:case 7:case 8:{auto& p=b.processor->getParameters();if(!juce::isPositiveAndBelow(index,p.size()))return 0;copy(ptr,op==6?p[index]->getLabel():op==7?p[index]->getText(p[index]->getValue(),8):p[index]->getName(8),8);return 1;}
   case 10:if(std::isfinite(opt)&&opt>=8000){b.rate=opt;if(b.running)b.start();}return 1;
   case 11:if(value>0&&value<=1048576){b.block=int(value);if(b.running)b.start();}return 1;
   case 12:if(value)b.start();else b.stop();return 1;
   case 13:b.rect.right=short(b.editor?b.editor->getWidth():DLAB_VST2_MODEL==286?1600:1460);b.rect.bottom=short(b.editor?b.editor->getHeight():DLAB_VST2_MODEL==286?880:717);if(ptr)*static_cast<Rect**>(ptr)=&b.rect;return 1;
   case 14:b.closeEditor();b.editor.reset(b.processor->createEditorIfNeeded());if(!b.editor)return 0;b.editor->addComponentListener(&b);b.editor->addToDesktop(0,ptr);b.editor->setVisible(true);return 1;
   case 15:b.closeEditor();return 1;
   case 19:return 1;
   case 23:b.processor->getStateInformation(b.chunk);if(ptr)*static_cast<void**>(ptr)=b.chunk.getData();return b.chunk.getSize();
   case 24:if(ptr&&value>0&&value<16*1024*1024)b.processor->setStateInformation(ptr,int(value));return 1;
   case 26:return index>=0&&index<b.effect.numParams;
   case 29:copy(ptr,"Default",24);return 1;
   case 35:return 1; // ordinary effect, never shell
   case 45:copy(ptr,b.processor->getName(),32);return 1;
   case 47:copy(ptr,"Frozz",64);return 1;
   case 48:copy(ptr,b.processor->getName(),64);return 1;
   case 49:return 10000;
   case 51:if(ptr&&(std::strcmp(static_cast<char*>(ptr),"receiveVstTimeInfo")==0))return 0;return 0;
   case 58:return 2400;
   case 71:return 1;case 72:return 1;
   case 77:b.processor->setProcessingPrecision(value==1?juce::AudioProcessor::doublePrecision:juce::AudioProcessor::singlePrecision);return 1;
   default:return 0;
  }
 }
 void audioProcessorParameterChanged(juce::AudioProcessor*,int index,float v)override {if(host&&settingFromHost!=this)host(&effect,audioMasterAutomate,index,0,nullptr,v);}
 void audioProcessorChanged(juce::AudioProcessor*,const juce::AudioProcessorListener::ChangeDetails&)override {effect.initialDelay=processor->getLatencySamples();if(host)host(&effect,audioMasterIOChanged,0,0,nullptr,0);}
 void componentMovedOrResized(juce::Component& c,bool,bool resized)override {if(resized&&host)host(&effect,audioMasterSizeWindow,c.getWidth(),c.getHeight(),nullptr,0);}
};
}
#if JUCE_WINDOWS
#define DLAB_EXPORT extern "C" __declspec(dllexport)
extern "C" BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)juce::Process::setCurrentModuleInstanceHandle(instance);return TRUE;}
#else
#define DLAB_EXPORT extern "C" __attribute__((visibility("default")))
#endif
DLAB_EXPORT AEffect* VSTPluginMain(audioMasterCallback host) {
 try {if(!host||host(nullptr,audioMasterVersion,0,0,nullptr,0)==0)return nullptr;return &(new Bridge(host))->effect;}
 catch(...){return nullptr;}
}
