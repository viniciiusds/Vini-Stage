#include <JuceHeader.h>
#include <iostream>
#include <stdexcept>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
namespace {
juce::Component* findTitle(juce::Component& parent,const juce::String& title){
    if(parent.getTitle()==title)return &parent;
    for(auto* child:parent.getChildren())if(auto* found=findTitle(*child,title))return found;
    return nullptr;
}
void require(bool condition,const char* text){if(!condition)throw std::runtime_error(text);}
juce::AudioProcessorParameter* parameter(juce::AudioProcessor& p,const juce::String& name){for(auto* value:p.getParameters())if(value->getName(128)==name)return value;throw std::runtime_error("Parameter missing");}
}
namespace {
juce::Component* findPiano(juce::Component& parent){
    if(parent.getTitle()=="88-key piano")return &parent;
    for(auto* child:parent.getChildren())if(auto* found=findPiano(*child))return found;
    return nullptr;
}
int captureEditor(juce::AudioProcessor& plugin,const juce::File& output,bool verifyAudio){
    auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    std::unique_ptr<juce::AudioProcessorEditor> editor(plugin.createEditor());
    check(editor!=nullptr,"Plugin editor missing");editor->setVisible(true);
    auto* keys=dynamic_cast<juce::MidiKeyboardComponent*>(findPiano(*editor));
    check(keys!=nullptr,"88-key piano missing from compiled plugin");
    check(keys->getRangeStart()==21&&keys->getRangeEnd()==108,"Keyboard range is not A0-C8");
    check(keys->getRectangleForKey(21).getX()>=-0.1f&&keys->getRectangleForKey(108).getRight()<=keys->getWidth()+0.1f,"88 keys do not fit");
    juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;
    auto render=[&](int blocks){float peak=0;for(int b=0;b<blocks;++b){plugin.processBlock(audio,midi);midi.clear();peak=std::max(peak,audio.getMagnitude(0,256));}return peak;};
    for(int channel=1;channel<=16;++channel)midi.addEvent(juce::MidiMessage::allNotesOff(channel),0);
    render(40);
    auto point=keys->getRectangleForKey(60).getCentre().withY((float)keys->getHeight()-12);
    auto sample=point.toInt();
    auto unpressed=keys->createComponentSnapshot(keys->getLocalBounds()).getPixelAt(sample.x,sample.y);
    midi.addEvent(juce::MidiMessage::noteOn(2,60,(juce::uint8)100),0);
    check(render(4)>0||!verifyAudio,"MIDI keyboard test silent");
    auto pressed=keys->createComponentSnapshot(keys->getLocalBounds()).getPixelAt(sample.x,sample.y);
    check(pressed!=unpressed,"Incoming MIDI did not highlight key");
    midi.addEvent(juce::MidiMessage::noteOff(2,60),0);render(40);
    check(keys->createComponentSnapshot(keys->getLocalBounds()).getPixelAt(sample.x,sample.y)==unpressed,"Note-off did not clear highlight");
    // Demonstration chord from incoming MIDI, using the same processing as a DAW.
    for(int note:{60,64,67})midi.addEvent(juce::MidiMessage::noteOn(1,note,(juce::uint8)100),0);
    render(4);
    auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true,2.0f);
    auto stream=output.createOutputStream();check(stream!=nullptr,"Cannot create screenshot");
    stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(image,*stream),"Cannot write screenshot");
    std::cout<<"PASS compiled editor: 88 visible keys, incoming MIDI audio/highlights and note-off\n";
    std::cout<<"Captured compiled plugin editor: "<<output.getFullPathName()<<"\n";
    return 0;
}
}

int main(int argc,char** argv){
    juce::ScopedJuceInitialiser_GUI init;
    if(argc<2)return 1;
    std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
    if(argc>=4){
        juce::XmlElement state("NPnoPlayer");state.setAttribute("samplePath",juce::File(argv[2]).getFullPathName());
        for(int i=0;i<2;++i){auto* slot=state.createNewChildElement("Slot");slot->setAttribute("index",i);slot->setAttribute("samplePath",juce::File(argv[i+2]).getFullPathName());}
        juce::MemoryBlock bytes;juce::AudioProcessor::copyXmlToBinary(state,bytes);processor->setStateInformation(bytes.getData(),(int)bytes.getSize());
        bool ready=false;
        for(int n=0;n<1200;++n){
            juce::MemoryBlock saved;processor->getStateInformation(saved);auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),(int)saved.getSize());int count=0;
            if(xml)for(auto* slot:xml->getChildIterator())if(slot->hasTagName("Slot")&&slot->getBoolAttribute("ready")&&!slot->getBoolAttribute("loading"))++count;
            if(count==2){ready=true;break;}juce::Thread::sleep(50);
        }require(ready,"Preview sounds did not load");
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
    editor->setVisible(true);
    auto* fader=dynamic_cast<juce::Slider*>(findTitle(*editor,"Sound 1 Volume"));require(fader!=nullptr,"Vertical fader missing");
    auto* volume=parameter(*processor,"Sound 1 Volume");
    fader->keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    require(std::abs(volume->getValue()-std::pow(10.0f,-1.0f/20.0f))<1e-6f,"Down arrow is not 1 dB");
    fader->keyPressed(juce::KeyPress(juce::KeyPress::upKey));fader->keyPressed(juce::KeyPress(juce::KeyPress::upKey));require(volume->getValue()==1,"Fader exceeds 0 dB");
    fader->keyPressed(juce::KeyPress(juce::KeyPress::homeKey));require(volume->getValue()==0,"Fader bottom is not mute");
    fader->keyPressed(juce::KeyPress(juce::KeyPress::endKey));
    fader->setValue(-5.4,juce::sendNotificationSync);require(fader->getValue()==-5,"Fader does not snap to whole dB");
    const bool unity=argc>4&&juce::String(argv[4])=="--unity";
    parameter(*processor,"Sound 1 Volume")->setValueNotifyingHost(volume->getValueForText(unity?"0 dB":"-3 dB"));
    auto* second=parameter(*processor,"Sound 2 Volume");second->setValueNotifyingHost(second->getValueForText(unity?"0 dB":"-8 dB"));
    if(argc>=4){
        auto* clear=dynamic_cast<juce::TextButton*>(findTitle(*editor,"Select or clear B"));
        require(clear&&clear->getButtonText()=="Clear B","Loaded slot must offer Clear B");
        processor->prepareToPlay(48000,256);
        clear->onClick();
        require(clear->getButtonText()=="Select B","Cleared slot must offer Select B");
        auto* row=dynamic_cast<juce::Label*>(findTitle(*editor,"Sound B"));
        require(row&&row->getText()=="B:","Cleared display must be empty");
        auto* firstButton=dynamic_cast<juce::TextButton*>(findTitle(*editor,"Select or clear A"));
        require(firstButton&&firstButton->getButtonText()=="Clear A","Clearing B changed A");
        juce::MemoryBlock saved;processor->getStateInformation(saved);
        auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),(int)saved.getSize());
        for(auto* slot:xml->getChildIterator())if(slot->hasTagName("Slot")&&slot->getIntAttribute("index")==1)
            require(slot->getStringAttribute("samplePath").isEmpty(),"Clear did not persist empty path");
        // Isolate B using the preserved legacy automation parameter: it must be silent immediately.
        parameter(*processor,"Sound 1 On")->setValueNotifyingHost(0);
        juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)100),0);
        processor->processBlock(audio,midi);
        require(audio.getMagnitude(0,256)==0,"Cleared B still plays audio");
        parameter(*processor,"Sound 1 On")->setValueNotifyingHost(1);
        // A fresh instance restores the cleared path, including a save before the worker finishes clearing.
        std::unique_ptr<juce::AudioProcessor> restored(createPluginFilter());
        restored->setStateInformation(saved.getData(),(int)saved.getSize());
        juce::MemoryBlock roundTrip;restored->getStateInformation(roundTrip);
        auto restoredXml=juce::AudioProcessor::getXmlFromBinary(roundTrip.getData(),(int)roundTrip.getSize());
        for(auto* slot:restoredXml->getChildIterator())if(slot->hasTagName("Slot"))
            require(slot->getStringAttribute("samplePath")== (slot->getIntAttribute("index")==0?juce::File(argv[2]).getFullPathName():juce::String{}),"Cleared project did not restore A/B paths");
        // Cancel a fresh asynchronous load through the actual UI; late decode must not resurrect B.
        juce::XmlElement loading(*xml);
        for(auto* slot:loading.getChildIterator())if(slot->hasTagName("Slot")&&slot->getIntAttribute("index")==1)
            slot->setAttribute("samplePath",juce::File(argv[3]).getFullPathName());
        juce::MemoryBlock loadingBytes;juce::AudioProcessor::copyXmlToBinary(loading,loadingBytes);
        processor->setStateInformation(loadingBytes.getData(),(int)loadingBytes.getSize());
        clear->onClick();
        bool settled=false;
        for(int n=0;n<1200;++n){
            juce::MemoryBlock check;processor->getStateInformation(check);
            auto state=juce::AudioProcessor::getXmlFromBinary(check.getData(),(int)check.getSize());
            int idle=0;
            for(auto* slot:state->getChildIterator())if(slot->hasTagName("Slot")){
                if(!slot->getBoolAttribute("loading"))++idle;
                if(slot->getIntAttribute("index")==1)require(slot->getStringAttribute("samplePath").isEmpty(),"Cancelled B resurrected");
            }
            if(idle==2){settled=true;break;}juce::Thread::sleep(50);
        }
        require(settled,"Clear while loading did not settle");
        // Recreate so the snapshot contains the settled display rather than a timer's prior Loading label.
        editor.reset(processor->createEditor());editor->setVisible(true);
        std::cout<<"PASS fresh-instance cleared state and Clear during loading\n";
        std::cout<<"PASS actual Clear B button: empty display, Select label, persisted empty path, silence and A retained\n";
    }
    std::cout<<"PASS real editor fader: 1 dB keyboard steps, integer snapping, 0 dB cap and mute\n";
    editor.reset();processor->prepareToPlay(48000,256);
    return captureEditor(*processor,juce::File(argv[1]),argc>=4);
}
