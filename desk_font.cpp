#include "desk_font.h"
#include <SD_MMC.h>
#include <new>
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

lv_font_t desk_font_16 = makeFont(&desk_blob_16, 23, 5, &lv_font_montserrat_14);
lv_font_t desk_font_16_bold = makeFont(&desk_blob_16_bold, 23, 5, &desk_font_16);
lv_font_t desk_font_14_bold = makeFont(&desk_blob_14_bold, 20, 4, &desk_font_16_bold);
lv_font_t desk_font_10_bold = makeFont(&desk_blob_10_bold, 15, 3, &desk_font_16);
lv_font_t desk_font_18 = makeFont(&desk_blob_18, 23, 5, &desk_font_16);
lv_font_t desk_font_20 = makeFont(&desk_blob_20, 29, 6, &desk_font_16);
lv_font_t desk_font_20_bold = makeFont(&desk_blob_20_bold, 26, 5, &desk_font_16_bold);
lv_font_t desk_font_24_weather = makeFont(&desk_blob_24_weather, 29, 5, &desk_font_16);
lv_font_t desk_font_24_moon = makeFont(&desk_blob_24_moon, 29, 5, &desk_font_16);
lv_font_t desk_font_34 = makeFont(&desk_blob_34, 42, 6, &desk_font_16);
lv_font_t desk_font_58 = makeFont(&desk_blob_58, 68, 8, &desk_font_16);
lv_font_t desk_font_70 = makeFont(&desk_blob_70, 80, 8, &lv_font_montserrat_14);
lv_font_t desk_font_86 = makeFont(&desk_blob_86, 98, 10, &lv_font_montserrat_14);
lv_font_t desk_font_93 = makeFont(&desk_blob_93, 105, 10, &lv_font_montserrat_14);

static bool sdMounted = false;
static lv_fs_drv_t sdFontFs;

static bool sdFontReady(lv_fs_drv_t *) { return sdMounted; }

static void *sdFontOpen(lv_fs_drv_t *, const char *path, lv_fs_mode_t mode) {
  if (mode != LV_FS_MODE_RD) return nullptr;
  File *file = new (std::nothrow) File(SD_MMC.open(path, FILE_READ));
  if (!file) return nullptr;
  if (!*file) { delete file; return nullptr; }
  return file;
}

static lv_fs_res_t sdFontClose(lv_fs_drv_t *, void *handle) {
  File *file = static_cast<File *>(handle);
  file->close();
  delete file;
  return LV_FS_RES_OK;
}

static lv_fs_res_t sdFontRead(lv_fs_drv_t *, void *handle, void *buffer,
                              uint32_t count, uint32_t *readCount) {
  *readCount = static_cast<File *>(handle)->read(static_cast<uint8_t *>(buffer), count);
  return LV_FS_RES_OK;
}

static lv_fs_res_t sdFontSeek(lv_fs_drv_t *, void *handle, uint32_t offset,
                              lv_fs_whence_t whence) {
  File *file = static_cast<File *>(handle);
  SeekMode mode = whence == LV_FS_SEEK_CUR ? SeekCur :
                  whence == LV_FS_SEEK_END ? SeekEnd : SeekSet;
  return file->seek(offset, mode) ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

static lv_fs_res_t sdFontTell(lv_fs_drv_t *, void *handle, uint32_t *position) {
  *position = static_cast<File *>(handle)->position();
  return LV_FS_RES_OK;
}

bool deskFontSdMounted() { return sdMounted; }

void deskFontInitSd() {
  if (sdMounted) return;
  if (!SD_MMC.setPins(41, 39, 40) ||
      !SD_MMC.begin("/sdcard", true, false, BOARD_MAX_SDMMC_FREQ, 20)) {
    Serial.println("[FONT] SD unavailable; using built-in glyphs");
    return;
  }
  if (SD_MMC.cardType() == CARD_NONE) {
    SD_MMC.end();
    Serial.println("[FONT] SD unavailable; using built-in glyphs");
    return;
  }
  sdMounted = true;
  lv_fs_drv_init(&sdFontFs);
  sdFontFs.letter = 'S';
  sdFontFs.cache_size = 1024;
  sdFontFs.ready_cb = sdFontReady;
  sdFontFs.open_cb = sdFontOpen;
  sdFontFs.close_cb = sdFontClose;
  sdFontFs.read_cb = sdFontRead;
  sdFontFs.seek_cb = sdFontSeek;
  sdFontFs.tell_cb = sdFontTell;
  lv_fs_drv_register(&sdFontFs);

  struct FontFile { lv_font_t *font; const char *path; uint16_t size; };
  static const FontFile files[] = {
    {&desk_font_16, "/fonts/NotoSansTC-VF.ttf", 16},
    {&desk_font_16_bold, "/fonts/NotoSansTC-VF.ttf", 16},
    {&desk_font_14_bold, "/fonts/NotoSansTC-VF.ttf", 14},
    {&desk_font_10_bold, "/fonts/NotoSansTC-VF.ttf", 10},
    {&desk_font_18, "/fonts/NotoSansTC-VF.ttf", 18},
    {&desk_font_20, "/fonts/NotoSansTC-VF.ttf", 20},
    {&desk_font_20_bold, "/fonts/NotoSansTC-VF.ttf", 20},
    {&desk_font_24_weather, "/fonts/NotoSansTC-VF.ttf", 24},
    {&desk_font_24_moon, "/fonts/NotoSansTC-VF.ttf", 24},
    {&desk_font_34, "/fonts/NotoSansTC-VF.ttf", 34},
    {&desk_font_58, "/fonts/NotoSansTC-VF.ttf", 58},
    {&desk_font_70, "/fonts/NotoSansTC-VF.ttf", 70},
    {&desk_font_86, "/fonts/NotoSansTC-VF.ttf", 86},
    {&desk_font_93, "/fonts/NotoSansTC-VF.ttf", 93},
  };
  unsigned loaded = 0;
  for (const FontFile &entry : files) {
    if (!SD_MMC.exists(entry.path)) continue;
    String lvPath = String("S:") + entry.path;
    lv_font_t *sdFont = lv_tiny_ttf_create_file_ex(
        lvPath.c_str(), entry.size, LV_FONT_KERNING_NORMAL, 16);
    if (!sdFont) continue;
    sdFont->fallback = entry.font->fallback;
    entry.font->fallback = sdFont;
    ++loaded;
  }
  Serial.printf("[FONT] SD mounted, %u/%u font fallbacks ready\n", loaded,
                (unsigned)(sizeof(files) / sizeof(files[0])));
}
