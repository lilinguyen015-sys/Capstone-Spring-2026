# g29_zmq_pub.py
import json
import time
import pygame
import zmq
import argparse

# Axis mapping for your G29 (confirmed)
STEER_AXIS_INDEX    = 0
THROTTLE_AXIS_INDEX = 2
BRAKE_AXIS_INDEX    = 3

REFRESH_HZ = 60

# ZMQ: bind on all interfaces, port 5558
PORT = 5558
ZMQ_BIND_ADDR = f"tcp://*:{PORT}"


def normalize_01_from_minus1_1(v: float) -> float:
    """Map input in [-1, 1] to [0, 1] with clamping."""
    return max(0.0, min(1.0, (v + 1.0) / 2.0))


def main(print_payload: bool = False):
    pygame.init()
    pygame.joystick.init()

    if pygame.joystick.get_count() == 0:
        print("No joystick found.")
        return

    js = pygame.joystick.Joystick(0)
    js.init()

    num_axes = js.get_numaxes()
    print(f"G29 publisher using joystick: {js.get_name()}")
    print(f"Number of axes: {num_axes}")
    print(f"Publishing at {REFRESH_HZ} Hz")

    ctx = zmq.Context()
    sock = ctx.socket(zmq.PUB)
    sock.bind(ZMQ_BIND_ADDR)
    sock.setsockopt(zmq.LINGER, 0)
    print(f"ZeroMQ PUB bound on {ZMQ_BIND_ADDR}")
    print(f"Subscriber should connect to tcp://<this-laptop-ip>:{PORT} with topic 'g29'.")

    period = 1.0 / REFRESH_HZ
    loop_count = 0
    seq = 0

    try:
        while True:
            t0 = time.time()

            # Pump pygame events
            pygame.event.get()

            axes = [js.get_axis(i) for i in range(num_axes)]

            steering_raw = axes[STEER_AXIS_INDEX]    if num_axes > STEER_AXIS_INDEX    else 0.0
            thr_raw      = axes[THROTTLE_AXIS_INDEX] if num_axes > THROTTLE_AXIS_INDEX else -1.0
            brk_raw      = axes[BRAKE_AXIS_INDEX]    if num_axes > BRAKE_AXIS_INDEX    else -1.0

            throttle = normalize_01_from_minus1_1(thr_raw)
            brake    = normalize_01_from_minus1_1(brk_raw)

            send_ts = time.time()
            msg = {
                "seq": seq,
                "event": "g29_state",
                "send_ts": send_ts,
                "steering": float(steering_raw),  # [-1, 1]
                "throttle": float(throttle),      # [0, 1]
                "brake": float(brake),            # [0, 1]
            }
            seq += 1

            # Topic + space + JSON payload (matches your subscriber)
            sock.send_string("g29 " + json.dumps(msg))

            # If requested, print every payload we send (single-line overwrite).
            if print_payload:
                # Print raw JSON on one terminal line, overwrite previous.
                print("\r" + json.dumps(msg) + "\x1b[K", end="", flush=True)
            else:
                # Print every 10 loops (~6 Hz) for readability
                loop_count += 1
                if loop_count >= 10:
                    line = (
                        f"SEQ {msg['seq']:6d} | "
                        f"Steer: {steering_raw:+.3f} | "
                        f"Gas raw: {thr_raw:+.3f} -> {throttle:.3f} | "
                        f"Brake raw: {brk_raw:+.3f} -> {brake:.3f}"
                    )
                    # Print in-place on a single terminal line. Prepend CR, append ANSI clear to end
                    # so shorter updates don't leave trailing characters from previous prints.
                    print("\r" + line + "\x1b[K", end="", flush=True)
                    loop_count = 0

            dt = time.time() - t0
            if dt < period:
                time.sleep(period - dt)

    except KeyboardInterrupt:
        print("\nStopped G29 publisher.")
    finally:
        js.quit()
        pygame.quit()
        sock.close()
        ctx.term()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="G29 publisher (ZMQ).")
    parser.add_argument(
        "-p",
        "--print",
        dest="print",
        action="store_true",
        help="Print every JSON payload sent (single-line overwrite).",
    )
    args = parser.parse_args()
    main(print_payload=args.print)