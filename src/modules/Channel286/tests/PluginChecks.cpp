#include "../src/PluginProcessor.h"
#include "../src/PluginEditor.h"
#include "../src/dsp/Calibration.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <stdexcept>

void require(bool ok,const char* msg) { if(!ok) throw std::runtime_error(msg); }
int main(int argc,char** argv) {
  try {
    juce::ScopedJuceInitialiser_GUI gui;
    using namespace channelSuite;
    require(manufacturerCode==0x46727a7a && fourCC(channel286.pluginCode)==0x43323836,"Published 286 codes changed");
    require(channel286.stateTag=="Channel286State" && channel286.bundleId=="com.frozz.channel286","Published 286 identity changed");
    std::vector<juce::VST3Interface::Id> ids;
    for(size_t i=0;i<identities.size();++i) {
        for(size_t j=0;j<i;++j) {
            const auto a=identities[i],b=identities[j];
            require(a.name!=b.name && a.pluginCode!=b.pluginCode && a.bundleId!=b.bundleId && a.stateTag!=b.stateTag,"Model identities collide");
        }
        for(auto type:{juce::VST3Interface::Type::component,juce::VST3Interface::Type::controller}) {
            auto id=juce::VST3Interface::jucePluginId(manufacturerCode,fourCC(identities[i].pluginCode),type);
            require(std::find(ids.begin(),ids.end(),id)==ids.end(),"VST3 class ID collision");ids.push_back(id);
        }
    }
    Channel286Processor p;
    const char* publishedParameters[]{"input","drive","density","frequency","deThreshold","lf","hf",
      "gateThreshold","gateRatio","output","hpf","processBypass","quality"};
    require(p.getName()=="dLab 286" && p.parameters.state.getType()==juce::Identifier("Channel286State"),"286 name/state identity changed");
    require(p.getParameters().size()>=13,"Published parameters removed");
    for(int i=0;i<13;++i) {
        auto* parameter=dynamic_cast<juce::AudioProcessorParameterWithID*>(p.getParameters()[i]);
        require(parameter && parameter->paramID==publishedParameters[i] && parameter->getVersionHint()==1,"Published parameter ID/order/version changed");
    }
    for(int channels:{1,2}) {
        auto layout=p.getBusesLayout();
        layout.inputBuses.set(0,juce::AudioChannelSet::canonicalChannelSet(channels));
        layout.outputBuses.set(0,juce::AudioChannelSet::canonicalChannelSet(channels));
        require(p.setBusesLayout(layout),"Bus layout failed");
        p.prepareToPlay(48000,256);
        juce::AudioBuffer<float> b(channels,256); juce::MidiBuffer midi;
        for(int c=0;c<channels;++c) for(int i=0;i<256;++i) b.setSample(c,i,.1f*std::sin(i*.1f));
        const juce::AudioBuffer<float> original(b);
        p.processBlock(b,midi);
        for(int c=0;c<channels;++c) for(int i=0;i<256;++i)
            require(std::abs(b.getSample(c,i)-(i<p.getLatencySamples()?0.f:original.getSample(c,i-p.getLatencySamples())))<1.e-6f,"Default LINE gain 0dB must preserve input");
    }
    int reportedLatency=-1;
    for(double rate:{44100.,48000.,96000.,192000.}) for(int q=0;q<3;++q) {
        Channel286Processor qualityProcessor;
        auto* parameter=qualityProcessor.parameters.getParameter("quality");
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(q)));
        qualityProcessor.prepareToPlay(rate,128);
        const int latency=qualityProcessor.getLatencySamples();
        if(reportedLatency<0) reportedLatency=latency;
        require(latency==reportedLatency && latency>0,"Quality must keep a stable nonzero latency");
        juce::AudioBuffer<float> large(2,4097); juce::MidiBuffer midi;
        for(int ch=0;ch<2;++ch) for(int i=0;i<large.getNumSamples();++i) large.setSample(ch,i,.7f*std::sin(i*.7f));
        qualityProcessor.processBlock(large,midi);
        for(int ch=0;ch<2;++ch) for(int i=0;i<large.getNumSamples();++i)
            require(std::isfinite(large.getSample(ch,i)) && std::abs(large.getSample(ch,i))<2.f,"Oversized block overload must stay finite/bounded");
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>((q+1)%3)));
        qualityProcessor.processBlock(large,midi);
        require(qualityProcessor.getLatencySamples()==latency,"Live switch changed host latency");
    }
    auto aliasAmplitude=[](int quality) {
        Channel286Processor processor;
        auto* parameter=processor.parameters.getParameter("quality");
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(quality)));
        auto* input=processor.parameters.getParameter("input");input->setValueNotifyingHost(input->convertTo0to1(10));
        processor.prepareToPlay(48000,256);
        juce::AudioBuffer<float> block(2,256); juce::MidiBuffer midi;
        double cosine=0,sine=0; int n=0;
        for(int offset=0;offset<48000;offset+=256) {
            const int count=std::min(256,48000-offset); block.setSize(2,count,false,false,true);
            for(int i=0;i<count;++i) for(int ch=0;ch<2;++ch)
                block.setSample(ch,i,.7f*static_cast<float>(std::sin(juce::MathConstants<double>::twoPi*9000*(offset+i)/48000.)));
            processor.processBlock(block,midi);
            if(offset>24000) for(int i=0;i<count;++i) {
                const double angle=juce::MathConstants<double>::twoPi*21000*(offset+i)/48000.;
                cosine+=block.getSample(0,i)*std::cos(angle); sine+=block.getSample(0,i)*std::sin(angle); ++n;
            }
        }
        return 2*std::hypot(cosine,sine)/n;
    };
    const double standardAlias=aliasAmplitude(0),ultraAlias=aliasAmplitude(2);
    std::cout<<"21kHz alias from overloaded 9kHz: 1x="<<standardAlias<<", 4x="<<ultraAlias<<"\n";
    require(ultraAlias<standardAlias*.3,"4x must reduce the folded third harmonic substantially");
    auto* drive=p.parameters.getParameter("drive");
    require(std::abs(Channel286Processor::rmsToDbu(static_cast<float>(.775/c286::calibration::clipPeakVolts)))<1.e-5f,"0.775 Vrms must read 0dBu");
    require(std::abs(Channel286Processor::rmsToDbu(1/std::sqrt(2.f))-19.48018f)<.001f,"Full-scale sine must use circuit clip reference");
    drive->setValueNotifyingHost(drive->convertTo0to1(7.25f));
    juce::MemoryBlock state; p.getStateInformation(state);
    Channel286Processor restored; restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    require(std::abs(restored.parameters.getRawParameterValue("drive")->load()-7.25f)<.001f,"State round trip");
    Channel286Processor independent;
    require(independent.parameters.getRawParameterValue("drive")->load()==0,"Instances share parameter state");
    for(const auto& model:{channel376,channel676}) {
        juce::ValueTree foreign(model.stateTag.data());
        foreign.setProperty("editorWidth",2000,nullptr);
        foreign.setProperty("guideScenario",18,nullptr);
        juce::ValueTree parameter("PARAM");
        parameter.setProperty("id","drive",nullptr);parameter.setProperty("value",10.f,nullptr);
        foreign.addChild(parameter,-1,nullptr);
        juce::MemoryBlock foreignBytes;
        juce::AudioProcessor::copyXmlToBinary(*foreign.createXml(),foreignBytes);
        const auto originalState=restored.parameters.copyState();
        const int originalWidth=restored.editorWidth.load(),originalGuide=restored.guideScenario.load();
        restored.setStateInformation(foreignBytes.getData(),static_cast<int>(foreignBytes.getSize()));
        require(restored.parameters.copyState().isEquivalentTo(originalState)
          && restored.editorWidth.load()==originalWidth && restored.guideScenario.load()==originalGuide,"286 accepted another model's state");
    }
    const char malformed[]{1,2,3}; restored.setStateInformation(malformed,3);
    require(std::abs(restored.parameters.getRawParameterValue("drive")->load()-7.25f)<.001f,"Bad state changes parameters");
    restored.editorWidth.store(1800);
    restored.getStateInformation(state);
    Channel286Processor recalled; recalled.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    require(recalled.editorWidth.load()==1800,"Editor size state round trip");
    std::unique_ptr<juce::AudioProcessorEditor> editor(restored.createEditor());
    require(editor!=nullptr,"No editor");
    for(int i=0;i<editor->getNumChildComponents();++i) if(auto* dial=dynamic_cast<RackSlider*>(editor->getChildComponent(i))) {
        if(dial->getName()=="gateRatio") {
            require(std::abs(dial->valueToProportionOfLength(2)-.5)<1.e-8,"2:1 must point straight up");
            require(std::abs(dial->proportionOfLengthToValue(.25)-1.5)<1.e-8,"Ratio quarter turn must be 1.5:1");
        }
        if(dial->getName()=="input") {
            require(dial->getMinimum()==-5 && dial->getMaximum()==45,"Project One LINE gain range -5 to +45");
            require(std::abs(dial->valueToProportionOfLength(15)-.5)<1.e-8,"+15dB at LINE GAIN midpoint");
            const double marks[]{-5,0,15,25,45};
            for(int i=0;i<5;++i)require(std::abs(dial->proportionOfLengthToValue(i*.25)-marks[i])<1.e-8,"Project One LINE landmarks from MIC minus15dB");
        }
        if(dial->getName()=="gateThreshold") {
            require(std::abs(dial->proportionOfLengthToValue(.5)+15)<1.e-8,"Gate midpoint must be -15dB");
            require(dial->proportionOfLengthToValue(1)==15,"Gate clockwise endpoint +15dB");
        }
        if(dial->getName()=="frequency") {
            const double values[]{800,1000,4000,8000,10000};
            for(int tick=0;tick<21;++tick) {
                const double value=dial->proportionOfLengthToValue(tick/20.);
                require(value>=800 && value<=10000,"Frequency tick out of range");
                if(tick%5==0) require(std::abs(value-values[tick/5])<1.e-6,"Frequency landmark at wrong division");
            }
        }
        if(dial->getName()=="output") require(std::abs(dial->valueToProportionOfLength(0)-.5)<1.e-8,"0dB must point straight up");
        for(double u:{0.,.1,.25,.5,.75,.9,1.})
            require(std::abs(dial->valueToProportionOfLength(dial->proportionOfLengthToValue(u))-u)<1.e-6,"Dial scale must invert correctly");
    }
    for(auto size:{juce::Point<int>{1440,792},juce::Point<int>{1600,880},juce::Point<int>{2000,1100}}) {
        editor->setSize(size.x,size.y);
        std::vector<juce::Rectangle<int>> dialBounds;
        for(int i=0;i<editor->getNumChildComponents();++i) if(auto* dial=dynamic_cast<RackSlider*>(editor->getChildComponent(i))) {
            const auto bounds=dial->getBoundsInParent();
            require(editor->getLocalBounds().contains(bounds),"Dial extends beyond window");
            for(const auto& previous:dialBounds) require(!bounds.intersects(previous),"Dial hit regions overlap");
            dialBounds.push_back(bounds);
        }
        auto image=editor->createComponentSnapshot(editor->getLocalBounds());
        require(image.isValid(),"GUI render failed");
        if(argc>1) {
            juce::File out=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
            if(size.x!=1600) out=out.getSiblingFile(out.getFileNameWithoutExtension()+"-"+juce::String(size.x)+".png");
            juce::FileOutputStream stream(out); juce::PNGImageFormat png;
            stream.setPosition(0); stream.truncate();
            require(stream.openedOk() && png.writeImageToStream(image,stream),"PNG write failed");
        }
    }
    if(argc>1) {
        juce::TextButton* modeButton=nullptr;
        std::vector<juce::Rectangle<int>> originalBounds;
        for(int i=0;i<editor->getNumChildComponents();++i) {
            auto* child=editor->getChildComponent(i);
            if(auto* button=dynamic_cast<juce::TextButton*>(child)) if(button->getName()=="modelS") modeButton=button;
            if(dynamic_cast<RackSlider*>(child)) originalBounds.push_back(child->getBounds());
        }
        require(modeButton && modeButton->getButtonText()=="S","Missing arrow/S mode switch");
        std::array<float,13> values{};
        for(int i=0;i<13;++i) values[i]=restored.getParameters()[i]->getValue();
        modeButton->setToggleState(true,juce::sendNotificationSync);
        require(restored.parameters.getRawParameterValue("modelS")->load()==1 && modeButton->getButtonText()=="1","Switch must enter 286s and offer Project 1");
        for(int i=0;i<editor->getNumChildComponents();++i)if(auto* dial=dynamic_cast<RackSlider*>(editor->getChildComponent(i)))if(dial->getName()=="input"){
            require(dial->getMinimum()==-15&&dial->getMaximum()==45,"286S LINE gain range");
            const double marks[]{-15,0,15,30,45};
            for(int j=0;j<5;++j)require(std::abs(dial->proportionOfLengthToValue(j*.25)-marks[j])<1.e-8,"286S LINE landmarks");
        }
        std::unique_ptr<juce::AudioProcessorEditor> otherEditor(independent.createEditor());
        require(!dynamic_cast<ProjectOneLookAndFeel&>(otherEditor->getLookAndFeel()).sMode
          && dynamic_cast<ProjectOneLookAndFeel&>(editor->getLookAndFeel()).sMode,"Editor palettes leak between instances");
        int dialIndex=0;
        for(int i=0;i<editor->getNumChildComponents();++i) if(auto* dial=dynamic_cast<RackSlider*>(editor->getChildComponent(i)))
            require(dial->getBounds()==originalBounds[dialIndex++],"Mode moved a dial");
        for(int i=0;i<13;++i) require(restored.getParameters()[i]->getValue()==values[i],"Mode altered existing parameters");
        restored.getStateInformation(state);
        Channel286Processor savedMode;savedMode.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
        require(savedMode.parameters.getRawParameterValue("modelS")->load()==1,"Mode state round trip");
        auto legacy=juce::AudioProcessor::getXmlFromBinary(state.getData(),static_cast<int>(state.getSize()));
        auto legacyTree=juce::ValueTree::fromXml(*legacy);
        legacyTree.removeChild(legacyTree.getChildWithProperty("id","modelS"),nullptr);
        juce::MemoryBlock legacyBytes;juce::AudioProcessor::copyXmlToBinary(*legacyTree.createXml(),legacyBytes);
        savedMode.setStateInformation(legacyBytes.getData(),static_cast<int>(legacyBytes.getSize()));
        require(savedMode.parameters.getRawParameterValue("modelS")->load()==0,"Legacy sessions must restore original mode");
        editor->setSize(1600,880);
        auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds());
        const auto output=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]).getSiblingFile("gui-preview-286s.png");
        juce::FileOutputStream stream(output);stream.setPosition(0);stream.truncate();juce::PNGImageFormat png;
        require(png.writeImageToStream(snapshot,stream),"286s preview failed");
        modeButton->setToggleState(false,juce::sendNotificationSync);
        require(modeButton->getButtonText()=="S","Switch must offer 286s after returning to Project 1");
        for(int i=0;i<editor->getNumChildComponents();++i) if(auto* dial=dynamic_cast<RackSlider*>(editor->getChildComponent(i))) {
            if(dial->getName()!="frequency") continue;
            auto& look=editor->getLookAndFeel();
            const auto bounds=look.getTooltipBounds(dial->getTooltip(),{50,50},{0,0,1000,800});
            juce::Image tip(juce::Image::RGB,bounds.getWidth(),bounds.getHeight(),true);
            { juce::Graphics graphics(tip); look.drawTooltip(graphics,dial->getTooltip(),tip.getWidth(),tip.getHeight()); }
            const auto out=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]).getSiblingFile("gui-tooltip.png");
            juce::FileOutputStream stream(out); stream.setPosition(0); stream.truncate();
            juce::PNGImageFormat png; require(png.writeImageToStream(tip,stream),"Tooltip preview write failed");
        }
    }
    juce::ComboBox* guide=nullptr;
    for(int i=0;i<editor->getNumChildComponents();++i) if(auto* combo=dynamic_cast<juce::ComboBox*>(editor->getChildComponent(i)))
        if(combo->getName()=="guideScenario") guide=combo;
    require(guide!=nullptr,"No guide scenario selector");
    // A menu click queues ComboBox notification; a meter timer can run before it arrives.
    guide->setSelectedId(10,juce::sendNotificationAsync);
    dynamic_cast<Channel286Editor&>(*editor).refreshFromProcessor();
    require(guide->getSelectedId()==10,"Timer must not undo a menu click before async notification");
    guide->onChange(); // Deliver the pending selection callback after the interleaved timer.
    require(restored.guideScenario.load()==9,"First click must commit the selected guide");
    // Restoring state must still refresh an already open editor.
    restored.guideScenario.store(7);
    dynamic_cast<Channel286Editor&>(*editor).refreshFromProcessor();
    require(guide->getSelectedId()==8,"External guide state must refresh an open editor");
    guide->setSelectedId(1,juce::sendNotificationSync);
    const auto beforeGuide=restored.parameters.copyState();
    for(int id=1;id<=19;++id) {
        guide->setSelectedId(id,juce::sendNotificationSync);
        require(restored.parameters.copyState().isEquivalentTo(beforeGuide),"Guide must not alter audio parameters");
        int marked=0;
        for(int i=0;i<editor->getNumChildComponents();++i) if(auto* dial=dynamic_cast<RackSlider*>(editor->getChildComponent(i))) {
            const auto& props=dial->getProperties();
            if(!props.contains("guideStart")) continue;
            ++marked;
            const float begin=static_cast<float>(props["guideStart"]),end=static_cast<float>(props["guideEnd"]);
            require(begin<=end,"Reversed guide interval");
            if(id<=5) require(dial->getName()=="drive" || dial->getName()=="density","Compressor guide leaked into another block");
            if(id>=6 && id<=9) require(dial->getName()=="gateThreshold" || dial->getName()=="gateRatio","Gate guide leaked into another block");
            if(id==10) {
                require(dial->getName()=="frequency","De-esser range belongs on frequency only");
                const double start=juce::MathConstants<double>::pi*1.25,travel=juce::MathConstants<double>::pi*1.5;
                require(std::abs(dial->proportionOfLengthToValue((begin-start)/travel)-4000)<.01,"Vocal guide lower landmark 4kHz");
                require(std::abs(dial->proportionOfLengthToValue((end-start)/travel)-8000)<.01,"Vocal guide upper landmark 8kHz");
            }
        }
        require(marked==(id==1?0:(id==10?1:(id==11?5:(id==12?1:(id>12?0:2))))),"Wrong number of highlighted controls");
        if(argc>1 && (id==2 || id==9 || id==10 || id==11 || id==12 || id==16)) {
            editor->setSize(1600,880);
            auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds());
            const auto file=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]).getSiblingFile("gui-guide-"+juce::String(id)+".png");
            juce::FileOutputStream stream(file); stream.setPosition(0); stream.truncate();
            juce::PNGImageFormat png; require(png.writeImageToStream(snapshot,stream),"Guide preview write failed");
        }
    }
    guide->setSelectedId(11,juce::sendNotificationSync);
    restored.getStateInformation(state);
    Channel286Processor rememberedGuide; rememberedGuide.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    require(rememberedGuide.guideScenario.load()==10,"Guide selection state round trip");
    guide->setSelectedId(1,juce::sendNotificationSync);
    std::cout<<"PASS: mono/stereo, LINE gain, circuit dBu, dial mappings, state, GUI at three sizes\n";
    return 0;
  } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
