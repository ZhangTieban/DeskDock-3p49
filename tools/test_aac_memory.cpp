#include <stdlib.h>
#include <assert.h>
static int internal_calls,external_calls;
static bool psram_available=true,external_failure=false;
static char internal_buffer[32],external_buffer[32];
static void *test_malloc(size_t) { ++internal_calls;return internal_buffer; }
#define ESP32 1
#define malloc test_malloc
#include "../aac_memory.h"
#undef malloc
size_t heap_caps_get_total_size(unsigned caps) { assert(caps==MALLOC_CAP_SPIRAM);return psram_available?1024:0; }
void *heap_caps_malloc(size_t,unsigned caps) {
  assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));++external_calls;
  return external_failure?nullptr:external_buffer;
}
int main() {
  assert(deskAacAllocate(16)==external_buffer && !internal_calls && external_calls==1);
  external_failure=true;
  assert(deskAacAllocate(16)==nullptr && !internal_calls && external_calls==2);
  psram_available=false;
  assert(deskAacAllocate(16)==internal_buffer && internal_calls==1 && external_calls==2);
}
