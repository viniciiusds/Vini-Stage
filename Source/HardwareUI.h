#pragma once
#include <JuceHeader.h>
#include "Volume.h"

namespace Hardware {
constexpr int faderInset=14;
inline juce::Image grain() {
    juce::Image image(juce::Image::ARGB,64,64,true);
    juce::Random random(42);
    for(int y=0;y<64;++y)for(int x=0;x<64;++x)
        image.setPixelAt(x,y,(random.nextBool()?juce::Colours::white:juce::Colours::black).withAlpha(0.02f+random.nextFloat()*0.035f));
    return image;
}
inline juce::Font font(float height,bool bold=false) {
    return juce::Font(juce::FontOptions(height).withStyle(bold?"Bold":"Regular"));
}
inline void lcd(juce::Graphics& g,juce::Rectangle<float> r) {
    g.setColour(juce::Colour(0xff858b85));g.fillRoundedRectangle(r,3);
    r=r.reduced(1.5f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff010403),r.getTopLeft(),juce::Colour(0xff101714),r.getBottomRight(),false));
    g.fillRoundedRectangle(r,2);
    g.setColour(juce::Colours::black.withAlpha(0.65f));g.drawRoundedRectangle(r.reduced(1),2,2);
}
inline void led(juce::Graphics& g,juce::Point<float> centre,bool on) {
    if(on){g.setGradientFill(juce::ColourGradient(juce::Colour(0xaaff3025),centre.x,centre.y,juce::Colours::transparentBlack,centre.x+15,centre.y,true));g.fillEllipse(centre.x-15,centre.y-15,30,30);}
    g.setColour(juce::Colour(0xff090c0c));g.fillEllipse(centre.x-8,centre.y-8,16,16);
    g.setColour(juce::Colour(0xff7d8585));g.drawEllipse(centre.x-7,centre.y-7,14,14,1);
    g.setGradientFill(juce::ColourGradient(on?juce::Colour(0xffffb09a):juce::Colour(0xff626b6b),centre.x-3,centre.y-4,on?juce::Colour(0xffd20e12):juce::Colour(0xff202828),centre.x+4,centre.y+5,false));
    g.fillEllipse(centre.x-5,centre.y-5,10,10);
}
inline void screw(juce::Graphics& g,float x,float y) {
    g.setColour(juce::Colours::black.withAlpha(0.5f));g.fillEllipse(x-7,y-6,15,15);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff777979),x-5,y-5,juce::Colour(0xff151919),x+5,y+5,false));g.fillEllipse(x-6,y-6,12,12);
    g.setColour(juce::Colour(0xff090a0b));g.drawLine(x-3,y-3,x+3,y+3,2);g.drawLine(x-3,y+3,x+3,y-3,2);
}

class Look final:public juce::LookAndFeel_V4 {
public:
    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider)override{
        juce::Slider::SliderLayout layout;layout.sliderBounds=slider.getLocalBounds().reduced(0,faderInset);return layout;
    }
    void drawButtonBackground(juce::Graphics& g,juce::Button& button,const juce::Colour&,bool over,bool down)override {
        auto r=button.getLocalBounds().toFloat().reduced(2);
        g.setColour(juce::Colours::black.withAlpha(0.6f));g.fillRoundedRectangle(r.translated(0,2),3);
        g.setColour(juce::Colour(0xff646c6d));g.fillRoundedRectangle(r,3);r=r.reduced(1.5f);
        g.setColour(juce::Colour(0xff080a0b));g.fillRoundedRectangle(r,2);r=r.reduced(3);
        bool power=button.getComponentID()=="power";
        auto top=down?juce::Colour(0xff14191a):juce::Colour(over?0xff41484a:0xff303637);
        g.setGradientFill(juce::ColourGradient(top,r.getTopLeft(),juce::Colour(0xff111516),r.getBottomLeft(),false));g.fillRoundedRectangle(r,2);
        g.setColour(juce::Colour(0xff747b7b).withAlpha(down?0.15f:0.5f));g.drawLine(r.getX()+3,r.getY()+1,r.getRight()-3,r.getY()+1,1);
        if(power){auto strip=r.withHeight(4).reduced(5,0).translated(0,5);g.setColour(button.getToggleState()?juce::Colour(0xffff3934):juce::Colour(0xff596162));g.fillRoundedRectangle(strip,1);}
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& button,bool,bool)override {
        if(button.getComponentID()=="power")return;
        g.setColour(juce::Colour(0xffd8dfde).withAlpha(button.isEnabled()?1.0f:0.45f));g.setFont(font(17));g.drawText(button.getButtonText(),button.getLocalBounds().reduced(6),juce::Justification::centred);
    }
    void drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float sliderPos,float,float,juce::Slider::SliderStyle,juce::Slider&)override {
        float centre=(float)x+(float)width*0.5f,top=(float)y,bottom=(float)(y+height);
        g.setColour(juce::Colour(0xff737c7d));g.fillRoundedRectangle(centre-4,top-1,8,bottom-top+2,3);
        g.setColour(juce::Colour(0xff020606));g.fillRoundedRectangle(centre-3,top,6,bottom-top,3);
        for(int i=0;i<=12;++i){float at=top+(bottom-top)*(float)i/12.0f;g.setColour(juce::Colour(0xffc0c9c9));g.drawLine(centre-17,at,centre-11,at,1);g.drawLine(centre+11,at,centre+17,at,1);}
        juce::Rectangle<float> cap(centre-16,juce::jlimit(top,bottom,sliderPos)-14,32,28);
        g.setColour(juce::Colours::black.withAlpha(0.55f));g.fillRoundedRectangle(cap.translated(2,3),2);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe0e4e3),cap.getTopLeft(),juce::Colour(0xff888f90),cap.getBottomLeft(),false));g.fillRoundedRectangle(cap,2);
        g.setColour(juce::Colour(0xfff1f3f0));g.drawRoundedRectangle(cap.reduced(1),1,1);
        g.setColour(juce::Colour(0xff555e60));g.drawLine(cap.getX()+4,cap.getCentreY(),cap.getRight()-4,cap.getCentreY(),2);
        g.setColour(juce::Colour(0xfff4f6f2));g.drawLine(cap.getX()+4,cap.getCentreY()+2,cap.getRight()-4,cap.getCentreY()+2,1);
    }
};

class Fader final:public juce::Slider {
public:
    Fader(){setSliderStyle(juce::Slider::LinearVertical);setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);setRange(Volume::muteDb,0,1);setDoubleClickReturnValue(true,0);setWantsKeyboardFocus(true);}
    void mouseDown(const juce::MouseEvent& event)override {
        if(!isEnabled()||!event.mods.isLeftButtonDown())return;
        cancelDrag();
        const float top=(float)faderInset,bottom=(float)getHeight()-faderInset;
        const float capY=bottom-(float)valueToProportionOfLength(getValue())*(bottom-top);
        const bool onCap=std::abs(event.position.y-capY)<=14
            &&std::abs(event.position.x-(float)getWidth()/2)<=16;
        grabOffset=onCap?event.position.y-capY:0;
        dragGesture=std::make_unique<juce::Slider::ScopedDragNotification>(*this);
        updateFromMouse(event.position.y);
    }
    void mouseDrag(const juce::MouseEvent& event)override {
        if(dragGesture&&isEnabled())updateFromMouse(event.position.y);
    }
    void mouseUp(const juce::MouseEvent& event)override {
        if(dragGesture&&isEnabled())updateFromMouse(event.position.y);
        cancelDrag();
    }
    void mouseDoubleClick(const juce::MouseEvent& event)override {
        cancelDrag();juce::Slider::mouseDoubleClick(event);
    }
    void cancelDrag(){dragGesture.reset();}
    void enablementChanged()override{if(!isEnabled())cancelDrag();}
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails& w)override {
        float delta=w.deltaY!=0?w.deltaY:w.deltaX;if(w.isReversed)delta=-delta;
        if(delta!=0)setValue(getValue()+(delta>0?1:-1),juce::sendNotificationSync);
    }
    bool keyPressed(const juce::KeyPress& key)override {
        auto k=key.getKeyCode();
        if(k==juce::KeyPress::upKey||k==juce::KeyPress::rightKey){setValue(getValue()+1,juce::sendNotificationSync);return true;}
        if(k==juce::KeyPress::downKey||k==juce::KeyPress::leftKey){setValue(getValue()-1,juce::sendNotificationSync);return true;}
        if(k==juce::KeyPress::homeKey){setValue(Volume::muteDb,juce::sendNotificationSync);return true;}
        if(k==juce::KeyPress::endKey){setValue(0,juce::sendNotificationSync);return true;}
        return juce::Slider::keyPressed(key);
    }
private:
    float grabOffset=0;
    std::unique_ptr<juce::Slider::ScopedDragNotification> dragGesture;
    void updateFromMouse(float mouseY){
        const float top=(float)faderInset,bottom=(float)getHeight()-faderInset;
        if(bottom<=top)return;
        const double proportion=juce::jlimit(0.0,1.0,(double)(bottom-(mouseY-grabOffset))/(double)(bottom-top));
        // Update UI and the host synchronously for every whole-dB step, while held.
        setValue(proportionOfLengthToValue(proportion),juce::sendNotificationSync);
    }

};
}
