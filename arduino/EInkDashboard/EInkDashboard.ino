// ─────────────────────────────────────────────────────────────────────────────
// E-Ink Dashboard
// Hardware: Freenove ESP32-WROOM + Waveshare 7.5" 800x480 three-color HAT (B)
//
// Required libraries (install via Arduino Library Manager):
//   - GxEPD2          by ZinggJM
//   - ArduinoJson     by Benoit Blanchon  (v7.x)
//   - Adafruit GFX    by Adafruit
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

// ── Weather condition bitmaps (16×16 px, 1-bit, MSB first, PROGMEM) ──────────
// Bit layout: byte[2*row] bit7 = col0, bit0 = col7; byte[2*row+1] bit7 = col8, bit0 = col15

// ☀  Sun: filled circle (cols 5-9, rows 4-8 core) with N/S/E/W rays
static const uint8_t PROGMEM icon_sun[] = {
  0x00,0x00,  // row  0
  0x01,0x00,  // row  1: top ray col 7
  0x01,0x00,  // row  2: top ray col 7
  0x00,0x00,  // row  3
  0x03,0x80,  // row  4: circle top cols 6-8
  0x07,0xC0,  // row  5: circle cols 5-9
  0xC7,0xC6,  // row  6: E/W rays (cols 0-1, 13-14) + circle cols 5-9
  0x07,0xC0,  // row  7: circle cols 5-9
  0x03,0x80,  // row  8: circle bottom cols 6-8
  0x00,0x00,  // row  9
  0x01,0x00,  // row 10: bottom ray col 7
  0x01,0x00,  // row 11: bottom ray col 7
  0x00,0x00,  // row 12
  0x00,0x00,  // row 13
  0x00,0x00,  // row 14
  0x00,0x00,  // row 15
};

// ⛅  Partly cloudy: tiny sun upper-left + cloud lower-right
static const uint8_t PROGMEM icon_partCloud[] = {
  0x20,0x00,  // row  0: sun top ray col 2
  0x70,0x00,  // row  1: sun body cols 1-3
  0x20,0x00,  // row  2: sun bottom col 2
  0x00,0x00,  // row  3
  0x03,0x80,  // row  4: cloud top cols 6-8
  0x07,0xE0,  // row  5: cloud cols 5-10
  0x0F,0xF8,  // row  6: cloud cols 4-12
  0x1F,0xFC,  // row  7: cloud cols 3-13
  0x1F,0xFC,  // row  8: cloud cols 3-13
  0x0F,0xF8,  // row  9: cloud bottom cols 4-12
  0x00,0x00,  // row 10
  0x00,0x00,  // row 11
  0x00,0x00,  // row 12
  0x00,0x00,  // row 13
  0x00,0x00,  // row 14
  0x00,0x00,  // row 15
};

// ☁  Cloud / Overcast: large cloud centered in rows 4-11
static const uint8_t PROGMEM icon_cloud[] = {
  0x00,0x00,  // row  0
  0x00,0x00,  // row  1
  0x00,0x00,  // row  2
  0x00,0x00,  // row  3
  0x07,0x00,  // row  4: top bump cols 5-7
  0x1F,0xC0,  // row  5: cols 3-9
  0x3F,0xE0,  // row  6: cols 2-10
  0x7F,0xF0,  // row  7: cols 1-11
  0xFF,0xF8,  // row  8: cols 0-12
  0xFF,0xFC,  // row  9: cols 0-13
  0xFF,0xFC,  // row 10: cols 0-13
  0x7F,0xF8,  // row 11: bottom taper cols 1-12
  0x00,0x00,  // row 12
  0x00,0x00,  // row 13
  0x00,0x00,  // row 14
  0x00,0x00,  // row 15
};

// 🌫  Fog: 4 horizontal bands
static const uint8_t PROGMEM icon_fog[] = {
  0x00,0x00,  // row  0
  0x00,0x00,  // row  1
  0x00,0x00,  // row  2
  0x7F,0xFE,  // row  3: cols 1-14
  0x00,0x00,  // row  4
  0x7F,0xFE,  // row  5: cols 1-14
  0x00,0x00,  // row  6
  0x7F,0xFE,  // row  7: cols 1-14
  0x00,0x00,  // row  8
  0x7F,0xFE,  // row  9: cols 1-14
  0x00,0x00,  // row 10
  0x00,0x00,  // row 11
  0x00,0x00,  // row 12
  0x00,0x00,  // row 13
  0x00,0x00,  // row 14
  0x00,0x00,  // row 15
};

// 🌦  Drizzle: cloud + 3 sparse scattered drops
static const uint8_t PROGMEM icon_drizzle[] = {
  0x00,0x00,  // row  0
  0x0E,0x00,  // row  1: cloud
  0x1F,0x80,  // row  2: cloud
  0x3F,0xE0,  // row  3: cloud
  0x7F,0xF0,  // row  4: cloud
  0x7F,0xF0,  // row  5: cloud
  0x3F,0xE0,  // row  6: cloud
  0x00,0x00,  // row  7
  0x00,0x00,  // row  8
  0x22,0x20,  // row  9: drops cols 2, 6, 10
  0x00,0x00,  // row 10
  0x22,0x20,  // row 11: drops cols 2, 6, 10
  0x00,0x00,  // row 12
  0x00,0x00,  // row 13
  0x00,0x00,  // row 14
  0x00,0x00,  // row 15
};

// 🌧  Rain: cloud + 3 vertical rain lines
static const uint8_t PROGMEM icon_rain[] = {
  0x00,0x00,  // row  0
  0x0E,0x00,  // row  1: cloud
  0x1F,0x80,  // row  2: cloud
  0x3F,0xE0,  // row  3: cloud
  0x7F,0xF0,  // row  4: cloud
  0x7F,0xF0,  // row  5: cloud
  0x3F,0xE0,  // row  6: cloud
  0x00,0x00,  // row  7
  0x22,0x20,  // row  8: rain cols 2, 6, 10
  0x22,0x20,  // row  9
  0x22,0x20,  // row 10
  0x22,0x20,  // row 11
  0x00,0x00,  // row 12
  0x00,0x00,  // row 13
  0x00,0x00,  // row 14
  0x00,0x00,  // row 15
};

// ❄  Snow: cloud + asterisk snowflake
static const uint8_t PROGMEM icon_snow[] = {
  0x00,0x00,  // row  0
  0x0E,0x00,  // row  1: cloud
  0x1F,0x80,  // row  2: cloud
  0x3F,0xE0,  // row  3: cloud
  0x7F,0xF0,  // row  4: cloud
  0x7F,0xF0,  // row  5: cloud
  0x3F,0xE0,  // row  6: cloud
  0x00,0x00,  // row  7
  0x04,0x00,  // row  8: snowflake N spoke col 5
  0x3F,0x80,  // row  9: snowflake horizontal bar cols 2-8
  0x15,0x00,  // row 10: snowflake diagonals cols 3,5,7
  0x3F,0x80,  // row 11: snowflake horizontal bar
  0x04,0x00,  // row 12: snowflake S spoke col 5
  0x00,0x00,  // row 13
  0x00,0x00,  // row 14
  0x00,0x00,  // row 15
};

// ⛈  Storm: cloud + lightning bolt
static const uint8_t PROGMEM icon_storm[] = {
  0x00,0x00,  // row  0
  0x0E,0x00,  // row  1: cloud
  0x1F,0x80,  // row  2: cloud
  0x3F,0xE0,  // row  3: cloud
  0x7F,0xF0,  // row  4: cloud
  0x7F,0xF0,  // row  5: cloud
  0x3F,0xE0,  // row  6: cloud
  0x00,0x00,  // row  7
  0x03,0x80,  // row  8: bolt top cols 6-8
  0x07,0x00,  // row  9: bolt cols 5-7
  0x1F,0x80,  // row 10: bolt wide cols 3-8
  0x0E,0x00,  // row 11: bolt cols 4-6
  0x1C,0x00,  // row 12: bolt tip cols 3-5
  0x00,0x00,  // row 13
  0x00,0x00,  // row 14
  0x00,0x00,  // row 15
};

// Maps WMO weather code to the correct PROGMEM bitmap
static const uint8_t* getWeatherIcon(int code) {
  if (code <= 1)                                      return icon_sun;
  if (code == 2)                                      return icon_partCloud;
  if (code == 3)                                      return icon_cloud;
  if (code == 45 || code == 48)                       return icon_fog;
  if (code >= 51 && code <= 57)                       return icon_drizzle;
  if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return icon_rain;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86)   return icon_snow;
  return icon_storm; // 95, 96, 99
}

// ── User configuration ────────────────────────────────────────────────────────
// WIFI_SSID, WIFI_PASSWORD, SERVER_URL are defined in secrets.h

// Fallback sleep duration used when the server returns an error.
// Keep this value in sync with REFRESH_RATE_MINUTES in your server's .env
const int DEFAULT_REFRESH_MINUTES = 60;

// ── Static IP ─────────────────────────────────────────────────────────────────
// Assigning a static IP skips DHCP negotiation, cutting WiFi-on time by ~1-2s.
// Uncomment WiFi.config() in setup() to use this instead of DHCP.
IPAddress STATIC_IP(192, 168,  50, 200);
IPAddress GATEWAY  (192, 168,  50,   1);
IPAddress SUBNET   (255, 255, 255,   0);
IPAddress DNS      (  8,   8,   8,   8);

// ── Pin definitions (Freenove ESP32-WROOM on Breakout Board v1.2) ─────────────
#define EPD_PWR  15  // Power enable (HIGH = on)
#define EPD_CS    5  // SPI chip select
#define EPD_DC   17  // Data / Command
#define EPD_RST  16  // Reset
#define EPD_BUSY  4  // Busy (LOW = busy)
// SPI hardware (VSPI defaults — no define needed):
//   CLK  → GPIO18
//   MOSI → GPIO23

// ── Display driver ────────────────────────────────────────────────────────────
// Waveshare 7.5" HAT (B) — 800x480, red/black/white
// GxEPD2_750c_Z08 = correct driver (Z90 is wrong — it's 880x528)
GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 4> display(
  GxEPD2_750c_Z08(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

// ── Deep sleep ────────────────────────────────────────────────────────────────
void deepSleepMinutes(int minutes) {
  if (minutes <= 0) minutes = DEFAULT_REFRESH_MINUTES;
  Serial.printf("Sleeping for %d minutes\n", minutes);
  Serial.flush();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_sleep_enable_timer_wakeup((uint64_t)minutes * 60 * 1000000ULL);
  esp_deep_sleep_start();
}

// ── Error display ─────────────────────────────────────────────────────────────
// Shows a short error message in the bottom-right corner without clearing the
// rest of the display.
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
const int MARGIN        = 20;
const int COL_DIVIDER_X = 500;                  // weather=500px, calendar=300px
const int COL_RIGHT_X   = COL_DIVIDER_X + MARGIN;
const int HEADER_Y    = 68;   // Date baseline (24pt)
const int DIVIDER_Y   = 84;   // Horizontal divider
const int CONTENT_Y   = 140;  // First content row baseline

// ── Scaled bitmap helper ──────────────────────────────────────────────────────
// Renders a 16×16 PROGMEM bitmap scaled to (16*scale) × (16*scale) pixels.
static void drawScaledIcon(int x, int y, const uint8_t* bmp, int scale) {
  for (int r = 0; r < 16; r++) {
    for (int c = 0; c < 16; c++) {
      uint8_t b = pgm_read_byte(&bmp[r * 2 + c / 8]);
      if ((b >> (7 - (c % 8))) & 1) {
        display.fillRect(x + c * scale, y + r * scale, scale, scale, GxEPD_BLACK);
      }
    }
  }
}

// ── Forecast cell helper ──────────────────────────────────────────────────────
// Draws one cell in the 3-column forecast grid: 32×32 icon centered,
// abbreviated day name centered below, high/low temp centered below that.
static const int FORECAST_CELL_W = 160;  // (COL_DIVIDER_X - MARGIN) / 3 = 480/3

void drawForecastCell(int cellX, int cellTopY, const char* day, int high, int low, int wmoCode) {
  // Day name centered at top of cell (12pt)
  display.setFont(&FreeMonoBold12pt7b);
  display.setTextColor(GxEPD_BLACK);
  int16_t tx, ty; uint16_t tw, th;
  display.getTextBounds(day, 0, 0, &tx, &ty, &tw, &th);
  display.setCursor(cellX + (FORECAST_CELL_W - (int)tw) / 2, cellTopY + 16);
  display.print(day);
  // Icon centered below day name (3× scale = 48×48)
  drawScaledIcon(cellX + (FORECAST_CELL_W - 48) / 2, cellTopY + 22, getWeatherIcon(wmoCode), 3);
  // High/low centered below icon (12pt)
  char tempBuf[10];
  snprintf(tempBuf, sizeof(tempBuf), "%d/%d", high, low);
  display.getTextBounds(tempBuf, 0, 0, &tx, &ty, &tw, &th);
  display.setCursor(cellX + (FORECAST_CELL_W - (int)tw) / 2, cellTopY + 22 + 48 + 14);
  display.print(tempBuf);
}

// ── Render ────────────────────────────────────────────────────────────────────
void renderDisplay(
  const char* date,
  int tempF, const char* condition, int highF, int lowF, int precipPct,
  int weatherCode,
  const char* updatedAt,
  JsonArray forecast,
  JsonArray events, int totalEventCount
) {
  display.init(115200, true, 10, false);
  display.setFullWindow();
  display.firstPage();

  do {
    display.fillScreen(GxEPD_WHITE);

    // ── Date (black) ────────────────────────────────────────────────────────
    display.setFont(&FreeMonoBold24pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(MARGIN, HEADER_Y);
    display.print(date);

    // Horizontal divider
    display.drawLine(MARGIN, DIVIDER_Y, display.width() - MARGIN, DIVIDER_Y, GxEPD_BLACK);

    // Vertical divider
    display.drawLine(
      COL_DIVIDER_X, DIVIDER_Y + 10,
      COL_DIVIDER_X, display.height() - MARGIN,
      GxEPD_BLACK
    );

    // ── Weather (left column) ────────────────────────────────────────────────
    display.setTextColor(GxEPD_BLACK);

    display.setFont(&FreeMonoBold18pt7b);
    display.setCursor(MARGIN, CONTENT_Y);
    display.print("WEATHER");
    // Large condition icon — left edge, just below heading
    drawScaledIcon(MARGIN, CONTENT_Y + 5, getWeatherIcon(weatherCode), 10);

    display.setFont(&FreeMonoBold24pt7b);
    display.setCursor(MARGIN + 172, CONTENT_Y + 75);
    display.printf("%d F", tempF);

    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(MARGIN + 172, CONTENT_Y + 120);
    display.print(condition);

    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(MARGIN + 172, CONTENT_Y + 158);
    display.printf("H: %d  L: %d", highF, lowF);

    display.setCursor(MARGIN + 172, CONTENT_Y + 185);
    display.printf("Rain: %d%%", precipPct);

    // ── 5-Day Forecast (3-column grid) ───────────────────────────────────────
    display.drawLine(MARGIN, 337, COL_DIVIDER_X, 337, GxEPD_BLACK);

    int fIdx = 0;
    for (JsonObject fDay : forecast) {
      if (fIdx >= 3) break;
      const char* fName = fDay["day"]         | "---";
      int         fHigh = fDay["high"]        | 0;
      int         fLow  = fDay["low"]         | 0;
      int         fWmo  = fDay["weatherCode"] | 0;
      int cellX = MARGIN + (fIdx % 3) * FORECAST_CELL_W;
      int cellY = (fIdx < 3) ? 343 : 402;
      drawForecastCell(cellX, cellY, fName, fHigh, fLow, fWmo);
      fIdx++;
    }

    // ── Calendar (right column) ──────────────────────────────────────────────
    display.setFont(&FreeMonoBold18pt7b);
    display.setCursor(COL_RIGHT_X, CONTENT_Y);
    display.print("TODAY");

    int eventY = CONTENT_Y + 45;

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

        // Truncate title to fit column width (~18 chars at 12pt)
        String titleStr = String(title);
        if (titleStr.length() > 18) {
          titleStr = titleStr.substring(0, 17) + "...";
        }
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

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);

  // 0. Power on the display
  pinMode(EPD_PWR, OUTPUT);
  digitalWrite(EPD_PWR, HIGH);

  // 1. Connect to WiFi
  // WiFi.config(STATIC_IP, GATEWAY, SUBNET, DNS);  // uncomment for static IP
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi failed — sleeping");
    showError("server unreachable");
    deepSleepMinutes(DEFAULT_REFRESH_MINUTES);
    return;
  }
  Serial.println("\nWiFi connected");

  // 2. Fetch dashboard data
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
    deepSleepMinutes(DEFAULT_REFRESH_MINUTES);
    return;
  }

  // 3. Parse JSON streamed directly — avoids holding two copies in heap
  JsonDocument doc;
  DeserializationError parseErr = deserializeJson(doc, http.getStream());
  http.end();

  // Turn WiFi off before driving the display — reduces peak current draw
  // and prevents radio interference during SPI communication.
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  if (parseErr) {
    Serial.printf("JSON parse failed: %s\n", parseErr.f_str());
    deepSleepMinutes(DEFAULT_REFRESH_MINUTES);
    return;
  }

  const char* date           = doc["date"]                          | "Unknown Date";
  int         tempF          = doc["weather"]["tempF"]              | 0;
  const char* condition      = doc["weather"]["condition"]          | "";
  int         highF          = doc["weather"]["highF"]              | 0;
  int         lowF           = doc["weather"]["lowF"]               | 0;
  int         precipPct      = doc["weather"]["precipitationPct"]   | 0;
  int         weatherCode    = doc["weather"]["weatherCode"]         | 0;
  const char* updatedAt      = doc["updatedAt"]                      | "";
  int         refreshMinutes = doc["settings"]["refreshRateMinutes"] | DEFAULT_REFRESH_MINUTES;
  JsonArray   forecast       = doc["weather"]["forecast"].as<JsonArray>();
  JsonArray   events         = doc["calendar"]["events"];
  int         totalCount     = doc["calendar"]["totalCount"]        | 0;

  // 4. Render
  renderDisplay(date, tempF, condition, highF, lowF, precipPct, weatherCode, updatedAt, forecast, events, totalCount);
  Serial.println("Display updated");

  // 5. Deep sleep
  deepSleepMinutes(refreshMinutes);
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  // Intentionally empty — deep sleep wakes into setup()
}
