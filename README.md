# Wireless Drive-By-Wire — Capstone Spring 2026

> ⚠️ **WARNING:** This repo is only recent as of Wednesday, April 22nd. Please create another branch and merge the most recent additions to main! Please be sure to keep the readme and presentations.

A system that allows a person to remotely steer, accelerate, and brake a real car using a Logitech G29 steering wheel and pedals over the internet.

---

## Table of Contents

1. [System Overview](#system-overview)
2. [Hardware Requirements](#hardware-requirements)
3. [Software Requirements](#software-requirements)
4. [Setup](#setup)
   - [OCI Relay Server](#1-oci-relay-server)
   - [ESP32 (Car PCB)](#2-esp32-car-pcb)
   - [Sender Laptop](#3-sender-laptop)
5. [Running the System](#running-the-system)
6. [How It Works](#how-it-works)
7. [Troubleshooting](#troubleshooting)

---

## System Overview

```
[Driver Side]                  [Cloud]                    [Car Side]

G29 Wheel ──USB──► Laptop  ──internet──► OCI Relay Server ──WiFi──► ESP32 on PCB
                 (g29_to_server_original.py)            (relay_server.py)           (esp32_car.ino)
                                                                           │
                                                                      DAC outputs
                                                                           │
                                                                     Steering motor
```

The driver's laptop reads the G29 steering wheel and pedals and sends control packets over the internet to a cloud relay server (Oracle Cloud). The relay server forwards those packets to an ESP32 microcontroller on a custom PCB inside the car. The ESP32 runs a PID control loop for steering, reading the actual angle from the CAN bus, and outputs analog voltages through DACs to drive the steering motor, throttle, and brake actuators.

The relay server is necessary because the ESP32 is behind a mobile hotspot and has no public IP address — it cannot be reached directly from the internet. Instead, the ESP32 registers itself with the relay server on startup, and the relay server uses that registration to forward packets to it.

---

## Hardware Requirements

| Component | Qty | Notes |
|---|---|---|
| Logitech G29 Steering Wheel + Pedals | 1 | Connected via USB to the driver's laptop |
| Driver Laptop | 1 | Any laptop capable of running Python 3 |
| ESP32 | 1 | Mounted on the custom DBW PCB |
| Custom DBW PCB | 1 | Houses the ESP32, CAN controller, and DACs |
| MCP2515 CAN Controller | 1 | Already on PCB — reads steering angle from car |
| MCP4728 DAC (×2) | 2 | Already on PCB — outputs analog voltages to car actuators |
| Mobile Hotspot | 1 | Provides WiFi for the ESP32 on the car side |
| Car with CAN bus | 1 | Steering angle sensor must transmit on CAN ID `0x2` |

### ESP32 Pin Reference

| Signal | ESP32 Pin |
|---|---|
| SPI SCK | 18 |
| SPI MISO | 19 |
| SPI MOSI | 23 |
| CAN CS | 5 |
| CAN INT | 4 |
| I2C Bus 1 SDA / SCL (DAC 1) | 21 / 22 |
| I2C Bus 2 SDA / SCL (DAC 2) | 33 / 32 |

### DAC Channel Assignments

| DAC | Channel | Function | Baseline Voltage |
|---|---|---|---|
| DAC 2 | C | Steering main | 2.47 V |
| DAC 2 | D | Steering sub | 2.47 V |
| DAC 1 | A | Throttle main | 0.37 V |
| DAC 1 | B | Throttle sub | 0.75 V |
| DAC 1 | C | Brake main | 3.42 V |
| DAC 1 | D | Brake sub | 1.51 V |

---

## Software Requirements

### Driver Laptop

- Python 3.8 or newer
- `pygame` library (for reading the G29)

```
pip install pygame
```

### OCI Relay Server

- Python 3.8 or newer
- No additional libraries required — uses only the Python standard library

### ESP32

- Arduino IDE 2.x
- ESP32 board support package (install via **File → Preferences → Additional Board Manager URLs**, then search "esp32" in **Tools → Board → Board Manager**)
- The following libraries installed via **Tools → Manage Libraries**:
  - `Adafruit MCP4728`
  - `mcp_can` (by Seeed Studio)

> `WiFi` and `WiFiUdp` are included automatically with the ESP32 board package — do not install them separately.

---

## Setup

Complete these steps in order before running the system.

### 1. OCI Relay Server

The relay server runs on an Oracle Cloud instance and should already be running persistently. If it is not running, SSH into the instance and start it manually:

```
python3 relay_server.py
```

When running correctly you will see:
```
Relay server live on port 4210. Waiting for car heartbeat...
```

> **Note:** UDP port 4210 must be open in the OCI security list and the instance's firewall (`iptables` or `firewalld`). If the car never connects, this is the first thing to check.

---

### 2. ESP32 (Car PCB)

#### Step 1 — Set WiFi credentials

Open `esp32_car.ino` in Arduino IDE and update the following lines with the mobile hotspot's name and password:

```cpp
const char* ssid = "YOUR_HOTSPOT_NAME";
const char* password = "YOUR_HOTSPOT_PASSWORD";
```

#### Step 2 — Flash the firmware

1. Connect the ESP32 to your laptop via USB.
2. In Arduino IDE, go to **Tools → Board** and select **ESP32 Dev Module**.
3. Go to **Tools → Port** and select the port your ESP32 is on (e.g. `COM3` on Windows, `/dev/ttyUSB0` on Linux).
4. Click **Upload** (→ button). Wait for "Done uploading."

#### Step 3 — Verify connection

1. Open **Tools → Serial Monitor** and set the baud rate to **115200**.
2. Turn on the mobile hotspot.
3. Press the reset button on the ESP32. You should see:

```
WiFi connected
ESP32 IP: 10.x.x.x
UDP listening on port: 4210
```

Once connected, the ESP32 sends a heartbeat to the relay server every 2 seconds. On the relay server terminal you should see:

```
CAR CONNECTED: ('x.x.x.x', 4210)
```

This confirms the full car-to-server link is working.

---

### 3. Sender Laptop

1. Plug the G29 into the laptop via USB. Wait for the wheel to complete its self-calibration (it will spin and return to center on its own).
2. Confirm `pygame` is installed:
   ```
   pip install pygame
   ```
3. No other configuration is needed. The server IP is already set in `g29_to_server_original.py`.

---

## Running the System

Once the relay server is running and the ESP32 shows `CAR CONNECTED` on the server, run the sender on the driver's laptop:

```
python3 g29_to_server_original.py
```

Expected output:
```
Joystick detected: Logitech G29 Driving Force Racing Wheel
Sending UDP to OCI Server at 147.224.143.221:4210
seq=00001 | steer= 0.0000 | throttle=0.0000 | brake=0.0000
seq=00002 | steer= 0.0012 | throttle=0.0000 | brake=0.0000
...
```

The system is now live. Turning the G29 wheel steers the car. Pressing the throttle and brake pedals controls acceleration and braking.

Press **Ctrl+C** to stop.

---

## How It Works

### Step 1 — Car registers with the relay server

When the ESP32 boots, it connects to the mobile hotspot and immediately begins sending a `HEARTBEAT` UDP packet to the relay server every 2 seconds. This does two things:
- Tells the server the ESP32's current public IP and port
- Keeps the hotspot's NAT mapping alive so the server can send packets back through it


### Step 2 — Driver sends steering commands

`g29_to_server_original.py` reads the G29 steering axis at 60 Hz and sends a UDP packet to the relay server formatted as:

```
CMD;<seq>;<steer>
```

Where `<steer>` is a float from -1.0 (full left) to +1.0 (full right).

### Step 3 — Relay server forwards to the car

`relay_server.py` receives `CMD` packets from the driver's laptop and forwards them verbatim to the last address it received a `HEARTBEAT` from.

### Step 4 — ESP32 drives the actuators

The ESP32 parses the `CMD` packet and controls three systems simultaneously:

**Steering (closed-loop PID):**
```
steer (-1.0 to +1.0)  x250  targetSTA (degrees)  low-pass filter  PID  torque

DAC 2 Ch C = steer midpoint (2.47V) + torque
DAC 2 Ch D = steer midpoint (2.47V) - torque
```
The actual steering angle is read from the CAN bus (ID `0x2`) and fed back into the PID every loop.

**Throttle (open-loop):**
```
throttle (0.0-1.0)  x300  accelMag (0-300)

DAC 1 Ch A = accel1Base (0.37V) + accelMag
DAC 1 Ch B = accel2Base (0.75V) + (2 x accelMag)
```

**Brake (open-loop):**
```
brake (0.0-1.0)  x900  brakeMag (0-900)

DAC 1 Ch C = brake1Base (3.42V) - brakeMag
DAC 1 Ch D = brake2Base (1.51V) + brakeMag
```

> The brake channels are always driven to their baseline voltages. Writing 0V is not the same as no brake and may be interpreted as a fault by the car's ECU.

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| `No joystick found.` | G29 not detected by pygame | Unplug and replug the G29, wait for calibration spin, then rerun |
| Relay server never prints `CAR CONNECTED` | ESP32 cannot reach the server | Confirm hotspot is on; confirm UDP port 4210 is open on OCI |
| Steering does not respond after `g29_to_server_original.py` starts | CMD packets not reaching ESP32 | Confirm relay server is running and `CAR CONNECTED` was printed |
| Steering drives to the limit and stays there | CAN bus not working — no angle feedback to PID | Check MCP2515 wiring; verify crystal is 8 MHz |
| Serial Monitor shows `MCP2515 Init Failed` | SPI wiring issue or wrong CS pin | Check wiring against pin table above |
| Relay server prints `Car heartbeat timed out` | ESP32 stopped sending heartbeats | Check ESP32 is powered and connected to hotspot |
