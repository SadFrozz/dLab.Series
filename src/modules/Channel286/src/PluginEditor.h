#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class RackSlider final : public juce::Slider {
public:
    std::vector<std::pair<double,double>> scalePoints; // value, dial proportion
    double valueToProportionOfLength(double value) override;
    double proportionOfLengthToValue(double proportion) override;
};

class ProjectOneLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    bool sMode=false;
    juce::Colour panelInk() const { return juce::Colour(sMode?0xff232526:0xffe3e8e9); }
    juce::Colour accent() const { return juce::Colour(sMode?0xffbd2730:0xff73bff3); }
    void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool) override;
    void drawButtonText(juce::Graphics&,juce::TextButton&,bool,bool) override;
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
    void drawToggleButton(juce::Graphics&,juce::ToggleButton&,bool,bool) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String&,juce::Point<int>,juce::Rectangle<int>) override;
    void drawTooltip(juce::Graphics&,const juce::String&,int,int) override;
    juce::Font getLabelFont(juce::Label&) override;
};

class Channel286Editor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit Channel286Editor(Channel286Processor&);
    ~Channel286Editor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshFromProcessor();
private:
    void applyGuide();
    void updateModelAppearance();
    void timerCallback() override { refreshFromProcessor();repaint(); }
    Channel286Processor& processor;
    ProjectOneLookAndFeel theme;
    juce::TooltipWindow tooltips{this,500};
    std::array<RackSlider,10> knobs;
    std::array<juce::Label,10> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,10> attachments;
    juce::ToggleButton hp{"80 Hz HPF"}, bypass{"Process Bypass"};
    juce::TextButton modelSwitch{"S"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hpAttachment,bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> modelAttachment;
    juce::ComboBox quality, guideScenario;
    int lastAppliedGuide=-1;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qualityAttachment;
    juce::Image panelTexture,silverTexture;
    std::unique_ptr<juce::Drawable> brandLogo;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Channel286Editor)
};
