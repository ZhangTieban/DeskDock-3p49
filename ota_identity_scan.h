#pragma once
#include <algorithm>
#include <cstring>
#include "ota_rollback_policy.h"

// Fixed carry exceeds the longest complete marker, including its terminator.
template<class Read, class Yield>
static bool deskReadImageIdentity(uint32_t imageSize, char (&version)[24], bool &found,
                                 Read read, Yield yield) {
  constexpr char prefix[] = DESKDOCK_ROLLBACK_ID_PREFIX;
  // OTA installation already owns a 4 KiB write buffer on its 12 KiB task stack.
  constexpr size_t chunk = 1024;
  uint8_t block[chunk + 96];
  size_t carry = 0;
  found = false;
  version[0] = 0;
  for (uint32_t offset = 0; offset < imageSize;) {
    const size_t count = std::min(chunk, size_t(imageSize-offset));
    if (!read(offset,block+carry,count)) return false;
    const size_t used = carry+count;
    for (size_t i=0; i+sizeof(prefix)<used; ++i) {
      if (memcmp(block+i,prefix,sizeof(prefix)-1)) continue;
      char candidate[24] = {};
      if (!rollbackMarkerVersion(reinterpret_cast<const char *>(block+i),used-i,candidate)) continue;
      if (found && strcmp(candidate,version)) return false;
      memcpy(version,candidate,sizeof(version)); found = true;
    }
    offset += count;
    carry = std::min(size_t(96),used);
    memmove(block,block+used-carry,carry);
    if (offset % 4096 == 0 || offset == imageSize) yield();
  }
  return true;
}
