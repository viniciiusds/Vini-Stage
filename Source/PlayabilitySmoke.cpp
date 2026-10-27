#include <JuceHeader.h>
#include "Playback.h"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
struct Renderer {
    juce::AudioPluginInstance& plugin;double rate=48000;
    juce::AudioBuffer<float> audio{2,256};juce::MidiBuffer midi;
    void event(juce::MidiMessage message){midi.addEvent(message,0);}
    std::vector<float> render(double seconds){
        const int blocks=(int)std::ceil(seconds*rate/256.0);std::vector<float> result;result.reserve((size_t)blocks*512);
        for(int b=0;b<blocks;++b){plugin.processBlock(audio,midi);midi.clear();
            for(int i=0;i<256;++i)for(int ch=0;ch<2;++ch){const float x=audio.getSample(ch,i);require(std::isfinite(x),"Non-finite playability audio");result.push_back(x);}}
        return result;
    }
    void reset(){event(juce::MidiMessage::allSoundOff(1));event(juce::MidiMessage::allSoundOff(2));render(0.03);}
};
double rms(const std::vector<float>& samples,double rate,double from,double to){
    const auto begin=std::min(samples.size(),(size_t)(from*rate)*2),end=std::min(samples.size(),(size_t)(to*rate)*2);
    if(end<=begin)return 0;double energy=0;for(size_t i=begin;i<end;++i)energy+=(double)samples[i]*samples[i];
    return std::sqrt(energy/(double)(end-begin));
}
void wav(const juce::File& file,const std::vector<float>& samples,double rate){
    file.deleteFile();auto stream=file.createOutputStream();require(stream!=nullptr,"Cannot create audition WAV");
    auto writer=std::unique_ptr<juce::AudioFormatWriter>(juce::WavAudioFormat().createWriterFor(stream.get(),rate,2,24,{},0));
    require(writer!=nullptr,"Cannot create WAV writer");stream.release();
    juce::AudioBuffer<float> audio(2,(int)(samples.size()/2));
    for(int i=0;i<audio.getNumSamples();++i)for(int c=0;c<2;++c)audio.setSample(c,i,samples[(size_t)i*2+(size_t)c]);
    require(writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples()),"Cannot write audition WAV");
}
void append(std::vector<float>& destination,const std::vector<float>& source){destination.insert(destination.end(),source.begin(),source.end());}
void selectionTests(){
    const float pcm[2]={0.1f,0.1f};std::vector<NpView> strokes;
    for(int layer:{0,15,60,120})strokes.push_back({pcm,1,2,60,0,(uint8_t)layer,0,0,1});
    require(Playback::choose(strokes,60,127,0,120)->layer==0,"Hard velocity chooses soft layer");
    require(Playback::choose(strokes,60,1,0,120)->layer==120,"Soft velocity misses softest layer");
    require(Playback::choose(strokes,60,64,0,120)->layer==60,"Layer spacing ignored");
    strokes.erase(strokes.begin()+1);
    require(Playback::choose(strokes,60,100,0,120)->layer==0,"Missing layer redistributes other thresholds");
    require(Playback::choose(strokes,61,64,0,120)==nullptr,"Uncovered root plays");
    float previous=0;for(int velocity=1;velocity<=127;++velocity){auto mapped=Playback::drivenVelocity((float)velocity/127);
        require(mapped>=previous&&mapped>=1.0f/127&&mapped<=1,"Drive curve invalid");previous=mapped;}
    require(juce::roundToInt(Playback::drivenVelocity(64.0f/127)*127)==32,"Default -0.50 Drive is not the documented x^2 curve");
    std::cout<<"PASS spaced/missing velocity layers and monotonic default Drive -0.50\n";
}
}

int testPlayability(juce::AudioPluginInstance& plugin,const juce::String& path,const juce::File& output){
    try {
        selectionTests();char error[1024]{};
        std::unique_ptr<NpLibrary,decltype(&np_free)> library(np_load(path.toRawUTF8(),error,sizeof(error)),np_free);
        require(library!=nullptr,"Cannot inspect piano for playability test");
        std::vector<NpView> recordings(np_count(library.get()));std::array<int,3> extents{};size_t loops=0,exact=0;
        for(size_t i=0;i<recordings.size();++i){require(np_recording(library.get(),i,&recordings[i]),"Missing decoded recording");const auto& r=recordings[i];
            if(r.bank<3)extents[r.bank]=std::max(extents[r.bank],(int)r.layer);
            if(Playback::hasLoop(r)){++loops;
                require(r.loop_start>0&&r.loop_decay>0&&r.loop_decay<=1,"Invalid loop/decay");
                if(r.loop_end+32<=r.frames){bool matches=true;for(size_t f=0;f<32;++f)for(size_t c=0;c<r.channels;++c)
                    matches&=r.data[(r.loop_start+f)*r.channels+c]==r.data[(r.loop_end+f)*r.channels+c];
                    if(matches)++exact;}
            }
        }
        require(loops>0&&exact>0,"No verified embedded loops in piano");
        std::cout<<"Verified file loops="<<loops<<" exact repeated boundaries="<<exact<<"\n";
        if(output.getFullPathName().isNotEmpty())require(output.createDirectory().wasOk(),"Cannot create audition folder");
        Renderer render{plugin};
        for(double rate:{44100.0,48000.0}){
            plugin.releaseResources();plugin.prepareToPlay(rate,256);render.rate=rate;
            for(int note:{36,60,61}){
                render.reset();render.event(juce::MidiMessage::controllerEvent(1,64,127));
                render.event(juce::MidiMessage::noteOn(1,note,(juce::uint8)100));auto onset=render.render(0.25);
                render.event(juce::MidiMessage::noteOff(1,note));auto tail=render.render(14);
                const double early=rms(tail,rate,5,6),late=rms(tail,rate,10,11);
                require(early>1e-14&&late>1e-16,"Sustain voice cuts at recording end");
                require(late<early,"Loop tail does not decay");
                render.event(juce::MidiMessage::controllerEvent(1,64,0));auto released=render.render(3);
                require(rms(released,rate,2.8,3)<1e-12,"Pedal release does not finish");
                if(rate==48000&&note==60){append(onset,tail);append(onset,released);wav(output.getChildFile("Royal-sustain-14s.wav"),onset,rate);}
                std::cout<<"PASS sustain at "<<rate<<" Hz note="<<note<<" RMS 5s="<<early<<" 10s="<<late<<" and pedal release\n";
            }
        }
        // Pedal pressed after note-on must hold the attack-bank recording too.
        render.reset();render.event(juce::MidiMessage::noteOn(1,60,(juce::uint8)100));render.render(0.1);
        render.event(juce::MidiMessage::controllerEvent(1,64,127));render.event(juce::MidiMessage::noteOff(1,60));
        require(rms(render.render(10),render.rate,8,9)>1e-16,"Pedal pressed after note-on fails to hold attack bank");
        render.reset();render.event(juce::MidiMessage::noteOn(1,60,(juce::uint8)100));
        require(rms(render.render(10),render.rate,8,9)>1e-16,"Physically held note cuts at sample end");
        std::cout<<"PASS pedal after note-on and held key use the embedded loop\n";
        // Compare actual release audio, after the main voice's 90 ms fade, against
        // decoded release PCM at a fixed 40% gain, independent of the main voice.
        render.reset();render.event(juce::MidiMessage::noteOn(1,60,(juce::uint8)127));render.render(0.25);
        render.event(juce::MidiMessage::noteOff(1,60));auto release=render.render(3);
        const auto* take=Playback::choose(recordings,np_root(library.get(),60),127,2,extents[2]);
        require(take!=nullptr,"Royal release sample missing");
        const double step=(double)np_rate()/render.rate*std::pow(2.0,((double)60-take->root+(double)np_fine(library.get(),60)*0.7/100)/12);
        const float level=np_gain(library.get())*0.65f*0.40f;double maxError=0;double position=0;
        for(size_t f=0;f<(size_t)(0.4*render.rate);++f){if(f>(size_t)(0.2*render.rate)){
            size_t a=(size_t)position,b=std::min(a+1,take->frames-1);const float fraction=(float)(position-a);
            for(size_t c=0;c<2;++c){size_t channel=std::min(c,(size_t)take->channels-1);
                float expected=(take->data[a*take->channels+channel]+fraction*(take->data[b*take->channels+channel]-take->data[a*take->channels+channel]))*level;
                maxError=std::max(maxError,std::abs((double)release[f*2+c]-expected));}}
            position+=step;
        }
        require(maxError<1e-6,"Release audio is not calibrated to fixed 40 percent");
        require(rms(release,render.rate,0.2,0.4)>1e-7,"Note-off release sample silent");
        render.event(juce::MidiMessage::noteOff(1,60));render.event(juce::MidiMessage::controllerEvent(1,64,0));
        require(rms(render.render(1),render.rate,0,1)<1e-12,"Unmatched note-off or repeated pedal-up retriggers release");
        wav(output.getChildFile("Royal-release-40-percent-fixed.wav"),release,render.rate);
        std::cout<<"PASS measured release sample at fixed 40 percent, no unmatched/repeated retrigger\n";
        for(int velocity:{16,64,127})for(double heldSeconds:{0.25,6.5}){
            render.reset();render.event(juce::MidiMessage::noteOn(1,60,(juce::uint8)velocity));render.render(heldSeconds);
            render.event(juce::MidiMessage::noteOff(1,60));auto actual=render.render(1.8);
            for(size_t i=(size_t)(0.2*render.rate)*2;i<(size_t)(0.4*render.rate)*2;++i)
                require(std::abs(actual[i]-release[i])<1e-6f,"Release gain or sample changes with velocity/hold duration");
        }
        render.reset();render.event(juce::MidiMessage::controllerEvent(1,64,127));
        render.event(juce::MidiMessage::noteOn(1,60,(juce::uint8)16));render.render(0.25);
        render.event(juce::MidiMessage::noteOff(1,60));render.render(6.5);
        render.event(juce::MidiMessage::controllerEvent(1,64,0));auto pedalRelease=render.render(1.8);
        for(size_t i=(size_t)(0.2*render.rate)*2;i<(size_t)(0.4*render.rate)*2;++i)
            require(std::abs(pedalRelease[i]-release[i])<1e-6f,"Release gain changes with sustain duration");
        std::cout<<"PASS identical fixed release for velocities 16/64/127, short/long notes and delayed pedal release\n";
        // A note-off while pedal is down must have no audible release insertion.
        auto pedalRun=[&](bool sendNoteOff){render.reset();render.event(juce::MidiMessage::controllerEvent(1,64,127));render.event(juce::MidiMessage::noteOn(1,60,(juce::uint8)100));render.render(0.25);
            if(sendNoteOff)render.event(juce::MidiMessage::noteOff(1,60));return render.render(0.5);};
        auto held=pedalRun(false),pedalled=pedalRun(true);require(held==pedalled,"Release sample fires while sustain is held");
        std::vector<float> dynamics;double weak=0,medium=0,strong=0;
        for(int velocity:{16,64,127}){render.reset();render.event(juce::MidiMessage::noteOn(1,60,(juce::uint8)velocity));auto attack=render.render(0.8);
            const double value=rms(attack,render.rate,0,0.25);if(velocity==16)weak=value;else if(velocity==64)medium=value;else strong=value;
            append(dynamics,attack);render.event(juce::MidiMessage::noteOff(1,60));append(dynamics,render.render(1.8));}
        require(weak>0&&weak<strong*0.05&&medium<strong*0.5&&weak<medium,"Quiet notes are too loud for default softer curve");
        wav(output.getChildFile("Royal-dynamics-16-64-127.wav"),dynamics,render.rate);
        std::cout<<"PASS default dynamic response: RMS v16="<<weak<<" v64="<<medium<<" v127="<<strong<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"Playability test failed: "<<e.what()<<"\n";return 30;}
}
