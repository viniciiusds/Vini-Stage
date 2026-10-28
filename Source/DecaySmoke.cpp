#include <JuceHeader.h>
#include <iostream>
#include <stdexcept>

// End-to-end test against a self-generated constant-amplitude sine. Expected
// slopes come from the fixture specification, not from Playback's implementation.
int testDecayPhases(juce::AudioPluginInstance& plugin){
    try{
        auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
        for(double rate:{44100.0,48000.0})for(int note:{60,72}){
            plugin.releaseResources();plugin.prepareToPlay(rate,256);
            juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::allSoundOff(1),0);plugin.processBlock(audio,midi);midi.clear();
            midi.addEvent(juce::MidiMessage::noteOn(1,note,(juce::uint8)127),0);
            std::vector<float> samples;
            for(int b=0;b<(int)std::ceil(2.2*rate/256);++b){plugin.processBlock(audio,midi);midi.clear();for(int i=0;i<256;++i)samples.push_back(audio.getSample(0,i));}
            auto rms=[&](double a,double b){double power=0;size_t lo=(size_t)(a*rate),hi=(size_t)(b*rate);for(size_t i=lo;i<hi;++i)power+=(double)samples[i]*samples[i];return std::sqrt(power/(hi-lo));};
            const double pitch=note==60?1:2,loopStart=(35000.0/35002.0)/pitch;
            const double before=20*std::log10(rms(.15,.25)/rms(.30,.40))/.15;
            const double after=20*std::log10(rms(loopStart+.2,loopStart+.3)/rms(loopStart+.6,loopStart+.7))/.4;
            require(std::abs(before-pitch)<.10,"Pre-loop phase does not use the 1 dB/s source-time coefficient");
            require(std::abs(after-6)<.10,"Loop phase does not use the 6 dB/s host-time coefficient");
            // Known PCM amplitude, trim=0 -> 1.5, velocity=127 -> unity.
            // Integrate the expected pre-loop envelope over the RMS window.
            const double lambda=std::log(10.0)*pitch/10.0;
            const double envelopePower=(std::exp(-lambda*.15)-std::exp(-lambda*.25))/(lambda*.10);
            const double base=6000.0/32768.0*1.5/std::sqrt(2.0)*std::sqrt(envelopePower);
            const double outputBoost=20*std::log10(rms(.15,.25)/base);
            require(std::abs(outputBoost-10)<.06,"Global output gain is not +10 dB");
            std::cout<<"PASS decay fixture rate="<<rate<<" note="<<note<<" before="<<before<<" dB/s after="<<after<<" dB/s output="<<outputBoost<<" dB\n";
        }
        return 0;
    }catch(const std::exception&e){std::cerr<<"Decay regression: "<<e.what()<<"\n";return 41;}
}
