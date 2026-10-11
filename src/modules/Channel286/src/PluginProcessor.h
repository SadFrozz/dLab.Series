#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Engine.h"
#include <juce_dsp/juce_dsp.h>
#include <ModelIdentities.h>

class Channel286Processor final : public juce::AudioProcessor {
public:
    Channel286Processor();
    void prepareToPlay(double,int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return channelSuite::channel286.name.data(); }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    juce::AudioProcessorValueTreeState parameters;
    std::array<std::atomic<float>,5> meters{};
    std::atomic<float> inputPeak{0};
    std::atomic<int> editorWidth{1600};
    std::atomic<int> guideScenario{0};
    static float rmsToDbu(float rms) noexcept;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
private:
    std::array<c286::Engine,3> engines;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>,2> oversamplers;
    std::array<int,3> pathLatency{};
    std::array<std::array<float,256>,2> delayMemory{};
    int delayPosition=0, activeQuality=-1, maximumBlock=1;
    float switchGain=1;
    std::array<std::atomic<float>*,14> raw{};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Channel286Processor)
};
