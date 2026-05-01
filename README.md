# Wireless Drive-By-Wire — Capstone Spring 2026

This repository is located at https://github.com/lilinguyen015-sys/Capstone-Spring-2026.

A system that lets a person remotely steer, accelerate, and brake a real car using a Logitech G29 steering wheel and pedals over the internet.

---

## System Overview

```
[Driver Side]                     [Cloud]                    [Car Side]

G29 Wheel ──USB──► Laptop  ──internet──► OCI Relay Server ──WiFi──► ESP32 on PCB
               (g29_to_server_original.py)   (relay_server.py)       (esp32_car.ino)
                                                                            │
                                                                       DAC outputs
                                                                            │
                                                             Steering motor / Throttle / Brake
```

The driver's laptop reads the G29 and sends control packets to an Oracle Cloud relay server. The relay server forwards those packets to an ESP32 on a custom PCB inside the car. The ESP32 runs a PID loop for steering, reads the actual steering angle from the CAN bus, and outputs analog voltages through DACs to drive the steering motor, throttle, and brake actuators.

The relay server is necessary because the ESP32 is behind a mobile hotspot NAT and has no public IP. The ESP32 registers itself with the relay server at startup by sending periodic heartbeat packets, which the server uses to forward incoming commands back through the NAT.

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

---

## Software Requirements

### Driver Laptop

- Python 3.8 or newer
- `pygame`

```
pip install pygame
```

### OCI Relay Server

- Python 3.8 or newer
- No additional libraries — uses only the standard library

### ESP32

- Arduino IDE 2.x
- ESP32 board support package (add via **File → Preferences → Additional Board Manager URLs**, then install via **Tools → Board → Board Manager**)
- Libraries (install via **Tools → Manage Libraries**):
  - `Adafruit MCP4728`
  - `mcp_can` (by Seeed Studio)

> `WiFi` and `WiFiUdp` are included with the ESP32 board package — do not install them separately.

---

## Setup

Complete these steps in order before running the system.

### 1. OCI Relay Server

The relay server runs on an Oracle Cloud instance and should already be running. If it is not, SSH in and start it:

```
python3 relay_server.py
```

Expected output:
```
Relay server live on port 4210. Waiting for car heartbeat...
```

> **Note:** UDP port 4210 must be open in the OCI security list and the instance firewall. If the car never connects, check this first.

---

### 2. ESP32 (Car PCB)

#### Step 1 — Set WiFi credentials

Open `esp32_car.ino` and update these lines with the hotspot's name and password:

```cpp
const char* ssid = "YOUR_HOTSPOT_NAME";
const char* password = "YOUR_HOTSPOT_PASSWORD";
```

#### Step 2 — Flash the firmware

1. Connect the ESP32 via USB.
2. In Arduino IDE, select **Tools → Board → ESP32 Dev Module**.
3. Select the correct port under **Tools → Port**.
4. Click **Upload** and wait for "Done uploading."

#### Step 3 — Verify connection

1. Open **Tools → Serial Monitor**, set baud rate to **115200**.
2. Turn on the hotspot and press the ESP32 reset button. You should see:

```
WiFi connected
ESP32 IP: 10.x.x.x
UDP listening on port: 4210
```

Once connected, the relay server terminal should show:

```
✅ CAR CONNECTED! Logged IP: ('x.x.x.x', 4210)
```

---

### 3. Sender Laptop

1. Plug in the G29 and wait for it to complete its self-calibration spin.
2. Confirm `pygame` is installed: `pip install pygame`
3. No other configuration needed — the server IP is already set in `g29_to_server_original.py`.

---

## Running the System

Once the relay server is running and the ESP32 shows `CAR CONNECTED` on the server, run the sender:

```
python3 g29_to_server_original.py
```

Expected output:
```
Joystick detected: Logitech G29 Driving Force Racing Wheel
Sending UDP to OCI Server at 147.224.143.221:4210
seq=00000 | steer= 0.0000 | throttle=0.0000 | brake=0.0000
seq=00001 | steer= 0.0012 | throttle=0.0000 | brake=0.0000
...
```

Turning the wheel steers the car. The throttle and brake pedals control acceleration and braking.

Press **Ctrl+C** to stop.
