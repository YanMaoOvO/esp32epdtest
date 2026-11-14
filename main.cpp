#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <WiFi.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "U8g2_for_Adafruit_GFX.h"
#include "calendar_utils.h"
#include "config.h"

// ================= 屏幕参数 =================
#define EPD_WIDTH  648
#define EPD_HEIGHT 480
#define BUFFER_SIZE (EPD_WIDTH * EPD_HEIGHT / 8)  // 38880 字节

// ================= 引脚定义 =================
#define CS_PIN   5
#define DC_PIN   17
#define RST_PIN  16
#define BUSY_PIN 4
#define BS_PIN   15
#define SCK_PIN  18
#define MOSI_PIN 23

// NTP 未同步时的合法性判断（2023-11-14 之后都算有效时间）
static const time_t TIME_VALID_EPOCH = 1700000000;

// ================= EPD 驱动 =================
class HINK_EPD : public Adafruit_GFX {
 private:
  uint8_t *_buffer;
  uint8_t *_old_buffer;  // 上一次显示的画布，用于差分刷新
  bool _first_update;
  bool _force_full_refresh;  // 请求下一次刷新强制全刷（清除残影）

  void sendCommand(uint8_t cmd) {
    digitalWrite(CS_PIN, LOW);
    digitalWrite(DC_PIN, LOW);
    SPI.transfer(cmd);
    digitalWrite(CS_PIN, HIGH);
  }
  void sendData(uint8_t data) {
    digitalWrite(CS_PIN, LOW);
    digitalWrite(DC_PIN, HIGH);
    SPI.transfer(data);
    digitalWrite(CS_PIN, HIGH);
  }
  void waitWhileBusy(const char *msg) {
    Serial.print(msg);
    uint32_t start = millis();
    while (digitalRead(BUSY_PIN) == LOW) {
      if (millis() - start > 20000) {
        Serial.println(" TIMEOUT!");
        return;
      }
      delay(10);
    }
    Serial.println(" OK!");
  }

 public:
  HINK_EPD() : Adafruit_GFX(EPD_WIDTH, EPD_HEIGHT) {
    _buffer = (uint8_t *)malloc(BUFFER_SIZE);
    _old_buffer = (uint8_t *)malloc(BUFFER_SIZE);
    _first_update = true;
    _force_full_refresh = false;
    if (!_buffer || !_old_buffer) {
      Serial.println("Buffer malloc failed!");
    }
  }

  void begin() {
    pinMode(BS_PIN, OUTPUT);
    digitalWrite(BS_PIN, LOW);
    pinMode(CS_PIN, OUTPUT);
    pinMode(DC_PIN, OUTPUT);
    pinMode(RST_PIN, OUTPUT);
    pinMode(BUSY_PIN, INPUT);
    digitalWrite(CS_PIN, HIGH);
    digitalWrite(DC_PIN, HIGH);
    digitalWrite(RST_PIN, HIGH);

    SPI.begin(SCK_PIN, -1, MOSI_PIN, CS_PIN);
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

    digitalWrite(RST_PIN, LOW);
    delay(20);
    digitalWrite(RST_PIN, HIGH);
    delay(20);
    waitWhileBusy("Reset ");

    sendCommand(0x00);
    sendData(0x5F);
    sendCommand(0x01);
    sendData(0x03);
    sendData(0x00);
    sendData(0x3F);
    sendData(0x3F);
    sendData(0x0F);
    sendCommand(0x61);
    sendData(0x02);
    sendData(0x88);
    sendData(0x01);
    sendData(0xE0);

    clear();
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if ((x < 0) || (x >= EPD_WIDTH) || (y < 0) || (y >= EPD_HEIGHT)) {
      return;
    }
    uint32_t byte_idx = ((uint32_t)y * EPD_WIDTH + x) / 8;
    uint8_t bit_idx = 7 - (x % 8);
    if (color == 1) {
      _buffer[byte_idx] |= (1 << bit_idx);
    } else {
      _buffer[byte_idx] &= ~(1 << bit_idx);
    }
  }

  void display() {
    Serial.println("Refreshing...");
    sendCommand(0x04);
    waitWhileBusy("PowerON ");

    // DTM1：上一次的画面
    sendCommand(0x10);
    if (_first_update) {
      Serial.println("  -> First update: Forcing full refresh (OLD=Black)");
      for (int i = 0; i < BUFFER_SIZE; i++) {
        sendData(0xFF);
      }
      _first_update = false;
      _force_full_refresh = false;
    } else if (_force_full_refresh) {
      Serial.println("  -> Hourly full refresh: Clearing ghosting (OLD=Black)");
      for (int i = 0; i < BUFFER_SIZE; i++) {
        sendData(0xFF);
      }
      _force_full_refresh = false;
    } else {
      for (int i = 0; i < BUFFER_SIZE; i++) {
        sendData(_old_buffer[i]);
      }
    }

    // DTM2：本次的新画面
    sendCommand(0x13);
    for (int i = 0; i < BUFFER_SIZE; i++) {
      sendData(_buffer[i]);
    }

    // 触发刷新
    sendCommand(0x12);
    waitWhileBusy("Refresh ");
    sendCommand(0x02);
    waitWhileBusy("PowerOFF ");

    // 保存本帧，供下一次差分刷新使用
    memcpy(_old_buffer, _buffer, BUFFER_SIZE);
  }

  // 请求下一次 display() 使用全刷波形（清除残影）
  void requestFullRefresh() {
    _force_full_refresh = true;
  }

  void bufclear() {
    memset(_buffer, 0x00, BUFFER_SIZE);
  }

  void clear() {
    memset(_buffer, 0x00, BUFFER_SIZE);
    display();
  }
};

// ================= 全局对象 =================
HINK_EPD epd;
U8G2_FOR_ADAFRUIT_GFX cjk;

// ================= 辅助函数 =================
static bool wifiConfigured() {
  return WIFI_SSID[0] != '\0';
}

static bool connectWiFi(uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

static void setTimeFromBuild() {
  char month[4] = {0};
  int day = 1;
  int year = 2024;
  int hour = 0;
  int minute = 0;
  int second = 0;

  sscanf(__DATE__, "%3s %d %d", month, &day, &year);
  sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);

  static const char MONTHS[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *p = strstr(MONTHS, month);

  struct tm tm_build = {};
  tm_build.tm_year = year - 1900;
  tm_build.tm_mon = p ? static_cast<int>((p - MONTHS) / 3) : 0;
  tm_build.tm_mday = day;
  tm_build.tm_hour = hour;
  tm_build.tm_min = minute;
  tm_build.tm_sec = second;
  tm_build.tm_isdst = 0;

  const time_t build_time = mktime(&tm_build);
  struct timeval tv = {build_time, 0};
  settimeofday(&tv, nullptr);
}

// 把 U8g2 的 16px 中文字体按整数倍放大绘制（电子墨水无需抗锯齿）
static void drawCjkText(Adafruit_GFX &dst, int x, int baseline, const char *text,
                        uint8_t scale = 2, bool rightAlign = false) {
  if (text == nullptr || *text == '\0') {
    return;
  }

  const int width = cjk.getUTF8Width(text);
  if (rightAlign) {
    x -= width * scale;
  }

  if (scale <= 1) {
    cjk.begin(dst);
    cjk.setCursor(x, baseline);
    cjk.print(text);
    return;
  }

  // 注意：getFontAscent() 返回的是字母 A 的高度（ascent_A），
  // 不含汉字上方的全部墨迹。字体真实包围盒为：
  //   总高度 = max_char_height
  //   基线以上 = max_char_height + y_offset
  const int maxHeight = cjk.u8g2.font_info.max_char_height;
  const int fontAscent = maxHeight + cjk.u8g2.font_info.y_offset;
  const int padX = 3;  // 留出字体左右负偏移
  const int padY = 2;  // 上下各留 2 行，避免边缘被裁掉
  const int canvasWidth = width + padX * 2;
  const int canvasHeight = maxHeight + padY * 2;

  GFXcanvas1 canvas(canvasWidth, canvasHeight);
  canvas.fillScreen(0);

  cjk.begin(canvas);
  cjk.setCursor(padX, padY + fontAscent);
  cjk.print(text);

  const int left = x - padX * scale;
  const int top = baseline - fontAscent - padY;
  for (int row = 0; row < canvasHeight; row++) {
    for (int col = 0; col < canvasWidth; col++) {
      if (canvas.getPixel(col, row)) {
        dst.fillRect(left + col * scale, top + row * scale, scale, scale, 1);
      }
    }
  }

  cjk.begin(dst);
}

static void drawAscii(Adafruit_GFX &dst, const char *text, int x, int baseline,
                      uint8_t size, bool rightAlign = false) {
  dst.setFont(&FreeSansBold24pt7b);
  dst.setTextSize(size);
  dst.setTextColor(1);

  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  dst.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  if (rightAlign) {
    x -= x1 + w;
  }
  dst.setCursor(x, baseline);
  dst.print(text);
}

static void drawAsciiCentered(Adafruit_GFX &dst, const char *text, int left,
                              int right, int baseline, uint8_t size) {
  dst.setFont(&FreeSansBold24pt7b);
  dst.setTextSize(size);
  dst.setTextColor(1);

  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  dst.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  const int x = left + (right - left - static_cast<int>(w)) / 2 - x1;
  dst.setCursor(x, baseline);
  dst.print(text);
}

// ================= 日历页面 =================
static void renderCalendar(const struct tm &now, bool wifiConnected, bool timeValid) {
  epd.bufclear();

  // 外框和分隔线
  epd.drawRect(8, 8, EPD_WIDTH - 16, EPD_HEIGHT - 16, 1);
  epd.drawFastHLine(24, 92, EPD_WIDTH - 48, 1);
  epd.drawFastHLine(24, 344, EPD_WIDTH - 48, 1);
  epd.drawFastVLine(356, 116, 220, 1);

  char buf[80];

  // 顶部：公历年月 + 星期
  snprintf(buf, sizeof(buf), "%d年%d月", now.tm_year + 1900, now.tm_mon + 1);
  drawCjkText(epd, 32, 72, buf, 2);
  drawCjkText(epd, 616, 72, weekdayName(now.tm_wday), 2, true);

  // 大号公历日期
  snprintf(buf, sizeof(buf), "%d", now.tm_mday);
  drawAsciiCentered(epd, buf, 32, 344, 296, 4);

  // 时间
  snprintf(buf, sizeof(buf), "%02d:%02d", now.tm_hour, now.tm_min);
  drawAscii(epd, buf, 616, 284, 2, true);

  // 农历、干支、节气
  const LunarDate lunar = solarToLunar(now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);
  if (lunar.year > 0) {
    formatLunarDate(lunar, buf, sizeof(buf));
    drawCjkText(epd, 32, 398, buf, 2);

    char ganzhi[8];
    char term[48];
    formatGanzhiYear(lunar.year, ganzhi, sizeof(ganzhi));
    formatSolarTermInfo(now, term, sizeof(term));
    snprintf(buf, sizeof(buf), "%s%s年  %s", ganzhi, zodiacName(lunar.year), term);
    drawCjkText(epd, 32, 448, buf, 2);
  } else {
    drawCjkText(epd, 32, 398, "农历数据超范围", 2);
  }

  // 右下角联网状态
  const char *status = "离线模式";
  if (wifiConfigured() && wifiConnected) {
    status = timeValid ? "网络在线" : "正在对时";
  }
  drawCjkText(epd, 616, 460, status, 1, true);
  drawCjkText(epd, 606, 480,"YanMaoOvO/esp32epdtest/", 1, true);

  // 恢复默认 GFX 字体，避免影响后续绘制
  epd.setFont();
}

// ================= 主程序 =================
static int lastHour = -1;
static int lastMinute = -1;
static int lastDay = -1;
static int lastFullRefreshHour = -1;
static bool lastWifiConnected = false;
static bool lastTimeValid = false;
static bool wifiWasConnected = false;
static uint32_t lastWifiAttempt = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nStarting E-Ink Calendar...");

  epd.begin();

  // 时区必须在对时/解析编译时间之前设置
  setenv("TZ", TIMEZONE, 1);
  tzset();

  // 中文字体
  cjk.begin(epd);
  cjk.setFontMode(1);
  cjk.setFontDirection(0);
  cjk.setForegroundColor(1);
  cjk.setBackgroundColor(0);
  cjk.setFont(u8g2_font_wqy16_t_gb2312);
  Serial.printf("CJK font bounds: maxH=%d yOff=%d ascent=%d\n",
                cjk.u8g2.font_info.max_char_height,
                cjk.u8g2.font_info.y_offset,
                cjk.u8g2.font_info.max_char_height + cjk.u8g2.font_info.y_offset);

  if (wifiConfigured()) {
    Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
    if (connectWiFi(15000)) {
      Serial.print("WiFi connected, IP: ");
      Serial.println(WiFi.localIP());
      configTzTime(TIMEZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);

      const uint32_t start = millis();
      while (time(nullptr) < TIME_VALID_EPOCH && millis() - start < 10000) {
        delay(200);
      }
    } else {
      Serial.println("WiFi connect failed");
    }
  } else {
    Serial.println("WiFi not configured. Edit include/config.h to enable NTP.");
  }

  if (time(nullptr) < TIME_VALID_EPOCH) {
    Serial.println("Using firmware build time");
    setTimeFromBuild();
  }

  const time_t now_epoch = time(nullptr);
  struct tm now_tm;
  localtime_r(&now_epoch, &now_tm);

  Serial.printf("Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday,
                now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);

  const bool connected = WiFi.status() == WL_CONNECTED;
  const bool time_valid = now_epoch >= TIME_VALID_EPOCH;

  renderCalendar(now_tm, connected, time_valid);
  epd.display();

  lastHour = now_tm.tm_hour;
  lastMinute = now_tm.tm_min;
  lastDay = now_tm.tm_mday;
  lastFullRefreshHour = now_tm.tm_hour;
  lastWifiConnected = connected;
  lastTimeValid = time_valid;
  wifiWasConnected = connected;
  lastWifiAttempt = millis();

  Serial.println("Calendar ready.");
}

void loop() {
  // WiFi 掉线后每分钟尝试重连一次
  bool connected = WiFi.status() == WL_CONNECTED;
  if (wifiConfigured() && !connected && millis() - lastWifiAttempt > 60000) {
    lastWifiAttempt = millis();
    Serial.println("WiFi reconnect...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wifiWasConnected) {
    configTzTime(TIMEZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
  }
  wifiWasConnected = connected;

  const time_t now_epoch = time(nullptr);
  struct tm now_tm;
  localtime_r(&now_epoch, &now_tm);
  const bool time_valid = now_epoch >= TIME_VALID_EPOCH;

  // 只在分钟变化、日期变化或联网状态变化时刷新，延长屏幕寿命
  if (now_tm.tm_hour != lastHour || now_tm.tm_min != lastMinute ||
      now_tm.tm_mday != lastDay || connected != lastWifiConnected ||
      time_valid != lastTimeValid) {
    // 每小时第一次刷新时强制全刷，清除累积残影
    if (now_tm.tm_hour != lastFullRefreshHour) {
      epd.requestFullRefresh();
      lastFullRefreshHour = now_tm.tm_hour;
      Serial.println("Hourly full refresh requested");
    }

    renderCalendar(now_tm, connected, time_valid);
    epd.display();

    lastHour = now_tm.tm_hour;
    lastMinute = now_tm.tm_min;
    lastDay = now_tm.tm_mday;
    lastWifiConnected = connected;
    lastTimeValid = time_valid;
  }

  delay(200);
}
