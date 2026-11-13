#include <cassert>
#include "../ota_tls_memory.h"

static int otaTask, otherTask;
static TaskHandle_t currentTask = &otherTask;
static char internalMemory, externalMemory;
static unsigned internalCalls, externalCalls, freeCalls;
static bool externalFailure;
static void *(*callocHook)(size_t, size_t);
static void (*freeHook)(void *);

TaskHandle_t xTaskGetCurrentTaskHandle() { return currentTask; }
void *esp_mbedtls_mem_calloc(size_t, size_t) { ++internalCalls; return &internalMemory; }
void esp_mbedtls_mem_free(void *) { ++freeCalls; }
void *heap_caps_calloc(size_t, size_t, unsigned caps) {
  assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  ++externalCalls;
  return externalFailure ? nullptr : &externalMemory;
}
int mbedtls_platform_set_calloc_free(void *(*alloc)(size_t, size_t), void (*release)(void *)) {
  callocHook = alloc; freeHook = release; return 0;
}

int main() {
  otaInitTlsMemory();
  assert(callocHook && freeHook == esp_mbedtls_mem_free);
  assert(callocHook(1, 16384) == &internalMemory);
  otaTlsOwner.store(&otaTask);
  // Concurrent background TLS keeps the SDK allocator while OTA owns the hook.
  assert(callocHook(1, 16384) == &internalMemory && externalCalls == 0);
  currentTask = &otaTask;
  assert(callocHook(1, 128) == &internalMemory);
  assert(callocHook(1, 16384) == &externalMemory && otaTlsPsramAllocations == 1);
  externalFailure = true;
  const unsigned before = internalCalls;
  assert(callocHook(1, 16384) == nullptr && internalCalls == before);
  assert(callocHook(SIZE_MAX, 2) == nullptr && externalCalls == 2);
  // The release hook accepts allocations from either heap and before init.
  freeHook(&internalMemory); freeHook(&externalMemory);
  assert(freeCalls == 2);
  otaTlsOwner.store(nullptr);
  assert(callocHook(1, 16384) == &internalMemory);
}
