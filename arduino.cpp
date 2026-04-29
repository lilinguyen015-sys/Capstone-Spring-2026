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
const char *ssid = "ATT-WIFI-L2d3";
const char *password = "AL62o32d";

IPAddress local_IP(192, 168, 1, 55);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

const unsigned int localPort = 4210;
char packetBuffer[128];

// ===================== STATE =====================
float steering = 0.0f;
unsigned long lastSeq = 0;
int16_t currentSTA = 0;

// DAC base
float proper = 4.096;
uint16_t steer1, steer2;

// ===================== PID =====================
float kp = 3.6;
float ki = 0.05;
float kd = 0.1;

float lastError = 0;
float integral = 0;

// ===================== FUNCTIONS =====================

// ---------- CAN ----------
void initCAN()
{
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_CS_PIN);

  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK)
  {
    Serial.println("MCP2515 Initialized");
  }
  else
  {
    Serial.println("MCP2515 Init Failed");
    while (1)
      ;
  }

  CAN0.setMode(MCP_NORMAL);
  pinMode(CAN_INT_PIN, INPUT);
}

void readCAN()
{
  if (!digitalRead(CAN_INT_PIN))
  {
    unsigned long id;
    uint8_t len, buf[8];

    if (CAN0.readMsgBuf(&id, &len, buf) == CAN_OK)
    {
      if (id == 0x2)
      {
        currentSTA = (int16_t)((buf[0]) | (buf[1] << 8)) / 10;
      }
    }
  }
}

// ---------- UDP ----------
void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());

  udp.begin(localPort);
}

void receiveUDP()
{
  int packetSize = udp.parsePacket();
  if (!packetSize)
    return;

  int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
  if (len <= 0)
    return;

  packetBuffer[len] = '\0';

  unsigned long seq;
  float s, t, b;

  if (sscanf(packetBuffer, "CMD;%lu;%f;%f;%f", &seq, &s, &t, &b) == 4)
  {
    lastSeq = seq;
    steering = s;
  }
}

// ---------- DAC ----------
void updateBaseVoltages()
{
  steer1 = 2.45 / proper * 4095;
  steer2 = 2.45 / proper * 4095;
}

void sendSteering(uint16_t s1, uint16_t s2)
{
  dac2.fastWrite(0, 0, s1, s2);

  // Force accel & brake OFF
  dac.fastWrite(0, 0, 0, 0);
}

// ---------- PID (FIXED) ----------
float calcTorque(float target, float current)
{
  float error = target - current;
  float dt = 0.01;

  // Reset integral near center
  if (abs(error) < 2.0)
  {
    integral = 0;
  }

  // Compute provisional output
  float tempOutput = kp * error + ki * integral;

  // Only integrate if not saturated
  // changed the tempOutput from 250 to 1500
  if (abs(tempOutput) < 1500)
  {
    integral += error * dt;
  }

  // Clamp integral tightly
  integral = constrain(integral, -100, 100);

  float derivative = (error - lastError) / dt;

  float output = kp * error + ki * integral + kd * derivative;

  lastError = error;

  return constrain(output, -250, 250);
}

// ===================== SETUP =====================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  // I2C
  W1.begin(21, 22, 400000);
  W2.begin(33, 32, 400000);

  dac.begin(0x60, &W1);
  dac2.begin(0x60, &W2);

  initCAN();
  initWiFi();
}

// ===================== LOOP =====================
unsigned long lastPrint = 0;

void loop()
{
  receiveUDP();
  readCAN();

  updateBaseVoltages();

  // Clamp input
  steering = constrain(steering, -1.0f, 1.0f);

  // Convert to target angle
  float targetSTA = steering * 250.0;

  // Smooth target
  static float smoothTarget = 0;
  smoothTarget += (targetSTA - smoothTarget) * 0.1;

  // PID (with corrected direction)
  float torque = calcTorque(smoothTarget, currentSTA);

  // Apply steering
  sendSteering(
      steer1 + torque,
      steer2 - torque);

  // Debug
  if (millis() - lastPrint > 100)
  {
    Serial.print("Current: ");
    Serial.print(currentSTA);
    Serial.print(" | Target: ");
    Serial.print(targetSTA);
    Serial.print(" | Error: ");
    Serial.print(targetSTA - currentSTA);
    Serial.print(" | Integral: ");
    Serial.print(integral);
    Serial.print(" | Torque: ");
    Serial.print(torque);
    Serial.print(" | Seq: ");
    Serial.println(lastSeq);

    lastPrint = millis();
  }
}
