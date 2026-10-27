#include <JuceHeader.h>
#include <iostream>

namespace {
void describe(const juce::XmlElement& xml,int depth=0){
    if(depth>4)return;
    std::cout<<juce::String::repeatedString(" ",depth*2)<<xml.getTagName();
    for(int i=0;i<xml.getNumAttributes();++i){
        auto name=xml.getAttributeName(i),value=xml.getAttributeValue(i);auto lower=name.toLowerCase();
        const bool privateField=lower.contains("licen")||lower.contains("cert")||lower.contains("auth")||lower.contains("serial")||lower.contains("token")||lower.contains("email");
        std::cout<<" "<<name;
        if(!privateField&&(name=="id"||name=="name"||value.endsWithIgnoreCase(".npno")||value.containsOnly("-+.0123456789")||value=="true"||value=="false"))std::cout<<"="<<value;
    }
    std::cout<<"\n";int count=0;for(auto* child:xml.getChildIterator()){if(++count>32){std::cout<<"... more children\n";break;}describe(*child,depth+1);}
}
}
int inspectReference(juce::AudioPluginInstance& plugin){
    std::cout<<"PLUGIN "<<plugin.getName()<<" programs="<<plugin.getNumPrograms()<<"\n";
    for(auto* parameter:plugin.getParameters())if(parameter->getParameterIndex()<256)std::cout<<"PARAM "<<parameter->getParameterIndex()<<" "<<parameter->getName(128)<<" normalized="<<parameter->getValue()<<" display="<<parameter->getCurrentValueAsText()<<"\n";
    juce::MemoryBlock state;plugin.getStateInformation(state);auto outer=juce::AudioProcessor::getXmlFromBinary(state.getData(),(int)state.getSize());
    if(!outer){std::cout<<"Opaque host state bytes="<<state.getSize()<<"\n";return 0;}
    std::cout<<"HOST STATE\n";describe(*outer,4);
    for(auto* item:outer->getChildIterator())if(item->hasTagName("IComponent")||item->hasTagName("IEditController")){
        juce::MemoryBlock decoded;if(!decoded.fromBase64Encoding(item->getAllSubText()))continue;
        auto inner=juce::AudioProcessor::getXmlFromBinary(decoded.getData(),(int)decoded.getSize());
        if(inner){std::cout<<"INNER "<<item->getTagName()<<"\n";describe(*inner);
            for(auto* child:inner->getChildIterator())if(!child->hasTagName("PARAM")){std::cout<<"OTHER TREE\n";describe(*child);}
        }
        else std::cout<<"Opaque inner "<<item->getTagName()<<" bytes="<<decoded.getSize()<<"\n";
    }
    return 0;
}

namespace {
void writeReferenceAudio(const juce::File& file,const std::vector<float>& interleaved){
    file.deleteFile();auto stream=file.createOutputStream();if(!stream)throw std::runtime_error("Cannot create reference audio");
    stream->write(interleaved.data(),interleaved.size()*sizeof(float));
}
void neutralPatch(juce::XmlElement& node,const juce::String& sound,bool load){
    if(node.hasAttribute("sampleA")){
        node.setAttribute("sampleA",load?sound:juce::String{});
        for(auto suffix:{"B","C","D"})node.setAttribute(juce::String("sample")+suffix,juce::String{});
    }
    for(auto* child:node.getChildIterator()){
        if(child->hasTagName("PARAM")){
            auto id=child->getStringAttribute("id");
            if(id=="a_enabled")child->setAttribute("value",1.0);
            else if(id=="b_enabled"||id=="c_enabled"||id=="d_enabled"||id=="split_enabled"||id=="long_release"||id=="string_res"||id=="dynamics"||id=="reverb_mix"||id=="master_comp"||(id.contains("_fx_")&&id.endsWith("_enabled")))child->setAttribute("value",0.0);
            else if(id=="eq_low_db"||id=="eq_mid_db"||id=="eq_high_db"||id=="tune_cents"||id=="a_transpose")child->setAttribute("value",0.0);
        }
    }
}
}
int renderReference(juce::AudioPluginInstance& plugin,const juce::File& sound,const juce::File& destination){
    try {
        if(!sound.existsAsFile())throw std::runtime_error("Piano fixture missing");
        juce::MemoryBlock state;plugin.getStateInformation(state);
        auto wrapper=juce::AudioProcessor::getXmlFromBinary(state.getData(),(int)state.getSize());
        if(!wrapper)throw std::runtime_error("Opaque reference host state");
        auto* component=wrapper->getChildByName("IComponent");if(!component)throw std::runtime_error("No component state");
        juce::MemoryBlock decoded;decoded.fromBase64Encoding(component->getAllSubText());
        auto xml=juce::AudioProcessor::getXmlFromBinary(decoded.getData(),(int)decoded.getSize());
        if(!xml)throw std::runtime_error("Opaque reference component state");
        if(xml->hasTagName("StagePianoState")){
            xml->setAttribute("selected",0);neutralPatch(*xml,sound.getFullPathName(),true);
            if(auto* patches=xml->getChildByName("Patches")){int index=0;for(auto* patch:patches->getChildIterator())neutralPatch(*patch,sound.getFullPathName(),index++==0);}
        }else if(xml->hasTagName("NPnoPlayer")){
            xml->setAttribute("samplePath",sound.getFullPathName());xml->deleteAllChildElementsWithTagName("Slot");
            for(int i=0;i<2;++i){auto* slot=xml->createNewChildElement("Slot");slot->setAttribute("index",i);slot->setAttribute("samplePath",i==0?sound.getFullPathName():juce::String{});}
        }else throw std::runtime_error("Unknown reference state format");
        juce::MemoryBlock inner;juce::AudioProcessor::copyXmlToBinary(*xml,inner);component->deleteAllTextElements();component->addTextElement(inner.toBase64Encoding());
        juce::MemoryBlock saved;juce::AudioProcessor::copyXmlToBinary(*wrapper,saved);plugin.setStateInformation(saved.getData(),(int)saved.getSize());
        juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;
        auto event=[&](juce::MidiMessage message){midi.addEvent(message,0);};
        auto render=[&](int blocks,std::vector<float>* output=nullptr){float peak=0;for(int b=0;b<blocks;++b){plugin.processBlock(audio,midi);midi.clear();peak=std::max(peak,audio.getMagnitude(0,256));if(output)for(int i=0;i<256;++i)for(int c=0;c<2;++c)output->push_back(audio.getSample(c,i));}return peak;};
        bool ready=false;
        for(int attempt=0;attempt<120;++attempt){event(juce::MidiMessage::allSoundOff(1));event(juce::MidiMessage::noteOn(1,60,(juce::uint8)127));if(render(32)>1e-7f){ready=true;break;}juce::Thread::sleep(500);}
        if(!ready)throw std::runtime_error("Reference produced no audio after normal file load; check its license/activation in the app");
        destination.createDirectory();
        struct Case{int note,velocity;const char* kind;};
        for(const auto& test:{Case{60,127,"held"},Case{60,100,"held"},Case{60,64,"held"},Case{60,32,"held"},Case{61,127,"held"},Case{60,127,"off1s"}}){
            event(juce::MidiMessage::allSoundOff(1));event(juce::MidiMessage::controllerEvent(1,64,0));render(32);
            event(juce::MidiMessage::noteOn(1,test.note,(juce::uint8)test.velocity));std::vector<float> captured;
            if(juce::String(test.kind)=="held")render(2250,&captured); // exactly 12 seconds at 48 kHz
            else {render(192,&captured);event(juce::MidiMessage::noteOff(1,test.note));render(558,&captured);}
            auto name="note"+juce::String(test.note)+"-v"+juce::String(test.velocity)+"-"+test.kind+".f32";
            writeReferenceAudio(destination.getChildFile(name),captured);
            std::cout<<"Rendered "<<plugin.getName()<<" "<<name<<" frames="<<captured.size()/2<<std::endl;
        }
        std::cout<<"48 kHz stereo float32 interleaved; FX off, one slot, reference latency="<<plugin.getLatencySamples()<<" samples\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"Reference comparison: "<<e.what()<<"\n";return 40;}
}
