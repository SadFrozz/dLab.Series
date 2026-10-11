#include <juce_core/juce_core.h>
#include "../../deps/obs-vst-abi/aeffectx.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cmath>
#if JUCE_WINDOWS
#define NOMINMAX
#include <windows.h>
#endif
static void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
static intptr_t host(AEffect*,int op,int,intptr_t,void*,float){return op==audioMasterVersion?2400:op==audioMasterGetSampleRate?48000:0;}
int main(int argc,char** argv){try{
 require(argc==3,"DLL path and model");int model=std::atoi(argv[2]);
 juce::DynamicLibrary library;require(library.open(argv[1]),"DLL load");
 auto main=reinterpret_cast<AEffect*(*)(audioMasterCallback)>(library.getFunction("VSTPluginMain"));require(main!=nullptr,"VST2 entry");
 auto* a=main(host);auto* b=main(host);require(a&&b&&a!=b,"Independent instances");
 require(a->magic==CCONST('V','s','t','P')&&a->uniqueID==(model==286?CCONST('C','2','8','6'):CCONST('C','3','7','6')),"ABI and identity");
 require(a->numParams==(model==286?14:17)&&a->numInputs==2&&a->numOutputs==2,"Distinct parameter layouts");
 require(a->dispatcher(a,35,0,0,nullptr,0)==1,"OBS requires ordinary effect");
 char name[64]{};a->dispatcher(a,45,0,0,name,0);require(std::string(name)=="dLab "+std::to_string(model),"Display name");
 auto before=b->getParameter(b,0);a->setParameter(a,0,.65f);require(std::abs(b->getParameter(b,0)-before)<1e-6f,"Shared state");
 void* data=nullptr;auto size=a->dispatcher(a,23,0,0,&data,0);require(size>0&&data,"State chunk");
 std::vector<char> saved(static_cast<char*>(data),static_cast<char*>(data)+size);

 a->setParameter(a,0,.1f);a->dispatcher(a,24,0,saved.size(),saved.data(),0);require(std::abs(a->getParameter(a,0)-.65f)<1e-5f,"State recall");
 a->dispatcher(a,10,0,0,nullptr,48000);a->dispatcher(a,11,0,512,nullptr,0);a->dispatcher(a,12,0,1,nullptr,0);
 std::vector<float> left(20000),right(20000),outL(20000),outR(20000);
 for(int i=0;i<20000;++i)left[i]=right[i]=float(.01*std::sin(i*.09));float* ins[]{left.data(),right.data()};float* outs[]{outL.data(),outR.data()};
 a->processReplacing(a,ins,outs,20000);double energy=0;for(float v:outL){require(std::isfinite(v),"Finite render");energy+=v*v;}
 require(energy>1e-8,"Silent output");
 a->dispatcher(a,12,0,0,nullptr,0);
 #if JUCE_WINDOWS
 HWND parent=CreateWindowExW(0,L"STATIC",L"dLab VST2 test",WS_OVERLAPPEDWINDOW,-3000,-3000,1700,1000,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 require(parent!=nullptr,"Hidden editor host");
 for(int i=0;i<2;++i){require(a->dispatcher(a,14,0,0,parent,0)==1,"Editor open");require(a->dispatcher(a,15,0,0,nullptr,0)==1,"Editor close");}
 DestroyWindow(parent);
 #endif
 a->dispatcher(a,1,0,0,nullptr,0);b->dispatcher(b,1,0,0,nullptr,0);
 std::cout<<"PASS VST2 ABI, OBS-style host, model identity, state and large block render\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
