// DeskDock 640x172 home screen. Included after the shared UI helpers.
static constexpr int kHomeRightX = 315;
static constexpr int kHomeRightW = 313;
static constexpr int kHomeContentW = 285;
static constexpr int kHomeStockContentW = kHomeContentW - 16;
static constexpr int kHomeStockH = 104;
static constexpr int kHomeTabHeight = 172 / kHomeCarouselCount;
static constexpr uint32_t kHomeBackground = 0x070B0A;

#include "weather_icons.inc"

// 16 x 16 icons with four-color palettes. Palette entry zero is transparent.
alignas(4) static const uint8_t kHomeMapleLeafPixels[] = {
  0x00, 0x00, 0x00, 0x00, 0x47, 0x78, 0xF3, 0xFF, 0x32, 0x4B, 0xD9, 0xFF, 0x37, 0x56, 0xAB, 0xFF,  // BGRA palette
  0x00, 0x01, 0x00, 0x00,
  0x00, 0x05, 0x80, 0x00,
  0x00, 0x05, 0x80, 0x00,
  0x04, 0x05, 0x40, 0x80,
  0x05, 0x05, 0x41, 0x80,
  0x15, 0x45, 0x45, 0x90,
  0x05, 0x55, 0x59, 0x60,
  0x01, 0x55, 0x95, 0x80,
  0x15, 0x55, 0x95, 0x58,
  0x05, 0x55, 0x95, 0xA0,
  0x01, 0x55, 0x96, 0x80,
  0x00, 0x5A, 0xA6, 0x00,
  0x00, 0x22, 0x28, 0x00,
  0x00, 0x03, 0x00, 0x00,
  0x00, 0x0C, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
};

alignas(4) static const uint8_t kHomeBirthdayCakePixels[] = {
  0x00, 0x00, 0x00, 0x00, 0xAE, 0xE1, 0xFD, 0xFF, 0x98, 0x78, 0xEA, 0xFF, 0x45, 0xB1, 0xF4, 0xFF,  // BGRA palette
  0x00, 0x03, 0x00, 0x00,
  0x00, 0x0F, 0x00, 0x00,
  0x00, 0x0F, 0xC0, 0x00,
  0x00, 0x02, 0x00, 0x00,
  0x00, 0x02, 0x00, 0x00,
  0x00, 0x55, 0x55, 0x00,
  0x01, 0x55, 0x55, 0x40,
  0x05, 0x55, 0x55, 0x50,
  0x0A, 0x5A, 0x5A, 0x50,
  0x0A, 0x96, 0x5A, 0xA0,
  0x0A, 0xAA, 0xAA, 0xA0,
  0x0A, 0xAA, 0xAA, 0xA0,
  0x05, 0x55, 0x55, 0x50,
  0x15, 0x55, 0x55, 0x54,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
};

static constexpr lv_image_header_t kOccasionIconHeader = {
  .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_I2, .w = 16, .h = 16, .stride = 4
};
static const lv_image_dsc_t kHomeMapleLeaf = {
  .header = kOccasionIconHeader,
  .data_size = sizeof(kHomeMapleLeafPixels), .data = kHomeMapleLeafPixels
};
static const lv_image_dsc_t kHomeBirthdayCake = {
  .header = kOccasionIconHeader,
  .data_size = sizeof(kHomeBirthdayCakePixels), .data = kHomeBirthdayCakePixels
};

static void homeSetText(lv_obj_t *item, const String &text) {
  if (item && strcmp(lv_label_get_text(item), text.c_str()))
    lv_label_set_text(item, text.c_str());
}

static lv_obj_t *homeRule(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *rule = lv_obj_create(parent);
  lv_obj_set_pos(rule, x, y); lv_obj_set_size(rule, w, h);
  lv_obj_set_style_pad_all(rule, 0, 0);
  lv_obj_set_style_radius(rule, 0, 0);
  lv_obj_set_style_border_width(rule, 0, 0);
  lv_obj_set_style_bg_color(rule, lv_color_hex(0x263A31), 0);
  lv_obj_clear_flag(rule, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(rule, LV_OBJ_FLAG_CLICKABLE);
  return rule;
}

static const char *homeStockName(const String &code) {
  if (code == "2330") return "台積電 2330";
  if (code == "0050") return "元大台灣50 0050";
  if (code == "0056") return "元大高股息 0056";
  if (code == "00878") return "國泰永續高股息 00878";
  return nullptr;
}

static String stockDisplayName(uint8_t index) {
  const String &code = stockCodes[index];
  if (!stockNames[index].isEmpty()) return stockNames[index] + " " + code;
  const char *known = homeStockName(code);
  return known ? String(known) : code;
}

static void setFooterPage(lv_obj_t *page, uint8_t index) {
  if (!stockCount) return;
  index %= stockCount;
  struct RenderedPage { lv_obj_t *page; uint8_t index; uint32_t revision; };
  static RenderedPage rendered[2] = {};
  for (const RenderedPage &item : rendered)
    if (item.page == page && item.index == index &&
        item.revision == stockUiRevision) return;
  RenderedPage &slot = rendered[0].page == page || !rendered[0].page ?
      rendered[0] : rendered[1];
  slot = {page, index, stockUiRevision};
  String price, change, stamp;
  splitStockLine(stockValues[index], 3, price, change, stamp);
  if (price.startsWith(stockCodes[index] + "  "))
    price.remove(0, stockCodes[index].length() + 2);
  homeSetText(lv_obj_get_child(page, 0), stockDisplayName(index));
  lv_obj_t *priceLabel = lv_obj_get_child(page, 1);
  lv_obj_set_style_text_font(priceLabel,
      price.length() && price[0] >= '0' && price[0] <= '9' ?
      &lv_font_montserrat_32 : &desk_font_16_bold, 0);
  homeSetText(priceLabel, price);
  String changeText = change;
  if (change.startsWith("+")) changeText = "▲" + change.substring(1);
  else if (change.startsWith("-")) changeText = "▼" + change.substring(1);
  else if (change.length() && change[0] >= '0' && change[0] <= '9')
    changeText = "●" + change;
  homeSetText(lv_obj_get_child(page, 2), changeText);
  lv_obj_set_style_text_color(priceLabel, lv_color_hex(stockChangeColor(change)), 0);
  lv_obj_set_style_text_color(lv_obj_get_child(page, 2),
                              lv_color_hex(stockChangeColor(change)), 0);
  const StockDetail &detail = stockDetails[index];
  homeSetText(lv_obj_get_child(page, 3), "最高 " + (detail.high.isEmpty() ? String("--") : detail.high));
  homeSetText(lv_obj_get_child(page, 4), "最低 " + (detail.low.isEmpty() ? String("--") : detail.low));
  homeSetText(lv_obj_get_child(page, 5), "成交量 " + (detail.volume.isEmpty() ? String("--") :
      detail.volume + (detail.volume.endsWith("張") ? "" : " 股")));
  const int dotStep = 9;
  const int dotStart = (kHomeContentW - (stockCount * dotStep - 4)) / 2;
  for (uint8_t i = 0; i < kMaxStocks; ++i) {
    if (i < stockCount) {
      lv_obj_set_x(stockPositionDots[i], dotStart + i * dotStep);
      lv_obj_set_style_bg_color(stockPositionDots[i],
                                lv_color_hex(i == index ? 0x83D7BB : 0x53605A), 0);
      lv_obj_remove_flag(stockPositionDots[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(stockPositionDots[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static lv_obj_t *makeFooterPage(lv_obj_t *parent, int y) {
  lv_obj_t *page = lv_obj_create(parent);
  lv_obj_set_pos(page, 0, y); lv_obj_set_size(page, kHomeStockContentW, kHomeStockH);
  lv_obj_set_style_pad_all(page, 0, 0);
  lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(page, 0, 0);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *name = label(page, "", 0, 0, kHomeStockContentW);
  lv_obj_set_style_text_font(name, &desk_font_16_bold, 0);
  lv_obj_set_style_text_color(name, lv_color_hex(0x83D7BB), 0);
  lv_obj_t *price = label(page, "", 0, 23, 176, 20);
  lv_obj_set_style_text_font(price, &lv_font_montserrat_32, 0);
  lv_obj_t *change = label(page, "", 185, 36, 84);
  lv_obj_set_style_text_font(change, &desk_font_16_bold, 0);
  lv_obj_set_style_text_align(change, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_t *high = label(page, "", 0, 63, 124);
  lv_obj_set_style_text_font(high, &desk_font_14_bold, 0);
  lv_obj_t *low = label(page, "", 143, 63, 126);
  lv_obj_set_style_text_font(low, &desk_font_14_bold, 0);
  lv_obj_set_style_text_align(low, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_t *volume = label(page, "", 0, 84, kHomeStockContentW);
  lv_obj_set_style_text_font(volume, &desk_font_14_bold, 0);
  lv_obj_set_style_text_color(volume, lv_color_hex(0xAABBB1), 0);
  return page;
}

static lv_obj_t *makeWeatherPage(lv_obj_t *parent) {
  lv_obj_t *page = lv_obj_create(parent);
  lv_obj_set_pos(page, 0, 0); lv_obj_set_size(page, kHomeContentW, 118);
  lv_obj_set_style_pad_all(page, 0, 0);
  lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(page, 0, 0);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *temperature = label(page, "", 60, 0, kHomeContentW - 60);
  lv_obj_set_style_text_font(temperature, &lv_font_montserrat_32, 0);
  for (int i = 0; i < 6; ++i) {
    lv_obj_t *metric = label(page, "", (i % 2) ? 153 : 8,
                             44 + (i / 2) * 25, (i % 2) ? 124 : 132);
    lv_obj_set_style_text_font(metric, &desk_font_16_bold, 0);
  }
  return page;
}

static void setWeatherPage(lv_obj_t *page) {
  const WeatherData &item = weatherData[weatherSlot];
  const int snapshot[] = {(int)weatherSlot, (int)item.valid, item.low, item.high,
                          item.rain, item.feels, item.humidity, item.wind,
                          item.uv10, item.sunset};
  static int rendered[10];
  static bool renderedOnce = false;
  if (renderedOnce && memcmp(snapshot, rendered, sizeof(snapshot)) == 0) return;
  memcpy(rendered, snapshot, sizeof(snapshot));
  renderedOnce = true;
  lv_obj_t *main = lv_obj_get_child(page, 0);
  if (!item.valid) {
    lv_obj_set_style_text_font(main, &desk_font_16_bold, 0);
    homeSetText(main, "等待天氣");
    for (int i = 1; i <= 6; ++i) homeSetText(lv_obj_get_child(page, i), "");
  } else {
    lv_obj_set_style_text_font(main, &lv_font_montserrat_32, 0);
    homeSetText(main, String(item.low) + "-" + item.high + "°C");
    homeSetText(lv_obj_get_child(page, 1), String("降雨 ") + item.rain + "%");
    homeSetText(lv_obj_get_child(page, 2), String("濕度 ") +
        (item.humidity < 0 ? String("--") : String(item.humidity) + "%"));
    homeSetText(lv_obj_get_child(page, 3), String("體感 ") +
        (item.feels == -999 ? String("--") : String(item.feels) + "°C"));
    homeSetText(lv_obj_get_child(page, 4), String("風速 ") +
        (item.wind < 0 ? String("--") : String(item.wind) + "km/h"));
    homeSetText(lv_obj_get_child(page, 5), String("UV最高 ") +
        (item.uv10 < 0 ? String("--") : String(item.uv10 / 10.0f, 1)));
    char timeText[6] = "--:--";
    if (item.sunset >= 0)
      snprintf(timeText, sizeof(timeText), "%02d:%02d", item.sunset / 60, item.sunset % 60);
    homeSetText(lv_obj_get_child(page, 6), String("日落 ") + timeText);
  }
}

static void moveFooterPages(void *obj, int32_t y) {
  // One timeline keeps the outgoing and incoming cards on the same frame.
  lv_obj_set_y((lv_obj_t *)obj, y);
  lv_obj_set_y(footerPageB, y + kHomeStockH);
}

static void finishFooterAnimation(lv_anim_t *) {
  lv_obj_t *old = footerPageA;
  footerPageA = footerPageB;
  footerPageB = old;
  lv_obj_set_y(footerPageA, 0);
  lv_obj_set_y(footerPageB, kHomeStockH);
  footerAnimating = false;
}

static void animateFooterPages() {
  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_exec_cb(&anim, moveFooterPages);
  lv_anim_set_completed_cb(&anim, finishFooterAnimation);
  lv_anim_set_duration(&anim, 300);
  lv_anim_set_var(&anim, footerPageA);
  lv_anim_set_values(&anim, 0, -kHomeStockH);
  lv_anim_start(&anim);
}

static void advanceFooter(lv_event_t *) {
  if (footerAnimating || stockCount < 2) return;
  footerPage = (footerPage + 1) % stockCount;
  setFooterPage(footerPageB, footerPage);
  footerAnimating = true;
  lastFooterFlip = millis();
  animateFooterPages();
}

static void homeVisible(lv_obj_t *item, bool visible) {
  if (visible) lv_obj_remove_flag(item, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(item, LV_OBJ_FLAG_HIDDEN);
}

static void showHomeCarousel(uint8_t page) {
  const bool enteringMp3 = homeCarouselPage != kHomeMp3Page && page == kHomeMp3Page;
  homeCarouselPage = page;
  if (homeCarouselPage == 0) lastWeatherFlip = millis();
  if (homeCarouselPage == 1) lastFooterFlip = millis();
  homeVisible(homeWeatherPane, homeCarouselPage == 0);
  homeVisible(homeStockPane, homeCarouselPage == 1);
  homeVisible(homeRadioPane, homeCarouselPage == 2);
  homeVisible(homeMp3Pane, homeCarouselPage == kHomeMp3Page);
  for (uint8_t i = 0; i < kHomeCarouselCount; ++i) {
    const bool active = i == homeCarouselPage;
    lv_obj_t *bar = lv_obj_get_child(homeCarouselTabs[i], 0);
    const int height = active ? 26 : 18;
    lv_obj_set_pos(bar, active ? 15 : 17, (kHomeTabHeight - height) / 2);
    lv_obj_set_size(bar, active ? 8 : 4, height);
    lv_obj_set_style_bg_color(bar,
        lv_color_hex(active ? 0x83D7BB : 0x536E5D), 0);
  }
  if (enteringMp3 && !mp3TrackCount) requestMp3Scan();
  if (homeCarouselPage == kHomeMp3Page) updateMp3Screen();
}

static void selectHomeCarousel(lv_event_t *event) {
  showHomeCarousel((uint8_t)(uintptr_t)lv_event_get_user_data(event));
}

static void swipeHomeCarousel(lv_event_t *) {
  const lv_dir_t direction = lv_indev_get_gesture_dir(lv_indev_active());
  if (direction == LV_DIR_TOP && homeCarouselPage + 1 < kHomeCarouselCount) {
    showHomeCarousel(homeCarouselPage + 1);
    lv_indev_wait_release(lv_indev_active());
  } else if (direction == LV_DIR_BOTTOM && homeCarouselPage > 0) {
    showHomeCarousel(homeCarouselPage - 1);
    lv_indev_wait_release(lv_indev_active());
  }
}

static void updateHomeWeatherIcon() {
  static const lv_image_dsc_t *rendered = nullptr;
  const WeatherData &item = weatherData[weatherSlot];
  const lv_image_dsc_t *image = weatherIconImage(item.valid ? item.code : -1, item.day);
  if (image == rendered) return;
  rendered = image;
  lv_image_set_src(homeWeatherIcon, image);
}

static void updateHomeRadio() {
  const int station = radioStation.load();
  const int state = radioUiState();
  static int renderedState = -1;
  const String stationName = station ? String(kRadioNames[station - 1]) : String("選擇電台");
  if (strcmp(lv_label_get_text(homeRadioStation), stationName.c_str())) {
    lv_point_t textSize;
    lv_text_get_size(&textSize, stationName.c_str(), &desk_font_20, 0, 0,
                     LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_obj_set_style_text_font(homeRadioStation,
        textSize.x > 125 ? &desk_font_16_bold : &desk_font_20, 0);
  }
  homeSetText(homeRadioStation, stationName);
  if (state != renderedState)
    lv_obj_set_style_text_color(homeRadioStation,
        lv_color_hex(state == 3 ? 0xF27873 : 0xF3F5F2), 0);
  const bool playing = state == 1 || state == 2;
  homeSetText(homeRadioStatus, state == 1 ? String("連線中") :
      state == 2 ? String("播放中") : state == 3 ? String("連線失敗") : String("已暫停"));
  if (state != renderedState)
    lv_obj_set_style_text_color(homeRadioStatus,
        lv_color_hex(state == 3 ? 0xF27873 : playing ? 0x83D7BB : 0xAABBB1), 0);
  homeSetText(lv_obj_get_child(homeRadioToggle, 0),
              playing ? String(LV_SYMBOL_PAUSE) : String(LV_SYMBOL_PLAY));
  homeSetText(lv_obj_get_child(homeRadioToggle, 1), playing ? String("暫停") : String("播放"));
  renderedState = state;
}

static void refreshHomeRows(uint32_t now) {
  const uint32_t rotateMs = (uint32_t)cfg.carouselSeconds * 1000UL;
  if (!cfg.carouselPaused && homeCarouselPage == 0 &&
      (uint32_t)(now - lastWeatherFlip) >= rotateMs) {
    weatherSlot = (weatherSlot + 1) % cfg.weatherCount;
    lastWeatherFlip = now;
  }
  setWeatherPage(weatherPageA);
  if (!footerAnimating) setFooterPage(footerPageA, footerPage);
  if (!cfg.carouselPaused && homeCarouselPage == 1 && !footerAnimating &&
      (uint32_t)(now - lastFooterFlip) >= rotateMs) advanceFooter(nullptr);
  String indexValue, indexChange, indexStamp;
  splitStockLine(remote.index, 1, indexValue, indexChange, indexStamp);
  homeSetText(homeStockIndexLabel, String("加權 ") + indexValue);
  String indexChangeText = indexChange;
  if (indexChange.startsWith("+")) indexChangeText = "▲" + indexChange.substring(1);
  else if (indexChange.startsWith("-")) indexChangeText = "▼" + indexChange.substring(1);
  homeSetText(homeStockChangeLabel, indexChangeText);
  const uint32_t changeColor = stockChangeColor(indexChange);
  static uint32_t renderedChangeColor = UINT32_MAX;
  if (changeColor != renderedChangeColor) {
    lv_obj_set_style_text_color(homeStockChangeLabel, lv_color_hex(changeColor), 0);
    renderedChangeColor = changeColor;
  }
  updateHomeWeatherIcon();
  updateHomeRadio();
  String weatherTitle = kCounties[cfg.counties[weatherSlot]];
  if (weatherTitle.length() <= 6) weatherTitle += " · 天氣";
  homeSetText(homeWeatherTitle, weatherTitle);
}

static lv_obj_t *homeControl(lv_obj_t *parent, const char *icon, const char *caption,
                             int x, int y, int w, lv_event_cb_t callback) {
  lv_obj_t *control = button(parent, icon, x, y, w, 50, callback);
  lv_obj_set_style_pad_all(control, 0, 0);
  lv_obj_set_style_border_width(control, 0, 0);
  lv_obj_t *symbol = lv_obj_get_child(control, 0);
  lv_obj_set_align(symbol, LV_ALIGN_TOP_LEFT);
  lv_obj_set_pos(symbol, 0, 3);
  lv_obj_set_width(symbol, w);
  lv_obj_set_style_text_align(symbol, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_t *name = label(control, caption, 0, 29, w);
  lv_obj_set_style_text_font(name, &desk_font_10_bold, 0);
  lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
  return control;
}

#include "home_dog.inc"

static int homeTextInkBottom(const lv_font_t *font, const char *text) {
  int bottom = 0;
  for (const unsigned char *c = reinterpret_cast<const unsigned char *>(text); *c; ++c) {
    lv_font_glyph_dsc_t glyph = {};
    if (lv_font_get_glyph_dsc(font,&glyph,*c,0) && glyph.box_h)
      bottom = std::max(bottom,int(font->line_height)-int(font->base_line)-int(glyph.ofs_y));
  }
  return bottom;
}

static void alignHomeClockPeriod(const char *clock, const char *period) {
  if (!period[0]) return;
  const int y = lv_obj_get_y(clockLabel) + homeTextInkBottom(&desk_font_88,clock) -
                homeTextInkBottom(&desk_font_14_bold,period);
  if (lv_obj_get_y(clockPeriodLabel) != y) lv_obj_set_y(clockPeriodLabel,y);
}

static void buildHome() {
  homeScreen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(homeScreen, lv_color_hex(kHomeBackground), 0);
  lv_obj_set_style_pad_all(homeScreen, 0, 0);
  lv_obj_clear_flag(homeScreen, LV_OBJ_FLAG_SCROLLABLE);
  homeRule(homeScreen, 307, 8, 1, 156);

  clockLabel = label(homeScreen, "--:--", 8, 3, 298, 88);
  lv_obj_set_style_text_font(clockLabel, &desk_font_88, 0);
  lv_obj_set_style_text_align(clockLabel, LV_TEXT_ALIGN_CENTER, 0);
  dateLabel = label(homeScreen, "", 11, 111, 286);
  lv_obj_set_style_text_font(dateLabel, &desk_font_16_bold, 0);
  lv_obj_set_style_text_align(dateLabel, LV_TEXT_ALIGN_CENTER, 0);
  buildHomeDog();
  clockPeriodLabel = label(homeScreen, "", 247, 78, 43);
  lv_obj_set_style_text_font(clockPeriodLabel, &desk_font_14_bold, 0);
  lv_obj_set_style_text_align(clockPeriodLabel, LV_TEXT_ALIGN_RIGHT, 0);

  wifiLabel = label(homeScreen, "Wi-Fi", 12, 9, 210);
  lv_obj_set_style_text_font(wifiLabel, &desk_font_10_bold, 0);
  occasionLabel = label(homeScreen, "", 82, 7, 145);
  lv_obj_set_style_text_font(occasionLabel, &desk_font_14_bold, 0);
  lv_obj_set_style_text_align(occasionLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_color(occasionLabel, lv_color_hex(0xF2C64B), 0);
  occasionIcon = lv_image_create(homeScreen);
  lv_obj_set_pos(occasionIcon, 231, 8);
  lv_obj_add_flag(occasionIcon, LV_OBJ_FLAG_HIDDEN);

  homeStockPane = lv_obj_create(homeScreen);
  lv_obj_set_pos(homeStockPane, kHomeRightX, 4);
  lv_obj_set_size(homeStockPane, kHomeRightW, 164);
  lv_obj_set_style_pad_all(homeStockPane, 0, 0);
  lv_obj_set_style_bg_opa(homeStockPane, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(homeStockPane, 0, 0);
  lv_obj_clear_flag(homeStockPane, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(homeStockPane, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_t *stockTitle = label(homeStockPane, "台北股市", 0, 8, 70);
  lv_obj_set_style_text_font(stockTitle, &desk_font_16_bold, 0);
  homeStockIndexLabel = label(homeStockPane, "", 72, 9, 121);
  lv_obj_set_style_text_font(homeStockIndexLabel, &desk_font_16_bold, 0);
  lv_obj_set_style_text_color(homeStockIndexLabel, lv_color_hex(0xAABBB1), 0);
  homeStockChangeLabel = label(homeStockPane, "", 195, 9, 90);
  lv_obj_set_style_text_font(homeStockChangeLabel, &desk_font_16_bold, 0);
  lv_obj_set_style_text_align(homeStockChangeLabel, LV_TEXT_ALIGN_RIGHT, 0);
  homeRule(homeStockPane, 0, 33, kHomeContentW, 1);
  lv_obj_t *stockCard = lv_obj_create(homeStockPane);
  lv_obj_set_pos(stockCard, 0, 40);
  lv_obj_set_size(stockCard, kHomeContentW, 109);
  lv_obj_set_style_pad_all(stockCard, 0, 0);
  lv_obj_set_style_radius(stockCard, 5, 0);
  lv_obj_set_style_bg_color(stockCard, lv_color_hex(0x0D1712), 0);
  lv_obj_set_style_border_width(stockCard, 1, 0);
  lv_obj_set_style_border_color(stockCard, lv_color_hex(0x344F40), 0);
  lv_obj_clear_flag(stockCard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(stockCard, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *stockStrip = lv_obj_create(homeStockPane);
  lv_obj_set_pos(stockStrip, 8, 43);
  lv_obj_set_size(stockStrip, kHomeStockContentW, kHomeStockH);
  lv_obj_set_style_pad_all(stockStrip, 0, 0);
  lv_obj_set_style_bg_opa(stockStrip, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(stockStrip, 0, 0);
  lv_obj_clear_flag(stockStrip, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(stockStrip, advanceFooter, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(stockStrip, openStocks, LV_EVENT_LONG_PRESSED, nullptr);
  footerPageA = makeFooterPage(stockStrip, 0);
  footerPageB = makeFooterPage(stockStrip, kHomeStockH);
  for (uint8_t i = 0; i < kMaxStocks; ++i) {
    stockPositionDots[i] = lv_obj_create(homeStockPane);
    lv_obj_set_pos(stockPositionDots[i], 0, 154);
    lv_obj_set_size(stockPositionDots[i], 5, 5);
    lv_obj_set_style_pad_all(stockPositionDots[i], 0, 0);
    lv_obj_set_style_border_width(stockPositionDots[i], 0, 0);
    lv_obj_set_style_radius(stockPositionDots[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(stockPositionDots[i], LV_OBJ_FLAG_CLICKABLE);
  }

  homeWeatherPane = lv_obj_create(homeScreen);
  lv_obj_set_pos(homeWeatherPane, kHomeRightX, 4);
  lv_obj_set_size(homeWeatherPane, kHomeRightW, 164);
  lv_obj_set_style_pad_all(homeWeatherPane, 0, 0);
  lv_obj_set_style_bg_opa(homeWeatherPane, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(homeWeatherPane, 0, 0);
  lv_obj_clear_flag(homeWeatherPane, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(homeWeatherPane, LV_OBJ_FLAG_GESTURE_BUBBLE);
  homeWeatherTitle = label(homeWeatherPane, "臺北 · 天氣", 0, 8, 103);
  lv_obj_set_style_text_font(homeWeatherTitle, &desk_font_16_bold, 0);
  homeRule(homeWeatherPane, 0, 33, kHomeContentW, 1);
  lv_obj_t *weatherCard = lv_obj_create(homeWeatherPane);
  lv_obj_set_pos(weatherCard, 0, 39);
  lv_obj_set_size(weatherCard, kHomeContentW, 124);
  lv_obj_set_style_pad_all(weatherCard, 0, 0);
  lv_obj_set_style_radius(weatherCard, 5, 0);
  lv_obj_set_style_bg_color(weatherCard, lv_color_hex(0x0D1712), 0);
  lv_obj_set_style_border_width(weatherCard, 1, 0);
  lv_obj_set_style_border_color(weatherCard, lv_color_hex(0x83D7BB), 0);
  lv_obj_clear_flag(weatherCard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(weatherCard, LV_OBJ_FLAG_CLICKABLE);
  homeWeatherIcon = lv_image_create(homeWeatherPane);
  lv_obj_set_pos(homeWeatherIcon, 4, 44);
  lv_obj_remove_flag(homeWeatherIcon, LV_OBJ_FLAG_CLICKABLE);
  homeRule(homeWeatherPane, 0, 83, kHomeContentW, 1);
  homeRule(homeWeatherPane, 143, 87, 1, 73);
  weatherPageA = makeWeatherPage(homeWeatherPane);
  lv_obj_set_pos(weatherPageA, 0, 43);
  setWeatherPage(weatherPageA);
  homeRadioPane = lv_obj_create(homeScreen);
  lv_obj_set_pos(homeRadioPane, kHomeRightX, 4);
  lv_obj_set_size(homeRadioPane, kHomeRightW, 164);
  lv_obj_set_style_pad_all(homeRadioPane, 0, 0);
  lv_obj_set_style_bg_opa(homeRadioPane, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(homeRadioPane, 0, 0);
  lv_obj_clear_flag(homeRadioPane, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(homeRadioPane, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_t *radioTitle = label(homeRadioPane, "網路電台", 0, 8, 125);
  lv_obj_set_style_text_font(radioTitle, &desk_font_16_bold, 0);
  homeRule(homeRadioPane, 0, 33, kHomeContentW, 1);
  lv_obj_t *radioHit = lv_obj_create(homeRadioPane);
  lv_obj_set_pos(radioHit, 0, 45); lv_obj_set_size(radioHit, kHomeContentW, 43);
  lv_obj_set_style_pad_all(radioHit, 0, 0);
  lv_obj_set_style_border_width(radioHit, 0, 0);
  lv_obj_set_style_bg_opa(radioHit, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(radioHit, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(radioHit, openRadio, LV_EVENT_PRESSED, nullptr);
  lv_obj_t *accent = homeRule(radioHit, 0, 4, 3, 29);
  lv_obj_set_style_bg_color(accent, lv_color_hex(0x83D7BB), 0);
  homeRadioStation = label(radioHit, "選擇電台", 12, 3, 125);
  lv_obj_set_style_text_font(homeRadioStation, &desk_font_20, 0);
  homeRadioStatus = label(radioHit, "已暫停", 145, 8, 100);
  lv_obj_set_style_text_font(homeRadioStatus, &desk_font_16_bold, 0);
  lv_obj_set_style_text_align(homeRadioStatus, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_t *prev = homeControl(homeRadioPane, LV_SYMBOL_LEFT, "上一台", 12, 97,
                               50, noAction);
  lv_obj_add_event_cb(prev, stepRadio, LV_EVENT_PRESSED, (void *)(intptr_t)-1);
  homeRadioToggle = homeControl(homeRadioPane, LV_SYMBOL_PLAY, "播放", 72, 97,
                                137, toggleRadio);
  lv_obj_set_style_bg_color(homeRadioToggle, lv_color_hex(0x398C62), 0);
  lv_obj_t *next = homeControl(homeRadioPane, LV_SYMBOL_RIGHT, "下一台", 219, 97,
                               50, noAction);
  lv_obj_add_event_cb(next, stepRadio, LV_EVENT_PRESSED, (void *)(intptr_t)1);

  homeMp3Pane = lv_obj_create(homeScreen);
  lv_obj_set_pos(homeMp3Pane, kHomeRightX, 4);
  lv_obj_set_size(homeMp3Pane, kHomeContentW, 164);
  lv_obj_set_style_pad_all(homeMp3Pane, 0, 0);
  lv_obj_set_style_bg_opa(homeMp3Pane, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(homeMp3Pane, 0, 0);
  lv_obj_clear_flag(homeMp3Pane, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *mp3Heading = label(homeMp3Pane, "MP3", 0, 8, 125);
  lv_obj_set_style_text_font(mp3Heading, &desk_font_16_bold, 0);
  homeRule(homeMp3Pane, 0, 33, kHomeContentW, 1);
  homeMp3.title = label(homeMp3Pane, "尚未掃描", 0, 43, kHomeContentW);
  lv_obj_set_style_text_font(homeMp3.title, &desk_font_16_bold, 0);
  lv_label_set_long_mode(homeMp3.title, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
  lv_obj_set_height(homeMp3.title, lv_font_get_line_height(&desk_font_16_bold));
  homeMp3.status = label(homeMp3Pane, "", 0, 67, 170);
  homeMp3.count = label(homeMp3Pane, "", 175, 67, kHomeContentW - 175);
  lv_obj_set_style_text_align(homeMp3.count, LV_TEXT_ALIGN_RIGHT, 0);
  homeMp3.progress = lv_bar_create(homeMp3Pane);
  lv_obj_set_pos(homeMp3.progress, 0, 92);
  lv_obj_set_size(homeMp3.progress, kHomeContentW, 6);
  lv_bar_set_range(homeMp3.progress, 0, kMp3ProgressWidth);
  button(homeMp3Pane, LV_SYMBOL_LEFT, 12, 107, 50, 43, previousMp3);
  homeMp3.play = button(homeMp3Pane, LV_SYMBOL_PLAY, 72, 107, 137, 43, toggleMp3);
  lv_obj_set_style_bg_color(homeMp3.play, lv_color_hex(0x398C62), 0);
  button(homeMp3Pane, LV_SYMBOL_RIGHT, 219, 107, 50, 43, nextMp3);

  homeCarouselSwitch = lv_obj_create(homeScreen);
  lv_obj_set_pos(homeCarouselSwitch, kHomeRightX + kHomeContentW, 0);
  lv_obj_set_size(homeCarouselSwitch, kWidth - (kHomeRightX + kHomeContentW), 172);
  lv_obj_set_style_pad_all(homeCarouselSwitch, 0, 0);
  lv_obj_set_style_radius(homeCarouselSwitch, 0, 0);
  lv_obj_set_style_bg_opa(homeCarouselSwitch, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(homeCarouselSwitch, 0, 0);
  lv_obj_set_style_shadow_width(homeCarouselSwitch, 0, 0);
  lv_obj_clear_flag(homeCarouselSwitch, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(homeCarouselSwitch, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(homeCarouselSwitch, swipeHomeCarousel, LV_EVENT_GESTURE, nullptr);
  for (uint8_t i = 0; i < kHomeCarouselCount; ++i) {
    lv_obj_t *tab = lv_button_create(homeCarouselSwitch);
    homeCarouselTabs[i] = tab;
    lv_obj_set_pos(tab, 1, i * kHomeTabHeight);
    lv_obj_set_size(tab, kWidth - (kHomeRightX + kHomeContentW) - 2, kHomeTabHeight);
    lv_obj_set_style_pad_all(tab, 0, 0);
    lv_obj_set_style_border_width(tab, 0, 0);
    lv_obj_set_style_radius(tab, 4, 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(tab, 0, 0);
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(tab, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(tab, selectHomeCarousel, LV_EVENT_PRESSED, (void *)(uintptr_t)i);
    lv_obj_add_event_cb(tab, swipeHomeCarousel, LV_EVENT_GESTURE, nullptr);
    lv_obj_t *bar = lv_obj_create(tab);
    lv_obj_set_pos(bar, 17, 8);
    lv_obj_set_size(bar, 4, 18);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
  }
  setFooterPage(footerPageA, 0);
  showHomeCarousel(0);
  updateHomeWeatherIcon();
  updateHomeRadio();
}
