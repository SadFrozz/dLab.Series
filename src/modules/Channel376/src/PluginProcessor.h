#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "AnalogEngine376.h"
class Channel376Processor final:public juce::AudioProcessor {
public:
    Channel376Processor();
    void prepareToPlay(double,int)override;
    void releaseResources()override{}
    bool isBusesLayoutSupported(const BusesLayout&)const override;
    bool supportsDoublePrecisionProcessing()const override{return true;}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
    void processBlock(juce::AudioBuffer<double>&,juce::MidiBuffer&)override;
    void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
    void processBlockBypassed(juce::AudioBuffer<double>&,juce::MidiBuffer&)override;
    juce::AudioProcessorEditor* createEditor()override;
    bool hasEditor()const override{return true;}
    const juce::String getName()const override{return "dLab 376";}
    bool acceptsMidi()const override{return false;}
    bool producesMidi()const override{return false;}
    double getTailLengthSeconds()const override{return 1;}
    int getNumPrograms()override{return 1;}
    int getCurrentProgram()override{return 0;}
    void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return {};}
    void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock&)override;
    void setStateInformation(const void*,int)override;
    juce::AudioProcessorValueTreeState parameters;
    // outputdBFS,input rectifierV,GR voltage,EQclip,DE voltage,U5D comparator
    std::array<std::atomic<double>,6> meters{};
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    static const std::array<const char*,5> switchIDs;
private:
    template<class Sample>void render(juce::AudioBuffer<Sample>&,bool);
    channel376::AnalogParameters376 snapshot()const noexcept;
    std::array<std::atomic<float>*,17> raw{};
    std::unique_ptr<channel376::AnalogEngine376> engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Channel376Processor)
};
