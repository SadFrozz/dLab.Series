#include "dlab/BrandLogo.h"
#include "dsp/InputGain.h"
#include "PluginEditor.h"

#include <Channel286Fonts.h>



namespace {

constexpr float designWidth=1600.f, designHeight=880.f;

constexpr float gridLeft=20.f, cellWidth=520.f;

constexpr int dialCentre(int column,int slot) { return 155+column*520+slot*245; }

const juce::Colour ink(0xffe3e8e9), blue(0xff73bff3);

enum class Face { caption, section, scale, wordmark, readout };

juce::Font panelFont(float size,Face face=Face::caption) {

    static const std::array<juce::Typeface::Ptr,5> faces {

        juce::Typeface::createSystemTypefaceFor(Channel286Fonts::BarlowCondensedBold_ttf,Channel286Fonts::BarlowCondensedBold_ttfSize),

        juce::Typeface::createSystemTypefaceFor(Channel286Fonts::BarlowCondensedExtraBold_ttf,Channel286Fonts::BarlowCondensedExtraBold_ttfSize),

        juce::Typeface::createSystemTypefaceFor(Channel286Fonts::BarlowCondensedSemiBold_ttf,Channel286Fonts::BarlowCondensedSemiBold_ttfSize),

        juce::Typeface::createSystemTypefaceFor(Channel286Fonts::BarlowLightItalic_ttf,Channel286Fonts::BarlowLightItalic_ttfSize),

        juce::Typeface::createSystemTypefaceFor(Channel286Fonts::BarlowRegular_ttf,Channel286Fonts::BarlowRegular_ttfSize)

    };

    return juce::Font(juce::FontOptions(faces[static_cast<size_t>(face)]).withHeight(size));

}

void text(juce::Graphics& g,const juce::String& s,float x,float y,float w,float h,float size,

          juce::Justification align=juce::Justification::centred,Face face=Face::caption) {

    g.setFont(panelFont(size,face));

    g.drawFittedText(s,juce::Rectangle<float>(x,y,w,h).toNearestInt(),align,s.containsChar('\n')?2:1,.9f);

}

void led(juce::Graphics& g,float x,float y,juce::Colour colour,bool on,float r=5.f) {

    g.setColour(juce::Colour(0xff080b0c)); g.fillEllipse(x-r-2,y-r-2,2*r+4,2*r+4);

    if(on) { g.setColour(colour.withAlpha(.13f)); g.fillEllipse(x-r-5,y-r-5,2*r+10,2*r+10); }

    const auto base=on?colour:colour.darker(2.3f);

    g.setGradientFill(juce::ColourGradient(base.brighter(.4f),x-r*.4f,y-r*.5f,base.darker(.7f),x+r,y+r,false));

    g.fillEllipse(x-r,y-r,2*r,2*r);

    g.setColour(juce::Colours::white.withAlpha(on?.65f:.12f)); g.fillEllipse(x-r*.5f,y-r*.6f,r*.7f,r*.5f);

}

void screw(juce::Graphics& g,float x,float y) {

    g.setColour(juce::Colours::black.withAlpha(.5f)); g.fillEllipse(x-9,y-7,18,18);

    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff526064),x-7,y-7,juce::Colour(0xff080a0b),x+7,y+7,false));

    g.fillEllipse(x-7,y-7,14,14);

    g.setColour(juce::Colour(0xff030506)); g.drawLine(x-4,y+2,x+4,y-2,2.f);

}

}



double RackSlider::valueToProportionOfLength(double value) {

    if(scalePoints.empty()) return Slider::valueToProportionOfLength(value);

    if(value<=scalePoints.front().first) return scalePoints.front().second;

    for(size_t i=1;i<scalePoints.size();++i) if(value<=scalePoints[i].first) {

        const auto a=scalePoints[i-1],b=scalePoints[i];

        return a.second+(value-a.first)/(b.first-a.first)*(b.second-a.second);

    }

    return scalePoints.back().second;

}

double RackSlider::proportionOfLengthToValue(double proportion) {

    if(scalePoints.empty()) return Slider::proportionOfLengthToValue(proportion);

    if(proportion<=scalePoints.front().second) return scalePoints.front().first;

    for(size_t i=1;i<scalePoints.size();++i) if(proportion<=scalePoints[i].second) {

        const auto a=scalePoints[i-1],b=scalePoints[i];

        return a.first+(proportion-a.second)/(b.second-a.second)*(b.first-a.first);

    }

    return scalePoints.back().first;

}



void ProjectOneLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,

 float pos,float start,float end,juce::Slider& slider) {
    const auto ink=panelInk(),blue=accent();

    const float cx=x+w*.5f, cy=y+h*.5f+8;

    const float radius=juce::jmin(w*.23f,h*.26f), scaleRadius=radius+14;

    auto point=[&](float a,float r) {return juce::Point<float>{cx+std::sin(a)*r,cy-std::cos(a)*r};};

    const bool guided=slider.getProperties().contains("guideStart");
    const float guideStart=static_cast<float>(slider.getProperties()["guideStart"]);
    const float guideEnd=static_cast<float>(slider.getProperties()["guideEnd"]);
    auto recommended=[&](float angle) { return guided && angle>=guideStart-.001f && angle<=guideEnd+.001f; };
    const bool singlePoint=guided && std::abs(guideEnd-guideStart)<.001f;
    if(guided) {
        g.setColour(blue.withAlpha(singlePoint?1.f:.8f));
        if(singlePoint) g.drawLine({point(guideStart,scaleRadius-8),point(guideStart,scaleRadius+8)},4.f);
        else {
            juce::Path arc; arc.addCentredArc(cx,cy,scaleRadius+1,scaleRadius+1,0,guideStart,guideEnd,true);
            g.strokePath(arc,juce::PathStrokeType(9.f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        }
    }
    g.setColour(ink.withAlpha(.8f));

    const int type=static_cast<int>(slider.getProperties()["scaleType"]);

    std::vector<std::pair<double,juce::String>> marks;

    if(type==0) {
        const auto values=c286::lineGainMarks(sMode);
        for(double v:values) marks.emplace_back(v,v>0?"+"+juce::String(v,0):juce::String(v,0));
    }

    else if(type==7) marks={{-60,"OFF"},{-15,"-15"},{15,"+15"}};

    else if(type==8) marks={{1,"MIN"},{1.5,"1.5:1"},{2,"2:1"},{5,"5:1"},{10,"10:1"}};

    else if(type==9) marks={{-30,"-30"},{-10,"-10"},{0,"0"},{5,"+5"},{10,"+10"}};

    else if(type==3) marks={{800,"800"},{1000,"1k"},{4000,"4k"},{8000,"8k"},{10000,"10k"}};

    else for(int i=0;i<=10;++i) marks.emplace_back(i,i==0?juce::String("OFF"):juce::String(i));

    if(type==3) for(int i=0;i<21;++i) {

        const float a=start+(end-start)*i/20.f;

        const bool major=i%5==0;

        g.setColour(recommended(a)?blue:ink.withAlpha(major?1.f:.7f));

        g.drawLine({point(a,scaleRadius-(major?3.f:1.f)),point(a,scaleRadius+(major?3.f:2.f))},major?1.7f:1.f);

    }

    else for(size_t i=1;i<marks.size();++i) {

        const double u=(slider.valueToProportionOfLength(marks[i-1].first)+slider.valueToProportionOfLength(marks[i].first))*.5;

        const float a=start+(end-start)*static_cast<float>(u);

        g.setColour(recommended(a)?blue:ink.withAlpha(.6f));g.drawLine({point(a,scaleRadius-1),point(a,scaleRadius+2)},1.f);

    }

    for(const auto& mark:marks) {

        const float a=start+(end-start)*static_cast<float>(slider.valueToProportionOfLength(mark.first));

        if(type!=3) {g.setColour(recommended(a)?blue:ink);g.drawLine({point(a,scaleRadius-3),point(a,scaleRadius+3)},1.7f);}

        const auto p=point(a,scaleRadius+23);

        g.setColour(recommended(a)?blue:ink);
        text(g,mark.second,p.x-28,p.y-15,56,30,23.f,juce::Justification::centred,Face::scale);

    }

    g.setColour(juce::Colours::black.withAlpha(.6f));

    g.fillEllipse(cx-radius-2,cy-radius+3,2*radius+4,2*radius+4);

    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff454c4b),cx-radius,cy-radius,

       juce::Colour(0xff080a0b),cx+radius,cy+radius,false));

    g.fillEllipse(cx-radius,cy-radius,2*radius,2*radius);

    for(int i=0;i<30;++i) {

        const float a=i*juce::MathConstants<float>::twoPi/30;

        g.setColour(i%2?juce::Colour(0xff090c0d):juce::Colour(0xff505755).withAlpha(.6f));

        g.drawLine({point(a,radius*.83f),point(a,radius*.97f)},2.4f);

    }

    const float inner=radius*.77f;

    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a5251),cx-inner,cy-inner,

       juce::Colour(0xff1a2020),cx+inner,cy+inner,false));

    g.fillEllipse(cx-inner,cy-inner,2*inner,2*inner);

    g.setColour(juce::Colours::white.withAlpha(.08f));

    g.drawEllipse(cx-inner,cy-inner,2*inner,2*inner,1.f);

    if(guided && !singlePoint) {
        juce::Path sector; sector.startNewSubPath(cx,cy);
        sector.lineTo(point(guideStart,inner));
        for(int step=1;step<=40;++step) sector.lineTo(point(guideStart+(guideEnd-guideStart)*step/40.f,inner));
        sector.closeSubPath(); g.setColour(blue.withAlpha(.22f)); g.fillPath(sector);
    }
    const juce::String guideNote=slider.getProperties()["guideNote"].toString();
    if(guideNote.isNotEmpty()) {
        g.setColour(blue);
        text(g,guideNote,cx-30,cy+radius+5,60,22,19.f,juce::Justification::centred,Face::caption);
    }
    const float a=start+pos*(end-start);

    const auto from=point(a,radius*.12f), to=point(a,radius*.68f);

    g.setColour(juce::Colours::black.withAlpha(.65f));

    g.drawLine(from.x+1,from.y+2,to.x+1,to.y+2,6.f);

    const auto colour=slider.findColour(juce::Slider::thumbColourId);

    g.setColour(colour.darker(.5f)); g.drawLine({from,to},5.f);

    g.setColour(colour); g.drawLine({from,to},3.f);

    g.fillEllipse(from.x-1.5f,from.y-1.5f,3,3);

}

void ProjectOneLookAndFeel::drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& colour,bool over,bool down) {
    if(b.getName()!="modelS") { LookAndFeel_V4::drawButtonBackground(g,b,colour,over,down);return; }
    if(over || down) {g.setColour(accent().withAlpha(down?.2f:.1f));g.fillRoundedRectangle(b.getLocalBounds().toFloat(),5.f);}
}
void ProjectOneLookAndFeel::drawButtonText(juce::Graphics& g,juce::TextButton& b,bool over,bool down) {
    if(b.getName()!="modelS") { LookAndFeel_V4::drawButtonText(g,b,over,down);return; }
    const auto colour=panelInk().withAlpha(over || down?1.f:.85f);
    g.setColour(colour);
    // Two opposing arrows, drawn as vectors so they do not depend on font coverage.
    g.drawLine(3.f,23.f,25.f,23.f,2.f);
    g.drawLine(25.f,23.f,19.f,18.f,2.f);g.drawLine(25.f,23.f,19.f,28.f,2.f);
    g.drawLine(25.f,38.f,3.f,38.f,2.f);
    g.drawLine(3.f,38.f,9.f,33.f,2.f);g.drawLine(3.f,38.f,9.f,43.f,2.f);
    if(b.getToggleState()) {
        juce::Path one;
        one.startNewSubPath(48,17);one.lineTo(73,5);one.lineTo(61,61);
        one.lineTo(39,61);one.lineTo(43,55);one.lineTo(48,55);
        one.lineTo(55,21);one.lineTo(48,25);one.closeSubPath();
        g.setColour(juce::Colour(0xff73bff3));g.fillPath(one,juce::AffineTransform::translation(2.f,1.f));
        g.setColour(colour);g.fillPath(one);
    } else {
        juce::GlyphArrangement letters;letters.addFittedText(panelFont(54.f,Face::wordmark),"S",34,0,43,65,juce::Justification::centred,1);
        juce::Path letter;letters.createPath(letter);g.fillPath(letter);g.strokePath(letter,juce::PathStrokeType(.65f));
    }
}

void ProjectOneLookAndFeel::drawToggleButton(juce::Graphics& g,juce::ToggleButton& button,bool over,bool down) {

    const float cx=button.getWidth()*.32f,cy=button.getHeight()*.5f,r=18.f;

    g.setColour(juce::Colour(0xff050708)); g.fillEllipse(cx-r-3,cy-r-3,2*r+6,2*r+6);

    g.setGradientFill(juce::ColourGradient(juce::Colour(down?0xff202828:0xff414a4a),cx-r,cy-r,

      juce::Colour(0xff0c1112),cx+r,cy+r,false)); g.fillEllipse(cx-r,cy-r,2*r,2*r);

    g.setColour(juce::Colour(0xff66716f).withAlpha(over?.8f:.35f)); g.drawEllipse(cx-r+3,cy-r+3,2*r-6,2*r-6,1.f);

    led(g,cx+47,cy,button.getName()=="hpf"?juce::Colour(0xff59db65):juce::Colour(0xffff4436),button.getToggleState(),7.f);

}

juce::Font ProjectOneLookAndFeel::getComboBoxFont(juce::ComboBox&) { return panelFont(25.f); }
namespace {
juce::TextLayout tooltipLayout(const juce::String& message,juce::Colour textColour=ink) {
    juce::AttributedString content;
    content.append(message);
    content.setFont(juce::Font(juce::FontOptions(22.f)));
    content.setColour(textColour);
    juce::TextLayout layout; layout.createLayout(content,520.f);
    return layout;
}
}
juce::Rectangle<int> ProjectOneLookAndFeel::getTooltipBounds(const juce::String& message,juce::Point<int> position,juce::Rectangle<int> area) {
    const auto layout=tooltipLayout(message);
    const int width=juce::roundToInt(layout.getWidth())+28,height=juce::roundToInt(layout.getHeight())+24;
    return juce::Rectangle<int>(position.x>area.getCentreX()?position.x-width-12:position.x+24,
      position.y>area.getCentreY()?position.y-height-12:position.y+20,width,height).constrainedWithin(area);
}
void ProjectOneLookAndFeel::drawTooltip(juce::Graphics& g,const juce::String& message,int width,int height) {
    g.fillAll(juce::Colour(sMode?0xffdededc:0xff192224));
    g.setColour(accent()); g.drawRect(0,0,width,height,1);
    tooltipLayout(message,panelInk()).draw(g,juce::Rectangle<float>(14.f,12.f,width-28.f,height-24.f));
}
juce::Font ProjectOneLookAndFeel::getLabelFont(juce::Label& label) {

    if(dynamic_cast<juce::Slider*>(label.getParentComponent())) return panelFont(24.f,Face::readout);

    return label.getFont();

}



Channel286Editor::Channel286Editor(Channel286Processor& p):AudioProcessorEditor(&p),processor(p) {

    theme.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);

    theme.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);

    theme.setColour(juce::Slider::textBoxTextColourId,ink.withAlpha(.85f));

    theme.setColour(juce::Label::textColourId,ink);

    setLookAndFeel(&theme);
    tooltips.setLookAndFeel(&theme);

    panelTexture=juce::Image(juce::Image::RGB,1600,880,true);
    silverTexture=juce::Image(juce::Image::RGB,1600,880,true);

    brandLogo=dlab::makeLogo();
    juce::Random random(286);

    for(int y=0;y<panelTexture.getHeight();++y) for(int x=0;x<panelTexture.getWidth();++x) {

        const int v=43+random.nextInt(10);

        panelTexture.setPixelAt(x,y,juce::Colour(static_cast<juce::uint8>(v),static_cast<juce::uint8>(v+5),static_cast<juce::uint8>(v+5)));
        const auto silver=static_cast<juce::uint8>(196+random.nextInt(5));
        silverTexture.setPixelAt(x,y,juce::Colour(silver,silver,silver));

    }

    const char* ids[]{"input","drive","density","frequency","deThreshold","lf","hf","gateThreshold","gateRatio","output"};

    const char* names[]{"LINE GAIN (dB)","DRIVE","DENSITY","FREQUENCY (Hz)","THRESHOLD","LF DETAIL","HF DETAIL","THRESHOLD (dB)","EXPANSION RATIO","GAIN (dB)"};

    const char* tips[]{
      "LINE GAIN\nSets the gain at the start of the LINE signal path. Project One: -5 to +45 dB; 286S: -15 to +45 dB. Both scales put +15 dB at noon. Leave headroom for loud phrases. Double-click resets to 0 dB.",
      "DRIVE\nFor smoother vocals, start with moderate compression. Higher Drive helps bring a signal forward in a mix and adds sustain, but raises noise between phrases. OFF disables the compressor. Harman: pp. 7, 14-16.",
      "DENSITY\nLower settings give a slower release for smooth vocals; higher settings track percussion more closely. Harman starts at 0 in its vocal example. Reduce Density if bass notes distort. Harman: pp. 7, 14-16.",
      "DE-ESSER FREQUENCY\nHarman recommends 4-8 kHz for vocals, with 4 kHz as the starting point in its vocal example. Adjust the frequency to match the sibilance of the individual voice. Harman: pp. 7, 12, 14.",
      "DE-ESSER THRESHOLD\nIncrease sensitivity only enough to control excessive sibilance. Too much processing can cause a lisp. Harman recommends processing an individual voice or instrument rather than a complete mix. OFF disables the de-esser. Harman: pp. 12, 17.",
      "LF DETAIL\nAdds body to voices and percussion. Harman's table does not recommend LF Detail for female vocals; use it as needed for male vocals, and it is recommended for bass guitar. LF Detail is off in the vocal example. Avoid excessive boominess. Harman: pp. 12, 14-15, 17.",
      "HF DETAIL\nAdd clarity to dull voices or instruments by ear. Use a low setting or OFF for material that is already bright. Reduce HF Detail if hiss or harshness becomes more noticeable. Harman: pp. 12, 14-17.",
      "GATE THRESHOLD\nSet the threshold so that every word and wanted decay passes through completely. In its vocal example, Harman suggests starting around 11 o'clock and adjusting for background noise. A threshold that is too high cuts off endings. OFF disables the gate. Harman: pp. 13-14, 16.",
      "EXPANSION RATIO\nUse gentle expansion for vocals and wind instruments, and stronger expansion up to gating for dry percussion. Preserve the decay of cymbals and piano. Reduce Ratio if you hear pumping or cut-off endings. Harman: pp. 8, 13-14, 16.",
      "OUTPUT GAIN\nCompensate for gain added by processing and watch CLIP. If CLIP stays on, reduce Drive, Detail, or LINE GAIN. Process Bypass also bypasses Output Gain. Harman: p. 9."};
    const juce::Colour colours[]{ink,juce::Colour(0xffed4c42),blue,blue,juce::Colour(0xffed8937),

      juce::Colour(0xff49b466),blue,juce::Colour(0xffed4c42),blue,juce::Colour(0xff49b466)};

    const double defaults[]{0,0,5,4000,0,0,0,-15,1,0};

    for(size_t i=0;i<knobs.size();++i) {

        auto& k=knobs[i]; k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);

        k.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);

        k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,150,34);

        k.setTooltip(juce::String::fromUTF8(tips[i])); k.setName(ids[i]); k.setNumDecimalPlacesToDisplay(i==3?0:1);

        k.setColour(juce::Slider::thumbColourId,colours[i]); k.getProperties().set("scaleType",static_cast<int>(i));

        k.setDoubleClickReturnValue(true,defaults[i]);

        if(i==0 || i==9) k.setTextValueSuffix(" dB");

        if(i==3) k.setTextValueSuffix(" Hz");

        if(i==7) k.setTextValueSuffix(" dB");

        if(i==8) k.setTextValueSuffix(":1");

        addAndMakeVisible(k);

        labels[i].setText(names[i],juce::dontSendNotification);

        labels[i].setJustificationType(juce::Justification::centred);

        labels[i].setFont(panelFont(28.f)); addAndMakeVisible(labels[i]);

        attachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters,ids[i],k);

    }

    knobs[0].scalePoints={{-5,0},{0,.25},{15,.5},{25,.75},{45,1}};

    knobs[3].scalePoints={{800,0},{1000,.25},{4000,.5},{8000,.75},{10000,1}};

    knobs[7].scalePoints={{-60,0},{-15,.5},{15,1}};

    knobs[8].scalePoints={{1,0},{1.5,.25},{2,.5},{5,.75},{10,1}};

    knobs[9].scalePoints={{-30,0},{-10,.25},{0,.5},{5,.75},{10,1}};

    hp.setName("hpf"); bypass.setName("processBypass");

    addAndMakeVisible(hp); addAndMakeVisible(bypass);

    hpAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters,"hpf",hp);

    bypassAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters,"processBypass",bypass);

    hp.setTooltip("80 Hz high-pass filter.");

    bypass.setTooltip("Routes input gain/HPF directly to output, including bypass of Output Gain.");
    modelSwitch.setName("modelS");modelSwitch.setClickingTogglesState(true);
    modelSwitch.onStateChange=[this] { updateModelAppearance(); };
    modelAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters,"modelS",modelSwitch);
    addAndMakeVisible(modelSwitch);

    guideScenario.setName("guideScenario");
    guideScenario.addItem("Guide: off",1);
    guideScenario.addSectionHeading("COMPRESSOR / Harman p.15");
    guideScenario.addItem("Comp: vocal smoothing",2);
    guideScenario.addItem("Comp: lift a signal out of a mix",3);
    guideScenario.addItem("Comp: kick / snare body",4);
    guideScenario.addItem("Comp: guitar / strings sustain",5);
    guideScenario.addSectionHeading("EXPANDER/GATE / Harman p.14");
    guideScenario.addItem("Gate: dry percussion",6);
    guideScenario.addItem("Gate: long decay (cymbal / piano)",7);
    guideScenario.addItem("Gate: hum / buzz",8);
    guideScenario.addItem("Gate: gentle vocal / woodwind expansion",9);
    guideScenario.addSectionHeading("TEXT GUIDANCE / Harman pp.7,12-14");
    guideScenario.addItem("De-esser: vocals (4-8 kHz)",10);
    guideScenario.addItem("Vocal processing example",11);
    guideScenario.addSectionHeading("ENHANCER USAGE / Harman p.15");
    const char* detailSources[]{"female voice","male voice","keyboard","guitar","bass guitar","drums","strings / horns","club / DJ"};
    for(int i=0;i<8;++i) guideScenario.addItem(juce::String("Detail: ")+detailSources[i],12+i);
    guideScenario.setTooltip("Choose an example from the Harman manual: blue sectors show recommended ranges, and individual marks show starting positions. Selecting an example does not change knob values. BY EAR means adjust by listening; OPTIONAL means use processing only if the material needs it. Guide: off removes the highlights.");
    guideScenario.onChange=[this] { processor.guideScenario.store(guideScenario.getSelectedId()-1); applyGuide(); };
    guideScenario.setSelectedId(processor.guideScenario.load()+1,juce::dontSendNotification);
    addAndMakeVisible(guideScenario); applyGuide();
    quality.addItemList(juce::StringArray{"Standard 1x","High 2x","Ultra 4x"},1);

    quality.setTooltip("Oversampling of the complete DSP chain. Higher quality reduces aliases during overload at greater CPU cost. Switching resets detectors and uses a brief fade-in; prefer changing while stopped.");

    qualityAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,"quality",quality);

    addAndMakeVisible(quality);
    updateModelAppearance();

    setResizable(true,true); setResizeLimits(1440,792,2000,1100);

    getConstrainer()->setFixedAspectRatio(designWidth/designHeight);

    setSize(processor.editorWidth.load(),juce::roundToInt(processor.editorWidth.load()*designHeight/designWidth)); startTimerHz(30);

}

Channel286Editor::~Channel286Editor() { stopTimer(); setLookAndFeel(nullptr); }

void Channel286Editor::refreshFromProcessor() {
    const int selected=processor.guideScenario.load()+1;
    if(lastAppliedGuide!=selected-1) { guideScenario.setSelectedId(selected,juce::dontSendNotification);applyGuide(); }
    updateModelAppearance();
}

void Channel286Editor::updateModelAppearance() {
    const bool modern=modelSwitch.getToggleState();
    if(theme.sMode==modern && modelSwitch.getTooltip().isNotEmpty()) return;
    theme.sMode=modern;
    auto& input=knobs[0];
    const double requested=processor.parameters.getRawParameterValue("input")->load();
    const auto values=c286::lineGainMarks(modern);
    input.scalePoints.clear();
    for(size_t i=0;i<values.size();++i)input.scalePoints.emplace_back(values[i],i/4.);
    input.setRange(c286::lineGainMinimum(modern),c286::lineGainMaximum,.01);
    input.setValue(c286::effectiveLineGain(static_cast<float>(requested),modern),juce::dontSendNotification);
    const auto ink=theme.panelInk();
    theme.setColour(juce::Label::textColourId,ink);
    theme.setColour(juce::Slider::textBoxTextColourId,ink);
    for(auto& label:labels) label.setColour(juce::Label::textColourId,ink);
    const juce::Colour colours[]{juce::Colour(0xffe3e8e9),juce::Colour(0xffed4c42),blue,blue,
      juce::Colour(0xffed8937),juce::Colour(0xff49b466),blue,juce::Colour(0xffed4c42),blue,juce::Colour(0xff49b466)};
    for(size_t i=0;i<knobs.size();++i) {
        knobs[i].setColour(juce::Slider::thumbColourId,modern?juce::Colour(0xfff3f2ed):colours[i]);
        knobs[i].setColour(juce::Slider::textBoxTextColourId,ink);
    }
    const auto surface=juce::Colour(modern?0xffd5d5d3:0xff263338);
    theme.setColour(juce::ComboBox::backgroundColourId,surface);
    theme.setColour(juce::ComboBox::textColourId,ink);
    theme.setColour(juce::ComboBox::arrowColourId,ink);
    theme.setColour(juce::PopupMenu::backgroundColourId,surface);
    theme.setColour(juce::PopupMenu::textColourId,ink);
    theme.setColour(juce::PopupMenu::highlightedBackgroundColourId,theme.accent());
    theme.setColour(juce::PopupMenu::highlightedTextColourId,juce::Colours::white);
    modelSwitch.setButtonText(modern?"1":"S");
    modelSwitch.setTooltip(juce::String::fromUTF8(modern?"Switch to Project One. LINE gain below -5 dB is limited to -5 dB; other valid gain values are retained.":"Switch to 286S. LINE gain values are retained; the scale extends down to -15 dB."));
    sendLookAndFeelChange();repaint();
}

void Channel286Editor::applyGuide() {
    for(auto& knob:knobs) { knob.getProperties().remove("guideStart"); knob.getProperties().remove("guideEnd"); knob.getProperties().remove("guideNote"); }
    auto span=[&](int knob,float first,float last) {
        // Angles measured from the PDF sector vectors. The current dial travel is +/-135 degrees.
        const float turn=juce::MathConstants<float>::twoPi;
        const float unit=juce::MathConstants<float>::pi/180.f;
        knobs[knob].getProperties().set("guideStart",turn+juce::jlimit(-135.f,135.f,first)*unit);
        knobs[knob].getProperties().set("guideEnd",turn+juce::jlimit(-135.f,135.f,last)*unit);
    };
    const int scenario=guideScenario.getSelectedId()-1;
    lastAppliedGuide=scenario;
    switch(scenario) {
      case 1: span(1,-59.23f,148.90f); span(2,-148.60f,59.26f); break;
      case 2: span(1,16.42f,90.f); span(2,-180.f,-106.42f); break;
      case 3: span(1,-31.36f,29.18f); span(2,58.77f,180.f); break;
      case 4: span(1,34.33f,155.85f); span(2,34.33f,155.85f); break;
      case 5: span(7,34.58f,110.17f); span(8,135.f,135.f); break;
      case 6: span(7,37.78f,37.78f); span(8,37.78f,37.78f); break;
      case 7: span(7,-1.07f,50.19f); span(8,17.98f,149.11f); break;
      case 8: span(7,-38.35f,37.16f); span(8,-93.83f,-17.98f); break;
      case 9:
        span(3,0.f,67.5f); // Existing frequency landmarks: 4k at noon, 8k at +67.5deg.
        knobs[4].getProperties().set("guideNote","BY EAR"); break;
      case 10:
        span(2,-135.f,-135.f); span(3,0.f,0.f); span(5,-135.f,-135.f);
        span(7,-30.f,-30.f); span(8,-30.f,-30.f);
        for(int i:{1,4,6}) knobs[i].getProperties().set("guideNote","BY EAR"); break;
      default: break;
    }
    if(scenario>=11 && scenario<=18) {
        // Table symbols: recommended / program-dependent / not recommended. No numeric ranges.
        const int row=scenario-11;
        const bool lfRecommended=row==4 || row==7;
        const bool hfRecommended=row==0 || row==1 || row==3 || row==6 || row==7;
        if(row==0) span(5,-135.f,-135.f);
        else knobs[5].getProperties().set("guideNote",lfRecommended?"BY EAR":"OPTIONAL");
        knobs[6].getProperties().set("guideNote",hfRecommended?"BY EAR":"OPTIONAL");
    }
    for(auto& knob:knobs) knob.repaint();
}
void Channel286Editor::resized() {

    processor.editorWidth.store(getWidth());

    const auto transform=juce::AffineTransform::scale(getWidth()/designWidth,getHeight()/designHeight);

    const std::array<int,10> centres{dialCentre(0,0),dialCentre(1,0),dialCentre(1,1),

      dialCentre(2,0),dialCentre(2,1),dialCentre(0,0),dialCentre(0,1),

      dialCentre(1,0),dialCentre(1,1),dialCentre(2,0)};

    for(size_t i=0;i<knobs.size();++i) {

        const int top=i<5?102:482;

        labels[i].setBounds(centres[i]-115,top,230,40);

        knobs[i].setBounds(centres[i]-115,top+44,230,232);

        labels[i].setTransform(transform); knobs[i].setTransform(transform);

    }

    hp.setBounds(310,247,100,60); bypass.setBounds(425,247,100,60);

    hp.setTransform(transform); bypass.setTransform(transform);

    guideScenario.setBounds(655,20,535,42);
    guideScenario.setTransform(transform);
    quality.setBounds(1220,20,280,42);

    quality.setTransform(transform);
    modelSwitch.setBounds(277,5,80,65);modelSwitch.setTransform(transform);

}

void Channel286Editor::paint(juce::Graphics& g) {
    const bool modern=theme.sMode;
    const auto ink=theme.panelInk(),blue=theme.accent();

    g.fillAll(juce::Colour(0xff12191a));

    g.addTransform(juce::AffineTransform::scale(getWidth()/designWidth,getHeight()/designHeight));

    g.drawImageAt(modern?silverTexture:panelTexture,0,0);

    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(.04f),0,0,

        juce::Colours::black.withAlpha(.12f),0,880,false));

    g.fillRect(0.f,0.f,1600.f,880.f);

    g.setColour(juce::Colour(modern?0xffcdcdcb:0xff151c1e));g.fillRect(0,0,1600,78);

    dlab::drawLogo(g,brandLogo.get(),{32.f,17.f,88.f,42.f});
    juce::GlyphArrangement modelName;
    modelName.addFittedText(panelFont(54.f,Face::wordmark),modern?"286s":"286",126.f,modern?7.f:0.f,modern?126.f:96.f,49.f,
                           juce::Justification::centred,1);
    juce::Path modelOutline;modelName.createPath(modelOutline);
    g.setColour(ink);g.fillPath(modelOutline);
    g.strokePath(modelOutline,juce::PathStrokeType(.65f));

    if(!modern) {
    // Project One badge: low blue stripe, light italic wordmark and tall slanted numeral.
    g.setColour(blue);g.fillRect(126.f,47.f,118.f,19.f);
    juce::Path one;
    one.startNewSubPath(312.f,20.f);
    one.lineTo(338.f,7.f);
    one.lineTo(325.f,65.f);
    one.lineTo(301.f,65.f);
    one.lineTo(305.f,59.f);
    one.lineTo(310.f,59.f);
    one.lineTo(319.f,22.f);
    one.lineTo(312.f,25.f);
    one.closeSubPath();
    g.fillPath(one,juce::AffineTransform::translation(-78.f,2.f));
    g.setColour(ink);g.fillPath(one,juce::AffineTransform::translation(-81.f,0.f));
    text(g,"Project",126,46,94,21,25,juce::Justification::centred,Face::wordmark);
    } else { g.setColour(blue);g.fillRect(32.f,65.f,219.f,3.f); }

    g.setColour(ink);text(g,"MICROPHONE PROCESSOR",370,23,265,36,26,juce::Justification::centredLeft);

    g.setColour(blue);g.drawLine(20,77,1580,77,2.f);

    screw(g,1556,35);

    const char* titles[]{"INPUT","COMPRESSOR","DE-ESSER","ENHANCER","EXPANDER/GATE","OUTPUT"};

    for(int row=0;row<2;++row) for(int col=0;col<3;++col) {

        const float left=gridLeft+col*cellWidth,right=left+cellWidth,mid=left+cellWidth/2;

        const float top=row==0?92.f:472.f,titleY=row==0?446.f:766.f;

        if(col>0) {g.setColour(ink.withAlpha(.45f));g.drawLine(left,top,left,titleY+28,1.f);}

        const float gap=col==1?225.f:160.f;

        g.setColour(blue);g.fillRect(left+12,titleY+17,mid-gap/2-left-22,5.f);

        g.fillRect(mid+gap/2+10,titleY+17,right-mid-gap/2-22,5.f);

        g.setColour(ink);text(g,titles[row*3+col],mid-gap/2,titleY,gap,35,30.f,juce::Justification::centred,Face::section);

    }

    g.setColour(ink);text(g,"HIGHPASS",310,155,100,33,25.f);

    text(g,"80 Hz",310,190,100,30,23.f,juce::Justification::centred,Face::scale);

    text(g,"PROCESS",425,155,100,33,25.f);text(g,"BYPASS",425,190,100,30,25.f);

    g.setColour(blue);g.drawLine(342,225,342,240,1.5f);g.drawLine(457,225,457,240,1.5f);

    const float inputDb=Channel286Processor::rmsToDbu(processor.meters[0].load());

    const float outputDb=juce::Decibels::gainToDecibels(processor.meters[1].load(),-90.f);

    g.setColour(ink);text(g,"LEVEL (dBu)",305,335,205,28,23.f);

    const int levels[]{-20,-10,0};

    for(int i=0;i<3;++i) {

        const float x=327+i*52.f;

        g.setColour(ink);text(g,juce::String(levels[i]),x-22,374,44,29,22.f,juce::Justification::centred,Face::scale);

        led(g,x,424,i==2?juce::Colour(0xffedca35):juce::Colour(0xff54d36b),inputDb>=levels[i],8.f);

    }

    g.setColour(ink);text(g,"CLIP",457,374,47,29,22.f);led(g,480,424,juce::Colour(0xffff4436),processor.inputPeak.load()>=1.f,8.f);

    g.setColour(ink);text(g,"GAIN REDUCTION (dB)",562,377,472,26,23.f);

    const int reductions[]{30,25,20,15,12,9,6,3};

    for(int i=0;i<8;++i) {

        const float x=590+i*60.f;

        g.setColour(ink);text(g,juce::String(reductions[i]),x-20,405,40,25,22.f,juce::Justification::centred,Face::scale);

        led(g,x,438,juce::Colour(0xfff83c35),processor.meters[2].load()>=reductions[i],7.f);

    }

    g.setColour(ink);text(g,"DE-ESS",1480,392,85,28,22.f);

    led(g,1520,434,juce::Colour(0xff51d16c),processor.meters[3].load()>.5f,8.f);

    led(g,(dialCentre(1,0)+dialCentre(1,1))*.5f,638,juce::Colour(0xffff7432),processor.meters[4].load()>.5f,8.f);

    g.setColour(ink);text(g,"CLIP",1450,628,90,30,25.f);

    led(g,1495,684,juce::Colour(0xffff4436),outputDb>=0,9.f);

    g.setColour(juce::Colour(modern?0xffbcbcb9:0xff192224));g.fillRect(0,813,1600,67);

    const char* meterNames[]{"INPUT dBu","OUTPUT dBFS","COMP GR dB","DE-ESS GR dB","GATE GR dB"};

    for(size_t i=0;i<5;++i) {

        const float value=i==0?inputDb:(i==1?outputDb:processor.meters[i].load());

        const float x=static_cast<float>(i)*320;

        g.setColour(ink.withAlpha(.75f));text(g,meterNames[i],x,819,320,25,22.f);

        g.setColour(ink);text(g,juce::String(value,1),x,849,320,26,24.f,juce::Justification::centred,Face::readout);

    }

}
