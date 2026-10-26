#pragma once
#include <algorithm>
#include <cmath>

namespace Volume {
constexpr int muteDb=-61;
constexpr int minimumDb=-60;
inline int decibels(float gain) {
    if(!std::isfinite(gain)||gain<=0.0f)return muteDb;
    return std::max(muteDb,std::min(0,(int)std::lround(20.0*std::log10((double)gain))));
}
inline float linear(int db) {
    if(db<minimumDb)return 0.0f;
    return std::pow(10.0f,(float)std::min(0,db)/20.0f);
}
inline float snap(float gain){return linear(decibels(gain));}
}
