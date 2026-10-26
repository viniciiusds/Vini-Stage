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
    parameter(*processor,"Sound 2 On")->setValueNotifyingHost(0);
    std::cout<<"PASS real editor fader: 1 dB keyboard steps, integer snapping, 0 dB cap and mute\n";
    auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true,2.0f);
    juce::File file(argv[1]);auto stream=file.createOutputStream();
    if(!stream)return 2;
    stream->setPosition(0);stream->truncate();
    if(!juce::PNGImageFormat().writeImageToStream(image,*stream))return 2;
    std::cout<<"Rendered plugin UI: "<<file.getFullPathName()<<"\n";
}
