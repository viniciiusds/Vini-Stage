#pragma once
#include "Bridge.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
// Immutable libraries shared by slots/instances of this loaded module. Access and
// eviction run on loader threads, never on the audio callback. Idle retention is bounded.
class LibraryCache {
public:
 using Ptr=std::shared_ptr<NpLibrary>;
 static LibraryCache& instance(){static LibraryCache value;return value;}
 Ptr acquire(const std::string& key,const std::function<Ptr()>& decode,const std::function<bool()>& cancelled,bool& hit){
  std::list<Ptr> retired;
  std::unique_lock<std::mutex> lock(mutex);
  for(;;){
   if(cancelled())return {};
   auto entry=std::find_if(entries.begin(),entries.end(),[&](const Entry& e){return e.key==key;});
   if(entry==entries.end()){entries.push_front({key,{},{},false});entry=entries.begin();}
   if(auto value=entry->live.lock()){hit=true;entries.splice(entries.begin(),entries,entry);return value;}
   if(!entry->decoding){entry->decoding=true;break;}
   changed.wait_for(lock,std::chrono::milliseconds(10));
  }
  lock.unlock();Ptr result;
  try{result=decode();}catch(...){lock.lock();find(key).decoding=false;changed.notify_all();throw;}
  lock.lock();auto& entry=find(key);entry.decoding=false;
  if(result){entry.live=result;if(np_pcm_bytes(result.get())<=idleBudget)entry.idle=result;}
  size_t retained=0;
  for(auto i=entries.begin();i!=entries.end();){
   if(i->idle){retained+=np_pcm_bytes(i->idle.get());if(retained>idleBudget){retired.push_back(std::move(i->idle));}}
   if(!i->decoding&&i->live.expired())i=entries.erase(i);else ++i;
  }
  changed.notify_all();lock.unlock();return result;
 }
 static constexpr size_t idleBudget=128u*1024u*1024u;
private:
 struct Entry{std::string key;std::weak_ptr<NpLibrary> live;Ptr idle;bool decoding;};
 std::list<Entry> entries;std::mutex mutex;std::condition_variable changed;
 Entry& find(const std::string& key){return *std::find_if(entries.begin(),entries.end(),[&](const Entry& e){return e.key==key;});}
};
