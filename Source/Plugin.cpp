#include <JuceHeader.h>
#include "Bridge.h"
#include <array>
#include <mutex>
#include <thread>

class Player final : public juce::AudioProcessor {
public:
    Player() : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}
    ~Player() override { if (loader.joinable()) loader.join(); }
    const juce::String getName() const override { return "NPno Player"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 30; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override {
        return l.getMainInputChannelSet().isDisabled() && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void prepareToPlay(double r, int) override { std::lock_guard<std::mutex> lock(mutex); rate = r; clearVoices(); }
    void reset() override { std::lock_guard<std::mutex> lock(mutex); clearVoices(); }
    void releaseResources() override { std::lock_guard<std::mutex> lock(mutex); clearVoices(); }
    juce::MidiKeyboardState keyboard;
    juce::String status() { std::lock_guard<std::mutex> lock(mutex); return message; }
    bool busy() const { return loading.load(); }
    void loadFile(const juce::File& file) {
        if (loading.exchange(true)) return;
        if (loader.joinable()) loader.join();
        { std::lock_guard<std::mutex> lock(mutex); message = "Loading " + file.getFileName() + "..."; }
        loader = std::thread([this, file] {
            char error[1024]{};
            std::unique_ptr<NpLibrary, decltype(&np_free)> loaded(np_load(file.getFullPathName().toRawUTF8(), error, sizeof(error)), np_free);
            std::vector<NpView> records;
            if (loaded) {
                records.resize(np_count(loaded.get()));
                for (size_t i=0; i<records.size(); ++i) np_recording(loaded.get(), i, &records[i]);
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (loaded) {
                    clearVoices();
                    library.swap(loaded);
                    recordings.swap(records);
                    path = file.getFullPathName();
                    message = juce::String::fromUTF8(np_name(library.get())) + "  |  " + juce::String((int)recordings.size()) + " recordings";
                } else message = "Could not load: " + juce::String::fromUTF8(error);
            }
            // The old sample data is released on this worker thread.
            loaded.reset();
            loading.store(false);
        });
    }
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override {
        juce::ScopedNoDenormals guard;
        audio.clear();
        keyboard.processNextMidiBuffer(midi, 0, audio.getNumSamples(), true);
        std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
        if (!lock.owns_lock()) { droppedMidi.store(true); midi.clear(); return; }
        if (droppedMidi.exchange(false)) clearVoices();
        if (!library) { midi.clear(); return; }
        int cursor = 0;
        for (const auto meta : midi) {
            int at = juce::jlimit(cursor, audio.getNumSamples(), meta.samplePosition);
            render(audio, cursor, at);
            handle(meta.getMessage());
            cursor = at;
        }
        render(audio, cursor, audio.getNumSamples());
        midi.clear();
    }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    void getStateInformation(juce::MemoryBlock& dest) override {
        juce::XmlElement xml("NPnoPlayer");
        { std::lock_guard<std::mutex> lock(mutex); xml.setAttribute("samplePath", path); }
        copyXmlToBinary(xml, dest);
    }
    void setStateInformation(const void* data, int bytes) override {
        if (auto xml = getXmlFromBinary(data, bytes)) {
            if (xml->hasTagName("NPnoPlayer")) {
                auto file = juce::File(xml->getStringAttribute("samplePath"));
                if (file.existsAsFile()) loadFile(file);
                else if (xml->getStringAttribute("samplePath").isNotEmpty()) {
                    std::lock_guard<std::mutex> lock(mutex); message = "Sound missing. Use Select Sound to locate it.";
                }
            }
        }
    }
private:
    struct Voice { const NpView* recording=nullptr; double position=0, step=1; float level=0, envelope=1, releaseStep=0; int note=0, channel=1; bool down=false, releasing=false; uint64_t age=0; };
    std::array<Voice, 128> voices{};
    std::array<bool, 16> sustain{};
    std::unique_ptr<NpLibrary, decltype(&np_free)> library{nullptr, np_free};
    std::vector<NpView> recordings;
    std::mutex mutex;
    std::thread loader;
    std::atomic<bool> loading{false}, droppedMidi{false};
    juce::String path, message="Select a Nord .npno sound";
    double rate=44100;
    uint64_t counter=0;
    void clearVoices() { voices.fill({}); sustain.fill(false); }
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
    void render(juce::AudioBuffer<float>& audio,int begin,int end) {
        auto* left=audio.getWritePointer(0); auto* right=audio.getWritePointer(1);
        for (auto& v:voices) {
            if(!v.recording) continue;
            const auto& r=*v.recording;
            for(int i=begin;i<end;++i) {
                if(v.position >= (double)r.frames || v.envelope<=0) {v.recording=nullptr;break;}
                size_t a=(size_t)v.position, b=std::min(a+1,r.frames-1);
                float f=(float)(v.position-(double)a), gain=v.level*v.envelope;
                auto sample=[&](size_t c){return r.data[a*r.channels+c]+f*(r.data[b*r.channels+c]-r.data[a*r.channels+c]);};
                left[i]+=sample(0)*gain; right[i]+=sample(r.channels==2?1:0)*gain;
                v.position+=v.step;
                if(v.releasing) v.envelope-=v.releaseStep;
            }
        }
    }
};

class Editor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit Editor(Player& p):AudioProcessorEditor(p), player(p), keys(p.keyboard,juce::MidiKeyboardComponent::horizontalKeyboard) {
        addAndMakeVisible(select); addAndMakeVisible(label); addAndMakeVisible(keys);
        select.setButtonText("Select Sound");
        select.onClick=[this] {
            chooser=std::make_unique<juce::FileChooser>("Select Nord piano",juce::File{},"*.npno",true);
            chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
                [safe=juce::Component::SafePointer<Editor>(this)](const juce::FileChooser& c){if(safe && c.getResult().existsAsFile()) safe->player.loadFile(c.getResult());});
        };
        label.setJustificationType(juce::Justification::centred);
        keys.setAvailableRange(21,108); keys.setLowestVisibleKey(48);
        setSize(520,190); startTimerHz(8);
    }
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff202329));g.setColour(juce::Colours::white);g.setFont(21);g.drawText("NPno Player",20,12,getWidth()-40,30,juce::Justification::centred);}
    void resized() override {select.setBounds(180,50,160,34);label.setBounds(12,90,getWidth()-24,30);keys.setBounds(12,130,getWidth()-24,48);}
private:
    Player& player; juce::TextButton select; juce::Label label; juce::MidiKeyboardComponent keys; std::unique_ptr<juce::FileChooser> chooser;
    void timerCallback() override {label.setText(player.status(),juce::dontSendNotification);select.setEnabled(!player.busy());}
};
juce::AudioProcessorEditor* Player::createEditor(){return new Editor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Player();}
