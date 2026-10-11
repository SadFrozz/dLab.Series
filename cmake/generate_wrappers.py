from pathlib import Path
import sys
juce,out=map(Path,sys.argv[1:3]);out.mkdir(parents=True,exist_ok=True)
client=juce/'modules/juce_audio_plugin_client'
def write_generated(path, data):
    encoded=data.encode()
    if not path.exists() or path.read_bytes()!=encoded:
        path.write_bytes(encoded)
def replace(s,a,b):
    assert s.count(a)==1,(a,s.count(a))
    return s.replace(a,b)
h=(client/'VST3/juce_VST3ModuleInfo.h').read_text()
h=replace(h,'bool includeFlagsAndCategory) noexcept\n            : info2', 'bool includeFlagsAndCategory, const char* displayName = JucePlugin_Name) noexcept\n            : info2')
h=replace(h,'                     JucePlugin_Name,','                     displayName,')
a=h.index('        static const ClassEntry classEntries[]');b=h.index('        return { classEntries',a)
h=h[:a]+'''        static const ClassEntry classEntries[] {
            { VST3Interface::jucePluginId(0x46727a7a,0x43323836,VST3Interface::Type::component), kVstAudioEffectClass,true,"dLab 286" },
            { VST3Interface::jucePluginId(0x46727a7a,0x43323836,VST3Interface::Type::controller), kVstComponentControllerClass,true,"dLab 286" },
            { VST3Interface::jucePluginId(0x46727a7a,0x43333736,VST3Interface::Type::component), kVstAudioEffectClass,true,"dLab 376" },
            { VST3Interface::jucePluginId(0x46727a7a,0x43333736,VST3Interface::Type::controller), kVstComponentControllerClass,true,"dLab 376" }
        };

'''+h[b:]
write_generated(out/'dLab_VST3ModuleInfo.h',h)
s=(client/'juce_audio_plugin_client_VST3.cpp').read_text()
s=replace(s,'#include <juce_audio_plugin_client/VST3/juce_VST3ModuleInfo.h>','#include "dLab_VST3ModuleInfo.h"\n#include "Processors.h"')
s=replace(s,'                       const RunLoop& l)\n        : scopedRunLoop (l),','                       const RunLoop& l, int model)\n        : scopedRunLoop (l),')
s=replace(s,'pluginInstance (createPluginFilterOfType (AudioProcessor::wrapperType_VST3).release()),','pluginInstance (dlab::createProcessor(model, AudioProcessor::wrapperType_VST3).release()),')
s=replace(s,'        memcpy (classID, JuceVST3EditController::iid, sizeof (TUID));','''        const auto code=pluginInstance->getName()=="dLab 376" ? 0x43333736u : 0x43323836u;
        const auto id=VST3Interface::jucePluginId(0x46727a7a,code,VST3Interface::Type::controller);
        memcpy(classID,id.data(),sizeof(TUID));''')
s=replace(s,'''        if (doUIDsMatch (entry.info2.cid, JuceVST3Component::iid))
            return static_cast<Vst::IAudioProcessor*> (new JuceVST3Component (host, runLoop));

        if (doUIDsMatch (entry.info2.cid, JuceVST3EditController::iid))
            return static_cast<Vst::IEditController*> (new JuceVST3EditController (host, runLoop));''','''        for(const int model : {286,376}) {
            const auto code=model==376 ? 0x43333736u : 0x43323836u;
            const auto component=toSteinbergUID(VST3Interface::jucePluginId(0x46727a7a,code,VST3Interface::Type::component));
            const auto controller=toSteinbergUID(VST3Interface::jucePluginId(0x46727a7a,code,VST3Interface::Type::controller));
            if(doUIDsMatch(entry.info2.cid,component))
                return static_cast<Vst::IAudioProcessor*>(new JuceVST3Component(host,runLoop,model));
            if(doUIDsMatch(entry.info2.cid,controller))
                return static_cast<Vst::IEditController*>(new JuceVST3EditController(host,runLoop));
        }''')
write_generated(out/'dLab_VST3.cpp',s)
s=(client/'juce_audio_plugin_client_AU_1.mm').read_text()
s=replace(s,'using namespace juce;','using namespace juce;\n#include "Processors.h"\nstatic std::unique_ptr<AudioProcessor> makeAUProcessor(AudioUnit unit) {\n    AudioComponentDescription desc{};\n    AudioComponentGetDescription(AudioComponentInstanceGetComponent(unit), &desc);\n    return dlab::createProcessor(desc.componentSubType==0x43333736u ? 376 : 286,AudioProcessor::wrapperType_AudioUnit);\n}')
s=replace(s,'    AudioProcessorHolder()','    explicit AudioProcessorHolder(AudioUnit component) : juceFilter(makeAUProcessor(component))')
s=replace(s,'    std::unique_ptr<AudioProcessor> juceFilter { createPluginFilterOfType (AudioProcessor::wrapperType_AudioUnit) };','    std::unique_ptr<AudioProcessor> juceFilter;')
s=replace(s,'        : MusicDeviceBase (component,','        : AudioProcessorHolder(component), MusicDeviceBase (component,')
write_generated(out/'dLab_AU_1.mm',s)
