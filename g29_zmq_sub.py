import time
import json
import zmq

ZMQ_CONNECT_ADDR = "tcp://localhost:5556"  # publisher address


def main():
    ctx = zmq.Context()
    sock = ctx.socket(zmq.SUB)
    sock.connect(ZMQ_CONNECT_ADDR)

    # If publisher uses topic prefix "g29 ", subscribe to it.
    sock.setsockopt_string(zmq.SUBSCRIBE, "g29 ")

    print(f"ZeroMQ SUB connected to {ZMQ_CONNECT_ADDR}")

    try:
        while True:
            raw = sock.recv_string()  # e.g. "g29 { ... }"
            topic, payload = raw.split(" ", 1)
            data = json.loads(payload)

            receive_ts = time.time()
            send_ts = data.get("send_ts") or data.get("t_send") or data.get("ts")

            latency = None
            if send_ts is not None:
                latency = receive_ts - float(send_ts)

            print(
                f"recv_ts={receive_ts:.6f} | send_ts={send_ts:.6f} | latency={latency:.6f} | "
                f"steer={data.get('steering'): .3f} throttle={data.get('throttle'): .3f} brake={data.get('brake'): .3f}"
                )

    except KeyboardInterrupt:
        print("Stopped.")
    finally:
        sock.close()
        ctx.term()


if __name__ == "__main__":
    main()
