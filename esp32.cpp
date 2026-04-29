#include <Adafruit_MCP4728.h>
#include <mcp_can.h>
#include <Wire.h>
#include <SPI.h>
#include <Bluepad32.h>
#include <math.h>
#include <SD.h>

ControllerPtr myControllers[BP32_MAX_GAMEPADS];
Adafruit_MCP4728 dac, dac2;
const int SPI_CS_PIN = 5;
const int CAN_INT_PIN = 4;
MCP_CAN CAN0(SPI_CS_PIN);
TwoWire W1 = TwoWire(0);
TwoWire W2 = TwoWire(1);

int STAngle = 0;
int BPP = 0;
int VSS = 0;
int APP = 0;

uint16_t brake1 = 3000;
uint16_t brake2 = 1300; // using ref 4.71V
uint16_t accel1 = 330;  // max 2020
uint16_t accel2 = 690;  // max 3756
uint16_t steer1 = 197;
uint16_t steer2 = 197;
float steermag = 0;
uint16_t gyrosteermag = 0;
uint16_t brakemag = 0;
uint16_t gyrobrakemag = 0;
uint16_t accelmag = 0;
uint16_t gyroaccelmag = 0;
uint8_t left = 1;
uint8_t gyroleft = 1;
uint16_t ramp = 1;
uint16_t steermag2 = 0;
int16_t currentSTA = 0;
float targetSTA = 0;

void readMessage()
{

  if (!digitalRead(CAN_INT_PIN))
  {
    unsigned long replyId;
    uint8_t dlc, data[8];
    if (CAN0.readMsgBuf(&replyId, &dlc, data) == CAN_OK)
    {

      // Serial.println();
      if (replyId == 0x2)
      {
        currentSTA = (int16_t)((data[0]) | data[1] << 8);
        currentSTA = currentSTA / 10;
        //   Serial.println(currentSTA);
        //   Serial.print("Message received with ID: 0x");
        //   Serial.println(replyId, HEX);
        //   Serial.print("Data: ");
        //   for (int i = 0; i < dlc; i++) {
        //       Serial.print(data[i], HEX);
        //       Serial.print(" ");
        //  }
        //  Serial.println();
      }
    }
  }
  // return currentSTA;
}

void onConnectedController(ControllerPtr ctl)
{
  bool foundEmptySlot = false;
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++)
  {
    if (myControllers[i] == nullptr)
    {
      Serial.printf("CALLBACK: Controller is connected, index=%d\n", i);
      // Additionally, you can get certain gamepad properties like:
      // Model, VID, PID, BTAddr, flags, etc.
      ControllerProperties properties = ctl->getProperties();
      Serial.printf("Controller model: %s, VID=0x%04x, PID=0x%04x\n",
                    ctl->getModelName().c_str(), properties.vendor_id,
                    properties.product_id);
      myControllers[i] = ctl;
      foundEmptySlot = true;
      break;
    }
  }

  if (!foundEmptySlot)
  {
      Serial.println("CALLBACK: Controller connected, but could not
found empty slot");
  }
}

void onDisconnectedController(ControllerPtr ctl)
{
  bool foundController = false;

  for (int i = 0; i < BP32_MAX_GAMEPADS; i++)
  {
    if (myControllers[i] == ctl)
    {
      Serial.printf("CALLBACK: Controller disconnected from index=%d\n", i);
      myControllers[i] = nullptr;
      foundController = true;
      break;
    }
  }

  if (!foundController)
  {
      Serial.println("CALLBACK: Controller disconnected, but not found
in myControllers");
  }
}

void processControllers()
{
  for (auto myController : myControllers)
  {

    if (myController && myController->isConnected() &&
        myController->hasData())
    {
      if (myController->isGamepad())
      {
        processGamepad(myController);
      }
      else
      {
        Serial.println("Unsupported controller");
      }
    }
  }
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(500000);
  W1.begin(21, 22, 3400000);
  W2.begin(33, 32, 3400000);

  dac.begin(0x60, &W1);
  dac2.begin(0x60, &W2);
  Serial.println(dac.begin(0x60, &W1));
  Serial.println(dac2.begin(0x60, &W2));
  pinMode(2, OUTPUT);

  while (!Serial)
    ;

  // Initialize MCP2515 running at 8MHz with a baudrate of 500kb/s
  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK)
  {
    Serial.println("MCP2515 Initialized Successfully!");
  }
  else
  {
        Serial.println("Error Initializing MCP2515... Check your
connections and try again.");
        while (1);
  }

  // Set normal operation mode
  CAN0.setMode(MCP_NORMAL);

  // Configure CAN interrupt pin
  pinMode(CAN_INT_PIN, INPUT);

  Serial.printf("Firmware: %s\n", BP32.firmwareVersion());
  const uint8_t *addr = BP32.localBdAddress();
  Serial.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0],
                addr[1], addr[2], addr[3], addr[4], addr[5]);

  dac.setChannelValue(MCP4728_CHANNEL_A, 0, MCP4728_VREF_INTERNAL,
                      MCP4728_GAIN_2X);
  dac.setChannelValue(MCP4728_CHANNEL_B, 0, MCP4728_VREF_INTERNAL,
                      MCP4728_GAIN_2X);
  dac.setChannelValue(MCP4728_CHANNEL_C, 2470,
                      MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
  dac.setChannelValue(MCP4728_CHANNEL_D, 2470,
                      MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
  dac2.setChannelValue(MCP4728_CHANNEL_A, 380,
                       MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
  dac2.setChannelValue(MCP4728_CHANNEL_B, 760,
                       MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
  dac2.setChannelValue(MCP4728_CHANNEL_C, 3380,
                       MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
  dac2.setChannelValue(MCP4728_CHANNEL_D, 1480,
                       MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
  dac.saveToEEPROM();
  dac2.saveToEEPROM();
  // Setup the Bluepad32 callbacks
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.forgetBluetoothKeys();
  BP32.enableVirtualDevice(false);
}
bool gyro = false;
void processGamepad(ControllerPtr ctl)
{
  if (ctl->buttons() == 0x0008) // triangle
  {
    gyro = true;
  }
  if (ctl->buttons() == 0x0002) // circle
  {
    gyro = false;
  }
  if (ctl->brake() > 100 && ctl->brake() <= 1023)
  {
    brakemag = float(ctl->brake()) / 1023.0 * 600;
  }
  else if (ctl->brake() < 100)
  {
    brakemag = 0;
  }
  if (ctl->throttle() > 100 && ctl->throttle() <= 1023)
  {
    accelmag = float(ctl->throttle() - 100.0) / 923.0 * 1500;
  }
  else if (ctl->throttle() < 100)
  {
    accelmag = 0;
  }
  if (ctl->axisX() > 25 && ctl->axisX() < 512)
  {
    left = 2;
    targetSTA = float(ctl->axisX()) / 512.0 * 450;
  }
  else if (ctl->axisX() < -25 && ctl->axisX() > -512)
  {
    left = 3;
    targetSTA = float(ctl->axisX()) / 512.0 * 450;
  }
  else if (ctl->axisX() >= 512)
  {
    left = 2;
    targetSTA = 450;
  }
  else if (ctl->axisX() <= -512)
  {
    left = 3;
    targetSTA = 300;
  }
  else if (ctl->axisX() <= 25 && ctl->axisX() >= -25)
  {
    left = 1;
    targetSTA = 0;
  }
  if (ctl->accelZ() < -2000 && ctl->accelZ() >= -9000)
  {
    gyrobrakemag = float(ctl->accelZ() + 2000.0) / -7000.0 * 600;
  }
  if (ctl->accelZ() > -2000)
  {
    gyrobrakemag = 0;
  }
  if (ctl->accelZ() > 2000 && ctl->accelZ() <= 9000)
  {
    gyroaccelmag = float(ctl->accelZ() - 2000.0) / 7000.0 * 1500;
  }
  if (ctl->accelZ() < 2000)
  {
    gyroaccelmag = 0;
  }
  if (ctl->accelX() > 2000 && ctl->accelX() < 9000 && gyro == true)
  {
    gyroleft = 3;
    targetSTA = float(ctl->accelX() - 2000.0) * -1 / 7000.0 * 450;
  }
  else if (ctl->accelX() < -2000 && ctl->accelX() > -9000 && gyro == true)
  {
    gyroleft = 2;
    targetSTA = float((ctl->accelX() + 2000.0) / 7000.0) * -1 * 450;
  }
  else if (ctl->accelX() >= 9000 && gyro == true)
  {
    gyroleft = 3;
    targetSTA = 450;
  }
  else if (ctl->accelX() <= -9000 && gyro == true)
  {
    gyroleft = 2;
    targetSTA = 450;
  }
  else if (ctl->accelX() <= 2000 && ctl->accelX() >= -2000 && gyro == true)
  {
    gyroleft = 1;
    targetSTA = 0;
  }
}
float lasterror = 0;
float error = 0;
float integral = 0;
float derivative = 0;
float kp = 5;
float ki = 0.1;
float kd = 0;
// float  kp = 3.6;
// float  ki = 1;
// float  kd = 0;

float calcTorque(float target, float current)
{
  float torque = 0; // range between -300 and 300 or 0 and 300 if
  direction included float u, dt;
  error = target - current;
  dt = .01;
  integral += error * dt;
  derivative = (error - lasterror) / dt;

  // kp = 3.6;
  // ki = 1;
  // kd = .1;
  u = kp * error + ki * integral + kd * derivative;

  lasterror = error;
  if (u > 250)
  {
    torque = 250;
  }
  else if (u < -250)
  {
    torque = -250;
  }
  else
  {
    torque = (u);
  }
  // Serial.print("torque: ");
  // Serial.print(torque);
  // Serial.print(" // current: ");
  // Serial.print(current);
  // Serial.print(" // target: ");
  // Serial.print(target);
  // Serial.print(" // error I D:");
  // Serial.print(error);
  // Serial.print(" ");
  // Serial.print(integral);
  // Serial.print(" ");
  // Serial.println(derivative);
  return torque;
}

void sendVolt(uint16_t s1, uint16_t s2, uint16_t a1, uint16_t a2,
              uint16_t b1, uint16_t b2)
{

  //
  dac2.fastWrite(0, 0, s1, s2);
  dac.fastWrite(a1, a2, b1, b2);
}

// main hook called when a full line is received
float steering = 0.0f;
float throttle = 0.0f;
float brake = 0.0f;
// ====== SERIAL BUFFER ======
static const int BUF_SIZE = 96;
char inputBuf[BUF_SIZE];
int bufPos = 0;
void handleCommand(const char *line)
{
  // Expected: CMD;<seq>;<steer>;<throttle>;<brake>
  unsigned long seq = 0;
  // float steering = 0.0f;
  // float throttle = 0.0f;
  // float brake = 0.0f;

  int matched = sscanf(line, "CMD;%lu;%f;%f;%f", &seq, &steering,
                       &throttle, &brake);
  if (matched != 4)
  {
    Serial.print("PARSE ERROR: ");
    Serial.println(line);
    return;
  }

  // Debug
  Serial.print("CMD seq=");
  Serial.print(seq);
  Serial.print(" S=");
  Serial.print(steering);
  Serial.print(" T=");
  Serial.print(throttle);
  Serial.print(" B=");
  Serial.println(brake);

  // Apply to car interface
  // applySteering(steering);
  // applyThrottle(throttle);
  // applyBrake(brake);

  // Send ACK back
  Serial.print("ACK;");
  Serial.println(seq);
}

long currenttime = millis();
float proper = 4.096;
void loop()
{

  char c = Serial.read();

  if (c == '\n' || c == '\r')
  {
    if (bufPos > 0)
    {
      inputBuf[bufPos] = '\0';
      handleCommand(inputBuf);
      bufPos = 0;
    }
  }
  else
  {
    if (bufPos < BUF_SIZE - 1)
    {
      inputBuf[bufPos++] = c;
    }
    else
    {
      // overflow; reset buffer
      bufPos = 0;
    }
  }

  brake1 = 3.38 / proper * 4095.0;
  brake2 = 1.48 / proper * 4095.0;
  accel1 = .37 / proper * 4095.0;
  accel2 = .75 / proper * 4095.0;
  steer1 = 2.45 / proper * 4095.0;
  steer2 = 2.45 / proper * 4095.0;
  readMessage();
  float angle = currentSTA;
  bool dataUpdated = BP32.update();
  if (dataUpdated)
    processControllers();

  // steermag = calcTorque(targetSTA, angle);
  steermag = steering * 250;
  if (gyro == false)
  {
    sendVolt((steer1 + steermag), (steer2 - steermag), accel1 + accelmag, accel2 + (2 * accelmag), brake1 - brakemag, brake2 + brakemag);
  }
  else if (gyro == true)
  {
    sendVolt(steer1 + steermag, steer2 - steermag, accel1 + gyroaccelmag, accel2 + (2 * gyroaccelmag), brake1 - gyrobrakemag,
             brake2 + gyrobrakemag);
  }
  // if (left == 1 && gyro == false)
  // {
  //   sendVolt(steer1, steer2, accel1 + accelmag, accel2 + (2 *
accelmag), brake1 - brakemag, brake2 + brakemag);
// }
// else if (left == 2 && gyro == false)
// {
//   sendVolt(steer1 + steermag, steer2 - steermag, accel1 +
accelmag, accel2 + (2 * accelmag), brake1 - brakemag, brake2 +
brakemag);
// }
// else if (left == 3 && gyro == false)
// {
//   sendVolt(steer1 - steermag, steer2 + steermag, accel1 +
accelmag, accel2 + (2 * accelmag), brake1 - brakemag, brake2 +
brakemag);
// }
// else if (gyroleft == 1 && gyro == true)
// {
//   sendVolt(steer1, steer2, accel1 + gyroaccelmag, accel2 + (2 *
gyroaccelmag), brake1 - gyrobrakemag, brake2 + gyrobrakemag);
// }
// else if (gyroleft == 2 && gyro == true)
// {
//   sendVolt(steer1 + steermag, steer2 - steermag, accel1 +
gyroaccelmag, accel2 + (2 * gyroaccelmag), brake1 - gyrobrakemag,
brake2 + gyrobrakemag);
// }
// else if (gyroleft == 3 && gyro == true)
// {
//   sendVolt(steer1 - steermag, steer2 + steermag, accel1 +
gyroaccelmag, accel2 + (2 * gyroaccelmag), brake1 - gyrobrakemag,
brake2 + gyrobrakemag);
// }
digitalWrite(2, LOW);

// Serial.println(steermag);
if (millis() - currenttime > 100)
{
  Serial.print("Main Signal 1: ");
  Serial.print(steer1 + steermag);
  Serial.print(" // Sub Signal 2: ");
  Serial.print(steer2 - steermag);
  Serial.print(" // Current Steering Angle: ");
  Serial.print(angle);
  Serial.print(" P term:");
  Serial.print(kp * error);
  Serial.print(" I:");
  Serial.print(ki * integral);
  Serial.print(" D:");
  Serial.println(kd * derivative);
  currenttime = millis();
  // delay(50);
  // Serial.println(ctl->axisX());
}
}