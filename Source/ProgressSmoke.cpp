#include "Bridge.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>

struct Events{std::array<uint32_t,101> values{};size_t count=0;bool invalid=false;};
static void progress(void* context,uint32_t percent){
    auto& events=*static_cast<Events*>(context);
    if(percent>100||events.count>=events.values.size()||(events.count>0&&percent<=events.values[events.count-1])){events.invalid=true;return;}
    events.values[events.count++]=percent;
}
int main(int argc,char** argv){
    if(argc<2)return 1;
    Events events;char error[1024]{};
    std::unique_ptr<NpLibrary,decltype(&np_free)> library(np_load_with_progress(argv[1],error,sizeof(error),progress,&events),np_free);
    if(!library){std::cerr<<error<<"\n";return 2;}
    if(events.invalid||events.count<4||events.values[0]!=0||events.values[events.count-1]!=100)return 3;
    bool decodedProgress=false;
    for(size_t i=0;i<events.count;++i){std::cout<<events.values[i]<<"% ";if(events.values[i]>25&&events.values[i]<100)decodedProgress=true;}
    if(!decodedProgress)return 4;
    Events failure;error[0]=0;
    std::unique_ptr<NpLibrary,decltype(&np_free)> invalid(np_load_with_progress(nullptr,error,sizeof(error),progress,&failure),np_free);
    if(invalid||failure.invalid||failure.count!=1||failure.values[0]!=0||error[0]==0)return 5;
    std::cout<<"\nPASS monotonic real read/decode progress, 100% only on success, and error reporting\n";
}
