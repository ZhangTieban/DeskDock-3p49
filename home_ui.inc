// DeskDock 640x172 home screen. Included after the shared UI helpers.
static constexpr int kHomeRightX = 315;
static constexpr int kHomeRightW = 313;
static constexpr int kHomeContentW = 285;
static constexpr int kHomeStockContentW = kHomeContentW - 16;
static constexpr int kHomeStockH = 104;

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
  homeSetText(lv_obj_get_child(page, 5), "成交量 " + (detail.volume.isEmpty() ? String("--") : detail.volume + " 股"));
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

static void moveHomePage(void *obj, int32_t y) { lv_obj_set_y((lv_obj_t *)obj, y); }

static void animateHomePages(lv_obj_t *first, lv_obj_t *second, int height) {
  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_exec_cb(&anim, moveHomePage);
  lv_anim_set_duration(&anim, 300);
  lv_anim_set_var(&anim, first);
  lv_anim_set_values(&anim, 0, -height);
  lv_anim_start(&anim);
  lv_anim_set_var(&anim, second);
  lv_anim_set_values(&anim, height, 0);
  lv_anim_start(&anim);
}

static void advanceFooter(lv_event_t *) {
  if (footerAnimating || stockCount < 2) return;
  footerPage = (footerPage + 1) % stockCount;
  setFooterPage(footerPageB, footerPage);
  animateHomePages(footerPageA, footerPageB, kHomeStockH);
  footerAnimating = true;
  lastFooterFlip = millis();
}

static void homeVisible(lv_obj_t *item, bool visible) {
  if (visible) lv_obj_remove_flag(item, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(item, LV_OBJ_FLAG_HIDDEN);
}

static void showHomeCarousel(uint8_t page) {
  homeCarouselPage = page % 3;
  if (homeCarouselPage == 0) lastWeatherFlip = millis();
  if (homeCarouselPage == 1) lastFooterFlip = millis();
  homeVisible(homeWeatherPane, homeCarouselPage == 0);
  homeVisible(homeStockPane, homeCarouselPage == 1);
  homeVisible(homeRadioPane, homeCarouselPage == 2);
  for (uint8_t i = 0; i < 3; ++i) {
    const bool active = i == homeCarouselPage;
    lv_obj_t *bar = lv_obj_get_child(homeCarouselTabs[i], 0);
    lv_obj_set_pos(bar, active ? 6 : 8, active ? 15 : 19);
    lv_obj_set_size(bar, active ? 8 : 4, active ? 26 : 18);
    lv_obj_set_style_bg_color(bar,
        lv_color_hex(active ? 0x83D7BB : 0x536E5D), 0);
  }
}

static void selectHomeCarousel(lv_event_t *event) {
  showHomeCarousel((uint8_t)(uintptr_t)lv_event_get_user_data(event));
}

static void swipeHomeCarousel(lv_event_t *) {
  const lv_dir_t direction = lv_indev_get_gesture_dir(lv_indev_active());
  if (direction == LV_DIR_TOP && homeCarouselPage < 2)
    showHomeCarousel(homeCarouselPage + 1);
  else if (direction == LV_DIR_BOTTOM && homeCarouselPage > 0)
    showHomeCarousel(homeCarouselPage - 1);
}

static void updateHomeWeatherIcon() {
  static int renderedCode = -999;
  static int renderedDay = -1;
  const WeatherData &item = weatherData[weatherSlot];
  const int code = item.valid ? item.code : -1;
  const int day = item.day ? 1 : 0;
  if (renderedCode == code && renderedDay == day) return;
  renderedCode = code; renderedDay = day;
  const uint32_t cloudColor = day ? 0x55A9E8 : 0x91A8D4;
  for (uint32_t i = 0; i < lv_obj_get_child_count(weatherCloud); ++i)
    lv_obj_set_style_bg_color(lv_obj_get_child(weatherCloud, i),
                              lv_color_hex(cloudColor), 0);
  const bool wet = code >= 51;
  const bool partial = code == 1 || code == 2;
  const bool sky = code == 0 || partial;
  homeVisible(weatherSun, sky && day);
  homeVisible(weatherMoon, sky && !day);
  homeVisible(weatherCloud, code < 0 || partial || code == 3 || code == 45 || code == 48);
  homeVisible(weatherRainIcon, wet);
}

static void updateHomeRadio() {
  const int station = radioStation.load();
  const int state = radioState.load();
  const String stationName = station ? String(kRadioNames[station - 1]) : String("選擇電台");
  if (strcmp(lv_label_get_text(homeRadioStation), stationName.c_str())) {
    lv_point_t textSize;
    lv_text_get_size(&textSize, stationName.c_str(), &desk_font_20, 0, 0,
                     LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_obj_set_style_text_font(homeRadioStation,
        textSize.x > 125 ? &desk_font_16_bold : &desk_font_20, 0);
  }
  homeSetText(homeRadioStation, stationName);
  lv_obj_set_style_text_color(homeRadioStation,
      lv_color_hex(state == 3 ? 0xF27873 : 0xF3F5F2), 0);
  const bool playing = state == 1 || state == 2;
  homeSetText(homeRadioStatus, state == 1 ? String("連線中") :
      state == 2 ? String("播放中") : state == 3 ? String("連線失敗") : String("已暫停"));
  lv_obj_set_style_text_color(homeRadioStatus,
      lv_color_hex(state == 3 ? 0xF27873 : playing ? 0x83D7BB : 0xAABBB1), 0);
  homeSetText(lv_obj_get_child(homeRadioToggle, 0),
              playing ? String(LV_SYMBOL_PAUSE) : String(LV_SYMBOL_PLAY));
  homeSetText(lv_obj_get_child(homeRadioToggle, 1), playing ? String("暫停") : String("播放"));
}

static void refreshHomeRows(uint32_t now) {
  const uint32_t rotateMs = (uint32_t)cfg.carouselSeconds * 1000UL;
  if (homeCarouselPage == 0 && (uint32_t)(now - lastWeatherFlip) >= rotateMs) {
    weatherSlot = (weatherSlot + 1) % kWeatherSlots;
    lastWeatherFlip = now;
  }
  setWeatherPage(weatherPageA);
  if (footerAnimating && (uint32_t)(now - lastFooterFlip) >= 350) {
    lv_obj_t *old = footerPageA;
    footerPageA = footerPageB; footerPageB = old;
    lv_obj_set_y(footerPageB, kHomeStockH);
    footerAnimating = false;
  }
  if (!footerAnimating) setFooterPage(footerPageA, footerPage);
  if (homeCarouselPage == 1 && !footerAnimating &&
      (uint32_t)(now - lastFooterFlip) >= rotateMs) advanceFooter(nullptr);
  String indexValue, indexChange, indexStamp;
  splitStockLine(remote.index, 1, indexValue, indexChange, indexStamp);
  homeSetText(homeStockIndexLabel, String("加權 ") + indexValue);
  String indexChangeText = indexChange;
  if (indexChange.startsWith("+")) indexChangeText = "▲" + indexChange.substring(1);
  else if (indexChange.startsWith("-")) indexChangeText = "▼" + indexChange.substring(1);
  homeSetText(homeStockChangeLabel, indexChangeText);
  lv_obj_set_style_text_color(homeStockChangeLabel,
                              lv_color_hex(stockChangeColor(indexChange)), 0);
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

static void homeCloudPart(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *part = lv_obj_create(parent);
  lv_obj_set_pos(part, x, y); lv_obj_set_size(part, w, h);
  lv_obj_set_style_pad_all(part, 0, 0);
  lv_obj_set_style_radius(part, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(part, lv_color_hex(0x55A9E8), 0);
  lv_obj_set_style_border_width(part, 0, 0);
  lv_obj_remove_flag(part, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(part, LV_OBJ_FLAG_SCROLLABLE);
}

static void buildHome() {
  homeScreen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(homeScreen, lv_color_hex(0x070B0A), 0);
  lv_obj_set_style_pad_all(homeScreen, 0, 0);
  lv_obj_clear_flag(homeScreen, LV_OBJ_FLAG_SCROLLABLE);
  homeRule(homeScreen, 307, 8, 1, 156);

  clockLabel = label(homeScreen, "--:--", -12, 24, 298, 93);
  lv_obj_set_style_text_font(clockLabel, &desk_font_93, 0);
  lv_obj_set_style_text_align(clockLabel, LV_TEXT_ALIGN_CENTER, 0);
  dateLabel = label(homeScreen, "", 11, 133, 286);
  lv_obj_set_style_text_font(dateLabel, &desk_font_16_bold, 0);
  lv_obj_set_style_text_align(dateLabel, LV_TEXT_ALIGN_CENTER, 0);
  clockPeriodLabel = label(homeScreen, "", 254, 111, 43);
  lv_obj_set_style_text_font(clockPeriodLabel, &desk_font_10_bold, 0);
  lv_obj_set_style_text_align(clockPeriodLabel, LV_TEXT_ALIGN_RIGHT, 0);

  wifiLabel = label(homeScreen, "Wi-Fi", 12, 9, 210);
  lv_obj_set_style_text_font(wifiLabel, &desk_font_10_bold, 0);

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
  homeWeatherTitle = label(homeWeatherPane, "臺北 · 天氣", 0, 8, 130);
  lv_obj_set_style_text_font(homeWeatherTitle, &desk_font_16_bold, 0);
  lv_obj_t *indoorTitle = label(homeWeatherPane, "室內", 137, 8, 34);
  lv_obj_set_style_text_font(indoorTitle, &desk_font_16_bold, 0);
  lv_obj_set_style_text_color(indoorTitle, lv_color_hex(0xAABBB1), 0);
  homeIndoorTemperature = label(homeWeatherPane, "--°C", 171, 8, 58);
  lv_obj_set_style_text_font(homeIndoorTemperature, &desk_font_16_bold, 0);
  homeIndoorHumidity = label(homeWeatherPane, "--%", 232, 8, 53);
  lv_obj_set_style_text_font(homeIndoorHumidity, &desk_font_16_bold, 0);
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
  weatherSun = label(homeWeatherPane, "☀", 5, 45, 32);
  lv_obj_set_style_text_font(weatherSun, &desk_font_24_weather, 0);
  lv_obj_set_style_text_color(weatherSun, lv_color_hex(0xF5C978), 0);
  weatherMoon = label(homeWeatherPane, "☾", 5, 45, 32);
  lv_obj_set_style_text_font(weatherMoon, &desk_font_24_moon, 0);
  lv_obj_set_style_text_color(weatherMoon, lv_color_hex(0xF5D899), 0);
  weatherCloud = lv_obj_create(homeWeatherPane);
  lv_obj_set_pos(weatherCloud, 0, 43); lv_obj_set_size(weatherCloud, 55, 40);
  lv_obj_set_style_pad_all(weatherCloud, 0, 0);
  lv_obj_set_style_bg_opa(weatherCloud, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(weatherCloud, 0, 0);
  lv_obj_remove_flag(weatherCloud, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(weatherCloud, LV_OBJ_FLAG_SCROLLABLE);
  homeCloudPart(weatherCloud, 13, 23, 37, 14);
  homeCloudPart(weatherCloud, 19, 13, 20, 20);
  homeCloudPart(weatherCloud, 34, 19, 13, 13);
  weatherRainIcon = label(homeWeatherPane, "☂", 8, 46, 32);
  lv_obj_set_style_text_font(weatherRainIcon, &desk_font_24_weather, 0);
  lv_obj_set_style_text_color(weatherRainIcon, lv_color_hex(0x83D7BB), 0);
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
  lv_obj_add_event_cb(radioHit, openRadio, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *accent = homeRule(radioHit, 0, 4, 3, 29);
  lv_obj_set_style_bg_color(accent, lv_color_hex(0x83D7BB), 0);
  homeRadioStation = label(radioHit, "選擇電台", 12, 3, 125);
  lv_obj_set_style_text_font(homeRadioStation, &desk_font_20, 0);
  homeRadioStatus = label(radioHit, "已暫停", 145, 8, 100);
  lv_obj_set_style_text_font(homeRadioStatus, &desk_font_16_bold, 0);
  lv_obj_set_style_text_align(homeRadioStatus, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_t *prev = homeControl(homeRadioPane, LV_SYMBOL_LEFT, "上一台", 12, 97,
                               50, noAction);
  lv_obj_add_event_cb(prev, stepRadio, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
  homeRadioToggle = homeControl(homeRadioPane, LV_SYMBOL_PLAY, "播放", 72, 97,
                                137, toggleRadio);
  lv_obj_set_style_bg_color(homeRadioToggle, lv_color_hex(0x398C62), 0);
  lv_obj_t *next = homeControl(homeRadioPane, LV_SYMBOL_RIGHT, "下一台", 219, 97,
                               50, noAction);
  lv_obj_add_event_cb(next, stepRadio, LV_EVENT_CLICKED, (void *)(intptr_t)1);

  homeCarouselSwitch = lv_obj_create(homeScreen);
  lv_obj_set_pos(homeCarouselSwitch, kHomeRightX + 291, 0);
  lv_obj_set_size(homeCarouselSwitch, 22, 172);
  lv_obj_set_style_pad_all(homeCarouselSwitch, 0, 0);
  lv_obj_set_style_radius(homeCarouselSwitch, 0, 0);
  lv_obj_set_style_bg_opa(homeCarouselSwitch, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(homeCarouselSwitch, 0, 0);
  lv_obj_set_style_shadow_width(homeCarouselSwitch, 0, 0);
  lv_obj_clear_flag(homeCarouselSwitch, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(homeCarouselSwitch, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(homeCarouselSwitch, swipeHomeCarousel, LV_EVENT_GESTURE, nullptr);
  for (uint8_t i = 0; i < 3; ++i) {
    lv_obj_t *tab = lv_button_create(homeCarouselSwitch);
    homeCarouselTabs[i] = tab;
    lv_obj_set_pos(tab, 1, 1 + i * 57);
    lv_obj_set_size(tab, 20, 56);
    lv_obj_set_style_pad_all(tab, 0, 0);
    lv_obj_set_style_border_width(tab, 0, 0);
    lv_obj_set_style_radius(tab, 4, 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(tab, 0, 0);
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(tab, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(tab, selectHomeCarousel, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    lv_obj_add_event_cb(tab, swipeHomeCarousel, LV_EVENT_GESTURE, nullptr);
    lv_obj_t *bar = lv_obj_create(tab);
    lv_obj_set_pos(bar, 8, 8);
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
