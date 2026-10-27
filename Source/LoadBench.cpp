#include "LibraryCache.h"
#include <iostream>
int main(int argc,char** argv){
 if(argc<2)return 1;LibraryCache::Ptr keep;
 for(int pass=0;pass<2;++pass){
  char error[1024]{};bool hit=false;auto begin=std::chrono::steady_clock::now();
  auto loaded=LibraryCache::instance().acquire(argv[1],[&]{return LibraryCache::Ptr(np_load(argv[1],error,sizeof(error)),np_free);},[]{return false;},hit);
  if(!loaded){std::cerr<<error<<"\n";return 2;}
  std::cout<<"pass="<<pass<<" seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<" PCMbytes="<<np_pcm_bytes(loaded.get())<<" cacheHit="<<hit<<"\n";
  if(pass==1&&(!hit||loaded.get()!=keep.get()))return 3;keep=loaded;
 }
}
