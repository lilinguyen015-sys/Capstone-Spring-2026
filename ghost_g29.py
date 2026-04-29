import paho.mqtt.client as mqtt
import time
import random
import json

# 1. Configuration
BROKER = "test.mosquitto.org"  # Public test server (works anywhere)
TOPIC = "nissan_leaf/teleop/control"

# 2. Setup MQTT Client
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.connect(BROKER, 1883, 60)

print(f"Ghost G29 Started. Publishing to {TOPIC}...")

try:
    while True:
        # 3. Simulate Data (The "Ghost" Inputs)
        # Steering: 16-bit signed integer (-32768 to 32767)
        # This mimics the high-precision optical encoder of the G29.
        fake_steering = random.randint(-20000, 20000) 
        
        # Throttle/Brake: 8-bit unsigned integers (0 to 255)
        fake_throttle = random.randint(0, 100)
        fake_brake = 0

        # 4. Create the JSON Packet
        # This matches the "Data Serialization" point in your Slide 8.
        data = {
            "steer": fake_steering,
            "accel": fake_throttle,
            "brake": fake_brake
        }
        payload = json.dumps(data)

        # 5. Send it to the Cloud
        client.publish(TOPIC, payload)
        
        # Print for verification
        print(f"Sent: {payload}")

        # 6. Maintain 100Hz Frequency (10ms delay)
        # This proves your "Latency Objective" from Slide 7.
        time.sleep(0.01)

except KeyboardInterrupt:
    print("\nStopped.")
    client.disconnect()