#include <JuceHeader.h>
#include "Bridge.h"
#include <array>
#include <condition_variable>
#include <mutex>
#include <thread>

class SoundLayer {
public:
    SoundLayer() { loader=std::thread([this]{loadLoop();}); }
    ~SoundLayer() {
        {std::lock_guard<std::mutex> lock(mutex);stopping=true;}
        condition.notify_one();loader.join();
    }
    void prepare(double r,float volume) {std::lock_guard<std::mutex> lock(mutex);rate=r;clearVoices();gain.reset(r,0.02);gain.setCurrentAndTargetValue(volume);}
    void reset(){std::lock_guard<std::mutex> lock(mutex);clearVoices();}
    juce::String status(){std::lock_guard<std::mutex> lock(mutex);return message;}
    bool busy(){std::lock_guard<std::mutex> lock(mutex);return pending||loading;}
    bool ready(){std::lock_guard<std::mutex> lock(mutex);return !pending&&!loading&&library!=nullptr;}
    juce::String savedPath(){std::lock_guard<std::mutex> lock(mutex);return pending||loading?requestedPath:path;}
    void loadFile(const juce::String& filePath,bool restoring=false) {
        {std::lock_guard<std::mutex> lock(mutex);requestedPath=filePath;restoreRequest=restoring;++generation;pending=true;
         message=filePath.isEmpty()?"Select a Nord .npno sound":"Loading "+juce::File(filePath).getFileName()+"...";}
        condition.notify_one();
    }
    void process(juce::AudioBuffer<float>& audio,const juce::MidiBuffer& midi,bool enabled,float volume) {
        std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
        if(!lock.owns_lock()){droppedMidi.store(true);return;}
        if(droppedMidi.exchange(false)) clearVoices();
        gain.setTargetValue(enabled?volume:0.0f);
        if(!library){gain.skip(audio.getNumSamples());return;}
        int cursor=0;
        for(const auto meta:midi){
            int at=juce::jlimit(cursor,audio.getNumSamples(),meta.samplePosition);
            render(audio,cursor,at);
            auto m=meta.getMessage();if(enabled||!m.isNoteOn()) handle(m);cursor=at;
        }
        render(audio,cursor,audio.getNumSamples());
        if(!enabled&&!gain.isSmoothing()) voices.fill({});
    }
private:
    struct Voice {const NpView* recording=nullptr;double position=0,step=1;float level=0,envelope=1,releaseStep=0;int note=0,channel=1;bool down=false,releasing=false;uint64_t age=0;};
    std::array<Voice,128> voices{};
    std::array<bool,16> sustain{};
    std::unique_ptr<NpLibrary,decltype(&np_free)> library{nullptr,np_free};
    std::vector<NpView> recordings;
    std::mutex mutex;std::condition_variable condition;
    bool stopping=false,pending=false,loading=false,restoreRequest=false;
    uint64_t generation=0,counter=0;
    juce::String path,requestedPath,message="Select a Nord .npno sound";
    double rate=44100;
    juce::SmoothedValue<float> gain{1.0f};
    std::atomic<bool> droppedMidi{false};
    std::thread loader;
    void clearVoices(){voices.fill({});sustain.fill(false);}
    void loadLoop(){
        for(;;){
            juce::String request;uint64_t ticket;bool restoring;
            {std::unique_lock<std::mutex> lock(mutex);condition.wait(lock,[this]{return stopping||pending;});
             if(stopping)return;request=requestedPath;ticket=generation;restoring=restoreRequest;pending=false;loading=true;}
            char error[1024]{};
            std::unique_ptr<NpLibrary,decltype(&np_free)> loaded(nullptr,np_free);
            std::vector<NpView> records;juce::String failure;
            if(request.isNotEmpty()){
                if(!juce::File(request).existsAsFile())failure="Sound missing. Use Select Sound to locate it.";
                else{
                    loaded.reset(np_load(request.toRawUTF8(),error,sizeof(error)));
                    if(loaded){records.resize(np_count(loaded.get()));for(size_t i=0;i<records.size();++i)np_recording(loaded.get(),i,&records[i]);}
                    else failure="Could not load: "+juce::String::fromUTF8(error);
                }
            }
            {std::lock_guard<std::mutex> lock(mutex);if(stopping)return;
             if(ticket==generation){
                if(loaded||request.isEmpty()||restoring){clearVoices();library.swap(loaded);recordings.swap(records);path=request;}
                if(failure.isNotEmpty())message=failure;
                else if(library)message=juce::String::fromUTF8(np_name(library.get()));
                else message="Select a Nord .npno sound";
             }loading=false;}
            // Retired sample memory is released on this worker thread.
        }
    }
    void release(Voice& v) { if (!v.releasing) { v.releasing=true; v.releaseStep=v.envelope / (float)(rate * 0.09); } }
    const NpView* choose(int note, float velocity, int bank) {
        auto root=np_root(library.get(), (uint8_t)note);
        if (root == 255) return nullptr;
        int count=0;
        const NpView* hardest=nullptr;
        for (auto& r : recordings) if (r.root==root && r.bank==bank && r.frames>0) {
            ++count;
            if (!hardest || r.layer<hardest->layer) hardest=&r;
        }
        if (!hardest) return nullptr;
        // Provisional velocity mapping: spread the ordered recordings over MIDI velocity.
        // Nord-specific thresholds still require comparison with the reference.
        const int midiVelocity=juce::jlimit(1,127,juce::roundToInt(velocity*127.0f));
        const int target=juce::roundToInt((127-midiVelocity)*(float)(count-1)/126.0f);
        for (auto& r : recordings) if (r.root==root && r.bank==bank && r.frames>0) {
            int rank=0;
            for(auto& other:recordings) if(other.root==root && other.bank==bank && other.frames>0 && other.layer<r.layer) ++rank;
            if(rank==target) return &r;
        }
        return hardest;
    }
    void start(int note, int channel, float velocity, int bank) {
        auto* recording=choose(note,velocity,bank);
        if (!recording && bank==1) recording=choose(note,velocity,0);
        if (!recording) return;
        Voice* slot=nullptr;
        for (auto& v : voices) if (!v.recording) { slot=&v; break; }
        if (!slot) slot=&*std::min_element(voices.begin(), voices.end(), [](const Voice& a,const Voice& b){return a.age<b.age;});
        *slot={}; slot->recording=recording; slot->note=note; slot->channel=channel; slot->down=bank!=2; slot->age=++counter;
        const double fineSemitones=(double)np_fine(library.get(),(uint8_t)note)*0.7/100.0;
        slot->step=(double)np_rate()/rate*std::pow(2.0, ((double)note-recording->root+fineSemitones)/12.0);
        // Gentle gain response between recorded layers, with output headroom.
        slot->level=np_gain(library.get())*(0.3f+0.7f*velocity)*0.65f;
    }
    void handle(const juce::MidiMessage& m) {
        const auto ch=m.getChannel();
        if (ch<1 || ch>16) return;
        if (m.isNoteOn()) {
            for (auto& v:voices) if (v.recording && v.note==m.getNoteNumber() && v.channel==ch && v.down) { v.down=false; release(v); }
            start(m.getNoteNumber(),ch,m.getFloatVelocity(),sustain[ch-1]?1:0);
        } else if (m.isNoteOff()) {
            for (auto& v:voices) if (v.recording && v.note==m.getNoteNumber() && v.channel==ch && v.down) {
                v.down=false;
                if (!sustain[ch-1] && m.getNoteNumber()<=np_damper(library.get())) release(v);
            }
            // Release recordings have not been calibrated against the main voices.
            // Starting one at a fixed level on every note-off causes a fresh,
            // unrelated burst, even while the sustain pedal is holding the note.
            // Let the existing voice fade out instead.
        } else if (m.isController() && m.getControllerNumber()==64) {
            sustain[ch-1]=m.getControllerValue()>=64;
            if (!sustain[ch-1]) for(auto& v:voices) if(v.recording && v.channel==ch && !v.down && v.note<=np_damper(library.get())) release(v);
        } else if (m.isAllSoundOff()) {
            for(auto& v:voices) if(v.channel==ch) v={};
            sustain[ch-1]=false;
        } else if (m.isAllNotesOff() || (m.isController() && m.getControllerNumber()==121)) {
            sustain[ch-1]=false;
            for(auto& v:voices) if(v.recording && v.channel==ch) {v.down=false; release(v);}
        }
    }
    void render(juce::AudioBuffer<float>& audio,int begin,int end){
        auto* left=audio.getWritePointer(0);auto* right=audio.getWritePointer(1);
        std::array<float,1024> ramp;
        for(int base=begin;base<end;){
            int size=std::min((int)ramp.size(),end-base);
            for(int i=0;i<size;++i)ramp[(size_t)i]=gain.getNextValue();
            for(auto& v:voices){
                if(!v.recording)continue;const auto& r=*v.recording;
                for(int i=0;i<size;++i){
                    if(v.position>=(double)r.frames||v.envelope<=0){v.recording=nullptr;break;}
                    size_t a=(size_t)v.position,b=std::min(a+1,r.frames-1);
                    float f=(float)(v.position-(double)a),level=v.level*v.envelope*ramp[(size_t)i];
                    auto sample=[&](size_t c){return r.data[a*r.channels+c]+f*(r.data[b*r.channels+c]-r.data[a*r.channels+c]);};
                    left[base+i]+=sample(0)*level;right[base+i]+=sample(r.channels==2?1:0)*level;
                    v.position+=v.step;if(v.releasing)v.envelope-=v.releaseStep;
                }
            }base+=size;
        }
    }
};

class Player final:public juce::AudioProcessor {
public:
    Player():AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),parameters(*this,nullptr,"Controls",makeParameters()){
        for(int i=0;i<2;++i){enabled[(size_t)i]=parameters.getRawParameterValue(onID(i));volumes[(size_t)i]=parameters.getRawParameterValue(volumeID(i));}
    }
    static juce::String onID(int i){return "layer"+juce::String(i+1)+"On";}
    static juce::String volumeID(int i){return "layer"+juce::String(i+1)+"Volume";}
    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboard;
    const juce::String getName()const override{return "Vini Stage";}
    bool acceptsMidi()const override{return true;}
    bool producesMidi()const override{return false;}
    double getTailLengthSeconds()const override{return 30;}
    int getNumPrograms()override{return 1;}
    int getCurrentProgram()override{return 0;}
    void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return "Default";}
    void changeProgramName(int,const juce::String&)override{}
    bool isBusesLayoutSupported(const BusesLayout& l)const override{return l.getMainInputChannelSet().isDisabled()&&l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
    void prepareToPlay(double r,int)override{for(int i=0;i<2;++i)layers[(size_t)i].prepare(r,enabled[(size_t)i]->load()>0.5f?volumes[(size_t)i]->load():0.0f);}
    void reset()override{for(auto& l:layers)l.reset();}
    void releaseResources()override{reset();}
    juce::String status(int i){return layers[(size_t)i].status();}
    bool busy(int i){return layers[(size_t)i].busy();}
    void loadFile(int i,const juce::File& file){layers[(size_t)i].loadFile(file.getFullPathName());}
    void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi)override{
        juce::ScopedNoDenormals guard;audio.clear();keyboard.processNextMidiBuffer(midi,0,audio.getNumSamples(),true);
        for(int i=0;i<2;++i)layers[(size_t)i].process(audio,midi,enabled[(size_t)i]->load()>0.5f,volumes[(size_t)i]->load());
        midi.clear();
    }
    bool hasEditor()const override{return true;}
    juce::AudioProcessorEditor* createEditor()override;
    void getStateInformation(juce::MemoryBlock& dest)override{
        juce::XmlElement xml("NPnoPlayer");xml.setAttribute("version",2);
        xml.setAttribute("samplePath",layers[0].savedPath());
        for(int i=0;i<2;++i){auto* slot=xml.createNewChildElement("Slot");slot->setAttribute("index",i);slot->setAttribute("samplePath",layers[(size_t)i].savedPath());slot->setAttribute("ready",layers[(size_t)i].ready());slot->setAttribute("loading",layers[(size_t)i].busy());}
        xml.addChildElement(parameters.copyState().createXml().release());copyXmlToBinary(xml,dest);
    }
    void setStateInformation(const void* data,int bytes)override{
        auto xml=getXmlFromBinary(data,bytes);if(!xml||!xml->hasTagName("NPnoPlayer"))return;
        if(auto* controls=xml->getChildByName("Controls"))parameters.replaceState(juce::ValueTree::fromXml(*controls));
        else for(int i=0;i<2;++i){parameters.getParameter(onID(i))->setValueNotifyingHost(1.0f);parameters.getParameter(volumeID(i))->setValueNotifyingHost(1.0f);}
        std::array<juce::String,2> paths{};paths[0]=xml->getStringAttribute("samplePath");
        for(auto* slot:xml->getChildIterator())if(slot->hasTagName("Slot")){int i=slot->getIntAttribute("index",-1);if(i>=0&&i<2)paths[(size_t)i]=slot->getStringAttribute("samplePath");}
        for(int i=0;i<2;++i)layers[(size_t)i].loadFile(paths[(size_t)i],true);
    }
private:
    std::array<SoundLayer,2> layers;
    std::array<std::atomic<float>*,2> enabled{},volumes{};
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters(){
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        for(int i=0;i<2;++i){auto name="Sound "+juce::String(i+1);
            layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(onID(i),1),name+" On",true));
            layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(volumeID(i),1),name+" Volume",juce::NormalisableRange<float>(0,1),1.0f));}
        return layout;
    }
};

class LayerPanel final:public juce::Component {
public:
    LayerPanel(Player& p,int number):player(p),index(number){
        for(auto* c:std::array<juce::Component*,5>{&title,&select,&on,&name,&volume})addAndMakeVisible(c);
        title.setText("Sound "+juce::String(index+1),juce::dontSendNotification);title.setFont(juce::Font(juce::FontOptions(18.0f)));
        select.setButtonText("Select Sound");on.setButtonText("ON");on.setTooltip("Enable or mute this sound");
        volume.setSliderStyle(juce::Slider::LinearHorizontal);volume.setTextBoxStyle(juce::Slider::TextBoxRight,false,55,24);
        volume.setTooltip("Volume of Sound "+juce::String(index+1));volume.setTitle("Sound "+juce::String(index+1)+" Volume");volume.setDoubleClickReturnValue(true,1.0);
        name.setJustificationType(juce::Justification::centredLeft);
        toggleAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(player.parameters,Player::onID(index),on);
        volumeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(player.parameters,Player::volumeID(index),volume);
        // Attachment installs the parameter's formatter; replace it afterwards
        // so this fader displays percentages rather than the raw 0..1 value.
        volume.textFromValueFunction=[](double v){return juce::String(juce::roundToInt(v*100))+"%";};
        volume.valueFromTextFunction=[](const juce::String& s){return s.getDoubleValue()/100.0;};
        volume.updateText();
        select.onClick=[this]{
            chooser=std::make_unique<juce::FileChooser>("Select Nord piano — Sound "+juce::String(index+1),juce::File{},"*.npno",true);
            chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
                [safe=juce::Component::SafePointer<LayerPanel>(this)](const juce::FileChooser& c){if(safe&&c.getResult().existsAsFile())safe->player.loadFile(safe->index,c.getResult());});
        };refresh();
    }
    void refresh(){name.setText(player.status(index),juce::dontSendNotification);select.setEnabled(!player.busy(index));on.setButtonText(on.getToggleState()?"ON":"OFF");}
    void paint(juce::Graphics& g)override{g.setColour(juce::Colour(0xff2b3038));g.fillRoundedRectangle(getLocalBounds().toFloat(),8);g.setColour(juce::Colour(0xffa7acb5));g.setFont(13);g.drawText("Volume",16,110,70,24,juce::Justification::centredLeft);}
    void resized()override{title.setBounds(14,10,180,28);on.setBounds(getWidth()-80,10,68,28);select.setBounds(16,47,130,30);name.setBounds(12,80,getWidth()-24,24);volume.setBounds(72,110,getWidth()-86,26);}
private:
    Player& player;int index;
    juce::Label title,name;juce::TextButton select;juce::ToggleButton on;juce::Slider volume;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toggleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
};

class Editor final:public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit Editor(Player& p):AudioProcessorEditor(p),first(p,0),second(p,1),keys(p.keyboard,juce::MidiKeyboardComponent::horizontalKeyboard){
        addAndMakeVisible(first);addAndMakeVisible(second);addAndMakeVisible(keys);keys.setAvailableRange(21,108);keys.setLowestVisibleKey(48);setSize(760,286);startTimerHz(10);
    }
    void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff202329));g.setColour(juce::Colours::white);g.setFont(23);g.drawText("Vini Stage",20,8,getWidth()-40,32,juce::Justification::centred);}
    void resized()override{first.setBounds(16,48,356,150);second.setBounds(388,48,356,150);keys.setBounds(16,216,getWidth()-32,54);}
private:
    LayerPanel first,second;juce::MidiKeyboardComponent keys;
    void timerCallback()override{first.refresh();second.refresh();}
};
juce::AudioProcessorEditor* Player::createEditor(){return new Editor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Player();}
