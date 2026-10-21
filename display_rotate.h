#pragma once
#include <stdint.h>
#include <string.h>

// Transpose 16x16 tiles through internal stack memory. Column-by-column
// reads from PSRAM evict a cache line for almost every source pixel.
// The destination retains unchanged pixels and stores panel byte order.
static inline void desk_rotate270_rgb565(const uint16_t *source, uint16_t *destination,
    int height, int source_stride, int destination_stride,
    int x1, int y1, int x2, int y2) {
  uint16_t tile[16][16];
  for (int by = y1; by <= y2; by += 16) {
    const int rows = y2 - by + 1 < 16 ? y2 - by + 1 : 16;
    for (int bx = x1; bx <= x2; bx += 16) {
      const int columns = x2 - bx + 1 < 16 ? x2 - bx + 1 : 16;
      for (int y = 0; y < rows; ++y)
        memcpy(tile[y], source + (by + y) * source_stride + bx, columns * 2);
      for (int x = 0; x < columns; ++x) {
        uint16_t *out = destination + (bx + x) * destination_stride + height - 1 - by;
        for (int y = 0; y < rows; ++y) {
          const uint16_t pixel = tile[y][x];
          out[-y] = (uint16_t)((pixel >> 8) | (pixel << 8));
        }
      }
    }
  }
}
