#include <JuceHeader.h>
#include <iostream>

int testLayers(juce::AudioPluginFormatManager&,const juce::PluginDescription&,const juce::String&,const juce::String&);

int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if(argc<3) {std::cerr<<"Usage: NpnoSmoke plugin.vst3 sound.npno\n";return 1;}
    juce::AudioPluginFormatManager manager; manager.addFormat(std::make_unique<juce::VST3PluginFormat>());
    juce::OwnedArray<juce::PluginDescription> descriptions;
    for(int i=0;i<manager.getNumFormats();++i) manager.getFormat(i)->findAllTypesForFile(descriptions,argv[1]);
    if(descriptions.isEmpty()) {std::cerr<<"No VST3 instrument found\n";return 2;}
    juce::String error;
    auto plugin=manager.createPluginInstance(*descriptions[0],48000,256,error);
    if(!plugin) {std::cerr<<error<<"\n";return 3;}
    plugin->prepareToPlay(48000,256);
    juce::XmlElement xml("NPnoPlayer");xml.setAttribute("samplePath",juce::File(argv[2]).getFullPathName());
    juce::MemoryBlock inner; juce::AudioProcessor::copyXmlToBinary(xml,inner);
    juce::XmlElement wrapper("VST3PluginState");
    wrapper.createNewChildElement("IComponent")->addTextElement(inner.toBase64Encoding());
    juce::MemoryBlock state; juce::AudioProcessor::copyXmlToBinary(wrapper,state);
    plugin->setStateInformation(state.getData(),(int)state.getSize());
    juce::AudioBuffer<float> buffer(2,256);
    juce::MidiBuffer midi;
    bool loaded=false;
    for(int n=0;n<1200;++n) {
        juce::MemoryBlock saved;plugin->getStateInformation(saved);
        if(auto s=juce::AudioProcessor::getXmlFromBinary(saved.getData(),(int)saved.getSize())) {
            if(auto* component=s->getChildByName("IComponent")) {
                juce::MemoryBlock bytes;bytes.fromBase64Encoding(component->getAllSubText());
                if(auto content=juce::AudioProcessor::getXmlFromBinary(bytes.getData(),(int)bytes.getSize()))
                    if(content->getStringAttribute("samplePath").isNotEmpty()){
                        auto* slot=content->getChildByName("Slot");
                        if(!slot || (slot->getBoolAttribute("ready") && !slot->getBoolAttribute("loading"))){loaded=true;break;}
                    }
            }
        }
        plugin->processBlock(buffer,midi); juce::Thread::sleep(50);
    }
    if(!loaded){std::cerr<<"Sample loading timed out\n";return 4;}
    int tests=0;
    for(int note=21;note<=108;++note) for(int velocity: {1,32,64,96,127}) {
        midi.addEvent(juce::MidiMessage::allSoundOff(1),0);
        midi.addEvent(juce::MidiMessage::noteOn(1,note,(juce::uint8)velocity),0);
        double energy=0;
        for(int block=0;block<30;++block){
            buffer.clear();plugin->processBlock(buffer,midi);midi.clear();
            for(int ch=0;ch<2;++ch) for(int i=0;i<256;++i){float s=buffer.getSample(ch,i);if(!std::isfinite(s)){std::cerr<<"Non-finite sample\n";return 5;}energy+=s*s;}
        }
        if(energy<1e-9){std::cerr<<"Silent note "<<note<<" velocity "<<velocity<<"\n";return 6;}
        std::cout<<"PASS note="<<note<<" velocity="<<velocity<<" energy="<<energy<<"\n";++tests;
    }
    auto renderEnergy=[&](int blocks){double energy=0;for(int b=0;b<blocks;++b){buffer.clear();plugin->processBlock(buffer,midi);midi.clear();for(int c=0;c<2;++c)for(int i=0;i<256;++i){double s=buffer.getSample(c,i);energy+=s*s;}}return energy;};
    midi.addEvent(juce::MidiMessage::allSoundOff(1),0);
    midi.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)100),0);
    renderEnergy(20);
    midi.addEvent(juce::MidiMessage::noteOff(1,60),0);renderEnergy(30);
    if(renderEnergy(5)>1e-12){std::cerr<<"Note-off did not silence the voice\n";return 7;}
    std::cout<<"PASS note-off release\n";
    midi.addEvent(juce::MidiMessage::noteOff(1,60),0);
    if(renderEnergy(5)>1e-12){std::cerr<<"Note-off started a new sound without an active note\n";return 10;}
    std::cout<<"PASS repeated note-off does not start a new voice\n";
    midi.addEvent(juce::MidiMessage::controllerEvent(1,64,127),0);
    midi.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)100),0);renderEnergy(20);
    midi.addEvent(juce::MidiMessage::noteOff(1,60),0);renderEnergy(30);
    if(renderEnergy(5)<1e-9){std::cerr<<"Sustain did not hold the voice\n";return 8;}
    midi.addEvent(juce::MidiMessage::controllerEvent(1,64,0),0);renderEnergy(30);
    if(renderEnergy(5)>1e-12){std::cerr<<"Sustain release did not silence the voice\n";return 9;}
    std::cout<<"PASS sustain hold and release\n";
    plugin->releaseResources();
    std::cout<<"PASS "<<tests<<" note/velocity combinations; state restored from .npno path\n";
    if(argc>=4) return testLayers(manager,*descriptions[0],juce::File(argv[2]).getFullPathName(),juce::File(argv[3]).getFullPathName());
}
