#pragma once
#include <cstdint>
#include <cstddef>
extern "C" {
struct NpLibrary;
struct NpView { const int16_t* data; size_t frames; uint16_t channels; uint8_t root, bank, layer; size_t loop_start,loop_end; float loop_decay; float attack_decay=1, sample_gain=1, release_ms=500; uint16_t boundary=0; };
NpLibrary* np_load(const char*, char*, size_t);
using NpProgressCallback=void (*)(void*,uint32_t);
NpLibrary* np_load_with_progress(const char*,char*,size_t,NpProgressCallback,void*);
using NpCancelCallback=bool (*)(void*);
NpLibrary* np_load_cancellable(const char*,char*,size_t,NpProgressCallback,NpCancelCallback,void*);
size_t np_pcm_bytes(const NpLibrary*);
void np_free(NpLibrary*);
const char* np_name(const NpLibrary*);
size_t np_count(const NpLibrary*);
uint8_t np_root(const NpLibrary*, uint8_t);
int8_t np_fine(const NpLibrary*, uint8_t);
float np_key_gain(const NpLibrary*,uint8_t);
float np_gain(const NpLibrary*);
uint8_t np_damper(const NpLibrary*);
uint32_t np_rate();
bool np_recording(const NpLibrary*, size_t, NpView*);
}
