#pragma once
#include "Bridge.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Playback {
// Keep the spacing stored in the file, including roots with missing layers.
// The library-wide extent handles vendor libraries with values above 127 as well
// as smaller layer scales. Exact Nord velocity curves remain uncalibrated.
inline const NpView* choose(const std::vector<NpView>& recordings,int root,int velocity,int bank,int extent){
    const float softness=(float)(127-std::clamp(velocity,1,127))*(float)extent/126.0f;
    const NpView* selected=nullptr;const NpView* hardest=nullptr;
    for(const auto& r:recordings)if(r.root==root&&r.bank==bank&&r.frames>0){
        if(!hardest||r.layer<hardest->layer)hardest=&r;
        if((float)r.layer<=softness&&(!selected||r.layer>selected->layer))selected=&r;
    }
    return selected?selected:hardest;
}
// Own Drive curve: negative values favour softer velocities. This is not a
// reproduction of Ableton's unpublished transfer function. -0.50 gives x^2.
constexpr float defaultDrive=-0.50f;
constexpr float releaseGain=0.15f;
constexpr double mainReleaseSeconds=0.500;
inline float drivenVelocity(float velocity,float drive=defaultDrive){
    const float curved=std::pow(std::clamp(velocity,0.0f,1.0f),std::exp2(-2.0f*std::clamp(drive,-1.0f,1.0f)));
    return (float)std::clamp((int)std::lround(curved*127.0f),1,127)/127.0f;
}
inline float velocityGain(float velocity){return std::clamp(velocity,0.0f,1.0f);}
inline bool hasLoop(const NpView& r){return r.loop_start<r.loop_end&&r.loop_end<=r.frames;}
inline double wrap(double position,const NpView& r){
    if(hasLoop(r)&&position>=(double)r.loop_end)
        return (double)r.loop_start+std::fmod(position-(double)r.loop_start,(double)(r.loop_end-r.loop_start));
    return position;
}
}
