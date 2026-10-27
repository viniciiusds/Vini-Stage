#pragma once
#include <cstdint>
#include <cstddef>
extern "C" {
struct NpLibrary;
struct NpView { const float* data; size_t frames; uint16_t channels; uint8_t root, bank, layer; size_t loop_start,loop_end; float loop_decay; };
NpLibrary* np_load(const char*, char*, size_t);
using NpProgressCallback=void (*)(void*,uint32_t);
NpLibrary* np_load_with_progress(const char*,char*,size_t,NpProgressCallback,void*);
void np_free(NpLibrary*);
const char* np_name(const NpLibrary*);
size_t np_count(const NpLibrary*);
uint8_t np_root(const NpLibrary*, uint8_t);
int8_t np_fine(const NpLibrary*, uint8_t);
float np_gain(const NpLibrary*);
uint8_t np_damper(const NpLibrary*);
uint32_t np_rate();
bool np_recording(const NpLibrary*, size_t, NpView*);
}
