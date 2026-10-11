#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static OSStatus input(void*,AudioUnitRenderActionFlags*,const AudioTimeStamp*,UInt32,UInt32 frames,AudioBufferList* data) {
 for(UInt32 c=0;c<data->mNumberBuffers;++c){auto* out=static_cast<float*>(data->mBuffers[c].mData);if(out)for(UInt32 i=0;i<frames;++i)out[i]=float(.01*std::sin(i*.09));}
 return noErr;
}
int main(int argc,char** argv){try{
 require(argc==2,"AU bundle path");
 auto path=CFStringCreateWithCString(nullptr,argv[1],kCFStringEncodingUTF8);
 auto url=CFURLCreateWithFileSystemPath(nullptr,path,kCFURLPOSIXPathStyle,true);CFRelease(path);
 auto bundle=CFBundleCreate(nullptr,url);CFRelease(url);require(bundle&&CFBundleLoadExecutable(bundle),"AU bundle load");
 auto entries=static_cast<CFArrayRef>(CFDictionaryGetValue(CFBundleGetInfoDictionary(bundle),CFSTR("AudioComponents")));
 require(entries&&CFArrayGetCount(entries)==2,"One AU bundle must register two models");
 for(int n=0;n<2;++n){
  auto entry=static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(entries,n));
  auto fnName=static_cast<CFStringRef>(CFDictionaryGetValue(entry,CFSTR("factoryFunction")));
  auto factory=reinterpret_cast<AudioComponentFactoryFunction>(CFBundleGetFunctionPointerForName(bundle,fnName));require(factory,"AU factory export");
  const int model=n==0?286:376;
  AudioComponentDescription desc{kAudioUnitType_Effect,model==286?0x43323836u:0x43333736u,0x46727a7au,0,0};
  auto name=static_cast<CFStringRef>(CFDictionaryGetValue(entry,CFSTR("name")));
  auto component=AudioComponentRegister(&desc,name,10000,factory);require(component,"Process-local registration");
  AudioUnit unit=nullptr;require(AudioComponentInstanceNew(component,&unit)==noErr&&unit,"AU model instantiate");
  UInt32 paramBytes=0;Boolean writable=false;
  require(AudioUnitGetPropertyInfo(unit,kAudioUnitProperty_ParameterList,kAudioUnitScope_Global,0,&paramBytes,&writable)==noErr,"AU parameter list");
  require(paramBytes/sizeof(AudioUnitParameterID)==(model==286?14u:17u),"AU models share wrong parameters");
  AudioStreamBasicDescription format{48000,kAudioFormatLinearPCM,kAudioFormatFlagIsFloat|kAudioFormatFlagIsPacked|kAudioFormatFlagIsNonInterleaved,4,1,4,2,32,0};
  require(AudioUnitSetProperty(unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input,0,&format,sizeof(format))==noErr,"Input format");
  require(AudioUnitSetProperty(unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Output,0,&format,sizeof(format))==noErr,"Output format");
  AURenderCallbackStruct cb{input,nullptr};require(AudioUnitSetProperty(unit,kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input,0,&cb,sizeof(cb))==noErr,"Input callback");
  require(AudioUnitInitialize(unit)==noErr,"AU initialize");
  struct StereoBuffers{UInt32 count=2;AudioBuffer data[2];} buffers;
  std::vector<float> left(512),right(512);buffers.data[0]={1,2048,left.data()};buffers.data[1]={1,2048,right.data()};
  AudioTimeStamp stamp{};stamp.mFlags=kAudioTimeStampSampleTimeValid;
  AudioUnitRenderActionFlags flags=0;double energy=0;
  for(int block=0;block<4;++block){stamp.mSampleTime=512*block;require(AudioUnitRender(unit,&flags,&stamp,0,512,reinterpret_cast<AudioBufferList*>(&buffers))==noErr,"AU render");for(float v:left){require(std::isfinite(v),"Nonfinite AU audio");energy+=v*v;}}
  require(energy>1e-8,"Silent AU model");
  AudioUnitUninitialize(unit);AudioComponentInstanceDispose(unit);
 }
 CFRelease(bundle);std::cout<<"PASS one AU component registers and renders both independent models\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
