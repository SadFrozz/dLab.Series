#include "Processors.h"
#include "../modules/Channel286/src/PluginProcessor.h"
#include "../modules/Channel376/src/PluginProcessor.h"
std::unique_ptr<juce::AudioProcessor> dlab::createProcessor(int model, juce::AudioProcessor::WrapperType type) {
    juce::PluginHostType::jucePlugInClientCurrentWrapperType=type;
    juce::AudioProcessor::setTypeOfNextNewPlugin(type);
    std::unique_ptr<juce::AudioProcessor> result;
    if(model==376) result=std::make_unique<Channel376Processor>();
    else if(model==286) result=std::make_unique<Channel286Processor>();
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    return result;
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return dlab::createProcessor(286,juce::PluginHostType::jucePlugInClientCurrentWrapperType).release();
}
void dlab::setParameter(juce::AudioProcessor& processor,int index,float value) {
    const auto& parameters=processor.getParameters();
    if(juce::isPositiveAndBelow(index,parameters.size())&&std::isfinite(value))
        parameters[index]->setValueNotifyingHost(juce::jlimit(0.f,1.f,value));
}
