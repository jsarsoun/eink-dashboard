// Minimal wiring test for Waveshare 7.5" B (800x480) + Freenove ESP32 WROOM
// No WiFi, no sleep — draws a test pattern and reports BUSY status.

#include <SPI.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold18pt7b.h>

// ── Pin definitions (Freenove ESP32 WROOM) ───────────────────────────────────
#define EPD_PWR   15  // Power enable (HIGH = on)
#define EPD_CS     5  // Chip select
#define EPD_DC    17  // Data/Command
#define EPD_RST   16  // Reset
#define EPD_BUSY   4  // Busy
// SPI hardware (VSPI defaults, no define needed):
//   CLK  → GPIO18
//   MOSI → GPIO23

// ── Display (Waveshare 7.5" B V2/V3 — 800x480, red/black/white) ─────────────
GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 4> display(
  GxEPD2_750c_Z08(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("=== EInk Wiring Test ===");

  // Power on display
  pinMode(EPD_PWR, OUTPUT);
  digitalWrite(EPD_PWR, HIGH);
  Serial.println("PWR HIGH");
  delay(500);

  Serial.printf("BUSY before init: %d\n", digitalRead(EPD_BUSY));
  display.init(115200, true, 10, false);
  Serial.printf("BUSY after init:  %d\n", digitalRead(EPD_BUSY));

  // Draw test pattern
  Serial.println("Drawing...");
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // Black border
    display.drawRect(5, 5, display.width() - 10, display.height() - 10, GxEPD_BLACK);

    // Red filled box top-left
    display.fillRect(20, 20, 150, 80, GxEPD_RED);

    // "HELLO" in black center
    display.setFont(&FreeMonoBold18pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(display.width() / 2 - 70, display.height() / 2);
    display.print("HELLO");

    // Dimensions in red
    display.setTextColor(GxEPD_RED);
    display.setCursor(display.width() / 2 - 80, display.height() / 2 + 50);
    display.printf("%dx%d", display.width(), display.height());

  } while (display.nextPage());

  display.hibernate();
  Serial.println("Done.");
}

void loop() {}
