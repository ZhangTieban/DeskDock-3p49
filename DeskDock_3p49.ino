#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include <esp_heap_caps.h>
#include <esp_sntp.h>
#include <cJSON.h>
#include <ESP_I2S.h>
#include <AudioFileSourceICYStream.h>
#include <AudioFileSourceBuffer.h>
#include <AudioFileSourceFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorAAC.h>
#include <AudioOutputI2S.h>
#include "BestRadioHlsSource.h"
#include <sys/time.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <math.h>
#include <lvgl.h>
#include "i2c_bsp.h"
#include "lvgl_port.h"
#include "src/lcd_bl_bsp/lcd_bl_pwm_bsp.h"
#include "es8311.h"
#include "desk_font.h"
#include "radio_eq.h"
#include "ota_config.h"

// The panel and touch driver come from the supplied Waveshare 3.49 example.
static constexpr int kWidth = 640;
static constexpr int kHeight = 172;
static constexpr int kBatteryPin = 4; // ADC1_CH3, 1:3 divider
static constexpr int kPowerButtonPin = 16;
static constexpr int kSettingsButtonPin = 0;
static constexpr uint32_t kButtonDebounceMs = 35;
static constexpr uint32_t kSettingsClickMaxMs = 1500;
static constexpr uint32_t kPowerHoldMs = 5000;
static constexpr uint32_t kPowerReleaseMs = 250;
static constexpr uint32_t kWeatherPeriodMs = 30UL * 60UL * 1000UL;
static constexpr uint32_t kStockPeriodMs = 60UL * 60UL * 1000UL;
static constexpr uint32_t kIntradayPeriodMs = 10UL * 60UL * 1000UL;
static constexpr char kTz[] = "CST-8";
static constexpr int kMaxWifiProfiles = 5;
static constexpr int kMaxWifiEntries = 14;
static constexpr int kMaxStocks = 10;
static constexpr int kWeatherSlots = 3;
static constexpr int kWeatherMetrics = 11;
static constexpr int kRadioStations = 9;
static constexpr size_t kRadioBufferBytes = 256 * 1024;
static const char *kRadioNames[kRadioStations] = {
  "Groove Salad", "Space Station", "自訂電台", "UFO 92.1", "BCC 103.3",
  "好事989", "好事903", "港都983", "好事935"
};
static const char *kRadioButtonNames[kRadioStations] = {
  "Groove", "Space", "自訂電台", "UFO 92.1", "BCC 103.3",
  "好事989", "好事903", "港都983", "好事935"
};
static const char *kRadioUrls[kRadioStations] = {
  "http://ice5.somafm.com/groovesalad-128-mp3",
  "http://ice5.somafm.com/spacestation-128-mp3",
  nullptr,
  "http://stream.rcs.revma.com/em90w4aeewzuv",
  "http://stream.rcs.revma.com/s1zttsg3qtzuv",
  nullptr, nullptr, nullptr, nullptr
};

static const char *kCounties[] = {
  "臺北", "新北", "桃園", "臺中", "臺南", "高雄",
  "基隆", "新竹市", "新竹縣", "苗栗", "彰化", "南投",
  "雲林", "嘉義市", "嘉義縣", "屏東", "宜蘭", "花蓮",
  "臺東", "澎湖", "金門", "連江"
};
static constexpr int kCountyCount = sizeof(kCounties) / sizeof(kCounties[0]);
struct WeatherPoint { float latitude, longitude; };
// County-seat representative points, in the same order as kCounties.
static constexpr WeatherPoint kWeatherPoints[] = {
  {25.0330f, 121.5654f}, {25.0120f, 121.4650f}, {24.9930f, 121.3010f},
  {24.1480f, 120.6740f}, {22.9990f, 120.2270f}, {22.6270f, 120.3010f},
  {25.1280f, 121.7410f}, {24.8040f, 120.9690f}, {24.8380f, 121.0040f},
  {24.5600f, 120.8210f}, {24.0740f, 120.5380f}, {23.9600f, 120.9720f},
  {23.7090f, 120.5410f}, {23.4800f, 120.4490f}, {23.4590f, 120.3330f},
  {22.6760f, 120.4950f}, {24.7570f, 121.7540f}, {23.9910f, 121.6010f},
  {22.7580f, 121.1440f}, {23.5660f, 119.5660f}, {24.4340f, 118.3170f},
  {26.1600f, 119.9500f}
};

struct Config {
  String ssid, password, stockCode = "2330", customRadioUrl;
  int counties[kWeatherSlots] = {0, 3, 5};
  int brightness = 75;
  int sleepMinutes = 10;
  int volume = 40;
  int eqDb[3] = {0, 0, 0};
  int weatherMain = 0, weatherDetail = 1, weatherThird = 4;
  int carouselSeconds = 10;
  bool hour12 = false;
  int batteryMode = 2; // 0: voltage fallback, 1: installed, 2: absent
};
struct WeatherData {
  String summary = "等待 Open-Meteo 資料";
  String stamp;
  int low = 0, high = 0, rain = 0, code = -1;
  int temperature = -999, feels = -999, humidity = -1, wind = -1;
  int cloud = -1, uv10 = -1, rainMm10 = -1, sunrise = -1, sunset = -1;
  bool day = true, valid = false;
};
struct RemoteData {
  String stock = "請連線取得資料";
  String index = "請連線取得資料";
  String night = "請連線取得資料";
  String stockStamp, indexStamp, nightStamp;
};

static Config cfg;
static RemoteData remote;
static WeatherData weatherData[kWeatherSlots];
static float indoorTemperature = 0, indoorHumidity = 0;
static bool indoorValid = false;
static bool aht30Pending = false;
static uint32_t aht30StartedAt = 0, aht30NextAt = 0;
static uint8_t weatherSlot = 0;
static uint32_t lastWeatherFlip = 0;
static SemaphoreHandle_t configMutex;
static Preferences prefs;
static std::atomic<bool> ntpSynced{false};
static std::atomic<bool> beepPending{false};
static I2SClass audioI2s;
static es8311_handle_t codec = nullptr;
static bool audioReady = false;
static int appliedVolume = -1;
static SemaphoreHandle_t audioMutex;
static SemaphoreHandle_t networkHttpMutex;
static constexpr int kMp3PlayCommand = 100;
static constexpr int kMp3PauseCommand = -3;
static constexpr int kMp3ResumeCommand = -4;
static constexpr int kMp3MaxTracks = 32;
static std::atomic<int> radioCommand{-1}; // -1 none, -2 failed, 0 stop, 1..9 station, 100 SD MP3
static std::atomic<int> radioState{0}; // 0 stopped, 1 connecting, 2 playing, 3 failed
static std::atomic<int> radioStation{0};
static std::atomic<int> mp3State{0}; // 0 stopped, 1 opening, 2 playing, 3 paused, 4 failed, 5 ended
static std::atomic<int> mp3Progress{0}; // 0..1000, based on file bytes
static SemaphoreHandle_t mp3RequestMutex;
static char mp3RequestedPath[192] = {};
static String mp3Tracks[kMp3MaxTracks];
static int mp3TrackCount = 0, mp3SelectedTrack = 0;
static const char *mp3StorageStatus = "尚未掃描";
static std::atomic<int> eqGain[3];
static TaskHandle_t radioTaskHandle = nullptr;
static TaskHandle_t networkTaskHandle = nullptr;
static constexpr uint32_t kFetchWeather = 1;
static constexpr uint32_t kFetchMarkets = 2;
static constexpr uint32_t kFetchIntraday = 4;
static String stockCodes[kMaxStocks];
static String stockValues[kMaxStocks];
static String stockStamps[kMaxStocks];
static String stockNames[kMaxStocks];
struct StockDetail { String high, low, volume; };
struct IntradayQuote;
static StockDetail stockDetails[kMaxStocks];
static uint8_t stockCount = 1;
static uint8_t stockPage = 0;
static uint32_t stockUiRevision = 0;
static std::atomic<bool> networkRefresh{true};
static uint32_t lastWeatherAttempt = 0, lastStockAttempt = 0, lastIntradayAttempt = 0;
static uint32_t lastInputMs = 0, lastBatteryMs = 0;
static bool intradayWindow();
static std::atomic<uint32_t> lastTouchMs{0};
static int batteryMv = 0, batteryPct = 0;
static bool screenDimmed = false;
struct PhysicalButton {
  int pin;
  bool rawPressed = false, stablePressed = false, armed = false;
  bool touchedDuringPress = false;
  uint32_t changedMs = 0, pressedMs = 0;
};
static PhysicalButton powerButton{kPowerButtonPin};
static PhysicalButton settingsButton{kSettingsButtonPin};
static bool settingsTogglePending = false;
static bool powerOffRequested = false;
static uint32_t powerReleasedMs = 0;
struct WifiProfile { String ssid, password; };
struct WifiEntry { String ssid; int rssi = 0; bool secured = true; bool saved = false; };
static WifiProfile wifiProfiles[kMaxWifiProfiles];
static WifiEntry wifiEntries[kMaxWifiEntries];
static int wifiProfileCount = 0, wifiEntryCount = 0, wifiAttempt = -1;
static uint32_t wifiAttemptMs = 0;
static bool wifiScanPending = false, wifiListDirty = true, wifiWasConnected = false;
static String selectedWifiSsid;
static String wifiCandidateSsid, wifiCandidatePassword, wifiBadSsid;
static bool wifiCandidateActive = false, wifiHold = false;
static bool wifiTransientFailure = false;
static uint32_t wifiRetryAtMs = 0, wifiRetryDelayMs = 30000;
static int wifiStatusError = 0; // 1: authentication, 2: timeout/unavailable
static std::atomic<int> wifiDisconnectReason{0};
extern "C" void desk_touch_activity(void) {
  const uint32_t now = millis();
  lastInputMs = now;
  lastTouchMs.store(now);
}
extern "C" void desk_touch_report(int x, int y) {
  Serial.printf("[TOUCH] raw=%d,%d ui=%d,%d\n", x, y, y, kWidth - 1 - x);
}
static lv_obj_t *homeScreen, *settingsScreen, *editorScreen, *stockScreen;
static lv_obj_t *stockReturnScreen;
static constexpr uint8_t kSettingsPages = 7;
static constexpr uint8_t kSettingsNavCount = kSettingsPages - 1;
static constexpr uint8_t kRadioSettingsPage = 5;
static constexpr uint8_t kMp3SettingsPage = 6;
static lv_obj_t *settingsPages[kSettingsPages], *settingsNavButtons[kSettingsNavCount];
static lv_obj_t *settingsNavIndicators[kSettingsNavCount];
static lv_obj_t *mp3TitleLabel, *mp3StatusLabel, *mp3CountLabel;
static lv_obj_t *mp3PlayButton, *mp3ProgressBar;
static lv_obj_t *clockLabel, *clockPeriodLabel, *dateLabel, *wifiLabel;
static lv_obj_t *footerPageA, *footerPageB;
static uint8_t footerPage = 0;
static uint32_t lastFooterFlip = 0;
static bool footerAnimating = false;
static lv_obj_t *weatherPageA, *homeWeatherTitle;
static lv_obj_t *homeWeatherPane, *homeStockPane, *homeRadioPane;
static lv_obj_t *homeCarouselSwitch, *homeCarouselTabs[3];
static lv_obj_t *homeStockIndexLabel, *homeStockChangeLabel, *carouselValueLabel;
static lv_obj_t *homeIndoorTemperature, *homeIndoorHumidity;
static uint8_t homeCarouselPage = 0;
static lv_obj_t *weatherSun, *weatherMoon, *weatherCloud;
static lv_obj_t *weatherRainIcon, *stockPositionDots[kMaxStocks];
static lv_obj_t *homeRadioStation, *homeRadioStatus, *homeRadioToggle;
static lv_obj_t *networkLabel, *wifiList, *weatherPreviewLabels[kWeatherSlots], *stockCodesLabel;
static lv_obj_t *stockPreviewLabel, *hourModeButton;
static lv_obj_t *otaButton, *otaStatusLabel;
static lv_obj_t *eqValueLabels[3];
static void otaButtonClicked(lv_event_t *);
static void refreshOtaUi();
static void otaCheckFirstBoot();
static void otaConfirmFirstBoot(uint32_t now);
static lv_obj_t *stockCard[3], *stockPageLabel, *radioStatusLabel, *radioNowLabel, *radioPlayButton;
static lv_obj_t *radioStationButtons[kRadioStations];
static lv_obj_t *brightnessValueLabel, *sleepValueLabel, *volumeValueLabel;
static lv_obj_t *editorText, *editorKeyboard;
static lv_obj_t *editorReturnScreen;
static void openStocks(lv_event_t *);
static void openRadio(lv_event_t *);
static void selectSettingsPage(uint8_t index);
static void toggleRadio(lv_event_t *);
static void stepRadio(lv_event_t *);
static void editStock(lv_event_t *);
static void editRadioUrl(lv_event_t *);

enum class EditField { Ssid, Password, Stock, DateTime, RadioUrl };
static EditField editField = EditField::Ssid;

static uint8_t bcd(int value) { return (uint8_t)((value / 10) * 16 + value % 10); }
static int unbcd(uint8_t value) { return (value >> 4) * 10 + (value & 15); }

static bool rtcRead(tm *out) {
  uint8_t reg = 0x04, data[7] = {};
  if (i2c_master_transmit_receive(rtc_dev_handle, &reg, 1, data, 7, 100) != ESP_OK) return false;
  tm value = {};
  value.tm_sec = unbcd(data[0] & 0x7f);
  value.tm_min = unbcd(data[1] & 0x7f);
  value.tm_hour = unbcd(data[2] & 0x3f);
  value.tm_mday = unbcd(data[3] & 0x3f);
  value.tm_mon = unbcd(data[5] & 0x1f) - 1;
  value.tm_year = unbcd(data[6]) + 100;
  if (value.tm_year < 124 || value.tm_year > 199 || value.tm_mon < 0 || value.tm_mon > 11 ||
      value.tm_mday < 1 || value.tm_mday > 31 || value.tm_hour > 23 || value.tm_min > 59 || value.tm_sec > 59) return false;
  *out = value;
  return true;
}

static bool rtcWrite(const tm &value) {
  uint8_t data[] = {0x04, bcd(value.tm_sec), bcd(value.tm_min), bcd(value.tm_hour),
                    bcd(value.tm_mday), bcd(value.tm_wday), bcd(value.tm_mon + 1), bcd((value.tm_year + 1900) % 100)};
  return i2c_master_transmit(rtc_dev_handle, data, sizeof(data), 100) == ESP_OK;
}

static void setSystemFromRtc() {
  tm value;
  if (!rtcRead(&value)) return;
  time_t epoch = mktime(&value);
  timeval tv = {.tv_sec = epoch, .tv_usec = 0};
  settimeofday(&tv, nullptr);
}

static void onNtpSync(struct timeval *) { ntpSynced.store(true); }

static bool parseStockCodes(const String &input) {
  String parsed[kMaxStocks];
  uint8_t count = 0;
  int start = 0;
  while (start < (int)input.length()) {
    int end = input.indexOf(',', start);
    if (end < 0) end = input.length();
    String code = input.substring(start, end);
    code.trim();
    if (code.length() < 4 || code.length() > 6 || count >= kMaxStocks) return false;
    for (size_t j = 0; j < code.length(); ++j)
      if (!isDigit(code[j])) return false;
    for (uint8_t j = 0; j < count; ++j)
      if (parsed[j] == code) return false;
    parsed[count++] = code;
    start = end + 1;
  }
  if (!count || input.endsWith(",")) return false;
  stockCount = count;
  for (uint8_t i = 0; i < kMaxStocks; ++i) stockCodes[i] = i < count ? parsed[i] : "";
  ++stockUiRevision;
  return true;
}

static void decodeStockDetail(uint8_t index, const String &encoded) {
  const int first = encoded.indexOf('|');
  const int second = first < 0 ? -1 : encoded.indexOf('|', first + 1);
  if (second < 0) return;
  stockDetails[index].high = encoded.substring(0, first);
  stockDetails[index].low = encoded.substring(first + 1, second);
  stockDetails[index].volume = encoded.substring(second + 1);
}

static String encodeStockDetail(const StockDetail &detail) {
  return detail.high + "|" + detail.low + "|" + detail.volume;
}

static void loadConfig() {
  prefs.begin("desk349", false);
  cfg.ssid = prefs.getString("ssid", "");
  cfg.password = prefs.getString("wifi_pw", "");
  cfg.stockCode = prefs.getString("stock", "2330");
  if (!parseStockCodes(cfg.stockCode)) {
    cfg.stockCode = "2330";
    parseStockCodes(cfg.stockCode);
  }
  cfg.customRadioUrl = prefs.getString("radio_url", "");
  cfg.counties[0] = constrain(prefs.getInt("county", 0), 0, kCountyCount - 1);
  cfg.counties[1] = constrain(prefs.getInt("county1", cfg.counties[0] == 3 ? 0 : 3),
                              0, kCountyCount - 1);
  cfg.counties[2] = constrain(prefs.getInt("county2", cfg.counties[0] == 5 ? 0 : 5),
                              0, kCountyCount - 1);
  const int savedBrightness = prefs.getInt("bright_ui_v2", -1);
  const int oldBrightness = constrain(prefs.getInt("bright", 75), 0, 100);
  cfg.brightness = savedBrightness >= 0 ? constrain(savedBrightness, 0, 100) :
                   constrain(((oldBrightness - 15) * 100 + 42) / 85, 0, 100);
  cfg.sleepMinutes = constrain(prefs.getInt("sleep", 10), 0, 60);
  cfg.volume = constrain(prefs.getInt("volume", 40), 0, 100);
  static const char *eqKeys[3] = {"eq_b", "eq_m", "eq_t"};
  for (int i = 0; i < 3; ++i) {
    cfg.eqDb[i] = constrain(prefs.getInt(eqKeys[i], 0), -6, 6);
    eqGain[i].store(cfg.eqDb[i]);
  }
  cfg.hour12 = prefs.getBool("hour12", false);
  cfg.weatherMain = constrain(prefs.getInt("wx_main", 0), 0, kWeatherMetrics - 1);
  cfg.weatherDetail = constrain(prefs.getInt("wx_detail", 1), 0, kWeatherMetrics - 1);
  cfg.weatherThird = constrain(prefs.getInt("wx_third", 4), 0, kWeatherMetrics - 1);
  cfg.carouselSeconds = constrain(prefs.getInt("home_rotate", 10), 5, 60);
  cfg.batteryMode = constrain(prefs.getInt("batt_mode2", 2), 0, 3);
  if (cfg.batteryMode == 3) {
    cfg.batteryMode = 0;
    prefs.putInt("batt_mode2", cfg.batteryMode);
  }
  for (int i = 0; i < kWeatherSlots; ++i) {
    const String key = i ? String("om") + i : String("last_om");
    const String stampKey = i ? String("om_at") + i : String("last_om_at");
    int cachedCounty, low, high, rain, code, day;
    int temperature = -999, feels = -999, humidity = -1, wind = -1;
    int cloud = -1, uv10 = -1, rainMm10 = -1, sunrise = -1, sunset = -1;
    const String cached = prefs.getString(key.c_str(), "");
    if (sscanf(cached.c_str(), "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
               &cachedCounty, &low, &high, &rain, &code, &day,
               &temperature, &feels, &humidity, &wind, &cloud, &uv10,
               &rainMm10, &sunrise, &sunset) >= 6 &&
        cachedCounty == cfg.counties[i]) {
      WeatherData &item = weatherData[i];
      item.low = low; item.high = high; item.rain = rain; item.code = code;
      item.temperature = temperature; item.feels = feels;
      item.humidity = humidity; item.wind = wind; item.cloud = cloud;
      item.uv10 = uv10; item.rainMm10 = rainMm10;
      item.sunrise = sunrise; item.sunset = sunset;
      item.day = day != 0; item.valid = true;
      item.summary = String(kCounties[cfg.counties[i]]) + "  " + low + "-" + high +
                     " C  降雨 " + rain + "%";
      item.stamp = prefs.getString(stampKey.c_str(), "");
    }
  }
  remote.stock = prefs.getString("last_st", "請連線取得資料");
  remote.stockStamp = prefs.getString("last_st_at", "");
  stockValues[0] = remote.stock;
  stockStamps[0] = remote.stockStamp;
  if (prefs.getString("st0_code", "") == stockCodes[0]) {
    stockNames[0] = prefs.getString("st0_name", "");
    decodeStockDetail(0, prefs.getString("st0_detail", ""));
  }
  for (uint8_t i = 1; i < stockCount; ++i) {
    const String key = String("st") + i;
    if (prefs.getString((key + "_code").c_str(), "") == stockCodes[i]) {
      stockValues[i] = prefs.getString((key + "_value").c_str(), "請連線取得資料");
      stockStamps[i] = prefs.getString((key + "_stamp").c_str(), "");
      stockNames[i] = prefs.getString((key + "_name").c_str(), "");
      decodeStockDetail(i, prefs.getString((key + "_detail").c_str(), ""));
    } else stockValues[i] = "請連線取得資料";
  }
  remote.index = prefs.getString("last_idx", "請連線取得資料");
  remote.indexStamp = prefs.getString("last_idx_at", "");
  remote.night = prefs.getString("last_night", "請連線取得資料");
  remote.nightStamp = prefs.getString("last_night_at", "");
  wifiProfileCount = constrain(prefs.getInt("wifi_count", 0), 0, kMaxWifiProfiles);
  for (int i = 0; i < wifiProfileCount; ++i) {
    wifiProfiles[i].ssid = prefs.getString((String("w_ssid") + i).c_str(), "");
    wifiProfiles[i].password = prefs.getString((String("w_pass") + i).c_str(), "");
  }
  if (wifiProfileCount == 0 && cfg.ssid.length()) {
    wifiProfiles[0] = {cfg.ssid, cfg.password};
    wifiProfileCount = 1;
  }
}

static void persistWifiProfiles() {
  prefs.putInt("wifi_count", wifiProfileCount);
  for (int i = 0; i < kMaxWifiProfiles; ++i) {
    const String ssid = i < wifiProfileCount ? wifiProfiles[i].ssid : "";
    const String pass = i < wifiProfileCount ? wifiProfiles[i].password : "";
    prefs.putString((String("w_ssid") + i).c_str(), ssid);
    prefs.putString((String("w_pass") + i).c_str(), pass);
  }
}

static int findWifiProfile(const String &ssid) {
  for (int i = 0; i < wifiProfileCount; ++i)
    if (wifiProfiles[i].ssid == ssid) return i;
  return -1;
}

static void rememberWifi(const String &ssid, const String &password) {
  if (!ssid.length()) return;
  int index = findWifiProfile(ssid);
  if (index < 0) {
    if (wifiProfileCount < kMaxWifiProfiles) ++wifiProfileCount;
    index = wifiProfileCount - 1;
  }
  for (int i = index; i > 0; --i) wifiProfiles[i] = wifiProfiles[i - 1];
  wifiProfiles[0] = {ssid, password};
  cfg.ssid = ssid;
  cfg.password = password;
  persistWifiProfiles();
  wifiListDirty = true;
}

static void startWifiProfile(int index) {
  if (index < 0 || index >= wifiProfileCount) return;
  if (index == 0) wifiTransientFailure = false;
  wifiCandidateActive = false;
  wifiHold = false;
  wifiRetryAtMs = 0;
  wifiStatusError = 0;
  wifiDisconnectReason.store(0);
  wifiAttempt = index;
  wifiAttemptMs = millis();
  WiFi.disconnect();
  WiFi.begin(wifiProfiles[index].ssid.c_str(), wifiProfiles[index].password.c_str());
  Serial.printf("[WIFI] trying priority %d: %s\n", index + 1, wifiProfiles[index].ssid.c_str());
}

static void startWifiCandidate(const String &ssid, const String &password) {
  if (!ssid.length()) return;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  wifiCandidateSsid = ssid;
  wifiCandidatePassword = password;
  wifiCandidateActive = true;
  wifiHold = false;
  wifiRetryAtMs = 0;
  wifiRetryDelayMs = 30000;
  wifiStatusError = 0;
  xSemaphoreGive(configMutex);
  wifiAttempt = -1;
  wifiAttemptMs = millis();
  wifiDisconnectReason.store(0);
  WiFi.disconnect();
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.printf("[WIFI] testing credentials for %s\n", ssid.c_str());
}

static bool wifiAuthFailure(int reason) {
  return reason == WIFI_REASON_AUTH_FAIL || reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
         reason == WIFI_REASON_HANDSHAKE_TIMEOUT;
}

static void scheduleWifiRetry(uint32_t now) {
  wifiAttempt = -1;
  wifiHold = true;
  wifiStatusError = 2;
  wifiRetryAtMs = now + wifiRetryDelayMs;
  Serial.printf("[WIFI] retry in %lu seconds\n", (unsigned long)(wifiRetryDelayMs / 1000));
  wifiRetryDelayMs = wifiRetryDelayMs < 150000 ? wifiRetryDelayMs * 2 : 300000;
}

static void maintainWifi(uint32_t now) {
  const int reason = wifiDisconnectReason.exchange(0);
  if (wifiCandidateActive) {
    if (WiFi.status() == WL_CONNECTED) {
      xSemaphoreTake(configMutex, portMAX_DELAY);
      const String ssid = wifiCandidateSsid, password = wifiCandidatePassword;
      wifiCandidateActive = false;
      wifiBadSsid = "";
      rememberWifi(ssid, password);
      xSemaphoreGive(configMutex);
      wifiWasConnected = true;
      wifiRetryDelayMs = 30000;
      networkRefresh = true;
      Serial.printf("[WIFI] credentials verified for %s\n", ssid.c_str());
      return;
    }
    if (wifiAuthFailure(reason) || (uint32_t)(now - wifiAttemptMs) >= 15000) {
      xSemaphoreTake(configMutex, portMAX_DELAY);
      wifiBadSsid = wifiCandidateSsid;
      wifiCandidateActive = false;
      wifiHold = true;
      wifiStatusError = wifiAuthFailure(reason) ? 1 : 2;
      xSemaphoreGive(configMutex);
      WiFi.disconnect();
      Serial.printf("[WIFI] credentials rejected, reason=%d\n", reason);
    }
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiWasConnected) networkRefresh = true;
    wifiWasConnected = true;
    wifiRetryDelayMs = 30000;
    wifiRetryAtMs = 0;
    return;
  }
  if (wifiWasConnected) { wifiWasConnected = false; wifiAttempt = -1; }
  if (wifiAuthFailure(reason) && wifiAttempt >= 0) {
    wifiBadSsid = wifiProfiles[wifiAttempt].ssid;
    wifiStatusError = 1;
    Serial.printf("[WIFI] saved password rejected, reason=%d\n", reason);
    if (wifiAttempt + 1 < wifiProfileCount) startWifiProfile(wifiAttempt + 1);
    else if (wifiTransientFailure) scheduleWifiRetry(now);
    else { wifiAttempt = -1; wifiHold = true; }
    return;
  }
  if (wifiHold) {
    if (!wifiRetryAtMs || (int32_t)(now - wifiRetryAtMs) < 0) return;
    startWifiProfile(0);
    return;
  }
  if (wifiProfileCount == 0) return;
  if (wifiAttempt < 0) startWifiProfile(0);
  else if ((uint32_t)(now - wifiAttemptMs) >= 12000) {
    wifiTransientFailure = true;
    if (wifiAttempt + 1 < wifiProfileCount) startWifiProfile(wifiAttempt + 1);
    else scheduleWifiRetry(now);
  }
}

static void scanWifi(lv_event_t *) {
  if (wifiScanPending) return;
  WiFi.scanDelete();
  wifiScanPending = WiFi.scanNetworks(true) == WIFI_SCAN_RUNNING;
  wifiListDirty = true;
  Serial.println(wifiScanPending ? "[WIFI] scan started" : "[WIFI] scan failed");
}

static void collectWifiScan() {
  if (!wifiScanPending) return;
  const int found = WiFi.scanComplete();
  if (found == WIFI_SCAN_RUNNING) return;
  wifiScanPending = false;
  wifiEntryCount = 0;
  if (found > 0) {
    for (int i = 0; i < found && wifiEntryCount < kMaxWifiEntries; ++i) {
      const String ssid = WiFi.SSID(i);
      if (!ssid.length()) continue;
      bool duplicate = false;
      for (int j = 0; j < wifiEntryCount; ++j) if (wifiEntries[j].ssid == ssid) duplicate = true;
      if (duplicate) continue;
      wifiEntries[wifiEntryCount++] = {ssid, WiFi.RSSI(i), WiFi.encryptionType(i) != WIFI_AUTH_OPEN,
                                       findWifiProfile(ssid) >= 0};
    }
  }
  WiFi.scanDelete();
  wifiListDirty = true;
  Serial.printf("[WIFI] scan found %d networks\n", wifiEntryCount);
}

static uint16_t backlightDuty(int brightness) {
  // New 0% matches the previous 15% setting; keep the existing low-end curve.
  const float level = (15.0f + constrain(brightness, 0, 100) * 0.85f) / 100.0f;
  const int onDuty = (int)lroundf(sqrtf(level) * 255.0f);
  return 255 - constrain(onDuty, 0, 255);
}

static void applyBrightness() {
  // Keep the idle dim level separate from the user brightness curve.
  setUpduty(screenDimmed ? 255 - (8 * 255 / 100) : backlightDuty(cfg.brightness));
}

static void sampleBattery() {
  uint32_t total = 0;
  for (int i = 0; i < 8; ++i) total += analogReadMilliVolts(kBatteryPin);
  batteryMv = (int)(total / 8) * 3;
  batteryPct = constrain((batteryMv - 3300) * 100 / 900, 0, 100);
  lastBatteryMs = millis();
}

static bool batteryPresent() {
  if (cfg.batteryMode == 2) return false;
  if (cfg.batteryMode == 1) return true;
  return batteryMv >= 2900;
}

static bool expanderWrite(uint8_t reg, uint8_t value) {
  const uint8_t data[2] = {reg, value};
  return i2c_master_transmit(expander_dev_handle, data, 2, 100) == ESP_OK;
}

static bool expanderUpdate(uint8_t reg, uint8_t setBits, uint8_t clearBits) {
  uint8_t value;
  if (i2c_master_transmit_receive(expander_dev_handle, &reg, 1, &value, 1, 100) != ESP_OK)
    return false;
  value = (value | setBits) & (uint8_t)~clearBits;
  if (!expanderWrite(reg, value)) return false;
  uint8_t actual;
  return i2c_master_transmit_receive(expander_dev_handle, &reg, 1, &actual, 1, 100) == ESP_OK &&
         actual == value;
}

static bool initPowerLatch() {
  return expanderUpdate(0x02, 0, 0xC0) &&
         expanderUpdate(0x01, 0x40, 0) && expanderUpdate(0x03, 0, 0x40);
}

static void pollPhysicalButton(PhysicalButton &key, uint32_t now, bool powerKey) {
  const bool pressed = digitalRead(key.pin) == LOW;
  const uint32_t touchMs = lastTouchMs.load();
  const bool touchRecent = touchMs && (uint32_t)(now - touchMs) < 500;
  if (pressed != key.rawPressed) {
    key.rawPressed = pressed;
    key.changedMs = now;
  }
  if (pressed == key.stablePressed) {
    if (pressed && touchRecent) key.touchedDuringPress = true;
    if (powerKey && pressed && key.armed && now - key.pressedMs >= kPowerHoldMs)
      powerOffRequested = true;
    return;
  }
  if (now - key.changedMs < kButtonDebounceMs) return;
  key.stablePressed = pressed;
  Serial.printf("[KEY] GPIO%d %s\n", key.pin, pressed ? "down" : "up");
  if (pressed) {
    key.touchedDuringPress = touchRecent;
    if (key.armed) key.pressedMs = now;
    return;
  }
  if (!key.armed) {
    key.armed = true; // Ignore a key held during boot.
    return;
  }
  if (!powerKey && !powerOffRequested && now - key.pressedMs < kSettingsClickMaxMs &&
      !key.touchedDuringPress && !touchRecent)
    settingsTogglePending = true;
  key.touchedDuringPress = false;
}

static void powerOff() {
  Serial.println("[POWER] releasing battery power latch");
  Serial.flush();
  setUpduty(255);
  WiFi.disconnect(true);
  if (!expanderUpdate(0x01, 0, 0x40)) {
    Serial.println("[POWER] latch write/readback failed; shutdown cancelled");
    powerOffRequested = false;
    powerReleasedMs = 0;
    applyBrightness();
    return;
  }
  // Battery-only supply turns off here. USB bypasses the battery switch.
  ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 1);
  gpio_set_direction(GPIO_NUM_8, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_8, 1);
  gpio_hold_en(GPIO_NUM_8);
  gpio_deep_sleep_hold_en();
  rtc_gpio_pulldown_dis(GPIO_NUM_16);
  rtc_gpio_pullup_en(GPIO_NUM_16);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_16, 0);
  Serial.println("[POWER] latch low verified; supply remains, entering USB sleep");
  Serial.flush();
  esp_deep_sleep_start();
}

static int codecVolumePercent(int uiVolume) {
  if (uiVolume <= 0) return 0;
  static constexpr int kUi[] = {1, 10, 25, 50, 75, 100};
  static constexpr int kCodec[] = {53, 60, 64, 70, 75, 100};
  for (size_t i = 1; i < sizeof(kUi) / sizeof(kUi[0]); ++i) {
    if (uiVolume <= kUi[i])
      return kCodec[i - 1] + (uiVolume - kUi[i - 1]) *
             (kCodec[i] - kCodec[i - 1]) / (kUi[i] - kUi[i - 1]);
  }
  return 100;
}

static bool writeCodecVolume(int uiVolume) {
  if (uiVolume <= 0) return es8311_voice_mute(codec, true) == ESP_OK;
  int actual = 0;
  if (es8311_voice_volume_set(codec, codecVolumePercent(uiVolume), &actual) != ESP_OK)
    return false;
  return es8311_voice_mute(codec, false) == ESP_OK;
}

static void initAudio() {
  // Configure only the audio pin; the power latch is initialized independently.
  if (!expanderUpdate(0x01, 0x80, 0) || !expanderUpdate(0x03, 0, 0x80)) return;
  audioI2s.setPins(15, 46, 45, 6, 7);
  if (!audioI2s.begin(I2S_MODE_STD, 24000, I2S_DATA_BIT_WIDTH_16BIT,
                      I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) return;
  codec = es8311_create(0, ES8311_ADDRESS_0);
  if (!codec) return;
  const es8311_clock_config_t clock = {
    .mclk_inverted = false, .sclk_inverted = false, .mclk_from_mclk_pin = true,
    .mclk_frequency = 24000 * 256, .sample_frequency = 24000
  };
  if (es8311_init(codec, &clock, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) return;
  (void)es8311_microphone_config(codec, false);
  if (!writeCodecVolume(cfg.volume)) return;
  appliedVolume = cfg.volume;
  audioReady = true;
  Serial.println("[AUDIO] ready");
}

static void audioTick() {
  if (!audioReady) return;
  const int playing = radioState.load();
  const int local = mp3State.load();
  if (playing == 1 || playing == 2 || local == 1 || local == 2 || local == 3) {
    beepPending.store(false);
    return;
  }
  if (audioMutex) xSemaphoreTake(audioMutex, portMAX_DELAY);
  if (appliedVolume != cfg.volume) {
    if (writeCodecVolume(cfg.volume)) appliedVolume = cfg.volume;
  }
  if (!beepPending.exchange(false) || cfg.volume == 0) {
    if (audioMutex) xSemaphoreGive(audioMutex);
    return;
  }
  static int16_t pcm[128 * 2];
  for (int block = 0; block < 10; ++block) {
    for (int i = 0; i < 128; ++i) {
      const int16_t sample = (i + block * 128) % 20 < 10 ? 9000 : -9000;
      pcm[i * 2] = pcm[i * 2 + 1] = sample;
    }
    (void)audioI2s.write((uint8_t *)pcm, sizeof(pcm));
  }
  if (audioMutex) xSemaphoreGive(audioMutex);
}

static bool setCodecRate(int hz) {
  const es8311_clock_config_t clock = {
    .mclk_inverted = false, .sclk_inverted = false, .mclk_from_mclk_pin = true,
    .mclk_frequency = hz * 256, .sample_frequency = hz
  };
  if (es8311_init(codec, &clock, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK)
    return false;
  if (!writeCodecVolume(cfg.volume)) return false;
  appliedVolume = cfg.volume;
  return true;
}

class RadioI2SOutput : public AudioOutputI2S {
 public:
  bool SetRate(int hz) override {
    if (!AudioOutputI2S::SetRate(hz)) return false;
    if (hz == codecRate) return true;
    if (!setCodecRate(hz)) {
      Serial.printf("[RADIO] codec rate failed: %d Hz\n", hz);
      return false;
    }
    codecRate = hz;
    updateEq(true);
    Serial.printf("[RADIO] sample rate: %d Hz\n", hz);
    return true;
  }
  bool ConsumeSample(int16_t sample[2]) override {
    if (--eqCheck == 0) {
      eqCheck = 256;
      updateEq(false);
    }
    int16_t filtered[2] = {sample[0], sample[1]};
    eq.process(filtered);
    return AudioOutputI2S::ConsumeSample(filtered);
  }
  bool codecConfigured() const { return codecRate > 0; }

 private:
  void updateEq(bool force) {
    const int bass = eqGain[0].load(std::memory_order_relaxed);
    const int mid = eqGain[1].load(std::memory_order_relaxed);
    const int treble = eqGain[2].load(std::memory_order_relaxed);
    if (!force && bass == appliedEq[0] && mid == appliedEq[1] &&
        treble == appliedEq[2]) return;
    eq.configure(codecRate, bass, mid, treble);
    appliedEq[0] = bass; appliedEq[1] = mid; appliedEq[2] = treble;
  }
  int codecRate = 0;
  uint16_t eqCheck = 256;
  int appliedEq[3] = {0, 0, 0};
  RadioEq eq;
};

class RadioAACDecoder : public AudioGeneratorAAC {
 public:
  RadioAACDecoder() {
    // HE-AAC SBR produces 2048 samples per channel. The library's default
    // 1024 * 2 buffer is only large enough for AAC-LC stereo.
    int16_t *samples = (int16_t *)heap_caps_malloc(2048 * 2 * sizeof(int16_t),
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (samples) {
      free(outSample);
      outSample = samples;
      outputSafe = true;
    }
  }
  bool ready() const { return outputSafe && buff && hAACDecoder; }

 private:
  bool outputSafe = false;
};

static void radioAudioTask(void *) {
  AudioFileSource *source = nullptr;
  BestRadioHlsSource *hls = nullptr;
  AudioFileSourceBuffer *buffer = nullptr;
  uint8_t *encodedBuffer = nullptr;
  AudioGenerator *decoder = nullptr;
  RadioI2SOutput *output = nullptr;
  bool driverSwitched = false;
  uint32_t lastAudioReport = 0;
  uint32_t playingSinceMs = 0, retryAtMs = 0;
  int retryStation = 0;
  uint8_t retryCount = 0;
  wifi_ps_type_t previousWifiSleep = WIFI_PS_MIN_MODEM;
  bool wifiSleepChanged = false;
  auto closeStream = [&]() {
    if (decoder) {
      if (decoder->isRunning()) decoder->stop();
      delete decoder;
      decoder = nullptr;
    }
    if (buffer) { delete buffer; buffer = nullptr; }
    if (encodedBuffer) { heap_caps_free(encodedBuffer); encodedBuffer = nullptr; }
    if (source) { delete source; source = nullptr; hls = nullptr; }
    if (output) { output->stop(); delete output; output = nullptr; }
  };
  auto restoreI2s = [&]() {
    if (!driverSwitched) return;
    xSemaphoreTake(audioMutex, portMAX_DELAY);
    audioI2s.setPins(15, 46, 45, 6, 7);
    audioReady = audioI2s.begin(I2S_MODE_STD, 24000, I2S_DATA_BIT_WIDTH_16BIT,
                                I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH);
    if (audioReady) (void)setCodecRate(24000);
    xSemaphoreGive(audioMutex);
    driverSwitched = false;
  };
  for (;;) {
    int command = radioCommand.exchange(-1);
    if (command == kMp3PauseCommand || command == kMp3ResumeCommand) {
      if (command == kMp3PauseCommand && mp3State.load() == 2) mp3State.store(3);
      if (command == kMp3ResumeCommand && mp3State.load() == 3) mp3State.store(2);
      command = -1;
    }
    bool automaticRetry = false;
    if (command == -1 && retryStation && WiFi.status() == WL_CONNECTED &&
        (int32_t)(millis() - retryAtMs) >= 0) {
      command = retryStation;
      retryStation = 0;
      automaticRetry = true;
      Serial.printf("[RADIO] retry station=%d attempt=%u\n", command, retryCount);
    }
    if (command != -1) {
      const bool mp3Requested = command == kMp3PlayCommand;
      const bool wasMp3 = mp3State.load() != 0;
      if (command == 0 || mp3Requested || (command > 0 && !automaticRetry)) {
        retryStation = 0;
        retryCount = 0;
      }
      radioState.store(mp3Requested ? 0 : command > 0 ? 1 : command == -2 ? 3 : 0);
      mp3State.store(mp3Requested ? 1 : 0);
      closeStream();
      const bool canStartRadio = audioReady || driverSwitched;
      if (command <= 0) restoreI2s();
      if (command > 0 && !mp3Requested && !wifiSleepChanged) {
        previousWifiSleep = WiFi.getSleep();
        wifiSleepChanged = WiFi.setSleep(false);
      } else if ((command <= 0 || mp3Requested) && wifiSleepChanged) {
        WiFi.setSleep(previousWifiSleep);
        wifiSleepChanged = false;
      }
      if (command <= 0) {
        Serial.println(command == 0 && wasMp3 ? "[MP3] stopped" :
                       command == 0 ? "[RADIO] stopped" : "[RADIO] stopped after error");
        if (command == -2 && retryCount < 3 && radioStation.load() > 0) {
          static constexpr uint32_t kRetryDelaysMs[] = {3000, 10000, 30000};
          retryStation = radioStation.load();
          retryAtMs = millis() + kRetryDelaysMs[retryCount++];
          Serial.printf("[RADIO] reconnect in %lu seconds\n",
                        (unsigned long)(kRetryDelaysMs[retryCount - 1] / 1000));
        }
      } else if (mp3Requested) {
        char path[sizeof(mp3RequestedPath)] = {};
        if (mp3RequestMutex && xSemaphoreTake(mp3RequestMutex, portMAX_DELAY) == pdTRUE) {
          strlcpy(path, mp3RequestedPath, sizeof(path));
          xSemaphoreGive(mp3RequestMutex);
        }
        if (canStartRadio && deskFontSdMounted() && path[0]) {
          if (!driverSwitched) {
            xSemaphoreTake(audioMutex, portMAX_DELAY);
            audioReady = false;
            audioI2s.end();
            driverSwitched = true;
            xSemaphoreGive(audioMutex);
          }
          source = new AudioFileSourceFS(SD_MMC, path);
          if (source && source->isOpen()) {
            output = new RadioI2SOutput();
            decoder = new AudioGeneratorMP3();
            if (output && decoder && output->SetBuffers(8, 2304) &&
                output->SetPinout(15, 46, 45, 7) &&
                decoder->begin(source, output) && output->codecConfigured()) {
              mp3Progress.store(0);
              mp3State.store(2);
              Serial.printf("[MP3] playing %s\n", path);
            }
          }
        }
        if (mp3State.load() != 2) {
          closeStream();
          restoreI2s();
          mp3State.store(4);
          Serial.printf("[MP3] open/decode failed %s\n", path);
        }
      } else if (WiFi.status() != WL_CONNECTED || !canStartRadio) {
        radioState.store(3);
        Serial.println("[RADIO] unavailable: Wi-Fi or audio");
        int expected = -1;
        radioCommand.compare_exchange_strong(expected, -2);
      } else {
        String url;
        if (command >= 6) {
          // Official stations resolve their expiring HLS URL in the producer.
        } else if (command != 3) url = kRadioUrls[command - 1];
        else {
          xSemaphoreTake(configMutex, portMAX_DELAY);
          url = cfg.customRadioUrl;
          xSemaphoreGive(configMutex);
        }
        if (command < 6 && !url.startsWith("http://")) {
          radioState.store(3);
          Serial.println("[RADIO] HTTP audio URL required");
        } else {
          radioStation.store(command);
          if (!driverSwitched) {
            xSemaphoreTake(audioMutex, portMAX_DELAY);
            audioReady = false;
            audioI2s.end();
            driverSwitched = true;
            xSemaphoreGive(audioMutex);
          }
          bool opened = false;
          if (command >= 6) {
            hls = new BestRadioHlsSource();
            source = hls;
            opened = hls && hls->begin(command - 6, networkHttpMutex);
          } else {
            auto *icy = new AudioFileSourceICYStream();
            source = icy;
            if (icy) { icy->SetReconnect(3, 500); opened = icy->open(url.c_str()); }
          }
          if (opened) {
            if (!hls) {
              encodedBuffer = (uint8_t *)heap_caps_malloc(kRadioBufferBytes,
                                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
              if (encodedBuffer) buffer = new AudioFileSourceBuffer(source, encodedBuffer,
                                                                    kRadioBufferBytes);
            }
            if (buffer) {
              // A zero-length read initializes this library's ring buffer without
              // consuming MP3 bytes. Wait for a cushion before starting I2S.
              uint8_t unused = 0;
              (void)buffer->read(&unused, 0);
              const uint32_t start = millis();
               while (buffer->getFillLevel() < 98304 &&
                      (uint32_t)(millis() - start) < 12000 &&
                     WiFi.status() == WL_CONNECTED && radioCommand.load() == -1) {
                if (!buffer->loop()) break;
                vTaskDelay(pdMS_TO_TICKS(10));
              }
              Serial.printf("[RADIO] prebuffer=%u bytes\n",
                            (unsigned)buffer->getFillLevel());
            } else if (hls) {
              const uint32_t start = millis();
              while (hls->buffered() < 32768 && !hls->failed() &&
                     (uint32_t)(millis() - start) < 20000 &&
                     WiFi.status() == WL_CONNECTED && radioCommand.load() == -1)
                vTaskDelay(pdMS_TO_TICKS(10));
              Serial.printf("[HLS] prebuffer=%u bytes\n", (unsigned)hls->buffered());
            }
            output = new RadioI2SOutput();
            if (command >= 4) {
              RadioAACDecoder *aac = new RadioAACDecoder();
              if (aac && aac->ready()) decoder = aac;
              else {
                delete aac;
                Serial.println("[RADIO] AAC decoder memory unavailable");
              }
            } else decoder = new AudioGeneratorMP3();
            AudioFileSource *audioSource = hls ? source : static_cast<AudioFileSource *>(buffer);
            const bool prebuffered = hls ? hls->buffered() >= 32768 :
                                      buffer && buffer->getFillLevel() >= 65536;
            if (audioSource && prebuffered &&
                radioCommand.load() == -1 && output && decoder &&
                output->SetBuffers(8, 2304) &&
                output->SetPinout(15, 46, 45, 7) &&
                decoder->begin(audioSource, output) && output->codecConfigured()) {
              radioState.store(2);
              playingSinceMs = millis();
              Serial.printf("[RADIO] playing station=%d\n", command);
            } else radioState.store(3);
          } else radioState.store(3);
          if (radioState.load() == 3) {
            Serial.printf("[RADIO] stream failed station=%d\n", command);
            int expected = -1;
            radioCommand.compare_exchange_strong(expected, -2);
          }
        }
      }
    }
    if (radioState.load() == 2 && decoder) {
      const uint32_t audioNow = millis();
      if ((uint32_t)(audioNow - lastAudioReport) >= 5000) {
        lastAudioReport = audioNow;
        Serial.printf("[RADIO] fill=%u wifi=%d heap=%u largest=%u min=%u psram=%u stack=%u\n",
                      hls ? (unsigned)hls->buffered() :
                            buffer ? (unsigned)buffer->getFillLevel() : 0,
                      WiFi.RSSI(),
                      (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                      (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                      (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                      (unsigned)uxTaskGetStackHighWaterMark(nullptr));
      }
      if (appliedVolume != cfg.volume) {
        if (writeCodecVolume(cfg.volume)) appliedVolume = cfg.volume;
      }
      if (WiFi.status() != WL_CONNECTED || !decoder->isRunning() || !decoder->loop()) {
        Serial.println("[RADIO] stream ended");
        if ((uint32_t)(millis() - playingSinceMs) >= 60000) retryCount = 0;
        int expected = -1;
        radioCommand.compare_exchange_strong(expected, -2);
        radioState.store(3);
      }
    }
    if (mp3State.load() == 2 && decoder && source) {
      const uint32_t size = source->getSize();
      if (size) mp3Progress.store((int)std::min((uint64_t)1000,
                                                   (uint64_t)source->getPos() * 1000 / size));
      if (appliedVolume != cfg.volume) {
        if (writeCodecVolume(cfg.volume)) appliedVolume = cfg.volume;
      }
      if (!decoder->isRunning() || !decoder->loop()) {
        Serial.println("[MP3] playback ended");
        closeStream();
        restoreI2s();
        mp3State.store(5);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

static void buttonCue(lv_event_t *) { beepPending.store(true); lastInputMs = millis(); }
static void noAction(lv_event_t *) {}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int w, int size = 14) {
  lv_obj_t *o = lv_label_create(parent);
  lv_label_set_text(o, text);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_width(o, w);
  lv_label_set_long_mode(o, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(o, lv_color_hex(0xF3F5F2), 0);
  lv_obj_set_style_text_font(o, size >= 80 ? &desk_font_86 : size >= 60 ? &desk_font_70 :
                                 size >= 20 ? &desk_font_20 : &desk_font_16, 0);
  return o;
}

static lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                        lv_event_cb_t callback) {
  lv_obj_t *o = lv_button_create(parent);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, 6, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(0x202824), 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(0x344C40), LV_STATE_PRESSED);
  lv_obj_set_style_border_width(o, 1, 0);
  lv_obj_set_style_border_color(o, lv_color_hex(0x34433B), 0);
  lv_obj_set_style_shadow_width(o, 0, 0);
  lv_obj_add_event_cb(o, buttonCue, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(o, callback, LV_EVENT_PRESSED, nullptr);
  lv_obj_t *l = label(o, text, 0, 0, w - 8);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(l);
  return o;
}

static void goHome(lv_event_t *) {
  lv_screen_load(homeScreen); lastInputMs = millis(); Serial.println("[UI] home");
}
static void goSettings(lv_event_t *) {
  lv_screen_load(settingsScreen); lastInputMs = millis(); Serial.println("[UI] settings");
}

static uint32_t stockChangeColor(const String &change) {
  if (change.isEmpty()) return 0xF3F5F2;
  char *end = nullptr;
  const float amount = strtof(change.c_str(), &end);
  if (end == change.c_str()) return 0xF3F5F2;
  return amount > 0 ? 0xF27873 : amount < 0 ? 0x7AD44A : 0xF2C64B;
}

static void splitStockLine(const String &value, uint8_t index,
                           String &price, String &change, String &stamp) {
  price = value;
  change = "";
  stamp = "";
  if (index == 0) return;
  const int first = value.indexOf("  ");
  if (first < 0) return;
  if (index == 1) {
    price = value.substring(0, first);
    change = value.substring(first + 2);
  } else if (index == 2) {
    price = value.substring(0, first);
    const int second = value.indexOf("  ", first + 2);
    if (second >= 0) {
      change = value.substring(first + 2, second);
      stamp = value.substring(second + 2);
    } else {
      stamp = value.substring(first + 2);
    }
  } else {
    const int second = value.indexOf("  ", first + 2);
    if (second >= 0) {
      price = value.substring(0, second);
      change = value.substring(second + 2);
    }
  }
}

#include "home_ui.inc"

static void updateStockScreen() {
  if (!stockScreen) return;
  const uint8_t pages = (stockCount + 2) / 3;
  if (stockPage >= pages) stockPage = pages - 1;
  static lv_obj_t *renderedScreen = nullptr;
  static uint8_t renderedPage = 0xff;
  static uint32_t renderedRevision = UINT32_MAX;
  if (renderedScreen == stockScreen && renderedPage == stockPage &&
      renderedRevision == stockUiRevision) return;
  renderedScreen = stockScreen;
  renderedPage = stockPage;
  renderedRevision = stockUiRevision;
  lv_label_set_text_fmt(stockPageLabel, "%u / %u", stockPage + 1, pages);
  for (uint8_t card = 0; card < 3; ++card) {
    const uint8_t slot = stockPage * 3 + card;
    lv_obj_t *panel = stockCard[card];
    if (slot >= stockCount) {
      lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_HIDDEN);
    String price, change, stamp;
    splitStockLine(stockValues[slot], 3, price, change, stamp);
    if (price.startsWith(stockCodes[slot] + "  "))
      price.remove(0, stockCodes[slot].length() + 2);
    const String name = stockDisplayName(slot);
    lv_label_set_text(lv_obj_get_child(panel, 0), name.c_str());
    lv_label_set_text(lv_obj_get_child(panel, 1), price.c_str());
    lv_label_set_text(lv_obj_get_child(panel, 2), change.c_str());
    lv_obj_set_style_text_color(lv_obj_get_child(panel, 2),
                                lv_color_hex(stockChangeColor(change)), 0);
    lv_label_set_text(lv_obj_get_child(panel, 3), stockStamps[slot].c_str());
  }
}

static void prevStocks(lv_event_t *) {
  if (stockPage) --stockPage;
  updateStockScreen();
}
static void nextStocks(lv_event_t *) {
  if ((stockPage + 1) * 3 < stockCount) ++stockPage;
  updateStockScreen();
}
static void openStocks(lv_event_t *) {
  stockReturnScreen = lv_screen_active();
  updateStockScreen();
  lv_screen_load(stockScreen);
  lastInputMs = millis();
}
static void backFromStocks(lv_event_t *) {
  lv_screen_load(stockReturnScreen ? stockReturnScreen : homeScreen);
  lastInputMs = millis();
}

static void buildStockScreen() {
  stockScreen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(stockScreen, lv_color_hex(0x080B0B), 0);
  lv_obj_set_style_pad_all(stockScreen, 0, 0);
  lv_obj_clear_flag(stockScreen, LV_OBJ_FLAG_SCROLLABLE);
  button(stockScreen, LV_SYMBOL_LEFT, 15, 9, 44, 36, backFromStocks);
  label(stockScreen, "自選個股", 72, 13, 148, 20);
  label(stockScreen, "收盤資料", 292, 15, 92);
  button(stockScreen, "編輯", 402, 9, 85, 36, editStock);
  stockPageLabel = label(stockScreen, "1 / 1", 510, 15, 48);
  button(stockScreen, LV_SYMBOL_LEFT, 558, 9, 31, 36, prevStocks);
  button(stockScreen, LV_SYMBOL_RIGHT, 596, 9, 31, 36, nextStocks);
  for (uint8_t i = 0; i < 3; ++i) {
    lv_obj_t *panel = lv_obj_create(stockScreen);
    stockCard[i] = panel;
    lv_obj_set_pos(panel, 18 + i * 205, 54);
    lv_obj_set_size(panel, 194, 94);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x141C19), 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x34433B), 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    label(panel, "", 13, 7, 178);
    label(panel, "", 13, 36, 108, 20);
    label(panel, "", 119, 38, 68);
    label(panel, "", 13, 70, 166);
  }
  label(stockScreen, "證交所最新收盤資料 · 長按首頁股市可進入此頁", 18, 150, 580);
  updateStockScreen();
}

static void updateRadioScreen() {
  if (!radioStatusLabel) return;
  const int state = radioState.load();
  const int station = radioStation.load();
  static int renderedState = -1, renderedStation = -1;
  if (state == renderedState && station == renderedStation) return;
  renderedState = state;
  renderedStation = station;
  const char *status = state == 1 ? "連線中" : state == 2 ? "播放中" :
                       state == 3 ? "連線失敗" : "已停止";
  lv_label_set_text(radioStatusLabel, status);
  lv_label_set_text(radioNowLabel, station ? kRadioNames[station - 1] : "選擇電台");
  lv_label_set_text(lv_obj_get_child(radioPlayButton, 0),
                    state == 1 || state == 2 ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
  for (int i = 0; i < kRadioStations; ++i)
    lv_obj_set_style_bg_color(radioStationButtons[i],
                              lv_color_hex(station == i + 1 ? 0x244038 : 0x202824), 0);
}

static void selectRadio(lv_event_t *event) {
  const int station = (int)(uintptr_t)lv_event_get_user_data(event) + 1;
  if (station == 3 && cfg.customRadioUrl.isEmpty()) {
    editRadioUrl(nullptr);
    return;
  }
  if (station == radioStation.load() && radioState.load() <= 2 &&
      radioState.load() != 0) return;
  radioStation.store(station);
  radioCommand.store(station);
  radioState.store(1);
  updateRadioScreen();
  updateHomeRadio();
}
static void toggleRadio(lv_event_t *) {
  if (radioState.load() == 1 || radioState.load() == 2) {
    radioCommand.store(0);
    radioState.store(0);
  }
  else {
    const int station = radioStation.load() ? radioStation.load() : 1;
    radioStation.store(station);
    radioCommand.store(station);
    radioState.store(1);
  }
  updateRadioScreen();
  updateHomeRadio();
}
static void stepRadio(lv_event_t *event) {
  const int delta = (int)(intptr_t)lv_event_get_user_data(event);
  int station = radioStation.load() + delta;
  if (station < 1) station = kRadioStations;
  if (station > kRadioStations) station = 1;
  if (station == 3 && cfg.customRadioUrl.isEmpty()) station += delta;
  if (station < 1) station = kRadioStations;
  if (station > kRadioStations) station = 1;
  radioStation.store(station);
  radioCommand.store(station);
  radioState.store(1);
  updateRadioScreen();
  updateHomeRadio();
}
static void openRadio(lv_event_t *) {
  selectSettingsPage(kRadioSettingsPage);
  lv_screen_load(settingsScreen);
  updateRadioScreen();
  lastInputMs = millis();
}
static void buildRadioSettings(lv_obj_t *radio) {
  for (uint8_t i = 0; i < kRadioStations; ++i) {
    radioStationButtons[i] = button(radio, kRadioButtonNames[i], 20 + (i % 3) * 112,
                                      5 + (i / 3) * 40, 104, 34, noAction);
    lv_obj_add_event_cb(radioStationButtons[i], selectRadio, LV_EVENT_PRESSED,
                        (void *)(uintptr_t)i);
  }
  lv_obj_t *rule = lv_obj_create(radio);
  lv_obj_set_pos(rule, 369, 9); lv_obj_set_size(rule, 1, 108);
  lv_obj_set_style_pad_all(rule, 0, 0);
  lv_obj_set_style_border_width(rule, 0, 0);
  lv_obj_set_style_bg_color(rule, lv_color_hex(0x32403B), 0);
  label(radio, "網路電台", 389, 8, 124);
  radioStatusLabel = label(radio, "已停止", 518, 8, 102);
  lv_obj_set_style_text_align(radioStatusLabel, LV_TEXT_ALIGN_RIGHT, 0);
  radioNowLabel = label(radio, "選擇電台", 389, 40, 231, 20);
  lv_obj_t *prev = button(radio, LV_SYMBOL_LEFT, 389, 79, 53, 40, noAction);
  lv_obj_add_event_cb(prev, stepRadio, LV_EVENT_PRESSED, (void *)(intptr_t)-1);
  radioPlayButton = button(radio, LV_SYMBOL_PLAY, 452, 79, 105, 40, toggleRadio);
  lv_obj_t *next = button(radio, LV_SYMBOL_RIGHT, 567, 79, 53, 40, noAction);
  lv_obj_add_event_cb(next, stepRadio, LV_EVENT_PRESSED, (void *)(intptr_t)1);
  updateRadioScreen();
}

static void saveField(const String &value);

static void closeEditor() {
  lv_screen_load(editorReturnScreen ? editorReturnScreen : settingsScreen);
  lv_obj_delete_async(editorScreen);
  editorScreen = nullptr;
  lastInputMs = millis();
}

static void toggleSettingsFromButton() {
  if (!settingsTogglePending || !lvgl_port_lock(100)) return;
  settingsTogglePending = false;
  if (lv_screen_active() == settingsScreen) goHome(nullptr);
  else {
    if (editorScreen && lv_screen_active() == editorScreen) closeEditor();
    goSettings(nullptr);
  }
  lvgl_port_unlock();
}
static void cancelEdit(lv_event_t *) { closeEditor(); }
static void acceptEdit(lv_event_t *) {
  saveField(lv_textarea_get_text(editorText));
  closeEditor();
}
enum class KeyboardMode { English, Number, Symbol };
static KeyboardMode keyboardMode = KeyboardMode::English;
static bool keyboardUpper = false;
static const char *const kLowerRow1[] = {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"};
static const char *const kLowerRow2[] = {"a", "s", "d", "f", "g", "h", "j", "k", "l"};
static const char *const kLowerRow3[] = {"z", "x", "c", "v", "b", "n", "m"};
static const char *const kUpperRow1[] = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"};
static const char *const kUpperRow2[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L"};
static const char *const kUpperRow3[] = {"Z", "X", "C", "V", "B", "N", "M"};

static void keyPressed(lv_event_t *event) {
  lv_obj_t *keyObj = (lv_obj_t *)lv_event_get_target(event);
  lv_indev_t *indev = lv_indev_active();
  if (indev) {
    lv_point_t point;
    lv_area_t bounds;
    lv_indev_get_point(indev, &point);
    lv_obj_get_coords(keyObj, &bounds);
    if (point.x < bounds.x1 + 2 || point.x > bounds.x2 - 2 ||
        point.y < bounds.y1 + 2 || point.y > bounds.y2 - 2) return;
  }
  const char *key = static_cast<const char *>(lv_event_get_user_data(event));
  if (key[0] == '\b') lv_textarea_delete_char(editorText);
  else if (key[0] == '\x01') lv_textarea_cursor_left(editorText);
  else if (key[0] == '\x02') lv_textarea_cursor_right(editorText);
  else lv_textarea_add_text(editorText, key);
  beepPending.store(true);
  lastInputMs = millis();
}

static void addKey(const char *caption, const char *value, int x, int y, int w, int h) {
  lv_obj_t *key = lv_button_create(editorKeyboard);
  lv_obj_set_pos(key, x, y);
  lv_obj_set_size(key, w, h);
  lv_obj_set_style_pad_all(key, 0, 0);
  lv_obj_set_style_border_width(key, 0, 0);
  lv_obj_set_style_radius(key, 4, 0);
  lv_obj_set_style_shadow_width(key, 0, 0);
  lv_obj_set_style_transform_width(key, 0, LV_STATE_PRESSED);
  lv_obj_set_style_transform_height(key, 0, LV_STATE_PRESSED);
  lv_obj_set_ext_click_area(key, 0);
  lv_obj_set_style_bg_color(key, lv_color_hex(0x202824), 0);
  lv_obj_set_style_bg_color(key, lv_color_hex(0x344C40), LV_STATE_PRESSED);
  lv_obj_t *captionLabel = label(key, caption, 0, 0, w - 4);
  lv_obj_set_style_text_align(captionLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(captionLabel);
  lv_obj_add_event_cb(key, keyPressed, LV_EVENT_PRESSED, (void *)value);
}

static void renderKeyboard() {
  lv_obj_clean(editorKeyboard);
  if (keyboardMode == KeyboardMode::Number) {
    static const char *const row1[] = {"1", "2", "3", "4", "5"};
    static const char *const row2[] = {"6", "7", "8", "9", "0"};
    static const char *const row3[] = {"-", ".", "/", ",", " ", "\x01", "\x02"};
    static const char *const captions[] = {"-", ".", "/", ",", "空白", LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT};
    for (int i = 0; i < 5; ++i) {
      addKey(row1[i], row1[i], 10 + i * 104, 2, 92, 36);
      addKey(row2[i], row2[i], 10 + i * 104, 44, 92, 36);
    }
    addKey(LV_SYMBOL_BACKSPACE, "\b", 530, 2, 92, 36);
    addKey(":", ":", 530, 44, 92, 36);
    for (int i = 0; i < 7; ++i) addKey(captions[i], row3[i], 10 + i * 88, 86, 80, 36);
  } else if (keyboardMode == KeyboardMode::Symbol) {
    static const char *const row1[] = {"!", "@", "#", "$", "%", "^", "&", "*", "(", ")"};
    static const char *const row2[] = {"-", "_", "=", "+", "[", "]", "{", "}", "\\", "|"};
    static const char *const row3[] = {";", ":", "'", "\"", ",", ".", "<", ">", "?", "/"};
    for (int i = 0; i < 10; ++i) {
      addKey(row1[i], row1[i], 10 + i * 62, 1, 54, 26);
      addKey(row2[i], row2[i], 10 + i * 62, 33, 54, 26);
      addKey(row3[i], row3[i], 10 + i * 62, 65, 54, 26);
    }
    addKey(LV_SYMBOL_BACKSPACE, "\b", 10, 97, 80, 26);
    addKey("空白", " ", 98, 97, 340, 26);
    addKey(LV_SYMBOL_LEFT, "\x01", 446, 97, 50, 26);
    addKey(LV_SYMBOL_RIGHT, "\x02", 504, 97, 50, 26);
    addKey("~", "~", 562, 97, 60, 26);
  } else {
    const char *const *row1 = keyboardUpper ? kUpperRow1 : kLowerRow1;
    const char *const *row2 = keyboardUpper ? kUpperRow2 : kLowerRow2;
    const char *const *row3 = keyboardUpper ? kUpperRow3 : kLowerRow3;
    for (int i = 0; i < 10; ++i) addKey(row1[i], row1[i], 10 + i * 62, 1, 54, 26);
    for (int i = 0; i < 9; ++i) addKey(row2[i], row2[i], 10 + i * 62, 33, 54, 26);
    addKey(LV_SYMBOL_BACKSPACE, "\b", 568, 33, 54, 26);
    for (int i = 0; i < 7; ++i) addKey(row3[i], row3[i], 72 + i * 62, 65, 54, 26);
    addKey("@", "@", 506, 65, 54, 26);
    addKey("_", "_", 568, 65, 54, 26);
    addKey("-", "-", 10, 97, 70, 26);
    addKey(".", ".", 88, 97, 70, 26);
    addKey("/", "/", 166, 97, 70, 26);
    addKey("空白", " ", 244, 97, 172, 26);
    addKey(LV_SYMBOL_LEFT, "\x01", 424, 97, 62, 26);
    addKey(LV_SYMBOL_RIGHT, "\x02", 494, 97, 62, 26);
    addKey(":", ":", 564, 97, 62, 26);
  }
}

static void keyboardEnglish(lv_event_t *) { keyboardMode = KeyboardMode::English; renderKeyboard(); }
static void keyboardNumber(lv_event_t *) { keyboardMode = KeyboardMode::Number; renderKeyboard(); }
static void keyboardSymbol(lv_event_t *) { keyboardMode = KeyboardMode::Symbol; renderKeyboard(); }
static void keyboardCase(lv_event_t *) {
  keyboardMode = KeyboardMode::English;
  keyboardUpper = !keyboardUpper;
  renderKeyboard();
}

static lv_obj_t *editorButton(const char *text, int x, int w, lv_event_cb_t callback) {
  lv_obj_t *action = button(editorScreen, text, x, 4, w, 34, callback);
  lv_obj_set_style_transform_width(action, 0, LV_STATE_PRESSED);
  lv_obj_set_style_transform_height(action, 0, LV_STATE_PRESSED);
  lv_obj_set_ext_click_area(action, 0);
  return action;
}

static void openEditor(EditField field, const char *title, const String &initial, bool secret = false) {
  editField = field;
  editorReturnScreen = lv_screen_active();
  editorScreen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(editorScreen, lv_color_hex(0x080B0B), 0);
  lv_obj_set_style_text_font(editorScreen, &desk_font_16, 0);
  lv_obj_set_style_pad_all(editorScreen, 0, 0);
  lv_obj_set_style_border_width(editorScreen, 0, 0);
  lv_obj_clear_flag(editorScreen, LV_OBJ_FLAG_SCROLLABLE);
  if (field == EditField::DateTime) {
    editorButton("取消", 438, 90, cancelEdit);
    editorButton("儲存", 536, 96, acceptEdit);
  } else {
    editorButton("英文", 254, 60, keyboardEnglish);
    editorButton("數字", 322, 60, keyboardNumber);
    editorButton("符號", 390, 60, keyboardSymbol);
    editorButton("A/a", 458, 48, keyboardCase);
    editorButton("取消", 514, 52, cancelEdit);
    editorButton("儲存", 574, 58, acceptEdit);
  }
  editorText = lv_textarea_create(editorScreen);
  lv_obj_set_pos(editorText, 8, 4);
  lv_obj_set_size(editorText, field == EditField::DateTime ? 420 : 236, 34);
  lv_textarea_set_one_line(editorText, true);
  lv_textarea_set_placeholder_text(editorText, title);
  lv_obj_set_style_pad_top(editorText, 3, 0);
  lv_obj_set_style_pad_bottom(editorText, 3, 0);
  lv_obj_set_style_pad_left(editorText, 8, 0);
  lv_obj_set_style_pad_right(editorText, 8, 0);
  lv_obj_set_style_border_width(editorText, 1, 0);
  lv_obj_set_style_outline_width(editorText, 0, 0);
  lv_obj_set_style_shadow_width(editorText, 0, 0);
  lv_obj_set_style_bg_color(editorText, lv_color_hex(0x202824), 0);
  lv_obj_set_style_text_color(editorText, lv_color_hex(0xF3F5F2), 0);
  lv_obj_set_style_border_color(editorText, lv_color_hex(0x83D7BB), 0);
  lv_textarea_set_password_mode(editorText, secret && field != EditField::Password);
  lv_textarea_set_text(editorText, initial.c_str());
  editorKeyboard = lv_obj_create(editorScreen);
  lv_obj_set_pos(editorKeyboard, 0, 46);
  lv_obj_set_size(editorKeyboard, 640, 126);
  lv_obj_set_style_pad_all(editorKeyboard, 0, 0);
  lv_obj_set_style_border_width(editorKeyboard, 0, 0);
  lv_obj_set_style_bg_opa(editorKeyboard, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(editorKeyboard, LV_OBJ_FLAG_SCROLLABLE);
  keyboardMode = field == EditField::Stock || field == EditField::DateTime ?
                 KeyboardMode::Number : KeyboardMode::English;
  keyboardUpper = false;
  renderKeyboard();
  lv_screen_load(editorScreen);
  lastInputMs = millis();
}

static void editSsid(lv_event_t *) { openEditor(EditField::Ssid, "Wi-Fi 名稱", cfg.ssid); }
static void editPassword(lv_event_t *) { openEditor(EditField::Password, "Wi-Fi 密碼", cfg.password, true); }
static void editStock(lv_event_t *) { openEditor(EditField::Stock, "股票代號，逗號分隔", cfg.stockCode); }
static void editRadioUrl(lv_event_t *) {
  openEditor(EditField::RadioUrl, "自訂電台 HTTP MP3 網址", cfg.customRadioUrl);
}
static void editDateTime(lv_event_t *) {
  tm value; char buf[24] = "2026-01-01 12:00";
  if (getLocalTime(&value, 50)) strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &value);
  openEditor(EditField::DateTime, "日期時間 YYYY-MM-DD HH:MM", buf);
}

static void connectWifi(lv_event_t *) {
  networkRefresh = true;
  const int index = findWifiProfile(selectedWifiSsid.length() ? selectedWifiSsid : cfg.ssid);
  if (index >= 0 && wifiBadSsid == wifiProfiles[index].ssid)
    openEditor(EditField::Password, "密碼錯誤", wifiProfiles[index].password);
  else if (index >= 0) startWifiProfile(index);
  else if (cfg.ssid.length()) openEditor(EditField::Password, "Wi-Fi 密碼", "", true);
  lastInputMs = millis();
}

static void wifiEntryClicked(lv_event_t *event) {
  const int index = (int)(intptr_t)lv_event_get_user_data(event);
  if (index < 0 || index >= wifiEntryCount) return;
  selectedWifiSsid = wifiEntries[index].ssid;
  const int saved = findWifiProfile(selectedWifiSsid);
  if (saved >= 0) {
    cfg.ssid = selectedWifiSsid;
    cfg.password = wifiProfiles[saved].password;
    if (wifiBadSsid == selectedWifiSsid)
      openEditor(EditField::Password, "密碼錯誤", cfg.password);
    else startWifiProfile(saved);
  } else if (!wifiEntries[index].secured) {
    startWifiCandidate(selectedWifiSsid, "");
  } else {
    cfg.ssid = selectedWifiSsid;
    cfg.password = "";
    openEditor(EditField::Password, "Wi-Fi 密碼", "", true);
  }
}

static void wifiPriorityUp(lv_event_t *) {
  const int index = findWifiProfile(selectedWifiSsid);
  if (index <= 0) return;
  WifiProfile saved = wifiProfiles[index - 1];
  wifiProfiles[index - 1] = wifiProfiles[index];
  wifiProfiles[index] = saved;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  persistWifiProfiles();
  xSemaphoreGive(configMutex);
  wifiListDirty = true;
}

static void wifiForget(lv_event_t *) {
  const int index = findWifiProfile(selectedWifiSsid);
  if (index < 0) return;
  if (WiFi.SSID() == selectedWifiSsid) WiFi.disconnect();
  for (int i = index; i + 1 < wifiProfileCount; ++i) wifiProfiles[i] = wifiProfiles[i + 1];
  --wifiProfileCount;
  if (wifiAttempt >= wifiProfileCount) wifiAttempt = -1;
  if (!wifiProfileCount) {
    wifiHold = false;
    wifiRetryAtMs = 0;
    wifiStatusError = 0;
  }
  xSemaphoreTake(configMutex, portMAX_DELAY);
  persistWifiProfiles();
  xSemaphoreGive(configMutex);
  selectedWifiSsid = "";
  wifiListDirty = true;
}

static void rebuildWifiList() {
  if (!wifiList) return;
  lv_obj_clean(wifiList);
  WifiEntry shown[kMaxWifiEntries];
  int count = 0;
  for (int i = 0; i < wifiProfileCount && count < kMaxWifiEntries; ++i)
    shown[count++] = {wifiProfiles[i].ssid, 0, wifiProfiles[i].password.length() > 0, true};
  for (int i = 0; i < wifiEntryCount && count < kMaxWifiEntries; ++i) {
    bool duplicate = false;
    for (int j = 0; j < count; ++j) if (shown[j].ssid == wifiEntries[i].ssid) duplicate = true;
    if (!duplicate) {
      shown[count] = wifiEntries[i];
      shown[count].saved = findWifiProfile(shown[count].ssid) >= 0;
      ++count;
    }
  }
  wifiEntryCount = count;
  for (int i = 0; i < count; ++i) wifiEntries[i] = shown[i];
  for (int i = 0; i < count; ++i) {
    String ssid = wifiEntries[i].saved ? String(i + 1) + ". " + wifiEntries[i].ssid : wifiEntries[i].ssid;
    const char *status = wifiEntries[i].saved ? "已記憶" : wifiEntries[i].secured ? "需密碼" : "開放";
    lv_obj_t *row = lv_button_create(wifiList);
    lv_obj_set_pos(row, 0, i * 32);
    lv_obj_set_size(row, 600, 30);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_radius(row, 3, 0);
    lv_obj_set_style_shadow_width(row, 0, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(i % 2 ? 0x202824 : 0x141B18), 0);
    label(row, ssid.c_str(), 9, 5, 470);
    lv_obj_t *statusLabel = label(row, status, 505, 5, 84);
    lv_obj_set_style_text_align(statusLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_add_event_cb(row, buttonCue, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(row, wifiEntryClicked, LV_EVENT_PRESSED, (void *)(intptr_t)i);
  }
  wifiListDirty = false;
}

static void changeBrightness(lv_event_t *event) {
  const int delta = (int)(intptr_t)lv_event_get_user_data(event);
  const int next = constrain(delta > 0 ? (cfg.brightness / 5 + 1) * 5 :
                             ((cfg.brightness - 1) / 5) * 5, 0, 100);
  if (next == cfg.brightness) return;
  cfg.brightness = next;
  if (brightnessValueLabel) lv_label_set_text_fmt(brightnessValueLabel, "%d%%", cfg.brightness);
  xSemaphoreTake(configMutex, portMAX_DELAY);
  prefs.putInt("bright_ui_v2", cfg.brightness);
  xSemaphoreGive(configMutex);
  applyBrightness();
  lastInputMs = millis();
}
static void changeSleep(lv_event_t *event) {
  const int delta = (int)(intptr_t)lv_event_get_user_data(event);
  const int next = constrain(delta > 0 ? (cfg.sleepMinutes / 5 + 1) * 5 :
                             ((cfg.sleepMinutes - 1) / 5) * 5, 0, 60);
  if (next == cfg.sleepMinutes) return;
  cfg.sleepMinutes = next;
  if (sleepValueLabel) lv_label_set_text_fmt(sleepValueLabel, "%d 分", cfg.sleepMinutes);
  xSemaphoreTake(configMutex, portMAX_DELAY);
  prefs.putInt("sleep", cfg.sleepMinutes);
  xSemaphoreGive(configMutex);
  lastInputMs = millis();
}
static void changeVolume(lv_event_t *event) {
  const int delta = (int)(intptr_t)lv_event_get_user_data(event);
  const int next = constrain(cfg.volume + delta, 0, 100);
  if (next == cfg.volume) return;
  cfg.volume = next;
  if (volumeValueLabel) lv_label_set_text_fmt(volumeValueLabel, "%d%%", cfg.volume);
  xSemaphoreTake(configMutex, portMAX_DELAY);
  prefs.putInt("volume", cfg.volume);
  xSemaphoreGive(configMutex);
  lastInputMs = millis();
}
static void changeHomeCarouselInterval(lv_event_t *event) {
  const int delta = (int)(intptr_t)lv_event_get_user_data(event);
  const int next = constrain(cfg.carouselSeconds + delta, 5, 60);
  if (next == cfg.carouselSeconds) return;
  cfg.carouselSeconds = next;
  lv_label_set_text_fmt(carouselValueLabel, "%d 秒", next);
  lastWeatherFlip = lastFooterFlip = millis();
  xSemaphoreTake(configMutex, portMAX_DELAY);
  prefs.putInt("home_rotate", next);
  xSemaphoreGive(configMutex);
  lastInputMs = millis();
}
static void changeEq(lv_event_t *event) {
  const uint8_t action = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
  const uint8_t band = action / 2;
  if (band >= 3) return;
  const int next = constrain(cfg.eqDb[band] + (action % 2 ? 1 : -1), -6, 6);
  if (next == cfg.eqDb[band]) return;
  cfg.eqDb[band] = next;
  eqGain[band].store(next, std::memory_order_relaxed);
  lv_label_set_text_fmt(eqValueLabels[band], "%+d dB", next);
  static const char *keys[3] = {"eq_b", "eq_m", "eq_t"};
  xSemaphoreTake(configMutex, portMAX_DELAY);
  prefs.putInt(keys[band], next);
  xSemaphoreGive(configMutex);
  lastInputMs = millis();
}
static void resetEq(lv_event_t *) {
  static const char *keys[3] = {"eq_b", "eq_m", "eq_t"};
  xSemaphoreTake(configMutex, portMAX_DELAY);
  for (int band = 0; band < 3; ++band) {
    cfg.eqDb[band] = 0;
    eqGain[band].store(0, std::memory_order_relaxed);
    prefs.putInt(keys[band], 0);
    lv_label_set_text(eqValueLabels[band], "+0 dB");
  }
  xSemaphoreGive(configMutex);
  lastInputMs = millis();
}
static void eqAdjustButton(lv_obj_t *parent, const char *text, int x, int y,
                           uint8_t band, bool increase) {
  lv_obj_t *control = button(parent, text, x, y, 44, 34, noAction);
  const uintptr_t action = band * 2 + (increase ? 1 : 0);
  lv_obj_add_event_cb(control, changeEq, LV_EVENT_PRESSED, (void *)action);
}
static void changeCounty(lv_event_t *event) {
  const uint8_t slot = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
  if (slot >= kWeatherSlots) return;
  cfg.counties[slot] = lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(event));
  weatherData[slot] = WeatherData();
  weatherData[slot].summary = "更新中...";
  weatherSlot = slot;
  lastWeatherFlip = millis();
  xSemaphoreTake(configMutex, portMAX_DELAY);
  const String key = slot ? String("county") + slot : String("county");
  prefs.putInt(key.c_str(), cfg.counties[slot]);
  xSemaphoreGive(configMutex);
  lastWeatherAttempt = millis();
  if (networkTaskHandle) xTaskNotify(networkTaskHandle, kFetchWeather, eSetBits);
  lastInputMs = millis();
}
static void changeWeatherDisplay(lv_event_t *event) {
  const int field = (uintptr_t)lv_event_get_user_data(event);
  const int selected = lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(event));
  if (field == 0) cfg.weatherMain = selected;
  else if (field == 1) cfg.weatherDetail = selected;
  else cfg.weatherThird = selected;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  prefs.putInt(field == 0 ? "wx_main" : field == 1 ? "wx_detail" : "wx_third", selected);
  xSemaphoreGive(configMutex);
  if (networkTaskHandle) xTaskNotify(networkTaskHandle, kFetchWeather, eSetBits);
  lastInputMs = millis();
}
static void changeHourMode(lv_event_t *) {
  cfg.hour12 = !cfg.hour12;
  xSemaphoreTake(configMutex, portMAX_DELAY);
  prefs.putBool("hour12", cfg.hour12);
  xSemaphoreGive(configMutex);
  lv_label_set_text(lv_obj_get_child(hourModeButton, 0), cfg.hour12 ? "12 小時" : "24 小時");
  lastInputMs = millis();
}

static void adjustButton(lv_obj_t *parent, const char *text, int x, int y,
                         lv_event_cb_t callback, int delta, int width = 44,
                         int height = 34) {
  lv_obj_t *control = button(parent, text, x, y, width, height, noAction);
  lv_obj_set_style_text_font(lv_obj_get_child(control, 0), &desk_font_16_bold, 0);
  lv_obj_add_event_cb(control, callback, LV_EVENT_PRESSED, (void *)(intptr_t)delta);
}

static lv_obj_t *generalLabel(lv_obj_t *parent, const char *text, int x, int y, int w) {
  lv_obj_t *o = label(parent, text, x, y, w);
  lv_obj_set_style_text_font(o, &desk_font_16_bold, 0);
  return o;
}

static lv_obj_t *generalButton(lv_obj_t *parent, const char *text, int x, int y,
                               int w, int h, lv_event_cb_t callback) {
  lv_obj_t *o = button(parent, text, x, y, w, h, callback);
  lv_obj_set_style_text_font(lv_obj_get_child(o, 0), &desk_font_16_bold, 0);
  return o;
}

static void openWifiSettings(lv_event_t *) {
  selectSettingsPage(1);
  lastInputMs = millis();
}

static void returnGeneralSettings(lv_event_t *) {
  selectSettingsPage(0);
  lastInputMs = millis();
}

static void updateMp3Screen() {
  if (!mp3TitleLabel) return;
  const int state = mp3State.load();
  if (mp3TrackCount) {
    const String &path = mp3Tracks[mp3SelectedTrack];
    const int slash = path.lastIndexOf('/');
    homeSetText(mp3TitleLabel, path.c_str() + slash + 1);
    lv_label_set_text_fmt(mp3CountLabel, "%d / %d 首", mp3SelectedTrack + 1, mp3TrackCount);
  } else {
    homeSetText(mp3TitleLabel, mp3StorageStatus);
    homeSetText(mp3CountLabel, "0 首");
  }
  const char *status = state == 1 ? "開啟中" : state == 2 ? "播放中" :
                       state == 3 ? "已暫停" : state == 4 ? "開啟或解碼失敗" :
                       state == 5 ? "播放已結束" :
                       mp3TrackCount ? "已停止" : mp3StorageStatus;
  homeSetText(mp3StatusLabel, status);
  homeSetText(lv_obj_get_child(mp3PlayButton, 0),
              state == 2 ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
  lv_bar_set_value(mp3ProgressBar, mp3Progress.load(), LV_ANIM_OFF);
}

static void scanMp3(lv_event_t *) {
  const int state = mp3State.load();
  if (state == 1 || state == 2 || state == 3) {
    homeSetText(mp3StatusLabel, "請先停止播放再掃描");
    return;
  }
  mp3TrackCount = 0;
  mp3SelectedTrack = 0;
  mp3Progress.store(0);
  mp3State.store(0);
  if (!deskFontSdMounted()) deskFontInitSd();
  if (!deskFontSdMounted()) mp3StorageStatus = "SD 卡未掛載";
  else {
    File folder = SD_MMC.open("/music");
    if (!folder || !folder.isDirectory()) mp3StorageStatus = "缺少 /music 資料夾";
    else {
      for (int seen = 0; seen < 256 && mp3TrackCount < kMp3MaxTracks; ++seen) {
        File entry = folder.openNextFile();
        if (!entry) break;
        if (!entry.isDirectory()) {
          String path = entry.path();
          String lower = path;
          lower.toLowerCase();
          if (lower.endsWith(".mp3") && path.length() < sizeof(mp3RequestedPath))
            mp3Tracks[mp3TrackCount++] = path;
        }
        entry.close();
      }
      folder.close();
      for (int i = 1; i < mp3TrackCount; ++i) {
        String item = mp3Tracks[i];
        int j = i;
        while (j > 0 && mp3Tracks[j - 1].compareTo(item) > 0) {
          mp3Tracks[j] = mp3Tracks[j - 1];
          --j;
        }
        mp3Tracks[j] = item;
      }
      mp3StorageStatus = mp3TrackCount ? "已找到 MP3" : "找不到 MP3";
    }
  }
  updateMp3Screen();
  lastInputMs = millis();
  Serial.printf("[MP3] scan tracks=%d status=%s\n", mp3TrackCount, mp3StorageStatus);
}

static void playSelectedMp3() {
  if (!mp3TrackCount || !mp3RequestMutex) return;
  if (xSemaphoreTake(mp3RequestMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
  strlcpy(mp3RequestedPath, mp3Tracks[mp3SelectedTrack].c_str(), sizeof(mp3RequestedPath));
  xSemaphoreGive(mp3RequestMutex);
  mp3Progress.store(0);
  mp3State.store(1);
  radioState.store(0);
  radioCommand.store(kMp3PlayCommand);
  updateRadioScreen();
  updateHomeRadio();
  updateMp3Screen();
}

static void toggleMp3(lv_event_t *) {
  const int state = mp3State.load();
  if (state == 2) radioCommand.store(kMp3PauseCommand);
  else if (state == 3) radioCommand.store(kMp3ResumeCommand);
  else playSelectedMp3();
  lastInputMs = millis();
}

static void stopMp3(lv_event_t *) {
  const int state = mp3State.load();
  if (state == 1 || state == 2 || state == 3) radioCommand.store(0);
  lastInputMs = millis();
}

static void stepMp3(int delta) {
  if (!mp3TrackCount) return;
  mp3SelectedTrack = (mp3SelectedTrack + delta + mp3TrackCount) % mp3TrackCount;
  const int state = mp3State.load();
  if (state == 1 || state == 2 || state == 3) playSelectedMp3();
  else updateMp3Screen();
  lastInputMs = millis();
}

static void previousMp3(lv_event_t *) { stepMp3(-1); }
static void nextMp3(lv_event_t *) { stepMp3(1); }

static void selectSettingsPage(uint8_t index) {
  static uint8_t shown = 0xff;
  if (index == shown) return;
  shown = index;
  for (uint8_t i = 0; i < kSettingsPages; ++i) {
    if (i == index) lv_obj_remove_flag(settingsPages[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(settingsPages[i], LV_OBJ_FLAG_HIDDEN);
  }
  for (uint8_t i = 0; i < kSettingsNavCount; ++i) {
    const bool selected = (i == 0 ? index <= 1 : i + 1 == index);
    lv_obj_set_style_bg_color(settingsNavButtons[i],
                              lv_color_hex(selected ? 0x253A30 : 0x121916), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(settingsNavButtons[i], 0),
                                lv_color_hex(selected ? 0xF3F5F2 : 0xB3BDB7), 0);
    if (selected) lv_obj_remove_flag(settingsNavIndicators[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(settingsNavIndicators[i], LV_OBJ_FLAG_HIDDEN);
  }
}

static void settingsNavClicked(lv_event_t *event) {
  const uint8_t navIndex = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
  const uint8_t index = navIndex == 0 ? 0 : navIndex + 1;
  selectSettingsPage(index);
  if (index == kMp3SettingsPage) scanMp3(nullptr);
  lastInputMs = millis();
  Serial.printf("[UI] settings page=%u\n", index);
}

static void refreshWeatherNow(lv_event_t *) {
  lastWeatherAttempt = millis();
  if (networkTaskHandle) xTaskNotify(networkTaskHandle, kFetchWeather, eSetBits);
  lastInputMs = millis();
}

static void refreshMarketsNow(lv_event_t *) {
  lastStockAttempt = millis();
  lastIntradayAttempt = millis();
  if (networkTaskHandle) xTaskNotify(networkTaskHandle,
      kFetchMarkets | (intradayWindow() ? kFetchIntraday : 0), eSetBits);
  lastInputMs = millis();
}

static void buildSettings() {
  settingsScreen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(settingsScreen, lv_color_hex(0x080B0B), 0);
  lv_obj_set_style_pad_all(settingsScreen, 0, 0);
  lv_obj_set_style_border_width(settingsScreen, 0, 0);
  lv_obj_set_style_text_font(settingsScreen, &desk_font_16, 0);
  lv_obj_clear_flag(settingsScreen, LV_OBJ_FLAG_SCROLLABLE);
  label(settingsScreen, "設定", 18, 12, 60);
  const char *names[] = {"一般", "天氣", "股票", "音效", "網路電台", "MP3"};
  for (uint8_t i = 0; i < kSettingsPages; ++i) {
    settingsPages[i] = lv_obj_create(settingsScreen);
    lv_obj_set_pos(settingsPages[i], 0, 44);
    lv_obj_set_size(settingsPages[i], 640, 128);
    lv_obj_set_style_pad_all(settingsPages[i], 0, 0);
    lv_obj_set_style_border_width(settingsPages[i], 0, 0);
    lv_obj_set_style_bg_color(settingsPages[i], lv_color_hex(0x080B0B), 0);
    lv_obj_clear_flag(settingsPages[i], LV_OBJ_FLAG_SCROLLABLE);
  }
  for (uint8_t i = 0; i < kSettingsNavCount; ++i) {
    lv_obj_t *nav = lv_button_create(settingsScreen);
    settingsNavButtons[i] = nav;
    lv_obj_set_pos(nav, 82 + i * 80, 5);
    lv_obj_set_size(nav, 76, 34);
    lv_obj_set_style_pad_all(nav, 0, 0);
    lv_obj_set_style_radius(nav, 4, 0);
    lv_obj_set_style_shadow_width(nav, 0, 0);
    lv_obj_set_style_border_width(nav, 0, 0);
    lv_obj_set_style_bg_color(nav, lv_color_hex(0x121916), 0);
    lv_obj_add_event_cb(nav, buttonCue, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(nav, settingsNavClicked, LV_EVENT_PRESSED, (void *)(uintptr_t)i);
    lv_obj_t *navLabel = label(nav, names[i], 0, 0, 68);
    lv_obj_set_style_text_align(navLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(navLabel, lv_color_hex(0xB3BDB7), 0);
    lv_obj_center(navLabel);
    lv_obj_t *indicator = lv_obj_create(nav);
    settingsNavIndicators[i] = indicator;
    lv_obj_set_pos(indicator, 0, 31); lv_obj_set_size(indicator, 76, 3);
    lv_obj_set_style_pad_all(indicator, 0, 0);
    lv_obj_set_style_border_width(indicator, 0, 0);
    lv_obj_set_style_bg_color(indicator, lv_color_hex(0x83D7BB), 0);
    lv_obj_clear_flag(indicator, LV_OBJ_FLAG_SCROLLABLE);
  }
  lv_obj_t *general = settingsPages[0];
  lv_obj_t *network = settingsPages[1];
  lv_obj_t *weather = settingsPages[2];
  lv_obj_t *stocks = settingsPages[3];
  lv_obj_t *audio = settingsPages[4];
  lv_obj_t *radio = settingsPages[5];
  lv_obj_t *mp3Page = settingsPages[kMp3SettingsPage];
  lv_obj_t *generalCards[6];
  for (int i = 0; i < 6; ++i) {
    lv_obj_t *card = lv_obj_create(general);
    generalCards[i] = card;
    lv_obj_set_pos(card, 12 + (i % 3) * 208, 2 + (i / 3) * 66);
    lv_obj_set_size(card, 200, 58);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_radius(card, 7, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x121916), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x28372F), 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  }
  generalLabel(generalCards[0], "亮度", 10, 5, 70);
  brightnessValueLabel = generalLabel(generalCards[0], "", 100, 5, 90);
  lv_obj_set_style_text_align(brightnessValueLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text_fmt(brightnessValueLabel, "%d%%", cfg.brightness);
  adjustButton(generalCards[0], "-", 9, 26, changeBrightness, -5, 86, 27);
  adjustButton(generalCards[0], "+", 105, 26, changeBrightness, 5, 86, 27);

  generalLabel(generalCards[1], "省電", 10, 5, 70);
  sleepValueLabel = generalLabel(generalCards[1], "", 100, 5, 90);
  lv_obj_set_style_text_align(sleepValueLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text_fmt(sleepValueLabel, "%d 分", cfg.sleepMinutes);
  adjustButton(generalCards[1], "-", 9, 26, changeSleep, -5, 86, 27);
  adjustButton(generalCards[1], "+", 105, 26, changeSleep, 5, 86, 27);

  generalLabel(generalCards[2], "音量", 10, 5, 70);
  volumeValueLabel = generalLabel(generalCards[2], "", 100, 5, 90);
  lv_obj_set_style_text_align(volumeValueLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text_fmt(volumeValueLabel, "%d%%", cfg.volume);
  adjustButton(generalCards[2], "-", 9, 26, changeVolume, -1, 86, 27);
  adjustButton(generalCards[2], "+", 105, 26, changeVolume, 1, 86, 27);

  generalLabel(generalCards[3], "日期與時間", 10, 5, 180);
  generalButton(generalCards[3], "設定時間", 9, 26, 86, 27, editDateTime);
  hourModeButton = generalButton(generalCards[3], cfg.hour12 ? "12 小時" : "24 小時",
                                 105, 26, 86, 27, changeHourMode);

  generalLabel(generalCards[4], "內容輪播", 10, 5, 90);
  carouselValueLabel = generalLabel(generalCards[4], "", 100, 5, 90);
  lv_obj_set_style_text_align(carouselValueLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text_fmt(carouselValueLabel, "%d 秒", cfg.carouselSeconds);
  adjustButton(generalCards[4], "-", 9, 26, changeHomeCarouselInterval, -5, 86, 27);
  adjustButton(generalCards[4], "+", 105, 26, changeHomeCarouselInterval, 5, 86, 27);

  generalLabel(generalCards[5], "系統與網路", 10, 5, 90);
  otaStatusLabel = generalLabel(generalCards[5], DESKDOCK_VERSION, 100, 5, 90);
  lv_obj_set_style_text_align(otaStatusLabel, LV_TEXT_ALIGN_RIGHT, 0);
  generalButton(generalCards[5], "Wi-Fi", 9, 26, 86, 27, openWifiSettings);
  otaButton = generalButton(generalCards[5], "檢查更新", 105, 26, 86, 27, otaButtonClicked);

  button(network, "返回一般", 20, 2, 110, 26, returnGeneralSettings);
  networkLabel = label(network, "未連線", 142, 5, 476);
  button(network, "搜尋", 20, 31, 110, 30, scanWifi);
  button(network, "優先", 142, 31, 110, 30, wifiPriorityUp);
  button(network, "刪除", 264, 31, 110, 30, wifiForget);
  button(network, "手動", 386, 31, 110, 30, editSsid);
  button(network, "連線", 508, 31, 110, 30, connectWifi);
  wifiList = lv_obj_create(network);
  lv_obj_set_pos(wifiList, 20, 66);
  lv_obj_set_size(wifiList, 600, 62);
  lv_obj_set_style_pad_all(wifiList, 0, 0);
  lv_obj_set_style_border_width(wifiList, 0, 0);
  lv_obj_set_style_bg_opa(wifiList, LV_OPA_TRANSP, 0);
  rebuildWifiList();

  String options;
  for (int i = 0; i < kCountyCount; ++i) {
    if (i) options += '\n';
    options += kCounties[i];
  }
  static const char *metricOptions =
      "高低溫\n降雨機率\n目前溫度\n體感溫度\n濕度\n風速\n雲量\n紫外線\n降雨量\n日出\n日落";
  label(weather, "主", 20, 10, 28);
  label(weather, "副", 205, 10, 28);
  label(weather, "三", 390, 10, 28);
  for (int field = 0; field < 3; ++field) {
    lv_obj_t *metric = lv_dropdown_create(weather);
    lv_obj_set_pos(metric, 50 + field * 185, 4);
    lv_obj_set_size(metric, 145, 34);
    lv_obj_set_style_bg_color(metric, lv_color_hex(0x202824), 0);
    lv_obj_set_style_text_color(metric, lv_color_hex(0xF3F5F2), 0);
    lv_obj_set_style_border_color(metric, lv_color_hex(0x34433B), 0);
    lv_dropdown_set_options(metric, metricOptions);
    lv_dropdown_set_selected(metric, field == 0 ? cfg.weatherMain :
                                      field == 1 ? cfg.weatherDetail : cfg.weatherThird);
    lv_obj_add_event_cb(metric, changeWeatherDisplay, LV_EVENT_VALUE_CHANGED,
                        (void *)(uintptr_t)field);
  }
  button(weather, "更新", 575, 4, 52, 34, refreshWeatherNow);
  static const char *slotNames[kWeatherSlots] = {"地點 1", "地點 2", "地點 3"};
  const int settingsColumnX[3] = {20, 226, 432};
  for (int slot = 0; slot < kWeatherSlots; ++slot) {
    const int x = settingsColumnX[slot];
    label(weather, slotNames[slot], x, 43, 92);
    lv_obj_t *county = lv_dropdown_create(weather);
    lv_obj_set_pos(county, x, 65); lv_obj_set_size(county, 186, 36);
    lv_obj_set_style_bg_color(county, lv_color_hex(0x202824), 0);
    lv_obj_set_style_text_color(county, lv_color_hex(0xF3F5F2), 0);
    lv_obj_set_style_border_color(county, lv_color_hex(0x34433B), 0);
    lv_dropdown_set_options(county, options.c_str());
    lv_dropdown_set_selected(county, cfg.counties[slot]);
    lv_obj_add_event_cb(county, changeCounty, LV_EVENT_VALUE_CHANGED,
                        (void *)(uintptr_t)slot);
    weatherPreviewLabels[slot] = label(weather, "等待天氣", x, 103, 186);
  }

  label(stocks, "已選代號", 20, 9, 95);
  stockCodesLabel = label(stocks, cfg.stockCode.c_str(), 125, 9, 490);
  button(stocks, "編輯代號", settingsColumnX[0], 42, 186, 42, editStock);
  button(stocks, "自選股清單", settingsColumnX[1], 42, 186, 42, openStocks);
  button(stocks, "更新行情", settingsColumnX[2], 42, 188, 42, refreshMarketsNow);
  lv_obj_t *stockCard = lv_obj_create(stocks);
  lv_obj_set_pos(stockCard, 20, 92); lv_obj_set_size(stockCard, 600, 32);
  lv_obj_set_style_pad_all(stockCard, 0, 0);
  lv_obj_set_style_radius(stockCard, 6, 0);
  lv_obj_set_style_bg_color(stockCard, lv_color_hex(0x141C19), 0);
  lv_obj_set_style_border_width(stockCard, 0, 0);
  lv_obj_clear_flag(stockCard, LV_OBJ_FLAG_SCROLLABLE);
  stockPreviewLabel = label(stockCard, remote.index.c_str(), 12, 4, 580);

  static const char *eqNames[3] = {"低音", "人聲", "高音"};
  static const char *eqFrequencies[3] = {"120 Hz", "1 kHz", "6 kHz"};
  for (uint8_t band = 0; band < 3; ++band) {
    const int y = 5 + band * 40;
    label(audio, eqNames[band], 20, y + 7, 70);
    eqAdjustButton(audio, "-", 103, y, band, false);
    eqValueLabels[band] = label(audio, "", 157, y + 4, 80, 20);
    lv_obj_set_style_text_align(eqValueLabels[band], LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text_fmt(eqValueLabels[band], "%+d dB", cfg.eqDb[band]);
    eqAdjustButton(audio, "+", 247, y, band, true);
    label(audio, eqFrequencies[band], 325, y + 7, 110);
  }
  label(audio, "網路電台三段等化器", 447, 8, 180);
  button(audio, "恢復平坦", 470, 48, 143, 42, resetEq);

  buildRadioSettings(radio);

  label(mp3Page, "SD 音樂  /music", 20, 9, 360);
  mp3TitleLabel = label(mp3Page, "尚未掃描", 20, 36, 365);
  lv_obj_set_style_text_font(mp3TitleLabel, &desk_font_16_bold, 0);
  mp3StatusLabel = label(mp3Page, "尚未掃描", 20, 65, 365);
  mp3CountLabel = label(mp3Page, "0 首", 20, 92, 116);
  mp3ProgressBar = lv_bar_create(mp3Page);
  lv_obj_set_pos(mp3ProgressBar, 143, 100);
  lv_obj_set_size(mp3ProgressBar, 237, 8);
  lv_bar_set_range(mp3ProgressBar, 0, 1000);
  lv_bar_set_value(mp3ProgressBar, 0, LV_ANIM_OFF);
  button(mp3Page, LV_SYMBOL_LEFT, 400, 15, 55, 42, previousMp3);
  mp3PlayButton = button(mp3Page, LV_SYMBOL_PLAY, 463, 15, 80, 42, toggleMp3);
  button(mp3Page, LV_SYMBOL_RIGHT, 551, 15, 55, 42, nextMp3);
  button(mp3Page, "停止", 400, 73, 90, 35, stopMp3);
  button(mp3Page, "重新掃描", 499, 73, 107, 35, scanMp3);

  selectSettingsPage(0);
  button(settingsScreen, LV_SYMBOL_LEFT, 575, 5, 54, 34, goHome);
}

static void saveField(const String &value) {
  if (editField == EditField::Ssid) {
    cfg.ssid = value;
    cfg.ssid.trim();
    selectedWifiSsid = cfg.ssid;
    cfg.password = "";
  } else if (editField == EditField::Password) {
    cfg.password = value;
    startWifiCandidate(selectedWifiSsid.length() ? selectedWifiSsid : cfg.ssid, value);
  } else if (editField == EditField::Stock) {
    String codes = value; codes.trim();
    String previous[kMaxStocks];
    const uint8_t previousCount = stockCount;
    for (uint8_t i = 0; i < previousCount; ++i) previous[i] = stockCodes[i];
    if (parseStockCodes(codes)) {
      xSemaphoreTake(configMutex, portMAX_DELAY);
      cfg.stockCode = codes; prefs.putString("stock", codes);
      xSemaphoreGive(configMutex);
      for (uint8_t i = 0; i < stockCount; ++i) {
        if (i >= previousCount || previous[i] != stockCodes[i]) {
          stockValues[i] = "請連線取得資料";
          stockStamps[i] = "";
          stockNames[i] = "";
          stockDetails[i] = StockDetail{};
        }
      }
      ++stockUiRevision;
      if (stockCodesLabel) lv_label_set_text(stockCodesLabel, cfg.stockCode.c_str());
      if (footerPage >= stockCount) footerPage = 0;
      updateStockScreen();
      networkRefresh = true;
      Serial.printf("[STOCK] count=%u\n", stockCount);
    } else Serial.println("[STOCK] invalid code list");
  } else if (editField == EditField::RadioUrl) {
    String url = value; url.trim();
    if (url.isEmpty() || (url.startsWith("http://") && url.length() < 250)) {
      xSemaphoreTake(configMutex, portMAX_DELAY);
      cfg.customRadioUrl = url;
      prefs.putString("radio_url", url);
      xSemaphoreGive(configMutex);
      if (radioStation.load() == 3) radioCommand.store(0);
      Serial.println("[RADIO] custom URL updated");
    } else Serial.println("[RADIO] rejected URL: HTTP MP3 required");
  } else {
    int y, mo, d, h, mi;
    if (sscanf(value.c_str(), "%d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) == 5 &&
        y >= 2024 && y <= 2099 && mo >= 1 && mo <= 12 && d >= 1 && d <= 31 &&
        h >= 0 && h <= 23 && mi >= 0 && mi <= 59) {
      tm t = {}; t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
      t.tm_hour = h; t.tm_min = mi; t.tm_isdst = -1;
      const time_t epoch = mktime(&t);
      if (t.tm_mday == d && t.tm_mon == mo - 1) {
        timeval tv = {.tv_sec = epoch, .tv_usec = 0};
        settimeofday(&tv, nullptr);
        rtcWrite(t);
      }
    }
  }
}

static String urlEncode(const String &value) {
  String out;
  for (size_t i = 0; i < value.length(); ++i) {
    unsigned char c = (unsigned char)value[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.') out += (char)c;
    else { char temp[4]; snprintf(temp, sizeof(temp), "%%%02X", c); out += temp; }
  }
  return out;
}

struct HttpBuffer { String body; size_t limit; bool overflow = false; };
static bool radioNeedsNetwork() {
  const int state = radioState.load();
  const int command = radioCommand.load();
  return state == 1 || state == 2 || (command > 0 && command <= kRadioStations);
}

static void takeBackgroundHttpMutex() {
  for (;;) {
    while (radioNeedsNetwork()) vTaskDelay(pdMS_TO_TICKS(100));
    if (xSemaphoreTake(networkHttpMutex, pdMS_TO_TICKS(100)) != pdTRUE) continue;
    if (!radioNeedsNetwork()) return;
    xSemaphoreGive(networkHttpMutex);
  }
}

static esp_err_t httpEvent(esp_http_client_event_t *event) {
  if (event->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
  HttpBuffer *buf = (HttpBuffer *)event->user_data;
  if (buf->body.length() + event->data_len > buf->limit) { buf->overflow = true; return ESP_FAIL; }
  buf->body.concat((const char *)event->data, event->data_len);
  return ESP_OK;
}

static bool getJson(const String &url, size_t limit, String *body) {
  takeBackgroundHttpMutex();
  HttpBuffer buf; buf.limit = limit;
  if (!buf.body.reserve(limit < 200000 ? limit : 200000)) {
    xSemaphoreGive(networkHttpMutex);
    return false;
  }
  esp_http_client_config_t config = {};
  config.url = url.c_str();
  config.event_handler = httpEvent;
  config.user_data = &buf;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.timeout_ms = 10000;
  config.buffer_size = 4096;
  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) { xSemaphoreGive(networkHttpMutex); return false; }
  const esp_err_t result = esp_http_client_perform(client);
  const int status = esp_http_client_get_status_code(client);
  const int socketError = result == ESP_OK ? 0 : esp_http_client_get_errno(client);
  esp_http_client_cleanup(client);
  xSemaphoreGive(networkHttpMutex);
  if (result != ESP_OK || status != 200 || buf.overflow) {
    Serial.printf("[HTTP] %s failed: %d status=%d errno=%d overflow=%d\n",
                  url.c_str(), result, status, socketError, buf.overflow);
    return false;
  }
  *body = std::move(buf.body);
  return true;
}

static const char *jsonString(const cJSON *parent, const char *key) {
  const cJSON *field = cJSON_GetObjectItemCaseSensitive(parent, key);
  return cJSON_IsString(field) ? field->valuestring : "";
}

#include "ota_update.inc"

static String signedChange(const char *value) {
  if (!value || !*value || !strcmp(value, "-")) return "";
  char *end = nullptr;
  const float delta = strtof(value, &end);
  if (end != value && delta == 0) return String(value[0] == '-' || value[0] == '+' ? value + 1 : value);
  return String(value[0] == '-' || value[0] == '+' || !strcmp(value, "0") ? "" : "+") + value;
}

static String stampNow() {
  tm t; char out[32] = "";
  if (getLocalTime(&t, 50)) strftime(out, sizeof(out), "%m/%d %H:%M", &t);
  return String(out);
}

static int weatherTimeMinutes(const cJSON *value) {
  if (!cJSON_IsString(value) || !value->valuestring) return -1;
  const char *time = strchr(value->valuestring, 'T');
  int hour = -1, minute = -1;
  if (!time || sscanf(time + 1, "%d:%d", &hour, &minute) != 2 ||
      hour < 0 || hour > 23 || minute < 0 || minute > 59) return -1;
  return hour * 60 + minute;
}

static bool fetchWeather(WeatherData &item, int county) {
  const WeatherPoint &point = kWeatherPoints[county];
  String url = "https://api.open-meteo.com/v1/forecast?latitude=";
  url += String(point.latitude, 4);
  url += "&longitude=";
  url += String(point.longitude, 4);
  url += "&current=weather_code,is_day,temperature_2m,apparent_temperature,";
  url += "relative_humidity_2m,wind_speed_10m,cloud_cover";
  url += "&daily=temperature_2m_max,temperature_2m_min,";
  url += "precipitation_probability_max,uv_index_max,precipitation_sum,sunrise,sunset";
  url += "&timezone=Asia%2FTaipei&forecast_days=1";
  String body;
  if (!getJson(url, 16000, &body)) { item.summary = "天氣資料取得失敗"; return false; }
  cJSON *root = cJSON_Parse(body.c_str());
  if (!root) { item.summary = "天氣資料格式錯誤"; return false; }
  const cJSON *daily = cJSON_GetObjectItemCaseSensitive(root, "daily");
  const cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
  const cJSON *low = cJSON_GetArrayItem(
      cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_min"), 0);
  const cJSON *high = cJSON_GetArrayItem(
      cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_max"), 0);
  const cJSON *rain = cJSON_GetArrayItem(
      cJSON_GetObjectItemCaseSensitive(daily, "precipitation_probability_max"), 0);
  const cJSON *code = cJSON_GetObjectItemCaseSensitive(current, "weather_code");
  const cJSON *day = cJSON_GetObjectItemCaseSensitive(current, "is_day");
  const cJSON *temperature = cJSON_GetObjectItemCaseSensitive(current, "temperature_2m");
  const cJSON *feels = cJSON_GetObjectItemCaseSensitive(current, "apparent_temperature");
  const cJSON *humidity = cJSON_GetObjectItemCaseSensitive(current, "relative_humidity_2m");
  const cJSON *wind = cJSON_GetObjectItemCaseSensitive(current, "wind_speed_10m");
  const cJSON *cloud = cJSON_GetObjectItemCaseSensitive(current, "cloud_cover");
  const cJSON *uv = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(daily, "uv_index_max"), 0);
  const cJSON *rainMm = cJSON_GetArrayItem(
      cJSON_GetObjectItemCaseSensitive(daily, "precipitation_sum"), 0);
  const cJSON *sunrise = cJSON_GetArrayItem(
      cJSON_GetObjectItemCaseSensitive(daily, "sunrise"), 0);
  const cJSON *sunset = cJSON_GetArrayItem(
      cJSON_GetObjectItemCaseSensitive(daily, "sunset"), 0);
  const bool valid = cJSON_IsNumber(low) && cJSON_IsNumber(high) &&
                     cJSON_IsNumber(rain) && cJSON_IsNumber(code) && cJSON_IsNumber(day);
  if (valid) {
    item.low = (int)lround(cJSON_GetNumberValue(low));
    item.high = (int)lround(cJSON_GetNumberValue(high));
    item.rain = constrain((int)lround(cJSON_GetNumberValue(rain)), 0, 100);
    item.code = (int)cJSON_GetNumberValue(code);
    item.day = cJSON_GetNumberValue(day) != 0;
    if (cJSON_IsNumber(temperature))
      item.temperature = (int)lround(cJSON_GetNumberValue(temperature));
    if (cJSON_IsNumber(feels)) item.feels = (int)lround(cJSON_GetNumberValue(feels));
    if (cJSON_IsNumber(humidity))
      item.humidity = constrain((int)lround(cJSON_GetNumberValue(humidity)), 0, 100);
    if (cJSON_IsNumber(wind))
      item.wind = constrain((int)lround(cJSON_GetNumberValue(wind)), 0, 999);
    if (cJSON_IsNumber(cloud))
      item.cloud = constrain((int)lround(cJSON_GetNumberValue(cloud)), 0, 100);
    if (cJSON_IsNumber(uv))
      item.uv10 = constrain((int)lround(cJSON_GetNumberValue(uv) * 10), 0, 999);
    if (cJSON_IsNumber(rainMm))
      item.rainMm10 = constrain((int)lround(cJSON_GetNumberValue(rainMm) * 10), 0, 9999);
    item.sunrise = weatherTimeMinutes(sunrise);
    item.sunset = weatherTimeMinutes(sunset);
    item.valid = true;
  }
  cJSON_Delete(root);
  if (!valid) { item.summary = "天氣資料格式錯誤"; return false; }
  item.summary = String(kCounties[county]) + "  " + item.low + "-" +
                 item.high + " C  降雨 " + item.rain + "%";
  item.stamp = stampNow();
  Serial.printf("[WEATHER] %s %d-%dC rain=%d%% code=%d day=%d\n",
                kCounties[county], item.low, item.high,
                item.rain, item.code, item.day);
  return true;
}

static bool fetchStockOne(const String &code, String &stockValue, String &stockStamp,
                          String &stockName, StockDetail &detail) {
  stockName = "";
  detail = StockDetail{};
  tm month;
  if (!getLocalTime(&month, 100) || month.tm_year < 124) {
    stockValue = "請先設定日期時間";
    return false;
  }
  month.tm_mday = 1;
  for (int attempt = 0; attempt < 2; ++attempt) {
    mktime(&month);
    char date[9];
    strftime(date, sizeof(date), "%Y%m%d", &month);
    const String url = String("https://www.twse.com.tw/rwd/zh/afterTrading/STOCK_DAY?response=json&date=") +
                       date + "&stockNo=" + urlEncode(code);
    String body;
    if (!getJson(url, 30000, &body)) {
      stockValue = "臺股資料取得失敗";
      return false;
    }
    cJSON *root = cJSON_Parse(body.c_str());
    if (!root) { stockValue = "臺股資料格式錯誤"; return false; }
    String fetchedName;
    const cJSON *title = cJSON_GetObjectItemCaseSensitive(root, "title");
    if (cJSON_IsString(title) && title->valuestring) {
      const String titleText(title->valuestring);
      const String codeMarker = String(" ") + code + " ";
      const int codeAt = titleText.indexOf(codeMarker);
      if (codeAt >= 0) {
        const int nameStart = codeAt + codeMarker.length();
        const int nameEnd = titleText.indexOf("各日成交資訊", nameStart);
        if (nameEnd > nameStart) {
          fetchedName = titleText.substring(nameStart, nameEnd);
          fetchedName.trim();
          if (fetchedName.length() > 72) fetchedName = "";
        }
      }
    }
    const cJSON *rows = cJSON_GetObjectItemCaseSensitive(root, "data");
    bool found = false;
    for (int i = cJSON_GetArraySize(rows) - 1; i >= 0; --i) {
      const cJSON *row = cJSON_GetArrayItem(rows, i);
      const cJSON *price = cJSON_GetArrayItem(row, 6);
      const cJSON *change = cJSON_GetArrayItem(row, 7);
      const cJSON *stamp = cJSON_GetArrayItem(row, 0);
      const cJSON *volume = cJSON_GetArrayItem(row, 1);
      const cJSON *high = cJSON_GetArrayItem(row, 4);
      const cJSON *low = cJSON_GetArrayItem(row, 5);
      if (!cJSON_IsString(price) || !price->valuestring || !strcmp(price->valuestring, "--")) continue;
      stockValue = code + "  " + price->valuestring;
      if (cJSON_IsString(change) && change->valuestring)
        stockValue += String("  ") + signedChange(change->valuestring);
      stockStamp = cJSON_IsString(stamp) ? String(stamp->valuestring) : "";
      if (cJSON_IsString(high) && high->valuestring) detail.high = high->valuestring;
      if (cJSON_IsString(low) && low->valuestring) detail.low = low->valuestring;
      if (cJSON_IsString(volume) && volume->valuestring) detail.volume = volume->valuestring;
      found = true;
      break;
    }
    if (found) stockName = fetchedName;
    cJSON_Delete(root);
    if (found) {
      Serial.printf("[STOCK] code=%s name=%s value=%s date=%s\n", code.c_str(),
                    stockName.c_str(), stockValue.c_str(), stockStamp.c_str());
      return true;
    }
    month.tm_mon--;
  }
  stockValue = "無此代號或無收盤價";
  return false;
}

static bool intradayWindow() {
  tm local;
  if (!getLocalTime(&local, 50) || local.tm_year < 124 ||
      local.tm_wday == 0 || local.tm_wday == 6) return false;
  const int minute = local.tm_hour * 60 + local.tm_min;
  // The last poll after 13:30 captures the closing trade.
  return minute >= 9 * 60 && minute < 13 * 60 + 40;
}

struct IntradayQuote {
  bool valid = false;
  String value, stamp, name;
  StockDetail detail;
};

static bool fetchIntraday(const String *codes, uint8_t count, IntradayQuote *quotes) {
  tm local;
  if (!getLocalTime(&local, 100) || local.tm_year < 124) return false;
  char today[9];
  strftime(today, sizeof(today), "%Y%m%d", &local);
  String url = "https://mis.twse.com.tw/stock/api/getStockInfo.jsp?ex_ch=";
  for (uint8_t i = 0; i < count; ++i) {
    if (i) url += "%7C";
    url += "tse_" + codes[i] + ".tw";
  }
  url += "&json=1&delay=0";
  String body;
  if (!getJson(url, 30000, &body)) return false;
  cJSON *root = cJSON_Parse(body.c_str());
  if (!root) return false;
  const cJSON *rows = cJSON_GetObjectItemCaseSensitive(root, "msgArray");
  uint8_t validCount = 0;
  for (int rowIndex = 0; rowIndex < cJSON_GetArraySize(rows); ++rowIndex) {
    const cJSON *row = cJSON_GetArrayItem(rows, rowIndex);
    const char *code = jsonString(row, "c");
    const char *date = jsonString(row, "d");
    const char *tradeTime = jsonString(row, "t");
    const char *priceText = jsonString(row, "z");
    const char *previousText = jsonString(row, "y");
    if (strcmp(date, today) || strlen(tradeTime) < 5 ||
        !priceText[0] || priceText[0] == '-' || !previousText[0]) continue;
    const float price = strtof(priceText, nullptr);
    const float previous = strtof(previousText, nullptr);
    if (price <= 0 || previous <= 0) continue;
    for (uint8_t i = 0; i < count; ++i) {
      if (codes[i] != code) continue;
      IntradayQuote &quote = quotes[i];
      quote.value = codes[i] + "  " + String(price, 2) + "  " +
                    signedChange(String(price - previous, 2).c_str());
      quote.stamp = String(date + 4).substring(0, 2) + "/" + String(date + 6) +
                    " " + String(tradeTime).substring(0, 5);
      quote.name = jsonString(row, "n");
      quote.detail.high = jsonString(row, "h");
      quote.detail.low = jsonString(row, "l");
      const char *volume = jsonString(row, "v");
      if (volume[0] && volume[0] != '-') quote.detail.volume = String(volume) + " 張";
      quote.valid = true;
      ++validCount;
      break;
    }
  }
  cJSON_Delete(root);
  Serial.printf("[STOCK] intraday %u/%u quotes at %s\n", validCount, count, today);
  return validCount > 0;
}

static bool fetchIndex(RemoteData &remote) {
  tm month;
  if (!getLocalTime(&month, 100) || month.tm_year < 124) {
    remote.index = "請先設定日期時間";
    return false;
  }
  month.tm_mday = 1;
  for (int attempt = 0; attempt < 2; ++attempt) {
    mktime(&month);
    char date[9];
    strftime(date, sizeof(date), "%Y%m%d", &month);
    const String url = String("https://www.twse.com.tw/rwd/zh/afterTrading/FMTQIK?response=json&date=") + date;
    String body;
    if (!getJson(url, 30000, &body)) { remote.index = "大盤資料取得失敗"; return false; }
    cJSON *root = cJSON_Parse(body.c_str());
    if (!root) { remote.index = "大盤資料格式錯誤"; return false; }
    const cJSON *rows = cJSON_GetObjectItemCaseSensitive(root, "data");
    bool found = false;
    for (int i = cJSON_GetArraySize(rows) - 1; i >= 0; --i) {
      const cJSON *row = cJSON_GetArrayItem(rows, i);
      const cJSON *price = cJSON_GetArrayItem(row, 4);
      const cJSON *change = cJSON_GetArrayItem(row, 5);
      const cJSON *stamp = cJSON_GetArrayItem(row, 0);
      if (!cJSON_IsString(price) || !price->valuestring || !strcmp(price->valuestring, "--")) continue;
      remote.index = String(price->valuestring);
      if (cJSON_IsString(change) && change->valuestring)
        remote.index += String("  ") + signedChange(change->valuestring);
      remote.indexStamp = cJSON_IsString(stamp) ? String(stamp->valuestring) : "";
      found = true;
      break;
    }
    cJSON_Delete(root);
    if (found) {
      Serial.printf("[INDEX] %s %s\n", remote.indexStamp.c_str(), remote.index.c_str());
      return true;
    }
    month.tm_mon--;
  }
  remote.index = "大盤尚無收盤資料";
  return false;
}

struct FuturesStream {
  String object, date, month, price, change;
  size_t received = 0;
  int depth = 0;
  bool quoted = false, escaped = false, overflow = false;
};

static esp_err_t futuresEvent(esp_http_client_event_t *event) {
  if (event->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
  FuturesStream *stream = (FuturesStream *)event->user_data;
  stream->received += event->data_len;
  if (stream->received > 1500000) { stream->overflow = true; return ESP_FAIL; }
  const char *data = (const char *)event->data;
  for (int i = 0; i < event->data_len; ++i) {
    const char ch = data[i];
    if (stream->depth == 0) {
      if (ch != '{') continue;
      stream->object = "";
    }
    if (!stream->object.concat(ch) || stream->object.length() > 2048) {
      stream->overflow = true;
      return ESP_FAIL;
    }
    if (stream->quoted) {
      if (stream->escaped) stream->escaped = false;
      else if (ch == '\\') stream->escaped = true;
      else if (ch == '"') stream->quoted = false;
    } else if (ch == '"') stream->quoted = true;
    else if (ch == '{') stream->depth++;
    else if (ch == '}' && --stream->depth == 0) {
      cJSON *row = cJSON_Parse(stream->object.c_str());
      if (row && !strcmp(jsonString(row, "Contract"), "TX") &&
          !strcmp(jsonString(row, "TradingSession"), "盤後")) {
        const String date = jsonString(row, "Date");
        const String month = jsonString(row, "ContractMonth(Week)");
        const String price = jsonString(row, "Last");
        if (date.length() == 8 && month.length() == 6 && price.length() && price != "-" &&
            (date > stream->date || (date == stream->date &&
             (stream->month.isEmpty() || month < stream->month)))) {
          stream->date = date;
          stream->month = month;
          stream->price = price;
          stream->change = signedChange(jsonString(row, "Change"));
        }
      }
      cJSON_Delete(row);
      stream->object = "";
    }
  }
  return ESP_OK;
}

static bool fetchNight(RemoteData &remote) {
  takeBackgroundHttpMutex();
  FuturesStream stream;
  if (!stream.object.reserve(1024)) {
    xSemaphoreGive(networkHttpMutex);
    remote.night = "記憶體不足";
    return false;
  }
  esp_http_client_config_t config = {};
  config.url = "https://openapi.taifex.com.tw/v1/DailyMarketReportFut";
  config.event_handler = futuresEvent;
  config.user_data = &stream;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.timeout_ms = 15000;
  config.buffer_size = 4096;
  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    xSemaphoreGive(networkHttpMutex);
    remote.night = "夜盤資料取得失敗";
    return false;
  }
  const esp_err_t result = esp_http_client_perform(client);
  const int status = esp_http_client_get_status_code(client);
  const int socketError = result == ESP_OK ? 0 : esp_http_client_get_errno(client);
  esp_http_client_cleanup(client);
  xSemaphoreGive(networkHttpMutex);
  if (result != ESP_OK || status != 200 || stream.overflow || stream.depth != 0) {
    Serial.printf("[NIGHT] failed: %d status=%d errno=%d bytes=%u\n",
                  result, status, socketError, (unsigned)stream.received);
    remote.night = "夜盤資料取得失敗";
    return false;
  }
  if (stream.price.isEmpty()) { remote.night = "尚無臺指期夜盤資料"; return false; }
  remote.night = String("TX ") + stream.price;
  if (stream.change.length() && stream.change != "-") remote.night += "  " + stream.change;
  remote.nightStamp = stream.date.substring(4, 6) + "/" + stream.date.substring(6, 8);
  remote.night += "  " + remote.nightStamp;
  Serial.printf("[NIGHT] %s %s\n", stream.date.c_str(), remote.night.c_str());
  return true;
}

static void networkTask(void *) {
  for (;;) {
    uint32_t requests = 0;
    xTaskNotifyWait(0, UINT32_MAX, &requests, portMAX_DELAY);
    if (WiFi.status() != WL_CONNECTED) continue;
    Serial.printf("[NETWORK] start heap=%u largest=%u min=%u psram=%u task_stack=%u\n",
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                   (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                  (unsigned)uxTaskGetStackHighWaterMark(nullptr));

    if (requests & kFetchIntraday) {
      String codes[kMaxStocks];
      uint8_t count;
      while (!lvgl_port_lock(1000)) vTaskDelay(pdMS_TO_TICKS(10));
      count = stockCount;
      for (uint8_t i = 0; i < count; ++i) codes[i] = stockCodes[i];
      lvgl_port_unlock();
      IntradayQuote quotes[kMaxStocks];
      if (count && fetchIntraday(codes, count, quotes)) {
        while (!lvgl_port_lock(1000)) vTaskDelay(pdMS_TO_TICKS(10));
        for (uint8_t i = 0; i < count; ++i) {
          if (!quotes[i].valid || i >= stockCount || stockCodes[i] != codes[i]) continue;
          stockValues[i] = quotes[i].value;
          stockStamps[i] = quotes[i].stamp;
          if (!quotes[i].name.isEmpty()) stockNames[i] = quotes[i].name;
          stockDetails[i] = quotes[i].detail;
          if (i == 0) { remote.stock = quotes[i].value; remote.stockStamp = quotes[i].stamp; }
          ++stockUiRevision;
        }
        lvgl_port_unlock();
      }
    }

    if (requests & kFetchWeather) {
      for (int slot = 0; slot < kWeatherSlots && WiFi.status() == WL_CONNECTED; ++slot) {
        while (!lvgl_port_lock(1000)) vTaskDelay(pdMS_TO_TICKS(10));
        const int county = cfg.counties[slot];
        lvgl_port_unlock();
        WeatherData result;
        const bool valid = fetchWeather(result, county);
        while (!lvgl_port_lock(1000)) vTaskDelay(pdMS_TO_TICKS(10));
        const bool current = cfg.counties[slot] == county;
        if (current && (valid || weatherData[slot].stamp.isEmpty())) {
          weatherData[slot] = result;
        }
        lvgl_port_unlock();
        if (current && valid) {
          const String key = slot ? String("om") + slot : String("last_om");
          const String stampKey = slot ? String("om_at") + slot : String("last_om_at");
           const String cached = String(county) + "," + result.low + "," +
                                 result.high + "," + result.rain + "," +
                                 result.code + "," + (result.day ? 1 : 0) + "," +
                                 result.temperature + "," + result.feels + "," +
                                 result.humidity + "," + result.wind + "," +
                                 result.cloud + "," + result.uv10 + "," +
                                 result.rainMm10 + "," + result.sunrise + "," +
                                 result.sunset;
          xSemaphoreTake(configMutex, portMAX_DELAY);
          prefs.putString(key.c_str(), cached);
          prefs.putString(stampKey.c_str(), result.stamp);
          xSemaphoreGive(configMutex);
        }
      }
    }

    if (requests & kFetchMarkets) {
      String codes[kMaxStocks];
      uint8_t count;
      while (!lvgl_port_lock(1000)) vTaskDelay(pdMS_TO_TICKS(10));
      count = stockCount;
      for (uint8_t i = 0; i < count; ++i) codes[i] = stockCodes[i];
      lvgl_port_unlock();
      for (uint8_t i = 0; i < count && !intradayWindow() &&
           WiFi.status() == WL_CONNECTED; ++i) {
        String value, stamp, name;
        StockDetail detail;
        const bool valid = fetchStockOne(codes[i], value, stamp, name, detail);
        while (!lvgl_port_lock(1000)) vTaskDelay(pdMS_TO_TICKS(10));
        const bool current = i < stockCount && stockCodes[i] == codes[i];
        if (current) {
          if (valid || stockStamps[i].isEmpty()) stockValues[i] = value;
          if (valid) {
            stockStamps[i] = stamp;
            if (!name.isEmpty()) stockNames[i] = name;
            stockDetails[i] = detail;
            if (i == 0) { remote.stock = value; remote.stockStamp = stamp; }
          }
          ++stockUiRevision;
        }
        lvgl_port_unlock();
        if (current && valid) {
          xSemaphoreTake(configMutex, portMAX_DELAY);
          if (i == 0) {
            prefs.putString("last_st", value);
            prefs.putString("last_st_at", stamp);
            prefs.putString("st0_code", codes[i]);
            prefs.putString("st0_name", name);
            prefs.putString("st0_detail", encodeStockDetail(detail));
          } else {
            const String id = String("st") + i;
            prefs.putString((id + "_code").c_str(), codes[i]);
            prefs.putString((id + "_value").c_str(), value);
            prefs.putString((id + "_stamp").c_str(), stamp);
            prefs.putString((id + "_name").c_str(), name);
            prefs.putString((id + "_detail").c_str(), encodeStockDetail(detail));
          }
          xSemaphoreGive(configMutex);
        }
      }
      if (WiFi.status() != WL_CONNECTED) continue;
      RemoteData result;
      const bool indexValid = fetchIndex(result);
      while (!lvgl_port_lock(1000)) vTaskDelay(pdMS_TO_TICKS(10));
      if (indexValid || remote.indexStamp.isEmpty()) {
        remote.index = result.index;
        remote.indexStamp = result.indexStamp;
      }
      lvgl_port_unlock();
      if (indexValid) {
        xSemaphoreTake(configMutex, portMAX_DELAY);
        prefs.putString("last_idx", result.index);
        prefs.putString("last_idx_at", result.indexStamp);
        xSemaphoreGive(configMutex);
      }
      if (WiFi.status() != WL_CONNECTED) continue;
      const bool nightValid = fetchNight(result);
      while (!lvgl_port_lock(1000)) vTaskDelay(pdMS_TO_TICKS(10));
      if (nightValid || remote.nightStamp.isEmpty()) {
        remote.night = result.night;
        remote.nightStamp = result.nightStamp;
      }
      lvgl_port_unlock();
      if (nightValid) {
        xSemaphoreTake(configMutex, portMAX_DELAY);
        prefs.putString("last_night", result.night);
        prefs.putString("last_night_at", result.nightStamp);
        xSemaphoreGive(configMutex);
      }
    }
    Serial.printf("[NETWORK] end heap=%u largest=%u min=%u psram=%u task_stack=%u\n",
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                  (unsigned)uxTaskGetStackHighWaterMark(nullptr));
  }
}

static uint8_t aht30Crc(const uint8_t *data) {
  uint8_t crc = 0xFF;
  for (int i = 0; i < 6; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit)
      crc = crc & 0x80 ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
  }
  return crc;
}

static void pollIndoorAht30(uint32_t now) {
  if (!aht30_dev_handle) return;
  if (!aht30Pending) {
    if ((int32_t)(now - aht30NextAt) < 0) return;
    aht30NextAt = now + 10000;
    const uint8_t command[] = {0xAC, 0x33, 0x00};
    if (i2c_master_transmit(aht30_dev_handle, command, sizeof(command), 30) == ESP_OK) {
      aht30StartedAt = now;
      aht30Pending = true;
      return;
    }
  } else {
    if ((uint32_t)(now - aht30StartedAt) < 85) return;
    uint8_t data[7];
    if (i2c_master_receive(aht30_dev_handle, data, sizeof(data), 30) == ESP_OK) {
      if ((data[0] & 0x80) && (uint32_t)(now - aht30StartedAt) < 250) return;
      if (!(data[0] & 0x80) && (data[0] & 0x08) && aht30Crc(data) == data[6]) {
        const uint32_t rh = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) |
                            (data[3] >> 4);
        const uint32_t temp = ((uint32_t)(data[3] & 0x0F) << 16) |
                              ((uint32_t)data[4] << 8) | data[5];
        indoorHumidity = rh * (100.0f / 1048576.0f);
        indoorTemperature = temp * (200.0f / 1048576.0f) - 50.0f;
        if (indoorHumidity >= 0 && indoorHumidity <= 100 &&
            indoorTemperature >= -40 && indoorTemperature <= 85) {
          if (!indoorValid) Serial.println("[AHT30] indoor sensor ready");
          indoorValid = true;
          aht30Pending = false;
          return;
        }
      }
    }
    aht30Pending = false;
  }
  if (indoorValid) Serial.println("[AHT30] indoor sensor unavailable");
  indoorValid = false;
}

static void refreshUi() {
  if (!lvgl_port_lock(100)) return;
  tm t; char clockText[12] = "--:--", dateText[40] = "---- / -- / --";
  const char *period = "";
  if (getLocalTime(&t, 10) && t.tm_year >= 124) {
    strftime(clockText, sizeof(clockText), cfg.hour12 ? "%I:%M" : "%H:%M", &t);
    if (cfg.hour12) period = t.tm_hour < 12 ? "AM" : "PM";
    static const char *weekdays[] = {"日", "一", "二", "三", "四", "五", "六"};
    snprintf(dateText, sizeof(dateText), "%04d / %02d / %02d  週%s",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, weekdays[t.tm_wday]);
  }
  homeSetText(clockLabel, clockText);
  homeSetText(clockPeriodLabel, period);
  homeSetText(dateLabel, dateText);
  char status[28];
  snprintf(status, sizeof(status), batteryPresent() ? "Wi-Fi · %d%%" : "Wi-Fi",
           batteryPct);
  homeSetText(wifiLabel, status);
  static bool wifiColorReady = false, wifiColorConnected = false;
  const bool wifiConnected = WiFi.status() == WL_CONNECTED;
  if (!wifiColorReady || wifiConnected != wifiColorConnected) {
    lv_obj_set_style_text_color(wifiLabel,
                                lv_color_hex(wifiConnected ? 0xDCE9E1 : 0x66716C), 0);
    wifiColorReady = true;
    wifiColorConnected = wifiConnected;
  }
  const uint32_t now = millis();
  refreshHomeRows(now);
  if (networkLabel) {
    String status = wifiStatusError == 1 ? "密碼錯誤，請重輸" :
      wifiStatusError == 2 ? (wifiRetryAtMs ? "連線失敗，重試中" : "連線失敗，請重試") :
      wifiScanPending ? "搜尋中..." : WiFi.status() == WL_CONNECTED ?
      "已連線 " + WiFi.SSID() : wifiCandidateActive ? "連線中 " + wifiCandidateSsid :
      wifiAttempt >= 0 ? "連線中 " + wifiProfiles[wifiAttempt].ssid : "未連線";
    homeSetText(networkLabel, status);
  }
  for (int slot = 0; slot < kWeatherSlots; ++slot) {
    if (!weatherPreviewLabels[slot]) continue;
    const WeatherData &item = weatherData[slot];
    const String preview = item.valid ?
        String(item.low) + "-" + item.high + "°C  降雨" + item.rain + "%" :
        item.summary;
    homeSetText(weatherPreviewLabels[slot], preview);
  }
  if (stockPreviewLabel && std::strcmp(lv_label_get_text(stockPreviewLabel),
                                       remote.index.c_str()) != 0)
    lv_label_set_text(stockPreviewLabel, remote.index.c_str());
  if (wifiListDirty) rebuildWifiList();
  if (lv_screen_active() == stockScreen) updateStockScreen();
  if (lv_screen_active() == settingsScreen) {
    updateRadioScreen();
    updateMp3Screen();
  }
  refreshOtaUi();
  lvgl_port_unlock();
}

void setup() {
  Serial.begin(115200);
  otaCheckFirstBoot();
  const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  if (wakeCause == ESP_SLEEP_WAKEUP_EXT0) rtc_gpio_deinit(GPIO_NUM_16);
  gpio_deep_sleep_hold_dis();
  gpio_hold_dis(GPIO_NUM_8);
  pinMode(kPowerButtonPin, INPUT_PULLUP);
  pinMode(kSettingsButtonPin, INPUT_PULLUP);
  powerButton.rawPressed = powerButton.stablePressed = digitalRead(kPowerButtonPin) == LOW;
  powerButton.armed = !powerButton.stablePressed;
  settingsButton.rawPressed = settingsButton.stablePressed =
      digitalRead(kSettingsButtonPin) == LOW;
  settingsButton.armed = !settingsButton.stablePressed;
  Serial.printf("[BOOT] wake=%d power_pin=%d\n", (int)wakeCause, digitalRead(kPowerButtonPin));
  setenv("TZ", kTz, 1); tzset();
  loadConfig();
  configMutex = xSemaphoreCreateMutex();
  audioMutex = xSemaphoreCreateMutex();
  mp3RequestMutex = xSemaphoreCreateMutex();
  networkHttpMutex = xSemaphoreCreateMutex();
  i2c_master_Init();
  Serial.printf("[POWER] latch initialization %s\n", initPowerLatch() ? "ok" : "failed");
  Serial.printf("[TOUCH] I2C %s\n", i2c_touch_probe() ? "ready" : "not found");
  initAudio();
  setSystemFromRtc();
  analogReadResolution(12);
  analogSetPinAttenuation(kBatteryPin, ADC_11db);
  sampleBattery();
  tm bootTime;
  if (rtcRead(&bootTime))
    Serial.printf("[RTC] %04d-%02d-%02d %02d:%02d\n", bootTime.tm_year + 1900,
                  bootTime.tm_mon + 1, bootTime.tm_mday, bootTime.tm_hour, bootTime.tm_min);
  else Serial.println("[RTC] invalid/unavailable");
  Serial.printf("[BAT] %dmV %d%% (voltage estimate)\n", batteryMv, batteryPct);
  Serial.printf("[BAT] display mode=%d\n", cfg.batteryMode);
  lvgl_port_init();
  lcd_bl_pwm_bsp_init(backlightDuty(cfg.brightness));
  if (lvgl_port_lock(2000)) {
    deskFontInitSd();
    buildHome(); buildSettings(); buildStockScreen();
    lv_screen_load(homeScreen);
    lv_mem_monitor_t lvMem;
    lv_mem_monitor(&lvMem);
    Serial.printf("[MEM] lvgl=%u/%u heap=%u largest=%u psram=%u\n",
                  (unsigned)(lvMem.total_size - lvMem.free_size),
                  (unsigned)lvMem.total_size, (unsigned)ESP.getFreeHeap(),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                  (unsigned)ESP.getFreePsram());
    lvgl_port_unlock();
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t info) {
    wifiDisconnectReason.store(info.wifi_sta_disconnected.reason);
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  if (wifiProfileCount) startWifiProfile(0);
  sntp_set_time_sync_notification_cb(onNtpSync);
  configTzTime(kTz, "pool.ntp.org", "time.google.com");
  if (xTaskCreatePinnedToCore(radioAudioTask, "radio", 12288, nullptr, 3,
                              &radioTaskHandle, 1) != pdPASS)
    Serial.println("[RADIO] task creation failed");
  if (xTaskCreatePinnedToCore(networkTask, "network", 8192, nullptr, 1,
                              &networkTaskHandle, 0) != pdPASS)
    Serial.println("[NETWORK] task creation failed");
  lastInputMs = millis();
  Serial.printf("[DeskDock] ready at %u ms\n", (unsigned)millis());
}

void loop() {
  const uint32_t now = millis();
  pollPhysicalButton(powerButton, now, true);
  pollPhysicalButton(settingsButton, now, false);
  if (powerOffRequested) {
    setUpduty(255);
    if (digitalRead(kPowerButtonPin) == LOW) powerReleasedMs = 0;
    else if (!powerReleasedMs) powerReleasedMs = now;
    else if (now - powerReleasedMs >= kPowerReleaseMs) powerOff();
    delay(20);
    return;
  }
  toggleSettingsFromButton();
  otaConfirmFirstBoot(now);
  audioTick();
  collectWifiScan();
  maintainWifi(now);
  pollIndoorAht30(now);
  if (ntpSynced.exchange(false)) {
    tm t; if (getLocalTime(&t, 100)) rtcWrite(t);
    networkRefresh = true;
  }
  if ((uint32_t)(now - lastBatteryMs) >= 15000) sampleBattery();
  const bool dim = cfg.sleepMinutes > 0 &&
      (uint32_t)(now - lastInputMs) >= (uint32_t)cfg.sleepMinutes * 60000UL;
  if (dim != screenDimmed) { screenDimmed = dim; applyBrightness(); }
  if (WiFi.status() == WL_CONNECTED) {
    const bool refresh = networkRefresh.exchange(false);
    uint32_t requests = 0;
    if (refresh || (uint32_t)(now - lastWeatherAttempt) >= kWeatherPeriodMs) {
      lastWeatherAttempt = now;
      requests |= kFetchWeather;
    }
    if (refresh || (uint32_t)(now - lastStockAttempt) >= kStockPeriodMs) {
      lastStockAttempt = now;
      requests |= kFetchMarkets;
    }
    if (intradayWindow() && (refresh || !lastIntradayAttempt ||
        (uint32_t)(now - lastIntradayAttempt) >= kIntradayPeriodMs)) {
      lastIntradayAttempt = now;
      requests |= kFetchIntraday;
    }
    if (requests && networkTaskHandle) xTaskNotify(networkTaskHandle, requests, eSetBits);
  }
  static uint32_t lastUi = 0;
  if ((uint32_t)(now - lastUi) >= 1000) { refreshUi(); lastUi = now; }
  delay(30);
}


