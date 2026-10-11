#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
namespace dlab {
std::unique_ptr<juce::AudioProcessor> createProcessor(int model, juce::AudioProcessor::WrapperType type);
void setParameter(juce::AudioProcessor& processor,int index,float value);
}
