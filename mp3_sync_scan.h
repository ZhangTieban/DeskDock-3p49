#pragma once
#include <stddef.h>

// Both bytes of a sync word must be inside the supplied buffer.
static constexpr int deskMp3SyncOffset(const unsigned char *data, int count) {
  for (int i = 0; i + 1 < count; ++i)
    if (data[i] == 0xff && (data[i+1] & 0xe0) == 0xe0) return i;
  return -1;
}
static constexpr bool deskMp3CarrySyncByte(const unsigned char *data, int count) {
  return count > 0 && data[count-1] == 0xff;
}
