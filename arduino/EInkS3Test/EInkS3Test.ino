// Minimal boot test — no GPIOViewer, no display
#include <WiFi.h>

const char* WIFI_SSID = "SARS24";
const char* WIFI_PASSWORD = "y#4DpEpr!";

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("=== XIAO ESP32-S3 boot test ===");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi failed");
  }
  Serial.println("Done.");
}

void loop() {
  delay(1000);
  Serial.println("alive");
}
