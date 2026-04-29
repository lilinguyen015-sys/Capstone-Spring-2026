# g29_zmq_pub.py
import json
import time
import pygame
import zmq

STEER_AXIS_INDEX    = 0
THROTTLE_AXIS_INDEX = 2
BRAKE_AXIS_INDEX    = 3

REFRESH_HZ = 60
ZMQ_BIND_ADDR = "tcp://*:5556"   # bind on all interfaces, port 5556

def normalize_01_from_minus1_1(v: float) -> float:
    return max(0.0, min(1.0, (v + 1.0) / 2.0))

def main():
    pygame.init()
    pygame.joystick.init()

    if pygame.joystick.get_count() == 0:
        print("No joystick found.")
        return

    js = pygame.joystick.Joystick(0)
    js.init()

    num_axes = js.get_numaxes()
    print(f"🚀 LOW LATENCY MODE: {js.get_name()}")
    print("Math is running at 60 Hz. Screen updates at ~6Hz.")

    # Counter to limit print speed without limiting math speed
    loop_count = 0
    sequence = 0  # monotonic packet sequence number
    print(f"Number of axes: {num_axes}")

    ctx = zmq.Context()
    sock = ctx.socket(zmq.PUB)
    sock.bind(ZMQ_BIND_ADDR)
    print(f"ZeroMQ PUB bound on {ZMQ_BIND_ADDR}")

    period = 1.0 / REFRESH_HZ

    try:
        while True:
            t0 = time.time()
            pygame.event.get()

            axes = [js.get_axis(i) for i in range(num_axes)]

            steering_raw = axes[STEER_AXIS_INDEX] if num_axes > STEER_AXIS_INDEX else 0.0
            thr_raw      = axes[THROTTLE_AXIS_INDEX] if num_axes > THROTTLE_AXIS_INDEX else -1.0
            brk_raw      = axes[BRAKE_AXIS_INDEX] if num_axes > BRAKE_AXIS_INDEX else -1.0

            throttle = normalize_01_from_minus1_1(thr_raw)
            brake    = normalize_01_from_minus1_1(brk_raw)

            send_ts = time.time()  # send timestamp for latency measurement
            sequence += 1
            msg = {
                "t_send": send_ts,
                "send_ts": send_ts,  # alias for compatibility
                "seq": sequence,
                "steering": float(steering_raw),
                "throttle": float(throttle),
                "brake": float(brake),
            }

            # optional: prefix with a topic string (ZMQ-style)
            sock.send_string("g29 " + json.dumps(msg))

            # 3. OPTIMIZATION: Only print every 10th loop (since 60 Hz, ~6 Hz print)
            loop_count += 1
            if loop_count >= 10:
                print(f"Steer: {steering_raw:.3f} | Gas: {thr_raw:.3f} | Brake: {brk_raw:.3f}", end="\r")
                loop_count = 0

            dt = time.time() - t0
            if dt < period:
                time.sleep(period - dt)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        js.quit()
        pygame.quit()
        sock.close()
        ctx.term()

if __name__ == "__main__":
    main()