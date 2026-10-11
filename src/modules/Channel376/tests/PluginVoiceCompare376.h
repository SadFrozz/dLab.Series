#pragma once
#include <filesystem>
int compareVoice376(const char* sourcePath,const char* folder,double drive){
 juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(sourcePath)));if(!reader)return 4;
 const int frames=int(std::min(reader->lengthInSamples,juce::int64(reader->sampleRate*12))),channels=int(std::min(reader->numChannels,2u));
 juce::AudioBuffer<float> source(channels,frames);reader->read(&source,0,frames,0,true,true);std::filesystem::create_directories(folder);
 std::ofstream report(std::filesystem::path(folder)/"voice-report.csv");report<<"model,drive,sampleRate,channels,frames,input_peak_dBFS,input_rms_dBFS,output_rms_dBFS,matched_delta_dBr,rms_gain_match\n";
 double inputEnergy=0,inputPeak=0,baseEnergy=0;std::vector<double> baseline;
 for(int c=0;c<channels;++c)for(int n=0;n<frames;++n){double x=source.getSample(c,n);inputEnergy+=x*x;inputPeak=std::max(inputPeak,std::abs(x));}
 for(int m=0;m<3;++m){Channel376Processor p;p.setProcessingPrecision(juce::AudioProcessor::doublePrecision);
  auto set=[&](const char* id,float value){auto* q=p.parameters.getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(value));};
  set("drive",float(drive));set("output",-15);p.prepareToPlay(reader->sampleRate,256);set("tubeModel",float(m));juce::AudioBuffer<double> b(2,256);juce::MidiBuffer midi;std::vector<double> output(frames*channels);
  for(int offset=-int(reader->sampleRate);offset<frames;offset+=256){const int size=std::min(256,frames-offset);b.setSize(2,size,false,false,true);
   for(int n=0;n<size;++n)for(int c=0;c<2;++c)b.setSample(c,n,offset+n<0?0:source.getSample(std::min(c,channels-1),offset+n));
   p.processBlock(b,midi);for(int n=0;n<size;++n)if(offset+n>=0)for(int c=0;c<channels;++c)output[(offset+n)*channels+c]=b.getSample(c,n);
  }
  double energy=0,dot=0;for(size_t n=0;n<output.size();++n){energy+=output[n]*output[n];if(m)dot+=output[n]*baseline[n];}
  double delta=0;if(m){double gain=dot/energy;for(size_t n=0;n<output.size();++n)delta+=std::pow(output[n]*gain-baseline[n],2);}else{baseline=output;baseEnergy=energy;}
  const double match=std::sqrt(baseEnergy/energy);std::ofstream raw(std::filesystem::path(folder)/("tube"+std::to_string(m)+".f64"),std::ios::binary);raw.write(reinterpret_cast<const char*>(output.data()),std::streamsize(output.size()*sizeof(double)));
  report<<m<<','<<drive<<','<<reader->sampleRate<<','<<channels<<','<<frames<<','<<20*std::log10(inputPeak+1e-30)<<','<<10*std::log10(inputEnergy/(channels*frames)+1e-30)<<','<<10*std::log10(energy/output.size()+1e-30)<<','<<(delta>0?10*std::log10(delta/baseEnergy):-150)<<','<<match<<'\n';
 }
 return 0;
}
