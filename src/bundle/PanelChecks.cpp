#include "Processors.h"
#include "dlab/BrandLogo.h"
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) {
 try {
  juce::ScopedJuceInitialiser_GUI gui;
  if(argc<2)throw std::runtime_error("Preview directory");
  if(argc>2) {
   juce::Image logoImage(juce::Image::ARGB,512,272,true);
   {
    juce::Graphics g(logoImage);auto logo=dlab::makeLogo();
    dlab::drawLogo(g,logo.get(),{0.f,0.f,512.f,272.f});
   } // Flush the graphics context before encoding the image.
   int visible=0;
   for(int y=0;y<logoImage.getHeight();++y)for(int x=0;x<logoImage.getWidth();++x)
    if(logoImage.getPixelAt(x,y).getAlpha()>0)++visible;
   if(visible<100)throw std::runtime_error("Empty installer logo");
   auto stream=juce::File(argv[2]).getChildFile("dLab-logo.png").createOutputStream();
   if(!stream)throw std::runtime_error("Logo output");
   stream->setPosition(0);stream->truncate();juce::PNGImageFormat png;png.writeImageToStream(logoImage,*stream);
  }
  for(int model:{286,376}) {
   auto p=dlab::createProcessor(model,juce::AudioProcessor::wrapperType_Undefined);
   auto editor=std::unique_ptr<juce::AudioProcessorEditor>(p->createEditorIfNeeded());
   auto image=editor->createComponentSnapshot(editor->getLocalBounds());
   int red=0;for(int y=5;y<70;++y)for(int x=32;x<120;++x){auto c=image.getPixelAt(x,y);if(c.getRed()>150&&c.getGreen()<80)++red;}
   if(red<30)throw std::runtime_error("Vector logo missing from panel");
   auto output=juce::File(argv[1]).getChildFile("panel-"+juce::String(model)+".png").createOutputStream();
   if(output){output->setPosition(0);output->truncate();}
   juce::PNGImageFormat png;if(!output||!png.writeImageToStream(image,*output))throw std::runtime_error("Preview encode");
   if(model==286) {
    p->editorBeingDeleted(editor.get());
    editor.reset();
    for(auto* parameter:p->getParameters())
     if(auto* id=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))
      if(id->paramID=="modelS")parameter->setValueNotifyingHost(1.f);
    editor.reset(p->createEditorIfNeeded());
    image=editor->createComponentSnapshot(editor->getLocalBounds());
    output=juce::File(argv[1]).getChildFile("panel-286s.png").createOutputStream();
    if(!output)throw std::runtime_error("286S preview output");
    output->setPosition(0);output->truncate();png.writeImageToStream(image,*output);
   }
   p->editorBeingDeleted(editor.get());
  }
  std::cout<<"PASS both native panels include original vector branding\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
