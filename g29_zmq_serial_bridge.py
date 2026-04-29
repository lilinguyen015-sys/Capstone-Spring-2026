# g29_zmq_serial_bridge.py
# ZMQ_CONNECT_ADDR = "tcp://10.204.196.196:5556"   # replace with sender laptop IP

# # Set this to whatever /dev/tty.* your ESP32 shows up as.
# ESP32_SERIAL_PORT = "/dev/tty.usbserial-0001"
# ESP32_BAUD = 115200
import json
import time
import zmq
import serial
import sys

# -----------------------------
# Configuration
# -----------------------------
SENDER_IP = "10.204.209.195"   # Change to the sender laptop IP
ZMQ_PORT = 5558
ZMQ_TOPIC = "g29"

SERIAL_PORT = "/dev/tty.usbserial-0001"  # Change if needed, e.g. /dev/ttyACM0
BAUD_RATE = 115200

STEERING_ONLY = True          # True = throttle/brake forced to 0 for safe testing
PRINT_DEBUG_LINES = False     # True = also print non-ACK ESP32 lines


def clamp(value, low, high):
    """Clamp a number into [low, high]."""
    return max(low, min(high, value))


def main():
    zmq_addr = f"tcp://{SENDER_IP}:{ZMQ_PORT}"

    # ZMQ subscriber: receives G29 state from sender laptop
    ctx = zmq.Context()
    sub = ctx.socket(zmq.SUB)
    sub.setsockopt(zmq.LINGER, 0)
    sub.setsockopt_string(zmq.SUBSCRIBE, ZMQ_TOPIC)
    sub.connect(zmq_addr)

    poller = zmq.Poller()
    poller.register(sub, zmq.POLLIN)

    # Serial connection to ESP32
    ser = serial.Serial(
        port=SERIAL_PORT,
        baudrate=BAUD_RATE,
        timeout=0.0,
        write_timeout=0.2,
    )

    print(f"Listening to ZMQ at {zmq_addr} on topic '{ZMQ_TOPIC}'")
    print(f"Sending serial commands to ESP32 on {SERIAL_PORT} @ {BAUD_RATE}")
    print("Press Ctrl+C to stop.\n")

    # Latest values for display
    last_seq = None
    last_steer = 0.0
    last_throttle = 0.0
    last_brake = 0.0
    last_send_ts = 0.0
    last_recv_ts = 0.0

    # Latest ESP32 acknowledgment info
    last_ack_seq = None
    last_ack_time = None

    sent_count = 0

    try:
        while True:
            # ---------------------------------
            # Read any lines coming back from ESP32
            # ---------------------------------
            while ser.in_waiting > 0:
                raw = ser.readline()
                if not raw:
                    break

                line = raw.decode("utf-8", errors="replace").strip()
                if not line:
                    continue

                if line.startswith("ACK;"):
                    try:
                        last_ack_seq = int(line.split(";", 1)[1])
                        last_ack_time = time.time()
                    except ValueError:
                        pass
                elif PRINT_DEBUG_LINES:
                    print(f"\nESP32: {line}")

            # ---------------------------------
            # Read newest ZMQ message
            # ---------------------------------
            events = dict(poller.poll(timeout=20))
            if sub in events and events[sub] == zmq.POLLIN:
                msg = sub.recv_string()

                try:
                    topic, payload = msg.split(" ", 1)
                    data = json.loads(payload)
                except Exception:
                    continue

                seq = int(data.get("seq", 0))
                steering = float(data.get("steering", 0.0))
                throttle = float(data.get("throttle", 0.0))
                brake = float(data.get("brake", 0.0))
                send_ts = float(data.get("send_ts", 0.0))

                # Keep values in expected range
                steering = clamp(steering, -1.0, 1.0)
                throttle = clamp(throttle, 0.0, 1.0)
                brake = clamp(brake, 0.0, 1.0)

                # Steering-only mode for safe testing
                if STEERING_ONLY:
                    throttle = 0.0
                    brake = 0.0

                # Format expected by ESP32:
                # CMD;<seq>;<steer>;<throttle>;<brake>
                cmd = f"CMD;{seq};{steering:.4f};{throttle:.4f};{brake:.4f}\n"
                ser.write(cmd.encode("utf-8"))
                ser.flush()

                sent_count += 1
                last_seq = seq
                last_steer = steering
                last_throttle = throttle
                last_brake = brake
                last_send_ts = send_ts
                last_recv_ts = time.time()

            # ---------------------------------
            # Live one-line status display
            # ---------------------------------
            now = time.time()

            net_latency_ms = 0.0
            if last_send_ts and last_recv_ts:
                net_latency_ms = (last_recv_ts - last_send_ts) * 1000.0

            if last_ack_seq is None:
                ack_text = "ACK: none"
            else:
                ack_age = now - last_ack_time if last_ack_time else 0.0
                ack_text = f"ACK: {last_ack_seq} ({ack_age:.2f}s ago)"

            status = (
                f"SEQ: {str(last_seq).rjust(6)} | "
                f"Steer: {last_steer:+.3f} | "
                f"Throttle: {last_throttle:.3f} | "
                f"Brake: {last_brake:.3f} | "
                f"Net: {net_latency_ms:7.2f} ms | "
                f"{ack_text} | "
                f"Sent: {sent_count}"
            )

            sys.stdout.write("\r" + status + "\x1b[K")
            sys.stdout.flush()

            time.sleep(0.01)

    except KeyboardInterrupt:
        print("\nStopped receiver bridge.")
    finally:
        try:
            ser.close()
        except Exception:
            pass
        sub.close()
        ctx.term()


if __name__ == "__main__":
    main()