#include <Adafruit_MCP4728.h>
#include <mcp_can.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>


// ===================== PINS =====================
#define SPI_CS_PIN 5
#define CAN_INT_PIN 4


#define SPI_SCK 18
#define SPI_MISO 19
#define SPI_MOSI 23


// ===================== OBJECTS =====================
MCP_CAN CAN0(SPI_CS_PIN);
Adafruit_MCP4728 dac, dac2;
TwoWire W1 = TwoWire(0);
TwoWire W2 = TwoWire(1);
WiFiUDP udp;


// ===================== WIFI =====================
const char* ssid = "ATT-WIFI-L2d3";
const char* password = "AL62o32d";


// ===================== SERVER =====================
const char* serverIP = "147.224.143.221";
const int serverPort = 4210;
unsigned long lastHeartbeat = 0;


// ===================== UDP =====================
const unsigned int localPort = 4210;
char packetBuffer[128];


// ===================== STATE =====================
float steering = 0.0f;
float throttle = 0.0f;
float brake = 0.0f;
unsigned long lastSeq = 0;
int16_t currentSTA = 0;


// DAC base
float proper = 4.096;
uint16_t accel1Base, accel2Base;
uint16_t brake1Base, brake2Base;
uint16_t steer1, steer2;
uint16_t accelMag = 0;
uint16_t brakeMag = 0;


// ===================== PID =====================
float kp = 3.6;
float ki = 0.05;
float kd = 0.1;


float lastError = 0;
float integral = 0;
float maxTorque = 1500.0;


// ===================== FUNCTIONS =====================


// ---------- CAN ----------
void initCAN() {
 SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_CS_PIN);


 if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK) {
   Serial.println("MCP2515 Initialized");
 } else {
   Serial.println("MCP2515 Init Failed");
   while (1);
 }


 CAN0.setMode(MCP_NORMAL);
 pinMode(CAN_INT_PIN, INPUT);
}


void readCAN() {
 if (!digitalRead(CAN_INT_PIN)) {
   unsigned long id;
   uint8_t len, buf[8];


   if (CAN0.readMsgBuf(&id, &len, buf) == CAN_OK) {
     if (id == 0x2) {
       currentSTA = (int16_t)((buf[0]) | (buf[1] << 8)) / 10;
     }
   }
 }
}


// ---------- WIFI / UDP ----------
void initWiFi() {
 WiFi.mode(WIFI_STA);
 WiFi.begin(ssid, password);


 Serial.print("Connecting WiFi");
 while (WiFi.status() != WL_CONNECTED) {
   delay(500);
   Serial.print(".");
 }


 Serial.println("\nWiFi connected");
 Serial.print("ESP32 IP: ");
 Serial.println(WiFi.localIP());


 udp.begin(localPort);
 Serial.print("UDP listening on port: ");
 Serial.println(localPort);
}


void sendHeartbeat() {
 udp.beginPacket(serverIP, serverPort);
 udp.print("HEARTBEAT");
 udp.endPacket();
}


void receiveUDP() {
 int packetSize = udp.parsePacket();
 if (!packetSize) return;


 int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
 if (len <= 0) return;


 packetBuffer[len] = '\0';


 unsigned long seq;
 float s, t, b;


 if (sscanf(packetBuffer, "CMD;%lu;%f;%f;%f", &seq, &s, &t, &b) == 4) {
   lastSeq = seq;
   steering = s;
   throttle = t;
   brake = b;
 }
}


// ---------- DAC ----------
void initDACChannels() {
 dac.setChannelValue(MCP4728_CHANNEL_A, 0, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
 dac.setChannelValue(MCP4728_CHANNEL_B, 0, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
 dac.setChannelValue(MCP4728_CHANNEL_C, 2470, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
 dac.setChannelValue(MCP4728_CHANNEL_D, 2470, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);


 dac2.setChannelValue(MCP4728_CHANNEL_A, 380, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
 dac2.setChannelValue(MCP4728_CHANNEL_B, 760, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
 dac2.setChannelValue(MCP4728_CHANNEL_C, 3380, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
 dac2.setChannelValue(MCP4728_CHANNEL_D, 1480, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
}


void updateBaseVoltages() {
 accel1Base = (uint16_t)(0.37 / proper * 4095.0f);
 accel2Base = (uint16_t)(0.75 / proper * 4095.0f);
 brake1Base = (uint16_t)(3.42 / proper * 4095.0f);
 brake2Base = (uint16_t)(1.51 / proper * 4095.0f);
 steer1 = (uint16_t)(2.47 / proper * 4095.0f);
 steer2 = (uint16_t)(2.47 / proper * 4095.0f);
}


uint16_t pedalMagnitude(float input, float maxMagnitude) {
 return (uint16_t)(constrain(input, 0.0f, 1.0f) * maxMagnitude);
}


void sendOutputs(int s1, int s2, int a1, int a2, int b1, int b2) {
 uint16_t final_s1 = constrain(s1, 0, 4095);
 uint16_t final_s2 = constrain(s2, 0, 4095);
 uint16_t final_a1 = constrain(a1, 0, 4095);
 uint16_t final_a2 = constrain(a2, 0, 4095);
 uint16_t final_b1 = constrain(b1, 0, 4095);
 uint16_t final_b2 = constrain(b2, 0, 4095);


 dac2.fastWrite(0, 0, final_s1, final_s2);
 dac.fastWrite(final_a1, final_a2, final_b1, final_b2);
}


// ---------- PID ----------
float calcTorque(float target, float current, float dt) {
 float error = target - current;


 if (abs(error) < 2.0) {
   integral = 0;
 }


 float tempOutput = kp * error + ki * integral;


 if (abs(tempOutput) < maxTorque) {
   integral += error * dt;
 }


 integral = constrain(integral, -250.0f, 250.0f);


 float derivative = (error - lastError) / dt;
 float output = kp * error + ki * integral + kd * derivative;


 lastError = error;


 return constrain(output, -maxTorque, maxTorque);
}


// ===================== SETUP =====================
void setup() {
 Serial.begin(115200);
 delay(1000);


 // I2C buses
 W1.begin(21, 22, 400000);
 W2.begin(33, 32, 400000);


 // DACs
 dac.begin(0x60, &W1);
 dac2.begin(0x60, &W2);
 initDACChannels();
 dac.saveToEEPROM();
 dac2.saveToEEPROM();


 // Init systems
 initCAN();
 initWiFi();
}


// ===================== LOOP =====================
unsigned long lastPrint = 0;
unsigned long lastMicros = 0;


void loop() {
 unsigned long currentMicros = micros();
 float dt = (currentMicros - lastMicros) / 1000000.0f;
 if (dt <= 0.0f || dt > 1.0f) dt = 0.01f;
 lastMicros = currentMicros;


 if (millis() - lastHeartbeat > 2000) {
   sendHeartbeat();
   lastHeartbeat = millis();
 }


 receiveUDP();
 readCAN();
 updateBaseVoltages();


 steering = constrain(steering, -1.0f, 1.0f);
 throttle = constrain(throttle, 0.0f, 1.0f);
 brake = constrain(brake, 0.0f, 1.0f);


 float targetSTA = steering * 250.0f;


 float appliedThrottle = throttle;
 float appliedBrake = brake;


 accelMag = pedalMagnitude(appliedThrottle, 300.0f);
 brakeMag = pedalMagnitude(appliedBrake, 900.0f);


 static float smoothTarget = 0.0f;
 smoothTarget += (targetSTA - smoothTarget) * (10.0f * dt);


 float torque = calcTorque(smoothTarget, currentSTA, dt);


 sendOutputs(
   (int)steer1 + (int)torque,
   (int)steer2 - (int)torque,
   (int)accel1Base + (int)accelMag,
   (int)accel2Base + (int)(2 * accelMag),
   (int)brake1Base - (int)brakeMag,
   (int)brake2Base + (int)brakeMag
 );


 if (millis() - lastPrint > 100) {
   Serial.print("Current: ");
   Serial.print(currentSTA);
   Serial.print(" | Target: ");
   Serial.print(targetSTA);
   Serial.print(" | Smooth: ");
   Serial.print(smoothTarget);
   Serial.print(" | Error: ");
   Serial.print(targetSTA - currentSTA);
   Serial.print(" | Integral: ");
   Serial.print(integral);
   Serial.print(" | Torque: ");
   Serial.print(torque);
   Serial.print(" | Throttle: ");
   Serial.print(appliedThrottle, 3);
   Serial.print(" | AccelMag: ");
   Serial.print(accelMag);
   Serial.print(" | Brake: ");
   Serial.print(appliedBrake, 3);
   Serial.print(" | BrakeMag: ");
   Serial.print(brakeMag);
   Serial.print(" | Seq: ");
   Serial.println(lastSeq);


   lastPrint = millis();
 }
}







