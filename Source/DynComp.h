#pragma once
#include <algorithm>
#include <cmath>

namespace DynComp {
// Gain only: layer selection keeps the original velocity. End slopes are zero.
inline float gain(float velocity,int level){
    level=std::max(0,std::min(3,level));
    if(level==0)return 1.0f;
    const float t=std::max(0.0f,std::min(1.0f,(velocity*127.0f-1.0f)/126.0f));
    const float weight=1.0f-t*t*(3.0f-2.0f*t);
    const float maximum=level==1?2.0f:level==2?3.0f:4.0f;
    return std::pow(10.0f,maximum*weight/20.0f);
}
}
