#include <JuceHeader.h>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
juce::AudioProcessorParameter* param(juce::AudioPluginInstance& p,const juce::String& name){for(auto* q:p.getParameters())if(q->getName(128)==name)return q;throw std::runtime_error("Missing parameter");}
std::unique_ptr<juce::XmlElement> state(juce::AudioPluginInstance& p){
    juce::MemoryBlock bytes;p.getStateInformation(bytes);auto outer=juce::AudioProcessor::getXmlFromBinary(bytes.getData(),(int)bytes.getSize());
    juce::MemoryBlock inner;require(outer&&outer->getChildByName("IComponent"),"Missing component");inner.fromBase64Encoding(outer->getChildByName("IComponent")->getAllSubText());return juce::AudioProcessor::getXmlFromBinary(inner.getData(),(int)inner.getSize());
}
void restore(juce::AudioPluginInstance& p,const juce::XmlElement& xml){juce::MemoryBlock inner;juce::AudioProcessor::copyXmlToBinary(xml,inner);juce::XmlElement outer("VST3PluginState");outer.createNewChildElement("IComponent")->addTextElement(inner.toBase64Encoding());juce::MemoryBlock bytes;juce::AudioProcessor::copyXmlToBinary(outer,bytes);p.setStateInformation(bytes.getData(),(int)bytes.getSize());}
using Notes=std::vector<std::pair<int,int>>;
std::vector<float> render(juce::AudioPluginInstance& p,int level,const Notes& notes,bool pedal=false,bool toggle=false){
    p.reset();param(p,"Dyn Comp")->setValueNotifyingHost((float)level/3);
    juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;
    // Settle fader smoothing and any pending state restoration.
    for(int i=0;i<10;++i)p.processBlock(audio,midi);
    if(pedal)midi.addEvent(juce::MidiMessage::controllerEvent(1,64,127),0);
    for(auto note:notes)midi.addEvent(juce::MidiMessage::noteOn(1,note.first,(juce::uint8)note.second),0);
    std::vector<float> result;
    for(int block=0;block<180;++block){
        if(toggle&&block==20)param(p,"Dyn Comp")->setValueNotifyingHost(1);
        if(block==90)for(auto note:notes)midi.addEvent(juce::MidiMessage::noteOff(1,note.first),0);
        if(pedal&&block==110)midi.addEvent(juce::MidiMessage::controllerEvent(1,64,0),0);
        p.processBlock(audio,midi);midi.clear();
        for(int i=0;i<256;++i)for(int c=0;c<2;++c){const auto v=audio.getSample(c,i);require(std::isfinite(v),"Non-finite output");result.push_back(v);}
    }return result;
}
void scaled(const std::vector<float>& base,const std::vector<float>& actual,double gain){
    double power=0,error=0;for(size_t i=0;i<base.size();++i){power+=(double)base[i]*base[i];const double difference=actual[i]-base[i]*gain;error+=difference*difference;}
    require(power>1e-12,"Silent fixture");require(std::sqrt(error/power)<2e-5,"Audio is not the same sample/envelope scaled by the expected gain");
}
}
int testDynComp(juce::AudioPluginInstance& plugin){try{
    auto* control=param(plugin,"Dyn Comp");require(control->getValue()==0,"Default must be OFF");
    for(double rate:{44100.0,48000.0}){
        plugin.prepareToPlay(rate,256);
        for(int velocity:{1,64,127})for(bool pedal:{false,true}){
            auto base=render(plugin,0,{{60,velocity}},pedal);
            for(int level=1;level<=3;++level){
                // Independent anchors: lowest = full boost; midpoint = half; highest = none.
                const double maximum=level+1,db=velocity==1?maximum:velocity==64?maximum/2:0;
                scaled(base,render(plugin,level,{{60,velocity}},pedal),std::pow(10.0,db/20));
            }
        }
        auto held=render(plugin,0,{{60,64}});scaled(held,render(plugin,0,{{60,64}},false,true),1);
        // Mixed velocities: each voice must keep its own gain, including the loud note.
        auto soft=render(plugin,0,{{60,1}}),loud=render(plugin,0,{{72,127}}),chord=render(plugin,3,{{60,1},{72,127}});
        std::vector<float> expected(soft.size());for(size_t i=0;i<soft.size();++i)expected[i]=(float)(soft[i]*std::pow(10.0,4.0/20)+loud[i]);
        // Before note-off, to avoid the existing release follower/voice ordering in chords.
        expected.resize(89*256*2);chord.resize(expected.size());scaled(expected,chord,1);
        std::cout<<"PASS Dyn Comp "<<rate<<" Hz: 3 gain anchors, 3 levels, sustain/release, mixed velocities, held-note stability\n";
    }
    control->setValueNotifyingHost(2.0f/3);auto saved=state(plugin);control->setValueNotifyingHost(0);restore(plugin,*saved);require(std::abs(control->getValue()-2.0f/3)<1e-5,"State did not restore level2");
    auto* controls=saved->getChildByName("Controls");require(controls!=nullptr,"Missing controls");
    for(auto* child:controls->getChildIterator())if(child->getStringAttribute("id")=="dynComp"){controls->removeChildElement(child,true);break;}
    control->setValueNotifyingHost(1);restore(plugin,*saved);require(control->getValue()==0,"Legacy state without Dyn Comp did not reset to OFF");
    std::cout<<"PASS parameter state round-trip and legacy OFF migration\n";
    // Load the same library into B to exercise both signal paths and their sum.
    auto both=state(plugin);auto path=both->getStringAttribute("samplePath");
    for(auto* slot:both->getChildIterator())if(slot->hasTagName("Slot"))slot->setAttribute("samplePath",path);
    restore(plugin,*both);bool ready=false;
    for(int n=0;n<1200;++n){auto xml=state(plugin);int count=0;for(auto* slot:xml->getChildIterator())if(slot->hasTagName("Slot")&&slot->getBoolAttribute("ready")&&!slot->getBoolAttribute("loading"))++count;if(count==2){ready=true;break;}juce::Thread::sleep(50);}
    require(ready,"A/B load timed out");
    param(plugin,"Sound 1 On")->setValueNotifyingHost(0);
    auto b=render(plugin,0,{{60,64}});scaled(b,render(plugin,3,{{60,64}}),std::pow(10.0,2.0/20));
    param(plugin,"Sound 1 On")->setValueNotifyingHost(1);
    scaled(b,render(plugin,0,{{60,64}}),2);scaled(b,render(plugin,3,{{60,64}}),2*std::pow(10.0,2.0/20));
    std::cout<<"PASS B-only and A+B: per-voice gain applied once in each slot\n";return 0;
}catch(const std::exception& e){std::cerr<<"Dyn Comp: "<<e.what()<<"\n";return 51;}}
