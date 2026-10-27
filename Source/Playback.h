#pragma once
#include "Bridge.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace Playback {
// Independently implemented piecewise transfer functions from observed numerical
// behaviour. These tables are calibration data, not recovered source code.
inline float interpolate(float x,const float* points,const float* values,size_t n){
 if(x<=points[0])return values[0];if(x>=points[n-1])return values[n-1];
 size_t i=1;while(x>points[i])++i;
 return values[i-1]+(values[i]-values[i-1])*(x-points[i-1])/(points[i]-points[i-1]);
}
inline float selectionCoordinate(int velocity){
 static constexpr float midi[]={1.000000f,40.000000f,48.000000f,56.000000f,64.000000f,72.000000f,80.000000f,84.000000f,88.000000f,96.000000f,100.000000f,104.000000f,112.000000f,116.000000f,120.000000f,127.000000f};
 static constexpr float coordinates[]={255.000000f,160.000000f,128.000000f,108.000000f,84.000000f,71.500000f,59.000000f,54.000000f,48.000000f,38.000000f,29.000000f,24.500000f,8.500000f,0.000000f,0.000000f,0.000000f};
 return interpolate((float)std::clamp(velocity,1,127),midi,coordinates,16);
}
inline const NpView* choose(const std::vector<NpView>& recordings,int root,int velocity,int bank,int){
 const float coordinate=selectionCoordinate(velocity);const NpView* selected=nullptr;const NpView* fallback=nullptr;
 for(const auto& r:recordings)if(r.root==root&&r.bank==bank&&r.frames>0){
  if(bank==2){if(!selected||r.layer>selected->layer)selected=&r;continue;}
  if(!fallback||r.boundary<fallback->boundary)fallback=&r;
  if(r.boundary<=coordinate&&(!selected||r.boundary>selected->boundary||(r.boundary==selected->boundary&&r.layer>selected->layer)))selected=&r;
 }
 return selected?selected:fallback;
}
constexpr float defaultDrive=0.0f;
constexpr double mainReleaseSeconds=0.500;
inline float drivenVelocity(float velocity,float drive=defaultDrive){
 return std::max(1.0f/127.0f,std::pow(std::clamp(velocity,0.0f,1.0f),std::exp2(-2.0f*std::clamp(drive,-1.0f,1.0f))));
}
inline float velocityGain(float velocity){
 const float v=std::clamp(velocity,0.0f,1.0f),midi=v*127;
 if(v<=0)return 0;if(midi>=116)return 1;
 static constexpr float low[]={16.000000f,24.000000f,28.000000f,32.000000f,36.000000f,40.000000f};
 static constexpr float lowDb[]={-8.430000f,-6.624000f,-5.018000f,-3.814000f,-1.205000f,0.000000f};
 static constexpr float high[]={40.000000f,48.000000f,56.000000f,64.000000f,72.000000f,80.000000f,88.000000f,96.000000f,104.000000f,112.000000f,116.000000f};
 static constexpr float highDb[]={-0.631000f,-0.176000f,-0.756000f,0.404000f,0.054000f,0.173000f,-0.275000f,-0.156000f,0.356000f,1.515000f,2.000000f};
 if(midi<40)return 0.051771488f*std::pow(10.0f,interpolate(midi,low,lowDb,6)/20.0f);
 return std::min(1.0f,std::pow(v,2.5f)*std::pow(10.0f,interpolate(midi,high,highDb,11)/20.0f));
}
inline float releaseCoefficient(double rate,float milliseconds){return (float)std::exp(-std::log(10000.0)/(rate*std::clamp(milliseconds,10.0f,4000.0f)*0.001));}
inline float pcm(const NpView& r,size_t frame,size_t channel){return (float)r.data[frame*r.channels+channel]/32768.0f;}
inline bool hasLoop(const NpView& r){return r.loop_start<r.loop_end&&r.loop_end<=r.frames;}
inline double wrap(double position,const NpView& r){
 if(hasLoop(r)&&position>=(double)r.loop_end)return (double)r.loop_start+std::fmod(position-(double)r.loop_start,(double)(r.loop_end-r.loop_start));
 return position;
}
}
