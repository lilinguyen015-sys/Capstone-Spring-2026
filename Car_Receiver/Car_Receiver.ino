#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> // You might need to install "ArduinoJson" library too

// --- CONFIGURATION ---
const char* ssid = "Simulated_WiFi";
const char* password = "password";
const char* mqtt_server = "test.mosquitto.org";

WiFiClient espClient;
PubSubClient client(espClient);

// --- FAILSAFE VARIABLES ---
unsigned long lastPacketTime = 0;
const int HEARTBEAT_TIMEOUT = 250; // 250ms Limit (Slide 7)

// --- CALLBACK FUNCTION (The "Brain") ---
// This runs automatically whenever a message arrives from the Python script.
void callback(char* topic, byte* payload, unsigned int length) {
  
  // 1. Reset Failsafe Timer
  lastPacketTime = millis();

  // 2. Parse the JSON (Slide 8 Logic)
  // We treat the payload as a stream of characters
  char json[length + 1];
  memcpy(json, payload, length);
  json[length] = '\0';

  // 3. Extract the 16-bit Steering Data
  // In the real car, this number goes directly to the DAC.
  Serial.print("New Command Received: ");
  Serial.println(json);
}

void setup() {
  Serial.begin(115200);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  // --- MAIN SAFETY LOOP ---
  
  // Check internet connection
  if (!client.connected()) {
    // In a real car, this would trigger an immediate E-STOP
    // reconnect(); 
  }
  client.loop();

  // --- FAILSAFE CHECK (Slide 7) ---
  // If we haven't heard from the Python script in 250ms...
  if (millis() - lastPacketTime > HEARTBEAT_TIMEOUT) {
    Serial.println("ALERT: Signal Lost! Engaging E-STOP.");
    // code to set throttle = 0 and brake = 255 goes here
  }
}