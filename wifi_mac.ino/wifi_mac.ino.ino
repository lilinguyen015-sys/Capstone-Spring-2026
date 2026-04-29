#include "WiFi.h"

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_MODE_STA);
  delay(500);

  Serial.println();
  Serial.println("-------------------------");
  Serial.print("MAC ADDRESS: ");
  Serial.println(WiFi.macAddress());
  Serial.println("-------------------------");
}

void loop() {
    // Do nothing
}