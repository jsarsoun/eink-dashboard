// ─────────────────────────────────────────────────────────────────────────────
// E-Ink Dashboard — XIAO ESP32-S3
// Hardware: Seeed XIAO ESP32-S3 + Waveshare 7.5" 800x480 three-color HAT (B)
//
// Wiring (XIAO label → GPIO → HAT pin):
//   D1  → GPIO2  → CS
//   D2  → GPIO3  → DC
//   D3  → GPIO4  → RST
//   D4  → GPIO5  → BUSY
//   D5  → GPIO6  → PWR (power enable)
//   D8  → GPIO7  → CLK  (hardware SPI SCK)
//   D10 → GPIO9  → DIN  (hardware SPI MOSI)
//   3V3 → 3.3V   → VCC
//   GND → GND    → GND
// ─────────────────────────────────────────────────────────────────────────────

#include "secrets.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <math.h>

// ── User configuration ────────────────────────────────────────────────────────
// WIFI_SSID, WIFI_PASSWORD, SERVER_URL, WAKE_URL are defined in secrets.h

const int DEFAULT_REFRESH_MINUTES = 60;

// ── Pin definitions (XIAO ESP32-S3) ──────────────────────────────────────────
#define EPD_PWR   6   // D5 — power enable (HIGH = on)
#define EPD_CS    2   // D1 — SPI chip select
#define EPD_DC    3   // D2 — data / command
#define EPD_RST   4   // D3 — reset
#define EPD_BUSY  5   // D4 — busy (LOW = busy)
// Hardware SPI (XIAO defaults — no defines needed):
//   CLK  → GPIO7  (D8)
//   MOSI → GPIO9  (D10)

// ── Display driver ────────────────────────────────────────────────────────────
GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 4> display(
  GxEPD2_750c_Z08(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

// ── Weather icon helpers ───────────────────────────────────────────────────────

uint8_t wmoIcon(int code) {
  if (code <= 1)                                                 return 0; // sun
  if (code == 2)                                                 return 1; // partly cloudy
  if (code == 3)                                                 return 2; // cloud
  if (code == 45 || code == 48)                                  return 3; // fog
  if (code >= 51 && code <= 55)                                  return 4; // drizzle
  if ((code >= 61 && code <= 65) || (code >= 80 && code <= 82)) return 5; // rain
  if ((code >= 71 && code <= 77) || code == 85 || code == 86)   return 6; // snow
  return 7; // thunder
}

void iconSun(int cx, int cy, int r, uint16_t col) {
  int core  = r * 42 / 100;
  int inner = core + max(2, r / 12);
  display.fillCircle(cx, cy, core, col);
  for (int i = 0; i < 8; i++) {
    float a = i * PI / 4.0f;
    display.drawLine(cx + inner * cosf(a), cy + inner * sinf(a),
                     cx + r     * cosf(a), cy + r     * sinf(a), col);
  }
}

void iconCloud(int cx, int cy, int r, uint16_t col) {
  int bR    = r * 32 / 100;
  int cR    = r * 42 / 100;
  int bodyY = cy + r * 5 / 100;
  int bodyH = r * 45 / 100;
  display.fillRoundRect(cx - r, bodyY, r * 2, bodyH, bodyH / 3, col);
  display.fillCircle(cx - r * 35 / 100, bodyY, bR, col);
  display.fillCircle(cx + r * 35 / 100, bodyY, bR, col);
  display.fillCircle(cx,                bodyY - r * 15 / 100, cR, col);
}

void iconFog(int cx, int cy, int r, uint16_t col) {
  int barH = max(2, r * 12 / 100);
  for (int i = 0; i < 4; i++) {
    int barW = r * (90 - abs(i - 1) * 15) / 100;
    int fy   = cy - r / 2 + i * (r * 18 / 100);
    display.fillRoundRect(cx - barW, fy, barW * 2, barH, barH / 2, col);
  }
}

void iconRainDrops(int cx, int cy, int r, int count, uint16_t col) {
  int len     = r * 30 / 100;
  int spread  = r * 54 / 100;
  int spacing = (count > 1) ? spread / (count - 1) : 0;
  int startX  = cx - spread / 2;
  for (int i = 0; i < count; i++) {
    int dx = startX + i * spacing;
    display.drawLine(dx, cy, dx - len / 3, cy + len, col);
    if (r > 30) display.drawLine(dx + 1, cy, dx + 1 - len / 3, cy + len, col);
  }
}

void iconSnowDots(int cx, int cy, int r, int count, uint16_t col) {
  int dotR    = max(1, r * 9 / 100);
  int spread  = r * 54 / 100;
  int spacing = (count > 1) ? spread / (count - 1) : 0;
  int startX  = cx - spread / 2;
  for (int i = 0; i < count; i++) {
    display.fillCircle(startX + i * spacing, cy, dotR, col);
  }
}

void iconLightning(int cx, int cy, int h, uint16_t col) {
  int w = h * 45 / 100;
  display.fillTriangle(cx + w / 2, cy,
                        cx - w / 2, cy + h * 55 / 100,
                        cx + w / 4, cy + h * 55 / 100, col);
  display.fillTriangle(cx - w / 4, cy + h * 45 / 100,
                        cx + w / 2, cy + h,
                        cx - w / 2, cy + h * 45 / 100, col);
}

const char* wmoCondition(int code) {
  if (code <= 1)                                                 return "Clear";
  if (code == 2)                                                 return "Pt. Cloudy";
  if (code == 3)                                                 return "Cloudy";
  if (code == 45 || code == 48)                                  return "Fog";
  if (code >= 51 && code <= 55)                                  return "Drizzle";
  if ((code >= 61 && code <= 65) || (code >= 80 && code <= 82)) return "Rain";
  if ((code >= 71 && code <= 77) || code == 85 || code == 86)   return "Snow";
  return "Thunder";
}

void drawWeatherIcon(int cx, int cy, int r, int wmoCode, uint16_t col) {
  uint8_t type = wmoIcon(wmoCode);
  int clR = r * 75 / 100;

  switch (type) {
    case 0: // Sun
      iconSun(cx, cy, r, col);
      break;

    case 1: { // Partly cloudy
      int sunCx = cx - r * 30 / 100, sunCy = cy - r * 20 / 100, sunR = r * 55 / 100;
      int clCx  = cx + r * 20 / 100, clCy  = cy + r * 20 / 100;
      iconSun(sunCx, sunCy, sunR, col);
      if (r >= 28) {
        display.fillRect(clCx - clR - 1, clCy - clR - 1, clR * 2 + 2, clR * 2 + 2, GxEPD_WHITE);
      }
      iconCloud(clCx, clCy, clR, GxEPD_BLACK);
      break;
    }

    case 2: // Cloud
      iconCloud(cx, cy, r, GxEPD_BLACK);
      break;

    case 3: // Fog
      iconFog(cx, cy, r, GxEPD_BLACK);
      break;

    case 4: // Drizzle — cloud + 3 light drops
      iconCloud(cx, cy - r * 20 / 100, clR, GxEPD_BLACK);
      iconRainDrops(cx, cy + r * 40 / 100, r, 3, GxEPD_BLACK);
      break;

    case 5: // Rain — cloud + 4 drops
      iconCloud(cx, cy - r * 20 / 100, clR, GxEPD_BLACK);
      iconRainDrops(cx, cy + r * 40 / 100, r, 4, GxEPD_BLACK);
      break;

    case 6: // Snow — cloud + dots
      iconCloud(cx, cy - r * 20 / 100, clR, GxEPD_BLACK);
      iconSnowDots(cx, cy + r * 55 / 100, r, 3, GxEPD_BLACK);
      break;

    case 7: // Thunder — cloud + bolt
      iconCloud(cx, cy - r * 30 / 100, clR, GxEPD_BLACK);
      iconLightning(cx - r * 15 / 100, cy + r * 10 / 100, r * 55 / 100, col);
      break;
  }
}

// ── Error display ─────────────────────────────────────────────────────────────
void showError(const char* message) {
  display.init(115200, true, 10, false);
  display.setPartialWindow(
    display.width() - 260, display.height() - 30,
    260, 24
  );
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(display.width() - 258, display.height() - 12);
    display.print(message);
  } while (display.nextPage());
  display.hibernate();
}

// ── Layout constants ──────────────────────────────────────────────────────────
const int MARGIN      = 20;
const int COL_RIGHT_X = display.width() / 2 + MARGIN;
const int HEADER_Y    = 68;
const int DIVIDER_Y   = 84;
const int CONTENT_Y   = 140;

// ── Render ────────────────────────────────────────────────────────────────────
void renderDisplay(
  const char* date,
  int tempF, const char* condition, int highF, int lowF, int precipPct,
  int wmoCode,
  const char* updatedAt,
  JsonArray forecast,
  JsonArray events, int totalEventCount,
  const char* alertEvent   // nullptr = no alert
) {
  display.init(115200, true, 10, false);
  display.setFullWindow();
  display.firstPage();

  const bool hasAlert = alertEvent && strlen(alertEvent) > 0;
  const int  contentY = CONTENT_Y + (hasAlert ? 36 : 0);

  do {
    display.fillScreen(GxEPD_WHITE);

    display.setFont(&FreeMonoBold24pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(MARGIN, HEADER_Y);
    display.print(date);

    display.drawLine(MARGIN, DIVIDER_Y, display.width() - MARGIN, DIVIDER_Y, GxEPD_BLACK);
    display.drawLine(
      display.width() / 2, DIVIDER_Y + 10,
      display.width() / 2, display.height() - MARGIN,
      GxEPD_BLACK
    );

    // ── Weather alert badge (full width, between divider and content) ─────────
    if (hasAlert) {
      const int BADGE_Y = DIVIDER_Y + 2;
      const int BADGE_H = 34;

      display.fillRect(0, BADGE_Y, display.width(), BADGE_H, GxEPD_RED);

      display.setFont(&FreeMonoBold12pt7b);
      display.setTextColor(GxEPD_WHITE);
      display.setCursor(10, BADGE_Y + 23);
      display.printf("! %s", alertEvent);
    }

    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold18pt7b);
    display.setCursor(MARGIN, contentY);
    display.print("WEATHER");

    display.setFont(&FreeMonoBold24pt7b);
    display.setCursor(MARGIN, contentY + 75);
    display.printf("%d F", tempF);

    display.setFont(&FreeMonoBold18pt7b);
    display.setCursor(MARGIN, contentY + 120);
    display.print(condition);

    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(MARGIN, contentY + 158);
    display.setTextColor(GxEPD_RED);
    display.printf("H: %d", highF);
    display.setTextColor(GxEPD_BLACK);
    display.printf("  L: %d", lowF);
    display.setCursor(MARGIN, contentY + 185);
    display.printf("Rain: %d%%", precipPct);

    // ── Large weather icon (right of "WEATHER" header) ───────────────────────
    drawWeatherIcon(230, contentY + 5, 55, wmoCode, GxEPD_RED);

    // ── 3-day forecast strip ──────────────────────────────────────────────────
    const int FORECAST_Y  = contentY + 215;
    const int HALF_W      = display.width() / 2;
    const int COL_W       = (HALF_W - MARGIN * 2) / 3;

    display.drawLine(MARGIN, FORECAST_Y - 8, HALF_W - MARGIN, FORECAST_Y - 8, GxEPD_BLACK);

    int fi = 0;
    for (JsonObject day : forecast) {
      const char* dayName = day["day"] | "---";
      int fHigh = day["high"]        | 0;
      int fLow  = day["low"]         | 0;
      int fCode = day["weatherCode"] | 0;
      int colX  = MARGIN + fi * COL_W;

      display.setFont(&FreeMonoBold12pt7b);
      display.setTextColor(GxEPD_BLACK);
      display.setCursor(colX, FORECAST_Y + 16);
      display.print(dayName);

      display.setFont(&FreeMonoBold9pt7b);
      display.setTextColor(GxEPD_RED);
      display.setCursor(colX, FORECAST_Y + 34);
      display.printf("%d", fHigh);
      display.setTextColor(GxEPD_BLACK);
      display.printf("/%d", fLow);

      display.setCursor(colX, FORECAST_Y + 52);
      display.print(wmoCondition(fCode));

      drawWeatherIcon(colX + COL_W / 2, FORECAST_Y + 80, 18, fCode, GxEPD_BLACK);

      if (++fi >= 3) break;
    }
    display.setTextColor(GxEPD_BLACK);

    display.setFont(&FreeMonoBold18pt7b);
    display.setCursor(COL_RIGHT_X, contentY);
    display.print("TODAY");

    int eventY = contentY + 45;
    if (totalEventCount == 0) {
      display.setFont(&FreeMonoBold12pt7b);
      display.setCursor(COL_RIGHT_X, eventY);
      display.print("No events today");
    } else {
      for (JsonObject event : events) {
        const char* title = event["title"] | "Untitled";
        const char* time  = event["time"]  | "";

        display.setFont(&FreeMonoBold12pt7b);
        display.setCursor(COL_RIGHT_X, eventY);
        display.print(time);
        eventY += 24;

        String titleStr = String(title);
        if (titleStr.length() > 28) titleStr = titleStr.substring(0, 27) + "...";
        display.setCursor(COL_RIGHT_X, eventY);
        display.print(titleStr);
        eventY += 34;
      }
      if (totalEventCount > 3) {
        display.setFont(&FreeMonoBold12pt7b);
        display.setCursor(COL_RIGHT_X, eventY);
        display.printf("+ %d more", totalEventCount - 3);
      }
    }
    // ── Last updated timestamp (bottom-right) ────────────────────────────────
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    char tsBuf[32];
    snprintf(tsBuf, sizeof(tsBuf), "Updated %s", updatedAt);
    int16_t tx, ty; uint16_t tw, th;
    display.getTextBounds(tsBuf, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor(display.width() - MARGIN - (int)tw, display.height() - 8);
    display.print(tsBuf);

  } while (display.nextPage());

  display.hibernate();
}

// ── Fetch and render ──────────────────────────────────────────────────────────
// Returns the server-provided refreshRateMinutes, or DEFAULT_REFRESH_MINUTES on error.
int fetchAndRender() {
  HTTPClient http;
  http.begin(SERVER_URL);
  http.setTimeout(10000);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.printf("HTTP error: %d\n", httpCode);
    const char* errorMsg = "server unreachable";
    String body = http.getString();
    JsonDocument errorDoc;
    if (deserializeJson(errorDoc, body) == DeserializationError::Ok) {
      const char* msg = errorDoc["error"];
      if (msg) errorMsg = msg;
    }
    http.end();
    showError(errorMsg);
    return DEFAULT_REFRESH_MINUTES;
  }

  JsonDocument doc;
  DeserializationError parseErr = deserializeJson(doc, http.getStream());
  http.end();

  if (parseErr) {
    Serial.printf("JSON parse failed: %s\n", parseErr.f_str());
    return DEFAULT_REFRESH_MINUTES;
  }

  const char* date        = doc["date"]                           | "Unknown Date";
  int   tempF             = doc["weather"]["tempF"]               | 0;
  const char* condition   = doc["weather"]["condition"]           | "";
  int   highF             = doc["weather"]["highF"]               | 0;
  int   lowF              = doc["weather"]["lowF"]                | 0;
  int   precipPct         = doc["weather"]["precipitationPct"]    | 0;
  int   wmoCode           = doc["weather"]["weatherCode"]         | 0;
  const char* updatedAt   = doc["updatedAt"]                      | "";
  JsonArray forecast      = doc["weather"]["forecast"];
  JsonArray events        = doc["calendar"]["events"];
  int   totalCount        = doc["calendar"]["totalCount"]         | 0;
  const char* alertEvent  = doc["alerts"]["event"] | (const char*)nullptr;
  int   refreshMinutes    = doc["settings"]["refreshRateMinutes"] | DEFAULT_REFRESH_MINUTES;

  renderDisplay(date, tempF, condition, highF, lowF, precipPct, wmoCode, updatedAt, forecast, events, totalCount, alertEvent);
  Serial.println("Display updated.");
  return refreshMinutes;
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);

  // 0. Power on the display
  pinMode(EPD_PWR, OUTPUT);
  digitalWrite(EPD_PWR, HIGH);

  // 1. Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi failed — restarting in 30s");
    showError("server unreachable");
    delay(30000);
    ESP.restart();
  }
  Serial.printf("\nWiFi connected — IP: %s\n", WiFi.localIP().toString().c_str());

  // 2. Fetch, render, then deep sleep
  int refreshMinutes = fetchAndRender();

  Serial.printf("Sleeping for %d minutes.\n", refreshMinutes);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_timer_wakeup((uint64_t)refreshMinutes * 60ULL * 1000000ULL);
  esp_deep_sleep_start();
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  // All logic runs in setup(); loop is never reached after deep sleep.
}
