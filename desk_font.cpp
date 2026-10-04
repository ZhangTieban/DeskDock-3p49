#include "desk_font.h"
#include <cstring>

struct DeskGlyph {
  uint32_t codepoint, offset;
  uint16_t advance, width, height;
  int16_t ofs_x, ofs_y;
};
struct DeskFontBlob {
  const DeskGlyph *glyphs;
  uint32_t count;
  const uint8_t *bits;
};

static const DeskGlyph *findGlyph(const DeskFontBlob *blob, uint32_t codepoint) {
  uint32_t lo = 0, hi = blob->count;
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (blob->glyphs[mid].codepoint == codepoint) return &blob->glyphs[mid];
    if (blob->glyphs[mid].codepoint < codepoint) lo = mid + 1; else hi = mid;
  }
  return nullptr;
}

static bool getGlyphDsc(const lv_font_t *font, lv_font_glyph_dsc_t *dsc,
                        uint32_t letter, uint32_t next) {
  LV_UNUSED(next);
  const auto *blob = static_cast<const DeskFontBlob *>(font->dsc);
  const DeskGlyph *glyph = findGlyph(blob, letter);
  if (!glyph) return false;
  std::memset(dsc, 0, sizeof(*dsc));
  dsc->resolved_font = font;
  dsc->adv_w = glyph->advance;
  dsc->box_w = glyph->width;
  dsc->box_h = glyph->height;
  dsc->ofs_x = glyph->ofs_x;
  dsc->ofs_y = glyph->ofs_y;
  dsc->stride = (glyph->width + 1) / 2;
  dsc->format = LV_FONT_GLYPH_FORMAT_A4;
  dsc->gid.src = blob->bits + glyph->offset;
  return true;
}

static const void *getGlyphBitmap(lv_font_glyph_dsc_t *dsc, lv_draw_buf_t *buf) {
  const uint8_t *src = static_cast<const uint8_t *>(dsc->gid.src);
  if (dsc->req_raw_bitmap || !buf) return src;
  for (uint32_t y = 0; y < dsc->box_h; ++y) {
    uint8_t *dst = buf->data + y * buf->header.stride;
    const uint8_t *row = src + y * dsc->stride;
    for (uint32_t x = 0; x < dsc->box_w; ++x) {
      uint8_t packed = row[x / 2];
      uint8_t a4 = (x & 1) ? packed & 15 : packed >> 4;
      dst[x] = (a4 << 4) | a4;
    }
    for (uint32_t x = dsc->box_w; x < buf->header.stride; ++x) dst[x] = 0;
  }
  lv_draw_buf_flush_cache(buf, nullptr);
  return buf;
}

static lv_font_t makeFont(const DeskFontBlob *blob, int height, int base,
                          const lv_font_t *fallback) {
  lv_font_t font = {};
  font.get_glyph_dsc = getGlyphDsc;
  font.get_glyph_bitmap = getGlyphBitmap;
  font.line_height = height;
  font.base_line = base;
  font.subpx = LV_FONT_SUBPX_NONE;
  font.kerning = LV_FONT_KERNING_NORMAL;
  font.static_bitmap = 1;
  font.underline_position = -2;
  font.underline_thickness = 1;
  font.dsc = blob;
  font.fallback = fallback;
  return font;
}

#include "desk_font_data.inc"

const lv_font_t desk_font_16 = makeFont(&desk_blob_16, 23, 5, &lv_font_montserrat_14);
const lv_font_t desk_font_16_bold = makeFont(&desk_blob_16_bold, 23, 5, &desk_font_16);
const lv_font_t desk_font_14_bold = makeFont(&desk_blob_14_bold, 20, 4, &desk_font_16_bold);
const lv_font_t desk_font_10_bold = makeFont(&desk_blob_10_bold, 15, 3, &desk_font_16);
const lv_font_t desk_font_18 = makeFont(&desk_blob_18, 23, 5, &desk_font_16);
const lv_font_t desk_font_20 = makeFont(&desk_blob_20, 29, 6, &desk_font_16);
const lv_font_t desk_font_20_bold = makeFont(&desk_blob_20_bold, 26, 5, &desk_font_16_bold);
const lv_font_t desk_font_24_weather = makeFont(&desk_blob_24_weather, 29, 5, &desk_font_16);
const lv_font_t desk_font_24_moon = makeFont(&desk_blob_24_moon, 29, 5, &desk_font_16);
const lv_font_t desk_font_34 = makeFont(&desk_blob_34, 42, 6, &desk_font_16);
const lv_font_t desk_font_58 = makeFont(&desk_blob_58, 68, 8, &desk_font_16);
const lv_font_t desk_font_70 = makeFont(&desk_blob_70, 80, 8, &lv_font_montserrat_14);
const lv_font_t desk_font_86 = makeFont(&desk_blob_86, 98, 10, &lv_font_montserrat_14);
const lv_font_t desk_font_93 = makeFont(&desk_blob_93, 105, 10, &lv_font_montserrat_14);
