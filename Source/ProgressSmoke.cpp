#include "Bridge.h"
#include "LibraryCache.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>

struct Events{std::array<uint32_t,101> values{};size_t count=0;bool invalid=false;bool cancel=false;uint32_t abortAt=101;};
static void progress(void* context,uint32_t percent){
    auto& events=*static_cast<Events*>(context);
    if(percent>100||events.count>=events.values.size()||(events.count>0&&percent<=events.values[events.count-1])){events.invalid=true;return;}
    events.values[events.count++]=percent;if(percent>=events.abortAt)events.cancel=true;
}
static bool cancelled(void* context){return static_cast<Events*>(context)->cancel;}
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
    const size_t bytes=np_pcm_bytes(library.get());size_t samples=0;
    for(size_t i=0;i<np_count(library.get());++i){NpView r{};np_recording(library.get(),i,&r);samples+=r.frames*r.channels;}
    if(bytes!=samples*2)return 6;
    Events interrupted;interrupted.abortAt=30;error[0]=0;
    std::unique_ptr<NpLibrary,decltype(&np_free)> stopped(np_load_cancellable(argv[1],error,sizeof(error),progress,cancelled,&interrupted),np_free);
    if(stopped||!interrupted.cancel||interrupted.values[interrupted.count-1]==100||error[0]==0)return 7;
    auto shared=LibraryCache::Ptr(library.release(),np_free);std::atomic<int> calls{0};bool hitA=false,hitB=false;
    LibraryCache::Ptr a,b;
    auto decode=[&]{++calls;std::this_thread::sleep_for(std::chrono::milliseconds(30));return shared;};
    std::thread first([&]{a=LibraryCache::instance().acquire("fixture:unchanged",decode,[]{return false;},hitA);});
    std::thread second([&]{b=LibraryCache::instance().acquire("fixture:unchanged",decode,[]{return false;},hitB);});
    first.join();second.join();if(calls!=1||a.get()!=b.get()||hitA==hitB)return 8;
    bool invalidatedHit=false;
    auto changed=LibraryCache::instance().acquire("fixture:new-mtime",decode,[]{return false;},invalidatedHit);
    if(calls!=2||invalidatedHit||!changed)return 9;
    bool cancelledHit=false;
    if(LibraryCache::instance().acquire("fixture:cancelled",decode,[]{return true;},cancelledHit)||calls!=2)return 10;
    std::cout<<"\nPASS PCM16 bytes="<<bytes<<", cooperative cancellation before completion, single-flight live sharing and changed-key cache miss\n";
    std::cout<<"\nPASS monotonic real read/decode progress, 100% only on success, and error reporting\n";
}
