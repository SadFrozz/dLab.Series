#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
class Channel376Editor final:public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit Channel376Editor(Channel376Processor&);
    ~Channel376Editor()override;
    void paint(juce::Graphics&)override;
    void resized()override;
private:
    struct PanelLook final:juce::LookAndFeel_V4 {
        void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&)override;
        void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool)override;
        void drawButtonText(juce::Graphics&,juce::TextButton&,bool,bool)override;
        juce::Typeface::Ptr condensed;
    } look;
    Channel376Processor& processor;
    juce::TooltipWindow tooltip{this,500};
    juce::Component panel;
    std::array<juce::Slider,10> knobs;
    std::array<juce::TextButton,5> switches;
    std::array<juce::TextButton,1> modes;
    struct TubeButton final:juce::Button {
        TubeButton():Button("Tube Model"){}
        void paintButton(juce::Graphics& g,bool over,bool)override{
            if(over){g.setColour(juce::Colour(0xff758185));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),5,1);}
        }
    } tubeSelector;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,10> knobAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>,5> switchAttachments;
    juce::Typeface::Ptr wordmark;
    std::unique_ptr<juce::Drawable> brandLogo;
    void timerCallback()override {repaint();}
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Channel376Editor)
};
