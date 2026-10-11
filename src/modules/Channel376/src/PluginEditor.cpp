#include "dlab/BrandLogo.h"
#include "PluginEditor.h"
#include "Channel376Fonts.h"
namespace {
const juce::Colour ink{0xff242a2d},green{0xffa6c93d},yellow{0xffc5b43e},red{0xffb04c46};
const auto minus = juce::String::fromUTF8(u8"\u2212");
const auto infinity = juce::String::fromUTF8(u8"\u221e");
const auto menuArrow = juce::String::fromUTF8(u8"\u25be");
constexpr std::array<juce::Point<int>,10> centres{{{247,276},{680,276},{890,276},{1100,276},{1310,276},{155,562},{430,562},{778,562},{994,562},{1210,562}}};
juce::Point<float> polar(float x,float y,float r,float degrees){const auto a=juce::degreesToRadians(degrees);return {x+std::sin(a)*r,y-std::cos(a)*r};}
}
void Channel376Editor::PanelLook::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float p,float,float,juce::Slider&){
    // Keep the rim and the offset shadow inside the slider's paint clip.
    const float cx=x+w*.5f,cy=y+h*.5f,r=std::min(w,h)*.5f-5.f;
    g.setColour(juce::Colour(0xff99a1a6));g.fillEllipse(cx-r+2,cy-r+4,r*2,r*2);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfffafbf7),cx-16,cy-22,juce::Colour(0xffb1bbbd),cx+26,cy+35,false));g.fillEllipse(cx-r,cy-r,r*2,r*2);
    g.setColour(juce::Colour(0xffa8b0b4));g.drawEllipse(cx-r,cy-r,r*2,r*2,1);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfffafbf7),cx-12,cy-18,juce::Colour(0xffdde3df),cx+20,cy+28,true));g.fillEllipse(cx-r+4,cy-r+2,r*2-8,r*2-8);
    const auto a=polar(cx,cy,9,-135+270*p),b=polar(cx,cy,32,-135+270*p);g.setColour(juce::Colour(0xff343b3d));g.drawLine({a,b},6);
}
void Channel376Editor::PanelLook::drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool over,bool down){
    auto box=b.getLocalBounds().toFloat();g.setColour(juce::Colour(0xff303638));g.fillRoundedRectangle(box,3);g.setColour(juce::Colour(0xff8a959b));g.drawRoundedRectangle(box,3,1);
    g.setColour(b.getToggleState()?green:juce::Colour(over?0xffa7b2b4:0xff899396));g.fillRoundedRectangle(box.reduced(down?6.f:5.f),2);
}
void Channel376Editor::PanelLook::drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool){g.setColour(ink);g.setFont(juce::Font(juce::FontOptions(condensed).withHeight(18)));g.drawText(b.getButtonText(),b.getLocalBounds(),juce::Justification::centred);}
Channel376Editor::Channel376Editor(Channel376Processor& p):AudioProcessorEditor(p),processor(p){
    look.condensed=juce::Typeface::createSystemTypefaceFor(Channel376Fonts::BarlowCondensedBold_ttf,Channel376Fonts::BarlowCondensedBold_ttfSize);
    brandLogo=dlab::makeLogo();
    wordmark=juce::Typeface::createSystemTypefaceFor(Channel376Fonts::BarlowLightItalic_ttf,Channel376Fonts::BarlowLightItalic_ttfSize);
    addAndMakeVisible(panel);panel.setLookAndFeel(&look);
    for(int j=0;j<10;++j){auto& k=knobs[j];panel.addAndMakeVisible(k);k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);k.setRotaryParameters(juce::degreesToRadians(225.f),juce::degreesToRadians(495.f),true);k.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);k.setDoubleClickReturnValue(true,channel376::continuous[j].initial);k.setMouseDragSensitivity(240);k.setTooltip("Double-click to reset. Drag to adjust; Shift-drag for fine control.");k.setName(channel376::continuous[j].id.data());knobAttachments[j]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters,channel376::continuous[j].id.data(),k);}
    const char* labels[]{"20dB PAD","PHASE","LOW CUT","OVEREASY","SLOW"};for(int j=0;j<5;++j){panel.addAndMakeVisible(switches[j]);switches[j].setButtonText(labels[j]);switches[j].setClickingTogglesState(true);switchAttachments[j]=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters,p.switchIDs[j],switches[j]);}
    for(int j=0;j<1;++j){panel.addAndMakeVisible(modes[j]);modes[j].setButtonText("OVERSAMPLING");modes[j].onClick=[this,j]{auto* parameter=processor.parameters.getParameter("oversampling");const int next=(int(std::round(parameter->convertFrom0to1(parameter->getValue())))+1)%3;parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(float(next)));parameter->endChangeGesture();repaint();};}
    panel.addAndMakeVisible(tubeSelector);tubeSelector.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    tubeSelector.setTooltip("Click to choose a clean tonal character: Warm, Bright or Muted. Fixed headroom trim keeps the profiles from adding gain or saturation. Use OUTPUT LEVEL to adjust loudness.");
    tubeSelector.onClick=[this,safe=juce::Component::SafePointer<Channel376Editor>(this)]{
        juce::PopupMenu menu;
        const int selected=int(processor.parameters.getRawParameterValue("tubeModel")->load());
        for(int j=0;j<int(channel376::tubeModels376.size());++j){const auto& tube=channel376::tubeModels376[j];menu.addItem(j+1,juce::String::fromUTF8(tube.name.data())+" ("+juce::String::fromUTF8(tube.hint.data())+")",true,j==selected);}
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&tubeSelector).withMinimumWidth(480).withStandardItemHeight(32),[safe](int result){
            if(safe==nullptr||result<1||result>int(channel376::tubeModels376.size()))return;
            auto* parameter=safe->processor.parameters.getParameter("tubeModel");parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(float(result-1)));parameter->endChangeGesture();safe->repaint();
        });
    };
    modes[0].setTooltip("Optional oversampling: OFF (default), 2x, 4x. Changes processing history and host latency (0/64/96 samples). Prefer changing while stopped.");
    switches[1].setTooltip("Reverses polarity (180 degrees). Compare alongside the original signal; a single track can sound the same.");
    switches[2].setTooltip("75 Hz LOW CUT. Second-order filter, 12 dB/octave, using the original circuit.");
    knobs[0].setTooltip("Input gain before the tone and EQ sections. Use OUTPUT LEVEL to compensate loudness. Watch the EQ CLIP indicator at high gain. Double-click resets to zero.");
    setResizable(true,true);setResizeLimits(1100,540,2200,1081);getConstrainer()->setFixedAspectRatio(1460.0/717.0);setSize(1460,717);startTimerHz(30);
}
Channel376Editor::~Channel376Editor(){stopTimer();panel.setLookAndFeel(nullptr);}
void Channel376Editor::resized(){panel.setBounds(0,0,1460,717);panel.setTransform(juce::AffineTransform::scale(getWidth()/1460.f,getHeight()/717.f));for(int j=0;j<10;++j)knobs[j].setBounds(centres[j].x-46,centres[j].y-46,92,92);switches[0].setBounds(44,161,100,30);switches[1].setBounds(44,231,100,30);switches[2].setBounds(44,301,100,30);switches[3].setBounds(255,502,88,30);switches[4].setBounds(255,577,88,30);modes[0].setBounds(440,28,150,30);tubeSelector.setBounds(353,116,180,189);}
void Channel376Editor::paint(juce::Graphics& g){
    g.addTransform(juce::AffineTransform::scale(getWidth()/1460.f,getHeight()/717.f));
    for(int y=0;y<717;++y){int s=202+int(8*std::cos(y/717.0*juce::MathConstants<double>::twoPi))+(y%4==0);g.setColour(juce::Colour::fromRGB(juce::uint8(s),juce::uint8(s+3),juce::uint8(s+5)));g.drawHorizontalLine(y,0,1460);}
    auto text=[&](float x,float y,const juce::String& t,float size=20,juce::Justification align=juce::Justification::centred,bool italic=false){g.setColour(ink);g.setFont(juce::Font(juce::FontOptions(italic?wordmark:look.condensed).withHeight(size)));const float w=italic?450.f:500.f;const float left=align==juce::Justification::centredRight?x-w:align==juce::Justification::centredLeft?x:x-w/2;g.drawText(t,juce::Rectangle<float>(left,y-size*.7f,w,size*1.4f),align,false);};
    auto lamp=[&](float x,float y,juce::Colour c,bool on){if(on){g.setColour(c.withAlpha(.2f));g.fillEllipse(x-9,y-9,18,18);}g.setColour(on?c:juce::Colour(0xff60686b));g.fillEllipse(x-6,y-6,12,12);g.setColour(juce::Colour(0xff424a4d));g.drawEllipse(x-6,y-6,12,12,1);};
    auto section=[&](float x,float y,float w,float h,const char* title){g.setColour(juce::Colour(0xffcdd1d2));g.fillRoundedRectangle(x,y,w,h,8);g.setColour(juce::Colour(0xff778185));g.drawRoundedRectangle(x,y,w,h,8,1);text(x+18,y+22,title,24,juce::Justification::centredLeft);g.setColour(juce::Colour(0xff9aa2a5));g.drawLine(x+18,y+39,x+w-18,y+39);};
    dlab::drawLogo(g,brandLogo.get(),{32.f,20.f,120.f,64.f});
    text(166,47,"376",66,juce::Justification::centredLeft,true);text(1415,43,"dLab 376",30,juce::Justification::centredRight,true);text(1415,76,"TUBE EMULATION - EQ - COMPRESSION - DE-ESSER",16,juce::Justification::centredRight);
    section(24,105,526,266,"INPUT / TUBE");section(570,105,866,266,"EQUALIZER");section(24,391,630,291,"COMPRESSOR");section(674,391,420,291,"DE-ESSER");section(1114,391,322,291,"OUTPUT");
    {const int model=juce::jlimit(0,2,int(processor.parameters.getRawParameterValue("tubeModel")->load()));text(532,127,juce::String::fromUTF8(channel376::tubeModels376[model].name.data())+" "+menuArrow,16,juce::Justification::centredRight);}
    const std::array<const char*,10> names{{"DRIVE","LOW","MID","FREQUENCY","HIGH","THRESHOLD","RATIO","FREQUENCY","AMOUNT","OUTPUT LEVEL"}};
    const juce::StringArray gain{minus+"15",minus+"10",minus+"5","0","+5","+10","+15"};
    const std::array<juce::StringArray,10> labels{{gain,gain,gain,{"100","200","1k","4k","8k"},gain,{minus+"40",minus+"30",minus+"20",minus+"10","0","+10","+20"},{"OFF","2:1","3:1","4:1","6:1","10:1",infinity+":1"},{"800","1k","4k","8k","10k"},{"OFF","2.5","5","7.5","10"},{minus+"20",minus+"10","0","+10","+20"}}};
    for(int j=0;j<10;++j){float x=float(centres[j].x),y=float(centres[j].y);text(x,y-106,names[j],21);const auto& ls=labels[j];std::vector<float> majors;for(int n=0;n<ls.size();++n)majors.push_back(-135+270.f*n/(ls.size()-1));if(j==3||j==7)majors={-135,-90,0,90,135};g.setColour(ink);for(int n=0;n<=40;++n){float a=-135+270.f*n/40;bool near=false;for(auto b:majors)near|=std::abs(a-b)<3;if(!near)g.drawLine({polar(x,y,51,a),polar(x,y,56,a)},1);}for(int n=0;n<ls.size();++n){g.setColour(ink);g.drawLine({polar(x,y,50,majors[n]),polar(x,y,62,majors[n])},2);auto pos=polar(x,y,79,majors[n]);text(pos.x,pos.y,ls[n],16);}
        const double v=knobs[j].getValue();juce::String value;if(j==3||j==7)value=v>=1000?juce::String(v/1000,2)+" kHz":juce::String(v,0)+" Hz";else if(j==6)value=v<=0?"OFF":juce::String(1./std::max(.001,1.-channel376::loadedRatio376(v).compression),2)+":1";else if(j==8)value=v<=0?"OFF":juce::String(v,1);else value=juce::String(v,1)+(j==5?" dBu":" dB");text(x,y+(j<5?75:95),value,20);
    }
    std::array<double,6> m{};for(int j=0;j<6;++j)m[j]=processor.meters[j].load(std::memory_order_relaxed);
    for(int i=0;i<6;++i){g.setColour(juce::Colour(0xff303331));g.fillRoundedRectangle(float(359+i*26),159,19,145,7);const float glow=float(std::clamp(m[1]/7.,0.,1.));g.setGradientFill(juce::ColourGradient(juce::Colour(0xffd78e40).withAlpha(.15f+.7f*glow),float(368+i*26),220,juce::Colour(0xff33332f),float(368+i*26),159,true));g.fillRoundedRectangle(float(362+i*26),164,13,135,5);}
    const double inputThreshold[]{.08115,.24750,.77444,6.92147};const juce::String inputLabels[]{minus+"20",minus+"10","0","CLIP"};for(int i=0;i<4;++i){float x=float(367+i*45+(i==3?1:0));lamp(x,327,i<2?green:i==2?yellow:red,m[1]>=inputThreshold[i]);text(x,351,inputLabels[i],15);}lamp(1330,127,red,m[3]>0);text(1346,127,"EQ CLIP",16,juce::Justification::centredLeft);
    lamp(272,642,green,m[5]>.3);lamp(299,642,yellow,switches[3].getToggleState()&&std::abs(m[5])<=.3);lamp(326,642,red,m[5]<-.3);text(272,664,minus,16);text(299,664,"0",16);text(326,664,"+",16);
    auto frame=[&](float x,float w){g.setColour(juce::Colour(0xff737d80));g.fillRoundedRectangle(x,450,w,182,4);g.setColour(juce::Colour(0xff23292c));g.fillRoundedRectangle(x+3,453,w-6,176,3);};frame(564,34);text(636,413,"GAIN REDUCTION",16,juce::Justification::centredRight);
    const int grLabels[]{30,25,20,15,10,6,3,1};const double grR[]{29960,24970,19980,14990,10000,6000,3000,2000};for(int i=0;i<8;++i){auto c=i<5?green:i==5?yellow:red;g.setColour(m[2]<=-15*grR[i]/60960?c:c.withMultipliedBrightness(.22f));g.fillRoundedRectangle(570,float(455+i*22),22,16,2);text(611,float(463+i*22),juce::String(grLabels[i]),15,juce::Justification::centredLeft);}text(581,657,"dB",16);
    // Display the dedicated de-esser control branch, not combined VCA gain.
    const double deDb=-m[4]*1000./49900./channel376::Blackmer376::voltsPerDb;lamp(874,456,red,deDb>=6);lamp(898,456,red,deDb>=1);text(874,441,"6",14);text(898,441,"1",14);text(886,476,"dB",14);
    frame(1340,40);for(int i=0;i<8;++i){auto c=i<2?red:i==2?yellow:green;g.setColour(m[0]>=-6*i?c:c.withMultipliedBrightness(.22f));g.fillRoundedRectangle(1346,float(455+i*22),28,16,2);text(float(1391+(i==0?6:0)),float(463+i*22),juce::String(-6*i),15,juce::Justification::centredLeft);}text(1371,657,std::isfinite(m[0])?juce::String(m[0],1):minus+infinity,18,juce::Justification::centredRight);text(1381,657,"dBFS",16,juce::Justification::centredLeft);
    {const int mode=int(processor.parameters.getRawParameterValue("oversampling")->load());const juce::StringArray labels{"OFF","2x","4x"};text(515,76,labels[juce::jlimit(0,2,mode)],16);lamp(608,43,green,mode!=0);}
    for(auto p:std::array<juce::Point<float>,4>{{{15,15},{1445,15},{15,702},{1445,702}}}){g.setColour(juce::Colour(0xff9da8ae));g.fillEllipse(p.x-6,p.y-6,12,12);g.setColour(juce::Colour(0xff546269));g.drawLine(p.x-3,p.y+2,p.x+3,p.y-2,2);}
}
