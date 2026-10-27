#include <JuceHeader.h>
#include <iostream>
#include <stdexcept>

namespace {
using Plugin=juce::AudioPluginInstance;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::unique_ptr<juce::XmlElement> snapshot(Plugin& plugin){
    juce::MemoryBlock state;plugin.getStateInformation(state);
    auto wrapper=juce::AudioProcessor::getXmlFromBinary(state.getData(),(int)state.getSize());
    require(wrapper!=nullptr,"Invalid host state");
    auto* component=wrapper->getChildByName("IComponent");require(component!=nullptr,"No component state");
    juce::MemoryBlock data;require(data.fromBase64Encoding(component->getAllSubText()),"Invalid component encoding");
    auto result=juce::AudioProcessor::getXmlFromBinary(data.getData(),(int)data.getSize());
    require(result!=nullptr,"Invalid player state");return result;
}
void restoreXml(Plugin& plugin,const juce::XmlElement& state){
    juce::MemoryBlock inner;juce::AudioProcessor::copyXmlToBinary(state,inner);
    juce::XmlElement wrapper("VST3PluginState");wrapper.createNewChildElement("IComponent")->addTextElement(inner.toBase64Encoding());
    juce::MemoryBlock bytes;juce::AudioProcessor::copyXmlToBinary(wrapper,bytes);
    plugin.setStateInformation(bytes.getData(),(int)bytes.getSize());
}
void setPaths(Plugin& plugin,const std::array<juce::String,2>& paths){
    auto xml=snapshot(plugin);xml->setAttribute("samplePath",paths[0]);xml->deleteAllChildElementsWithTagName("Slot");
    for(int i=0;i<2;++i){auto* slot=xml->createNewChildElement("Slot");slot->setAttribute("index",i);slot->setAttribute("samplePath",paths[(size_t)i]);}
    restoreXml(plugin,*xml);
}
void waitFor(Plugin& plugin,const std::array<juce::String,2>& paths){
    for(int n=0;n<1200;++n){
        auto xml=snapshot(plugin);int found=0;
        for(auto* slot:xml->getChildIterator())if(slot->hasTagName("Slot")){
            int i=slot->getIntAttribute("index",-1);if(i<0||i>=2)continue;
            const int percent=slot->getIntAttribute("progress",-1);
            require(percent>=0&&percent<=100,"Loading percentage out of range");
            if(slot->getBoolAttribute("loading"))require(slot->getStringAttribute("status").startsWith("Loading... ")&&slot->getStringAttribute("status").endsWith("%"),"Loading percentage missing from sound display");
            bool expectedReady=paths[(size_t)i].isNotEmpty()&&juce::File(paths[(size_t)i]).existsAsFile();
            if(slot->getStringAttribute("samplePath")==paths[(size_t)i]&&!slot->getBoolAttribute("loading")&&slot->getBoolAttribute("ready")==expectedReady)++found;
        }
        if(found==2)return;juce::Thread::sleep(50);
    }throw std::runtime_error("Two-layer loading timed out");
}
juce::AudioProcessorParameter* parameter(Plugin& plugin,const juce::String& name){
    for(auto* p:plugin.getParameters())if(p->getName(128)==name)return p;
    throw std::runtime_error(("Parameter missing: "+name).toStdString());
}
void controls(Plugin& p,bool first,bool second,float a=1,float b=1){
    parameter(p,"Sound 1 On")->setValueNotifyingHost(first?1:0);
    parameter(p,"Sound 2 On")->setValueNotifyingHost(second?1:0);
    parameter(p,"Sound 1 Volume")->setValueNotifyingHost(a);
    parameter(p,"Sound 2 Volume")->setValueNotifyingHost(b);
}
std::vector<float> render(Plugin& plugin){
    juce::AudioBuffer<float> buffer(2,256);juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::allSoundOff(1),0);
    // Settle volume/ON smoothing before the deterministic test note.
    for(int n=0;n<10;++n){plugin.processBlock(buffer,midi);midi.clear();}
    midi.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)100),0);
    std::vector<float> out;
    for(int n=0;n<30;++n){plugin.processBlock(buffer,midi);midi.clear();for(int i=0;i<256;++i)for(int ch=0;ch<2;++ch)out.push_back(buffer.getSample(ch,i));}
    return out;
}
double energy(const std::vector<float>& audio){double sum=0;for(auto v:audio){require(std::isfinite(v),"Non-finite mixed audio");sum+=v*v;}return sum;}
void compare(const std::vector<float>& actual,const std::vector<float>& a,const std::vector<float>& b,float ga,float gb){
    require(actual.size()==a.size()&&a.size()==b.size(),"Render size mismatch");
    double maxError=0;
    for(size_t i=0;i<actual.size();++i)maxError=std::max(maxError,std::abs((double)actual[i]-(ga*a[i]+gb*b[i])));
    require(maxError<1e-5,"Layer mix/volume is incorrect");
}
}

int testLayers(juce::AudioPluginFormatManager& manager,const juce::PluginDescription& description,const juce::String& first,const juce::String& second){
    try{
        require(juce::File(first).existsAsFile()&&juce::File(second).existsAsFile(),"Two-layer test fixture missing; provide two existing .npno files");
        juce::String error;
        auto make=[&]{auto instance=manager.createPluginInstance(description,48000,256,error);require(instance!=nullptr,"Could not create layer test instance");instance->prepareToPlay(48000,256);return instance;};
        auto plugin=make();
        const std::array<juce::String,2> paths{first,second};
        setPaths(*plugin,paths);waitFor(*plugin,paths);
        controls(*plugin,true,false);auto a=render(*plugin);require(energy(a)>1e-6,"Sound 1 silent");
        controls(*plugin,false,true);auto b=render(*plugin);require(energy(b)>1e-6,"Sound 2 silent");
        controls(*plugin,true,true);compare(render(*plugin),a,b,1,1);
        std::cout<<"PASS independent sounds and simultaneous sum\n";
        const float firstGain=std::pow(10.0f,-12.0f/20.0f),secondGain=std::pow(10.0f,-4.0f/20.0f);
        controls(*plugin,true,true,0.25f,0.65f);compare(render(*plugin),a,b,firstGain,secondGain);
        std::cout<<"PASS independent quantized dB volume parameters\n";
        auto* volume=parameter(*plugin,"Sound 1 Volume");
        controls(*plugin,true,false);
        float previous=0;
        for(int db=-60;db<=0;++db){
            volume->setValueNotifyingHost(volume->getValueForText(juce::String(db)+" dB"));
            float expected=std::pow(10.0f,(float)db/20.0f);
            require(std::abs(volume->getValue()-expected)<1e-6f,"dB step gain is incorrect");
            require(volume->getValue()>previous,"Volume steps are not increasing");previous=volume->getValue();
            if(db==-60||db==-12||db==-6||db==-1||db==0)compare(render(*plugin),a,b,expected,0);
        }
        volume->setValueNotifyingHost(volume->getValueForText("+6 dB"));
        require(volume->getValue()==1.0f,"Volume exceeds 0 dB");
        volume->setValueNotifyingHost(volume->getValueForText("-inf dB"));
        require(energy(render(*plugin))<1e-12,"Minus infinity is not silent");
        std::cout<<"PASS 1 dB increments, measured attenuation, maximum 0 dB and minus-infinity mute\n";
        // Import a 0.2.0 project whose volume properties are still linear gains.
        auto legacy=snapshot(*plugin);legacy->setAttribute("version",2);
        auto* legacyControls=legacy->getChildByName("Controls");require(legacyControls!=nullptr,"Controls missing");
        for(auto* item:legacyControls->getChildIterator()){
            if(item->getStringAttribute("id")=="layer1Volume")item->setAttribute("value",0.25);
            if(item->getStringAttribute("id")=="layer2Volume")item->setAttribute("value",0.65);
        }
        restoreXml(*plugin,*legacy);waitFor(*plugin,paths);
        require(std::abs(parameter(*plugin,"Sound 1 Volume")->getValue()-firstGain)<1e-6f&&std::abs(parameter(*plugin,"Sound 2 Volume")->getValue()-secondGain)<1e-6f,"Legacy linear volumes did not migrate to nearest dB");
        std::cout<<"PASS old project volumes migrate to nearest whole dB\n";

        controls(*plugin,false,true,0.25f,0.65f);auto before=render(*plugin);compare(before,a,b,0,secondGain);
        std::cout<<"PASS Sound 1 OFF leaves Sound 2 playing\n";
        // Save the real host state, destroy the instrument and reopen a fresh instance.
        juce::MemoryBlock saved;plugin->getStateInformation(saved);plugin->releaseResources();plugin.reset();
        plugin=make();plugin->setStateInformation(saved.getData(),(int)saved.getSize());waitFor(*plugin,paths);
        require(parameter(*plugin,"Sound 1 On")->getValue()<0.5f&&parameter(*plugin,"Sound 2 On")->getValue()>0.5f,"ON/OFF did not restore");
        require(std::abs(parameter(*plugin,"Sound 1 Volume")->getValue()-firstGain)<1e-5f&&std::abs(parameter(*plugin,"Sound 2 Volume")->getValue()-secondGain)<1e-5f,"Volumes did not restore");
        compare(render(*plugin),a,b,0,secondGain);
        std::cout<<"PASS fresh-instance project restore: both files, ON/OFF, volumes and matching audio\n";
        controls(*plugin,false,false);require(energy(render(*plugin))<1e-12,"Both sounds OFF not silent");
        std::cout<<"PASS both sounds OFF\n";
        controls(*plugin,true,true,1,1);setPaths(*plugin,{"",second});waitFor(*plugin,{"",second});compare(render(*plugin),a,b,0,1);
        std::cout<<"PASS second sound works with first section empty\n";
        auto missing=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("vini-stage-missing-"+juce::Uuid().toString()+".npno").getFullPathName();
        setPaths(*plugin,{missing,second});waitFor(*plugin,{missing,second});compare(render(*plugin),a,b,0,1);
        std::cout<<"PASS missing file in first section does not interrupt second\n";
        // Rapid project restores must use the latest request, not discard it while loading.
        setPaths(*plugin,{second,first});setPaths(*plugin,paths);waitFor(*plugin,paths);
        controls(*plugin,true,false);compare(render(*plugin),a,b,1,0);
        std::cout<<"PASS latest queued restore wins\n";
        setPaths(*plugin,{second,first});
        juce::MemoryBlock loadingState;plugin->getStateInformation(loadingState);
        plugin->releaseResources();plugin.reset();plugin=make();
        plugin->setStateInformation(loadingState.getData(),(int)loadingState.getSize());waitFor(*plugin,{second,first});
        controls(*plugin,true,false);compare(render(*plugin),a,b,0,1);
        std::cout<<"PASS project saved during loading restores the requested sounds\n";
        plugin->releaseResources();return 0;
    }catch(const std::exception& e){std::cerr<<"Layer test failed: "<<e.what()<<"\n";return 20;}
}
