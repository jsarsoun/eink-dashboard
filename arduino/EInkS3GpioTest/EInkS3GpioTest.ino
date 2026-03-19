#include <gpio_viewer.h>  // Must be first
#include <WiFi.h>

const char* WIFI_SSID     = "SARS24";
const char* WIFI_PASSWORD = "y#4DpEpr!";

GPIOViewer gpio_viewer;

void setup() {
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) delay(500);

  if (WiFi.status() == WL_CONNECTED) {
    gpio_viewer.begin();
  }
}

void loop() {
  delay(1);
}
