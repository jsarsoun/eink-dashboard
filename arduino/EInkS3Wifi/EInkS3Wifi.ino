#include <WiFi.h>

const char* WIFI_SSID = "SARS24";
const char* WIFI_PASSWORD = "y#4DpEpr!";

void setup() {
  Serial.begin(115200);
  unsigned long _start = millis();
  while (!Serial && millis() - _start < 5000) delay(10);  // Wait for CDC host
  Serial.println("=== WiFi test ===");
  Serial.printf("Free heap before WiFi: %d\n", ESP.getFreeHeap());
  
  WiFi.mode(WIFI_STA);
  Serial.println("WiFi mode set");
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("WiFi.begin called");
  
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.printf("  status=%d\n", WiFi.status());
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi failed");
  }
}

void loop() {
  delay(2000);
  Serial.printf("heap: %d  wifi: %d\n", ESP.getFreeHeap(), WiFi.status());
}
