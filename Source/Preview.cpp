#include <JuceHeader.h>
#include <iostream>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
int main(int argc,char** argv){
    juce::ScopedJuceInitialiser_GUI init;
    if(argc<2)return 1;
    std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
    editor->setVisible(true);
    auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true,2.0f);
    juce::File file(argv[1]);auto stream=file.createOutputStream();
    if(!stream)return 2;
    stream->setPosition(0);stream->truncate();
    if(!juce::PNGImageFormat().writeImageToStream(image,*stream))return 2;
    std::cout<<"Rendered plugin UI: "<<file.getFullPathName()<<"\n";
}
