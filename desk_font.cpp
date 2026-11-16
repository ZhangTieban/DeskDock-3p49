#include "desk_font.h"
#include <SD_MMC.h>
#include <new>
#include <atomic>
#include <cstring>
#include "sd_access.h"
#include "event_log.h"

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
lv_font_t desk_font_88 = makeFont(&desk_blob_88, 99, 9, &lv_font_montserrat_14);

static std::atomic<bool> sdMounted{false};
static std::atomic<uint32_t> sdGlyphReads{0};
static bool builtinSymbolDsc(const lv_font_t *,lv_font_glyph_dsc_t *dsc,uint32_t letter,uint32_t next) {
  if(letter<0xF000 || letter>0xF8FF)return false;
  return lv_font_montserrat_14.get_glyph_dsc(&lv_font_montserrat_14,dsc,letter,next);
}
static lv_fs_drv_t sdFontFs;

static bool sdFontReady(lv_fs_drv_t *) { return sdMounted; }

static void *sdFontOpen(lv_fs_drv_t *, const char *path, lv_fs_mode_t mode) {
  if (mode != LV_FS_MODE_RD) return nullptr;
  DeskSdLock lock;
  File *file = new (std::nothrow) File(SD_MMC.open(path, FILE_READ));
  if (!file) return nullptr;
  if (!*file) { delete file; return nullptr; }
  return file;
}

static lv_fs_res_t sdFontClose(lv_fs_drv_t *, void *handle) {
  DeskSdLock lock;
  File *file = static_cast<File *>(handle);
  file->close();
  delete file;
  return LV_FS_RES_OK;
}

static lv_fs_res_t sdFontRead(lv_fs_drv_t *, void *handle, void *buffer,
                              uint32_t count, uint32_t *readCount) {
  sdGlyphReads.fetch_add(1);
  DeskSdLock lock;
  *readCount = static_cast<File *>(handle)->read(static_cast<uint8_t *>(buffer), count);
  return LV_FS_RES_OK;
}

static lv_fs_res_t sdFontSeek(lv_fs_drv_t *, void *handle, uint32_t offset,
                              lv_fs_whence_t whence) {
  DeskSdLock lock;
  File *file = static_cast<File *>(handle);
  SeekMode mode = whence == LV_FS_SEEK_CUR ? SeekCur :
                  whence == LV_FS_SEEK_END ? SeekEnd : SeekSet;
  return file->seek(offset, mode) ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

static lv_fs_res_t sdFontTell(lv_fs_drv_t *, void *handle, uint32_t *position) {
  DeskSdLock lock;
  *position = static_cast<File *>(handle)->position();
  return LV_FS_RES_OK;
}

bool deskFontSdMounted() { return sdMounted.load(); }

bool deskFontMountSd() {
  DeskSdLock lock;
  if (sdMounted) return true;
  if (!SD_MMC.setPins(41, 39, 40) ||
      !SD_MMC.begin("/sdcard", true, false, BOARD_MAX_SDMMC_FREQ, 40)) {
    Serial.println("[FONT] SD unavailable; using built-in glyphs");
    return false;
  }
  if (SD_MMC.cardType() == CARD_NONE) {
    SD_MMC.end();
    Serial.println("[FONT] SD unavailable; using built-in glyphs");
    return false;
  }
  sdMounted = true;
  return true;
}

void deskFontInitSd() {
  static bool fontsRegistered = false;
  if (fontsRegistered) return;
  const uint32_t startedAt = millis();
  if (!deskFontMountSd()) return;
  fontsRegistered = true;
  lv_fs_drv_init(&sdFontFs);
  sdFontFs.letter = 'S';
  sdFontFs.cache_size = 2048;
  sdFontFs.ready_cb = sdFontReady;
  sdFontFs.open_cb = sdFontOpen;
  sdFontFs.close_cb = sdFontClose;
  sdFontFs.read_cb = sdFontRead;
  sdFontFs.seek_cb = sdFontSeek;
  sdFontFs.tell_cb = sdFontTell;
  lv_fs_drv_register(&sdFontFs);

  struct FontFile { lv_font_t *font; const char *path; uint16_t size; };
  // Only text-bearing sizes need a TTF handle. The icon and large digit fonts
  // already fall back to these sizes and never draw Chinese at their own size.
  static const FontFile files[] = {
    {&desk_font_16, "/fonts/NotoSansTC-Regular.ttf", 16},
    {&desk_font_16_bold, "/fonts/NotoSansTC-Bold.ttf", 16},
    {&desk_font_14_bold, "/fonts/NotoSansTC-Bold.ttf", 14},
    {&desk_font_10_bold, "/fonts/NotoSansTC-Bold.ttf", 10},
    {&desk_font_20, "/fonts/NotoSansTC-Regular.ttf", 20},
  };
  unsigned loaded = 0;
  static lv_font_t symbols[sizeof(files)/sizeof(files[0])];
  unsigned symbolIndex=0;
  for (const FontFile &entry : files) {
    const lv_font_t *oldFallback = entry.font->fallback;
    // Font Awesome icons already exist in the original Montserrat fallback.
    // Resolve only that PUA range before the four SD fonts; CJK/Latin keep
    // their original size, weight, and SD fallback order.
    lv_font_t *tail=&symbols[symbolIndex++];
    *tail=lv_font_montserrat_14;
    tail->get_glyph_dsc=builtinSymbolDsc;
    tail->fallback=nullptr;
    entry.font->fallback=tail;
    const char *paths[] = {entry.path, "/fonts/Jigmo.ttf", "/fonts/Jigmo2.ttf", "/fonts/Jigmo3.ttf"};
    for (unsigned i = 0; i < 4; ++i) {
      bool exists;
      { DeskSdLock lock; exists = SD_MMC.exists(paths[i]); }
      if (!exists) continue;
      String lvPath = String("S:") + paths[i];
      lv_font_t *sdFont = lv_tiny_ttf_create_file_ex(
          lvPath.c_str(), entry.size, LV_FONT_KERNING_NONE, i ? 8 : 64);
      if (!sdFont) continue;
      tail->fallback = sdFont;
      tail = sdFont;
      ++loaded;
    }
    tail->fallback = oldFallback;
  }
  const uint32_t readsBefore=sdGlyphReads.load();bool symbolOkay=true;
  for(const FontFile &entry : files){
    for(uint32_t code : {0xF04BU,0xF04CU,0xF053U,0xF054U,0xF013U}){
      lv_font_glyph_dsc_t found{},original{};
      symbolOkay=lv_font_get_glyph_dsc(entry.font,&found,code,0) &&
        lv_font_get_glyph_dsc(&lv_font_montserrat_14,&original,code,0) &&
        found.resolved_font->dsc==lv_font_montserrat_14.dsc && found.adv_w==original.adv_w &&
        found.box_w==original.box_w && found.box_h==original.box_h && symbolOkay;
    }
  }
  symbolOkay=symbolOkay && sdGlyphReads.load()==readsBefore;
  Serial.printf("[FONT] symbol fast path %s; SD reads=%u\n",symbolOkay ? "PASS" : "FAIL",
                (unsigned)(sdGlyphReads.load()-readsBefore));
  desk_log_event(symbolOkay ? DESK_LOG_INFO : DESK_LOG_ERROR,"FONT_SYMBOL_PATH",
                 symbolOkay,sdGlyphReads.load()-readsBefore,"original icon metrics; no SD glyph lookup");
  const uint32_t voltageReads=sdGlyphReads.load();lv_font_glyph_dsc_t voltage{};
  const bool voltageOkay=lv_font_get_glyph_dsc(&desk_font_10_bold,&voltage,'V',0) &&
    voltage.resolved_font==&desk_font_10_bold && sdGlyphReads.load()==voltageReads;
  Serial.printf("[FONT] voltage fast path %s; SD reads=%u\n",voltageOkay ? "PASS" : "FAIL",
                (unsigned)(sdGlyphReads.load()-voltageReads));
  desk_log_event(voltageOkay ? DESK_LOG_INFO : DESK_LOG_ERROR,"FONT_VOLTAGE_PATH",
                 voltageOkay,sdGlyphReads.load()-voltageReads,"built-in 10px V; no periodic SD lookup");
  Serial.printf("[FONT] SD mounted, %u/%u font fallbacks ready in %u ms\n", loaded,
                (unsigned)(4 * sizeof(files) / sizeof(files[0])), (unsigned)(millis() - startedAt));
}
