#include <juce_core/juce_core.h>
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/vst/ivstcomponent.h>

#include <cstring>
#include <iostream>
#include <stdexcept>
#if JUCE_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace {
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
std::array<char,16> componentId(bool controller=false) {
    // Frozen published JUCE/Frzz/C376 IDs, including the SDK's Windows GUID byte order.
   #if JUCE_WINDOWS
    const char* hex=controller ? "01EFCDAB3412CDAB46727A7A43333736" : "01EFCDAB8291EBFA46727A7A43333736";
   #else
    const char* hex=controller ? "ABCDEF011234ABCD46727A7A43333736" : "ABCDEF019182FAEB46727A7A43333736";
   #endif
    std::array<char,16> result{};
    for(int i=0;i<16;++i) result[i]=static_cast<char>(std::stoi(std::string(hex+2*i,2),nullptr,16));
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
    require(factory->countClasses()==2,"376 must export exactly its processor and controller");
    Steinberg::PFactoryInfo vendor{};
    require(factory->getFactoryInfo(&vendor)==Steinberg::kResultOk && std::strcmp(vendor.vendor,"Frozz")==0,"Wrong factory vendor");
    std::array<std::array<char,16>,2> classIds{};
    int processors=0,controllers=0;
    for(int i=0;i<factory->countClasses();++i) {
        Steinberg::PClassInfo info{};
        require(factory->getClassInfo(i,&info)==Steinberg::kResultOk,"Class enumeration failed");
        require(std::strcmp(info.name,"dLab 376")==0,"Wrong class name");
        std::memcpy(classIds[i].data(),info.cid,16);
        if(std::strcmp(info.category,"Audio Module Class")==0) {
            ++processors;require(classIds[i]==componentId(),"Published 376 processor CID changed");
        } else if(std::strcmp(info.category,"Component Controller Class")==0) {
            ++controllers;require(classIds[i]==componentId(true),"Published 376 controller CID changed");
        } else require(false,"Unexpected factory class category");
    }
    require(processors==1 && controllers==1 && classIds[0]!=classIds[1],"Duplicate or missing classes");
    const auto cid=componentId();
    const auto& componentInterface=INLINE_UID_OF(Steinberg::Vst::IComponent);
    Steinberg::Vst::IComponent* first=nullptr;
    Steinberg::Vst::IComponent* second=nullptr;
    require(factory->createInstance(cid.data(),componentInterface,reinterpret_cast<void**>(&first))==Steinberg::kResultOk && first,"Cannot instantiate 376");
    require(factory->createInstance(cid.data(),componentInterface,reinterpret_cast<void**>(&second))==Steinberg::kResultOk && second && first!=second,"Factory reused an instance");
    Steinberg::TUID controller{};
    require(first->getControllerClassId(controller)==Steinberg::kResultOk && std::memcmp(controller,componentId(true).data(),16)==0,"376 points to wrong controller");
    first->release();second->release();
    for(const std::string model:{"C286","C676"}) {
        auto missing=cid;
        for(int i=0;i<4;++i) missing[12+i]=model[i];
        void* result=nullptr;
        require(factory->createInstance(missing.data(),componentInterface,&result)!=Steinberg::kResultOk && result==nullptr,"Unimplemented model must not instantiate 376");
    }
    factory->release();
   #if JUCE_WINDOWS || JUCE_MAC
    require(exit(),"VST3 module exit failed");
   #endif
   #if JUCE_MAC
    CFRelease(bundle);
   #endif
    std::cout<<"PASS: real VST3 factory, frozen CIDs, independent instances, no 286/676 alias\n";
    return 0;
  } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n';return 1; }
}
