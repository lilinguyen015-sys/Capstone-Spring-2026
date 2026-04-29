import paho.mqtt.client as mqtt
import time
import random

# MQTT Configuration (Coordinate with Ousmane for the real IP)
BROKER = "test.mosquitto.org" 
TOPIC = "nissan_leaf/teleop/control"

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.connect(BROKER, 1883, 60)

print("G29 Mock Gateway Started. Sending 16-bit telemetry...")

try:
    while True:
        # Simulate 16-bit steering (-32768 to 32767)
        steer = random.randint(-32768, 32767)
        # Simulate 8-bit throttle (0 to 255)
        throttle = random.randint(0, 255)
        
        # Create the JSON payload
        payload = f'{{"steer": {steer}, "accel": {throttle}}}'
        
        # Publish to the broker
        client.publish(TOPIC, payload)
        
        # Sleep to maintain 100Hz frequency (0.01 seconds)
        time.sleep(0.01) 
        
except KeyboardInterrupt:
    print("Stopped.")
    client.disconnect()