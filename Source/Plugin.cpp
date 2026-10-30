#include <JuceHeader.h>
#include "Bridge.h"
#include "Volume.h"
#include "DynComp.h"
#include "Playback.h"
#include "LibraryCache.h"
#include "HardwareUI.h"
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
    void prepare(double r,float volume) {std::lock_guard<std::mutex> lock(mutex);rate=r;peakDecay=(float)std::exp(-1.0/(rate*0.05));clearVoices();gain.reset(r,0.02);gain.setCurrentAndTargetValue(volume);}
    void reset(){std::lock_guard<std::mutex> lock(mutex);clearVoices();}
    juce::String status(){std::lock_guard<std::mutex> lock(mutex);return requestedPath.isEmpty()?juce::String{}:(pending||loading?"Loading... "+juce::String(progress())+"%":message);}
    int progress()const{return (int)(progressState.load()&127);}
    bool busy(){std::lock_guard<std::mutex> lock(mutex);return pending||loading;}
    bool ready(){std::lock_guard<std::mutex> lock(mutex);return !pending&&!loading&&library!=nullptr;}
    juce::String savedPath(){std::lock_guard<std::mutex> lock(mutex);return pending||loading?requestedPath:path;}
    void writeState(juce::XmlElement& node){
        std::lock_guard<std::mutex> lock(mutex);const bool busy=pending||loading;
        node.setAttribute("samplePath",busy?requestedPath:path);node.setAttribute("ready",!busy&&library!=nullptr);
        node.setAttribute("loading",busy);node.setAttribute("progress",progress());
        node.setAttribute("pcmBytes",(double)(library?np_pcm_bytes(library.get()):0));node.setAttribute("cacheHit",cacheHit);
        node.setAttribute("status",busy?"Loading... "+juce::String(progress())+"%":message);
    }
    void loadFile(const juce::String& filePath,bool restoring=false) {
        {std::lock_guard<std::mutex> lock(mutex);requestedPath=filePath;if(filePath.isEmpty()){clearVoices();silenced=true;}restoreRequest=restoring;++generation;progressState.store(generation<<7);pending=true;
         message=filePath.isEmpty()?"Select a Nord .npno sound":"Loading "+juce::File(filePath).getFileName()+"...";}
        condition.notify_one();
    }
    void process(juce::AudioBuffer<float>& audio,const juce::MidiBuffer& midi,bool enabled,float volume,int compression) {
        std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
        if(!lock.owns_lock()){droppedMidi.store(true);return;}
        if(droppedMidi.exchange(false)) clearVoices();
        dynComp=compression;
        gain.setTargetValue(enabled?volume:0.0f);
        if(!library||silenced){gain.skip(audio.getNumSamples());return;}
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
    struct Voice {const NpView* recording=nullptr;double position=0,step=1;float strikeVelocity=1,level=0,envelope=1,releaseStep=0,tailGain=1,preLoopDecay=1,postLoopDecay=1,attackGain=1,peak=0,filterL=0,filterR=0,filterCoefficient=0;int note=0,channel=1;bool down=false,releasing=false;uint64_t age=0;};
    std::array<Voice,128> voices{};
    std::array<bool,16> sustain{};
    LibraryCache::Ptr library;
    std::vector<NpView> recordings;
    std::array<int,3> layerExtents{0,0,0};
    std::mutex mutex;std::condition_variable condition;
    bool pending=false,loading=false,restoreRequest=false,silenced=true;
    std::atomic<bool> stopping{false};
    std::atomic<uint64_t> generation{0};uint64_t counter=0;bool cacheHit=false;
    juce::String path,requestedPath,message="Select a Nord .npno sound";
    double rate=44100;float peakDecay=0.9995f;int dynComp=0;
    juce::SmoothedValue<float> gain{1.0f};
    std::atomic<bool> droppedMidi{false};
    std::atomic<uint64_t> progressState{0};
    struct ProgressContext{SoundLayer* layer;uint64_t ticket;};
    static bool loadCancelled(void* context){auto& v=*static_cast<ProgressContext*>(context);return v.layer->stopping.load()||v.layer->generation.load()!=v.ticket;}
    static void loadingProgress(void* context,uint32_t percent){
        auto& value=*static_cast<ProgressContext*>(context);
        auto current=value.layer->progressState.load();
        while((current>>7)==value.ticket)
            if(value.layer->progressState.compare_exchange_weak(current,(value.ticket<<7)|percent))break;
    }
    std::thread loader;
    void clearVoices(){voices.fill({});sustain.fill(false);}
    void loadLoop(){
        for(;;){
            juce::String request;uint64_t ticket;bool restoring;
            {std::unique_lock<std::mutex> lock(mutex);condition.wait(lock,[this]{return stopping||pending;});
             if(stopping)return;request=requestedPath;ticket=generation;restoring=restoreRequest;pending=false;loading=true;}
            char error[1024]{};
            LibraryCache::Ptr loaded;bool hit=false;
            std::vector<NpView> records;juce::String failure;
            if(request.isNotEmpty()){
                if(!juce::File(request).existsAsFile())failure="Sound missing. Use Select Sound to locate it.";
                else{
                    ProgressContext progressContext{this,ticket};
                    const auto file=juce::File(request);const auto key=(file.getFullPathName()+":"+juce::String(file.getSize())+":"+juce::String(file.getLastModificationTime().toMilliseconds())).toStdString();
                    loaded=LibraryCache::instance().acquire(key,[&]{return LibraryCache::Ptr(np_load_cancellable(request.toRawUTF8(),error,sizeof(error),loadingProgress,loadCancelled,&progressContext),np_free);},[&]{return loadCancelled(&progressContext);},hit);
                    if(hit)loadingProgress(&progressContext,100);
                    if(loaded){records.resize(np_count(loaded.get()));for(size_t i=0;i<records.size();++i)np_recording(loaded.get(),i,&records[i]);}
                    else failure="Could not load: "+juce::String::fromUTF8(error);
                }
            }
            {std::lock_guard<std::mutex> lock(mutex);if(stopping)return;
             if(ticket==generation){
                if(loaded||request.isEmpty()||restoring||silenced){cacheHit=hit;clearVoices();library.swap(loaded);recordings.swap(records);path=request;silenced=!library;
                    layerExtents.fill(0);for(const auto& r:recordings)if(r.bank<3)layerExtents[r.bank]=std::max(layerExtents[r.bank],(int)r.layer);
                }
                if(failure.isNotEmpty())message=failure;
                else if(library)message=juce::String::fromUTF8(np_name(library.get()));
                else message="Select a Nord .npno sound";
             }loading=false;}
            // Retired sample memory is released on this worker thread.
        }
    }
    void release(Voice& v) { if (!v.releasing) { v.releasing=true; v.releaseStep=Playback::releaseCoefficient(rate,v.recording?v.recording->release_ms:500.0f); } }
    const NpView* choose(int note, float velocity, int bank) {
        auto root=np_root(library.get(), (uint8_t)note);
        if (root == 255) return nullptr;
        return Playback::choose(recordings,root,juce::roundToInt(velocity*127.0f),bank,layerExtents[(size_t)bank]);
    }
    void start(int note, int channel, float velocity, int bank, float strikeVelocity=1.0f) {
        // Release level is driven by the main voice's recent pre-fader amplitude.
        if(bank==2)velocity=1.0f;
        auto* recording=choose(note,velocity,bank);
        if (!recording && bank==1) recording=choose(note,velocity,0);
        if (!recording) return;
        Voice* slot=nullptr;
        for (auto& v : voices) if (!v.recording) { slot=&v; break; }
        if (!slot) slot=&*std::min_element(voices.begin(), voices.end(), [](const Voice& a,const Voice& b){return a.age<b.age;});
        *slot={}; slot->strikeVelocity=strikeVelocity; slot->recording=recording; slot->note=note; slot->channel=channel; slot->down=bank!=2; slot->age=++counter;slot->attackGain=bank==2?1.0f:0.0f;
        const double fineSemitones=(double)np_fine(library.get(),(uint8_t)note)*0.7/100.0;
        slot->step=(double)np_rate()/rate*std::pow(2.0, ((double)note-recording->root+fineSemitones)/12.0);
        // Legacy bridge names: loop_decay is the pre-loop/source-time rate;
        // attack_decay is the in-loop/host-time metadata rate.
        slot->preLoopDecay=(float)std::pow((double)recording->loop_decay,slot->step);
        slot->postLoopDecay=bank==2?slot->preLoopDecay:(float)std::pow((double)recording->attack_decay,(double)np_rate()/rate);
        slot->filterCoefficient=(float)std::exp(-juce::MathConstants<double>::twoPi*3000.0*slot->step/(double)np_rate());
        slot->level=recording->sample_gain*(bank==2?7.6f*std::max(0.0f,strikeVelocity):np_key_gain(library.get(),(uint8_t)note)*Playback::velocityGain(velocity)*DynComp::gain(velocity,dynComp));
    }
    void damp(Voice& voice){
        if(!voice.recording||voice.recording->bank==2||voice.releasing)return;
        const int note=voice.note,channel=voice.channel;
        const float strikeVelocity=voice.peak;
        release(voice);
        // One release per actual damping event; never on an unmatched note-off,
        // never while CC64 still holds the voice, and never from another release.
        if(strikeVelocity>=1.0e-6f)start(note,channel,1.0f,2,strikeVelocity);
    }
    void handle(const juce::MidiMessage& m) {
        const auto ch=m.getChannel();
        if (ch<1 || ch>16) return;
        if (m.isNoteOn()) {
            for (auto& v:voices) if (v.recording && v.note==m.getNoteNumber() && v.channel==ch && v.down) { v.down=false; release(v); }
            start(m.getNoteNumber(),ch,Playback::drivenVelocity(m.getFloatVelocity()),sustain[ch-1]?1:0,m.getFloatVelocity());
        } else if (m.isNoteOff()) {
            for (auto& v:voices) if (v.recording && v.note==m.getNoteNumber() && v.channel==ch && v.down) {
                v.down=false;
                if (!sustain[ch-1] && m.getNoteNumber()<=np_damper(library.get())) damp(v);
            }
        } else if (m.isController() && m.getControllerNumber()==64) {
            sustain[ch-1]=m.getControllerValue()>=64;
            if (!sustain[ch-1]) for(auto& v:voices) if(v.recording && v.recording->bank!=2 && v.channel==ch && !v.down && v.note<=np_damper(library.get())) damp(v);
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
                    v.position=Playback::wrap(v.position,r);
                    if(v.position>=(double)r.frames||v.envelope<0.0001f||v.tailGain<1.0e-5f){v.recording=nullptr;break;}
                    size_t a=(size_t)v.position,b=std::min(a+1,r.frames-1);
                    if(Playback::hasLoop(r)&&b>=r.loop_end)b=r.loop_start;
                    float f=(float)(v.position-(double)a),level=v.level*v.envelope*v.tailGain*ramp[(size_t)i];
                    const float attack=v.attackGain;level*=attack;v.attackGain=std::min(1.0f,v.attackGain+1.0f/32.0f);
                    if(r.bank==2)level*=std::min(1.0f,(float)(((double)r.frames-v.position)/(v.step*64.0)));
                    auto sample=[&](size_t c){return Playback::pcm(r,a,c)+f*(Playback::pcm(r,b,c)-Playback::pcm(r,a,c));};
                    const float rawL=sample(0),rawR=sample(r.channels==2?1:0);
                    if(!v.releasing&&r.bank!=2){const float magnitude=std::max(std::abs(rawL),std::abs(rawR))*v.level*v.envelope*v.tailGain*attack;v.peak=std::max(magnitude,v.peak*peakDecay);}
                    v.filterL=v.filterCoefficient*v.filterL+(1-v.filterCoefficient)*rawL;
                    v.filterR=v.filterCoefficient*v.filterR+(1-v.filterCoefficient)*rawR;
                    left[base+i]+=v.filterL*level;right[base+i]+=v.filterR*level;
                    v.tailGain*=Playback::hasLoop(r)&&v.position>=(double)r.loop_start?v.postLoopDecay:v.preLoopDecay;
                    v.position+=v.step;if(v.releasing)v.envelope*=v.releaseStep;
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
    static constexpr const char* dynCompID="dynComp";
    int dynCompLevel()const{return juce::jlimit(0,3,juce::roundToInt(parameters.getRawParameterValue(dynCompID)->load()));}
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
    void prepareToPlay(double r,int)override{for(int i=0;i<2;++i)layers[(size_t)i].prepare(r,enabled[(size_t)i]->load()>0.5f?Volume::snap(volumes[(size_t)i]->load()):0.0f);}
    void reset()override{for(auto& l:layers)l.reset();}
    void releaseResources()override{reset();}
    juce::String status(int i){return layers[(size_t)i].status();}
    bool busy(int i){return layers[(size_t)i].busy();}
    bool selected(int i){return layers[(size_t)i].savedPath().isNotEmpty();}
    void clearFile(int i){layers[(size_t)i].loadFile({});updateHostDisplay(ChangeDetails().withNonParameterStateChanged(true));}
    void loadFile(int i,const juce::File& file){
        auto* on=parameters.getParameter(onID(i));on->beginChangeGesture();on->setValueNotifyingHost(1);on->endChangeGesture();
        layers[(size_t)i].loadFile(file.getFullPathName());updateHostDisplay(ChangeDetails().withNonParameterStateChanged(true));
    }
    void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi)override{
        juce::ScopedNoDenormals guard;audio.clear();keyboard.processNextMidiBuffer(midi,0,audio.getNumSamples(),true);
        const int compression=dynCompLevel();
        for(int i=0;i<2;++i)layers[(size_t)i].process(audio,midi,enabled[(size_t)i]->load()>0.5f,Volume::snap(volumes[(size_t)i]->load()),compression);
        audio.applyGain(Volume::outputGain);
        midi.clear();
    }
    bool hasEditor()const override{return true;}
    juce::AudioProcessorEditor* createEditor()override;
    void getStateInformation(juce::MemoryBlock& dest)override{
        juce::XmlElement xml("NPnoPlayer");xml.setAttribute("version",3);
        xml.setAttribute("samplePath",layers[0].savedPath());
        for(int i=0;i<2;++i){auto* slot=xml.createNewChildElement("Slot");slot->setAttribute("index",i);layers[(size_t)i].writeState(*slot);}
        xml.addChildElement(parameters.copyState().createXml().release());copyXmlToBinary(xml,dest);
    }
    void setStateInformation(const void* data,int bytes)override{
        auto xml=getXmlFromBinary(data,bytes);if(!xml||!xml->hasTagName("NPnoPlayer"))return;
        if(auto* controls=xml->getChildByName("Controls")){
            auto state=juce::ValueTree::fromXml(*controls);
            // Keep the legacy linear-gain IDs and normalized automation mapping.
            // Snap saved gain to the nearest whole dB when opening older projects.
            for(auto child:state)for(int i=0;i<2;++i)
                if(child.getProperty("id").toString()==volumeID(i))
                    child.setProperty("value",Volume::snap((float)child.getProperty("value",1.0f)),nullptr);
            bool hasDynComp=false;
            for(auto child:state)if(child.getProperty("id").toString()==dynCompID)hasDynComp=true;
            if(!hasDynComp){juce::ValueTree value("PARAM");value.setProperty("id",dynCompID,nullptr);value.setProperty("value",0,nullptr);state.appendChild(value,nullptr);}
            parameters.replaceState(state);
        }
        else {parameters.getParameter(dynCompID)->setValueNotifyingHost(0);for(int i=0;i<2;++i){parameters.getParameter(onID(i))->setValueNotifyingHost(1.0f);parameters.getParameter(volumeID(i))->setValueNotifyingHost(1.0f);}}
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
            auto range=juce::NormalisableRange<float>(0,1,
                [](float,float,float value){return value;},
                [](float,float,float value){return value;},
                [](float,float,float value){return Volume::snap(value);});
            auto attributes=juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction([](float value,int){int db=Volume::decibels(value);return db==Volume::muteDb?juce::String("-inf dB"):juce::String(db)+" dB";})
                .withValueFromStringFunction([](const juce::String& text){return text.containsIgnoreCase("inf")||text.containsChar((juce::juce_wchar)0x221e)?0.0f:Volume::linear(juce::roundToInt(text.getFloatValue()));});
            layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(volumeID(i),1),name+" Volume",range,1.0f,attributes));}
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(dynCompID,1),"Dyn Comp",juce::StringArray{"OFF","1","2","3"},0));
        return layout;
    }
};

class VolumePanel final:public juce::Component {
public:
    VolumePanel(Player& p,int number):player(p),index(number){
        setLookAndFeel(&look);addAndMakeVisible(volume);addAndMakeVisible(readout);
        readout.setJustificationType(juce::Justification::centred);readout.setFont(Hardware::font(18));
        readout.setColour(juce::Label::textColourId,juce::Colour(0xffadffd0));
        readout.setEditable(true,true,false);readout.setTitle("Volume in dB");
        readout.setTooltip("Type -60 to 0 dB; -inf mutes");
        volume.setTitle("Sound "+juce::String(index+1)+" Volume");
        volume.setTooltip("1 dB steps; maximum 0 dB; bottom mutes");
        volumeAttachment=std::make_unique<juce::ParameterAttachment>(*player.parameters.getParameter(Player::volumeID(index)),[this](float gain){volume.setValue(Volume::decibels(gain),juce::dontSendNotification);updateVolume();});
        volume.onValueChange=[this]{
            float gain=Volume::linear(juce::roundToInt(volume.getValue()));
            if(dragging)volumeAttachment->setValueAsPartOfGesture(gain);else volumeAttachment->setValueAsCompleteGesture(gain);
            updateVolume();
        };
        volume.onDragStart=[this]{dragging=true;volumeAttachment->beginGesture();};
        volume.onDragEnd=[this]{volumeAttachment->endGesture();dragging=false;};
        readout.onTextChange=[this]{
            auto text=readout.getText();int db=text.containsIgnoreCase("inf")||text.containsChar((juce::juce_wchar)0x221e)?Volume::muteDb:juce::roundToInt(text.getFloatValue());
            volume.setValue(juce::jlimit(Volume::muteDb,0,db),juce::sendNotificationSync);updateVolume();
        };
        volumeAttachment->sendInitialUpdate();
    }
    ~VolumePanel()override{volume.cancelDrag();setLookAndFeel(nullptr);}
    void paint(juce::Graphics& g)override{
        Hardware::lcd(g,readout.getBounds().toFloat());
        g.setColour(juce::Colour(0xffe1e8e7));g.setFont(Hardware::font(22));
        g.drawText(index==0?"A":"B",0,getHeight()-22,getWidth(),22,juce::Justification::centred);
        g.setColour(juce::Colour(0xff8d9a9b).withAlpha(0.65f));
        g.drawLine((float)readout.getX(),(float)getHeight()-26,(float)readout.getRight(),(float)getHeight()-26,1);
        // These LEDs show the stored fader setting, not the audio signal level.
        const int lit=juce::roundToInt((float)(volume.getValue()-Volume::muteDb)/(float)-Volume::muteDb*16.0f);
        const auto track=ledBounds.toFloat();
        g.setColour(juce::Colour(0xff040a09));g.fillRoundedRectangle(track,2);
        const auto inside=track.reduced(3,2);
        const float gap=3,cellHeight=(inside.getHeight()-15*gap)/16.0f;
        for(int i=0;i<16;++i){float at=inside.getY()+(float)(15-i)*(cellHeight+gap);juce::Rectangle<float> cell(inside.getX(),at,inside.getWidth(),cellHeight);
            if(i<lit){g.setColour(juce::Colour(0xff3dff0b).withAlpha(0.13f));g.fillRoundedRectangle(cell.expanded(2),2);g.setGradientFill(juce::ColourGradient(juce::Colour(0xff9dff3d),cell.getTopLeft(),juce::Colour(0xff19d90a),cell.getBottomLeft(),false));}
            else g.setColour(juce::Colour(0xff374649));g.fillRoundedRectangle(cell,1);
        }
    }
    void resized()override{
        const int left=(getWidth()-84)/2;
        readout.setBounds(left,0,84,26);
        // Equal 20 px gaps: LCD bottom -> LED top, and LED bottom -> separator.
        // Keep the lower end fixed and extend the track upward.
        volume.setBounds(left,32,52,getHeight()-64);
        ledBounds={volume.getRight()+10,volume.getY()+Hardware::faderInset,22,volume.getHeight()-2*Hardware::faderInset};
    }
private:
    Player& player;int index;bool dragging=false;
    Hardware::Look look;juce::Label readout;Hardware::Fader volume;juce::Rectangle<int> ledBounds;
    std::unique_ptr<juce::ParameterAttachment> volumeAttachment;
    void updateVolume(){int db=juce::roundToInt(volume.getValue());readout.setText(db==Volume::muteDb?juce::String::fromUTF8("−∞ dB"):juce::String(db)+" dB",juce::dontSendNotification);repaint();}
};

class PianoKeyboard final:public juce::MidiKeyboardComponent {
public:
    explicit PianoKeyboard(juce::MidiKeyboardState& state):MidiKeyboardComponent(state,horizontalKeyboard){
        setTitle("88-key piano");setAvailableRange(21,108);setLowestVisibleKey(21);
        setScrollButtonsVisible(false);setMidiChannelsToDisplay(0xffff);
        setColour(whiteNoteColourId,juce::Colour(0xfff1f0e8));
        setColour(blackNoteColourId,juce::Colour(0xff121617));
        setColour(keyDownOverlayColourId,juce::Colour(0xfff38679));
        setColour(mouseOverKeyOverlayColourId,juce::Colour(0x35f38679));
        setColour(keySeparatorLineColourId,juce::Colour(0xff262b2c));
    }
    juce::String getWhiteNoteText(int)override{return {};}
};

class DynCompButton final:public juce::Button {
public:
    DynCompButton():juce::Button("Dyn Comp"){setTitle("Dynamic compression");setWantsKeyboardFocus(true);}
    void paintButton(juce::Graphics& g,bool hover,bool pressed)override{
        auto r=getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff020404));g.fillRect(r);
        g.setColour(juce::Colour(0xff515a5b));g.drawRect(r.reduced(1),1);
        auto face=r.reduced(5,4);
        g.setGradientFill(juce::ColourGradient(juce::Colour(pressed?0xff202728:hover?0xff4b5354:0xff3b4142),face.getTopLeft(),juce::Colour(0xff141819),face.getBottomLeft(),false));g.fillRoundedRectangle(face,8);
        g.setColour(juce::Colour(0xff555c5c));g.drawRoundedRectangle(face.reduced(0.5f),8,0.6f);
        if(hasKeyboardFocus(true)){g.setColour(juce::Colour(0xffe8eeee));g.drawRect(r.reduced(2),1);}
    }
};

class Editor final:public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit Editor(Player& p):AudioProcessorEditor(p),player(p),first(p,0),second(p,1),keys(p.keyboard),texture(Hardware::grain()){
        setLookAndFeel(&look);addAndMakeVisible(dynButton);
        dynButton.onClick=[this]{auto* p=player.parameters.getParameter(Player::dynCompID);p->beginChangeGesture();p->setValueNotifyingHost((float)((player.dynCompLevel()+1)%4)/3.0f);p->endChangeGesture();refresh();};
        addAndMakeVisible(first);addAndMakeVisible(second);addAndMakeVisible(keys);
        logo.addLineOfText(Hardware::font(26,true),"vini ",20,0);
        const auto stageX=logo.getGlyph(logo.getNumGlyphs()-1).getRight();
        logo.addLineOfText(juce::Font(juce::FontOptions(26).withStyle("Medium")),"stage",stageX,0);
        juce::Path logoOutline;logo.createPath(logoOutline);
        // Align visible glyphs, including the g descender, to the panel's bottom.
        logo.moveRangeOfGlyphs(0,-1,0,levelsBounds().getBottom()-logoOutline.getBounds().getBottom());
        for(int i=0;i<2;++i){
            auto& row=rows[(size_t)i];addAndMakeVisible(row);row.setFont(Hardware::font(20));
            row.setColour(juce::Label::textColourId,juce::Colour(0xffe8eeee));
            row.setJustificationType(juce::Justification::centredLeft);
            row.setTitle(i==0?"Sound A":"Sound B");
            auto& button=buttons[(size_t)i];addAndMakeVisible(button);
            button.setTitle(i==0?"Select or clear A":"Select or clear B");
            button.onClick=[this,i]{
                if(player.selected(i)){player.clearFile(i);refresh();return;}
                chooser=std::make_unique<juce::FileChooser>(i==0?"Select sound A":"Select sound B",juce::File{},"*.npno",true);
                chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
                    [safe=juce::Component::SafePointer<Editor>(this),i](const juce::FileChooser& c){
                        if(safe&&c.getResult().existsAsFile()){safe->player.loadFile(i,c.getResult());safe->refresh();}
                    });
            };
        }
        setSize(1000,445);refresh();startTimerHz(10);
    }
    ~Editor()override{setLookAndFeel(nullptr);}
    void paint(juce::Graphics& g)override{
        g.fillAll(juce::Colour(0xff111514));auto r=getLocalBounds().toFloat().reduced(4);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffbd252a),r.getTopLeft(),juce::Colour(0xff79070e),r.getBottomRight(),false));g.fillRoundedRectangle(r,12);
        g.saveState();juce::Path clip;clip.addRoundedRectangle(r,12);g.reduceClipRegion(clip);g.setTiledImageFill(texture,0,0,1);g.fillRect(r);g.restoreState();
        g.setColour(juce::Colour(0xffed6a67).withAlpha(0.65f));g.drawRoundedRectangle(r.reduced(1),11,1);
        g.setColour(juce::Colour(0xffe2e5e1));logo.draw(g);
        g.setFont(Hardware::font(12));
        g.drawText("v" JucePlugin_VersionString,getWidth()-112,14,90,18,juce::Justification::centredRight);
        auto frame=keys.getBounds().toFloat().expanded(5);
        g.setColour(juce::Colour(0xff020606));g.fillRoundedRectangle(frame,5);
        g.setColour(juce::Colour(0xff525c5b));g.drawRoundedRectangle(frame,5,1);
        auto panel=levelsBounds();
        juce::Path panelClip;panelClip.addRoundedRectangle(panel,8);
        g.saveState();g.reduceClipRegion(panelClip);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff303b40),panel.getTopLeft(),juce::Colour(0xff1b252a),panel.getBottomRight(),false));g.fillRect(panel);
        g.setTiledImageFill(texture,0,0,1);g.fillRect(panel);
        auto header=panel.withHeight(35);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffd8dcd9),header.getTopLeft(),juce::Colour(0xff9fa8a7),header.getBottomLeft(),false));g.fillRect(header);
        g.restoreState();
        g.setColour(juce::Colour(0xff172126));g.setFont(Hardware::font(18));g.drawText("L E V E L S",header,juce::Justification::centred);
        // Two LEDs encode OFF / 1 / 2 / 3; both light for level 3.
        // Centre in the free area between B’s LED column (x=412) and the panel edge (x=515).
        g.saveState();g.addTransform(juce::AffineTransform::translation(13.5f,0));
        g.setColour(juce::Colour(0xffe8eeee));g.setFont(Hardware::font(11));
        g.drawText("DYN COMP",414,130,72,16,juce::Justification::centred);
        g.drawText("2",423,152,12,16,juce::Justification::centred);
        g.drawText("1",423,176,12,16,juce::Justification::centred);
        g.drawText("3",460,164,12,16,juce::Justification::centred);
        Hardware::led(g,{450,160},player.dynCompLevel()>=2);Hardware::led(g,{450,184},(player.dynCompLevel()&1)!=0);
        g.restoreState();
        Hardware::lcd(g,{525,20,335,200});
        g.setColour(juce::Colour(0xffe8eeee));g.setFont(Hardware::font(25,true));g.drawText("Sounds",550,50,285,34,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xffbfc8c6));g.drawLine(550,95,835,95,1);
    }
    void resized()override{
        dynButton.setBounds(440,204,48,24);
        first.setBounds(163,70,124,222);second.setBounds(308,70,124,222);
        rows[0].setBounds(546,113,290,34);rows[1].setBounds(546,155,290,34);
        buttons[0].setBounds(556,240,120,44);buttons[1].setBounds(710,240,120,44);
        keys.setBounds(22,322,956,92);keys.setKeyWidth(956.0f/52.0f);keys.setLowestVisibleKey(21);
    }
private:
    Player& player;Hardware::Look look;VolumePanel first,second;PianoKeyboard keys;DynCompButton dynButton;juce::Image texture;juce::GlyphArrangement logo;
    static juce::Rectangle<float> levelsBounds(){return {140,20,375,280};}
    std::array<juce::Label,2> rows;std::array<juce::TextButton,2> buttons;
    std::unique_ptr<juce::FileChooser> chooser;
    void refresh(){
        const auto level=player.dynCompLevel();
        dynButton.setTooltip("Dyn Comp: "+(level==0?juce::String("OFF"):juce::String(level))+"; click to cycle OFF / 1 / 2 / 3");
        dynButton.setDescription(dynButton.getTooltip());repaint();
        for(int i=0;i<2;++i){auto letter=i==0?juce::String("A"):juce::String("B");
            rows[(size_t)i].setText(letter+":"+(player.status(i).isEmpty()?juce::String{}:" "+player.status(i)),juce::dontSendNotification);
            rows[(size_t)i].setTooltip(player.status(i));
            buttons[(size_t)i].setButtonText((player.selected(i)?"Clear ":"Select ")+letter);
            buttons[(size_t)i].setTooltip(player.selected(i)?"Remove sound "+letter:"Choose a .npno file for "+letter);
        }
    }
    void timerCallback()override{refresh();}
};
juce::AudioProcessorEditor* Player::createEditor(){return new Editor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Player();}
