#include <juce_core/juce_core.h>
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <ModelIdentities.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#if JUCE_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace {
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
std::array<char,16> componentId(int model,bool controller=false) {
    // Frozen published JUCE/Frzz/C286 IDs, including the SDK's Windows GUID byte order.
   #if JUCE_WINDOWS
    const char* hex=controller ? "01EFCDAB3412CDAB46727A7A43323836" : "01EFCDAB8291EBFA46727A7A43323836";
   #else
    const char* hex=controller ? "ABCDEF011234ABCD46727A7A43323836" : "ABCDEF019182FAEB46727A7A43323836";
   #endif
    std::array<char,16> result{};
    for(int i=0;i<16;++i) result[i]=static_cast<char>(std::stoi(std::string(hex+2*i,2),nullptr,16));
    if(model==376) {result[12]='C';result[13]='3';result[14]='7';result[15]='6';}
    return result;
}
}

int main(int argc,char** argv) {
  try {
    require(argc==2,"Expected VST3 binary path");
    juce::DynamicLibrary library;
    require(library.open(argv[1]),"Cannot load built VST3 binary");
   #if JUCE_WINDOWS
    using Entry=bool (*)();
    auto entry=reinterpret_cast<Entry>(library.getFunction("InitDll"));
    auto exit=reinterpret_cast<Entry>(library.getFunction("ExitDll"));
    require(entry && exit && entry(),"VST3 module entry failed");
   #elif JUCE_MAC
    const auto bundlePath=juce::File(argv[1]).getParentDirectory().getParentDirectory().getParentDirectory().getFullPathName();
    auto path=CFStringCreateWithCString(kCFAllocatorDefault,bundlePath.toRawUTF8(),kCFStringEncodingUTF8);
    auto url=CFURLCreateWithFileSystemPath(kCFAllocatorDefault,path,kCFURLPOSIXPathStyle,true);
    auto bundle=CFBundleCreate(kCFAllocatorDefault,url);
    CFRelease(url);CFRelease(path);
    using Entry=bool (*)(CFBundleRef);
    auto entry=reinterpret_cast<Entry>(library.getFunction("bundleEntry"));
    auto exit=reinterpret_cast<bool (*)()>(library.getFunction("bundleExit"));
    require(bundle && entry && exit && entry(bundle),"VST3 bundle entry failed");
   #endif
    using FactoryFunction=Steinberg::IPluginFactory* (PLUGIN_API *)();
    auto getFactory=reinterpret_cast<FactoryFunction>(library.getFunction("GetPluginFactory"));
    require(getFactory!=nullptr,"Missing VST3 factory");
    auto* factory=getFactory();require(factory!=nullptr,"Null VST3 factory");
    require(factory->countClasses()==4,"Expected two processor/controller pairs");
    int processors=0,controllers=0;
    for(int i=0;i<4;++i) {
        Steinberg::PClassInfo info{};
        require(factory->getClassInfo(i,&info)==Steinberg::kResultOk,"Class enumeration");
        const int model=std::strcmp(info.name,"dLab 376")==0?376:286;
        require(std::strcmp(info.name,model==376?"dLab 376":"dLab 286")==0,"Display name");
        const bool ctrl=std::strcmp(info.category,"Component Controller Class")==0;
        const auto expected=componentId(model,ctrl);
        require(std::memcmp(info.cid,expected.data(),16)==0,"Published CID changed");
        if(ctrl)++controllers;else ++processors;
    }
    require(processors==2&&controllers==2,"Factory layout");
    for(int model:{286,376}) {
        const auto cid=componentId(model);
        Steinberg::Vst::IComponent* component=nullptr;
        const auto& iid=INLINE_UID_OF(Steinberg::Vst::IComponent);
        require(factory->createInstance(cid.data(),iid,reinterpret_cast<void**>(&component))==Steinberg::kResultOk&&component,"Model instance");
        Steinberg::TUID controller{};
        require(component->getControllerClassId(controller)==Steinberg::kResultOk&&std::memcmp(controller,componentId(model,true).data(),16)==0,"Wrong controller for model");
        component->release();
    }
    factory->release();
   #if JUCE_WINDOWS || JUCE_MAC
    require(exit(),"VST3 module exit failed");
   #endif
   #if JUCE_MAC
    CFRelease(bundle);
   #endif
    std::cout<<"PASS: shared factory preserves all four published CIDs and model/controller routing\n";
    return 0;
  } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n';return 1; }
}
