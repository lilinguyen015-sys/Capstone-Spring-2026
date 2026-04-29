"""
Logitech G29 UDP Streamer.

Captures input from a Logitech G29 racing wheel using Pygame and streams
the normalized axis data to a remote Oracle Cloud Server via UDP.
"""

import socket
import time
import pygame

# Constants
STEER_AXIS_INDEX = 0
THROTTLE_AXIS_INDEX = 2
BRAKE_AXIS_INDEX = 3

SERVER_IP = "147.224.143.221"
SERVER_PORT = 4210
REFRESH_HZ = 60


def normalize_01_from_minus1_1(val: float) -> float:
    """Normalize a value from range [-1, 1] to [0, 1]."""
    return max(0.0, min(1.0, (val + 1.0) / 2.0))


def main():
    """Main execution loop for capturing and sending joystick data."""
    pygame.init()
    pygame.joystick.init()

    if pygame.joystick.get_count() == 0:
        print("No joystick found.")
        return

    joystick = pygame.joystick.Joystick(0)
    joystick.init()

    print(f"Joystick detected: {joystick.get_name()}")
    print(f"Sending UDP to OCI Server at {SERVER_IP}:{SERVER_PORT}")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sequence = 0
    period = 1.0 / REFRESH_HZ

    try:
        while True:
            start_time = time.time()

            pygame.event.pump()

            steering = joystick.get_axis(STEER_AXIS_INDEX)
            throttle = 1.0 - normalize_01_from_minus1_1(
                joystick.get_axis(THROTTLE_AXIS_INDEX)
            )
            brake = 1.0 - normalize_01_from_minus1_1(
                joystick.get_axis(BRAKE_AXIS_INDEX)
            )

            message = f"CMD;{sequence};{steering:.4f};{throttle:.4f};{brake:.4f}"
            sock.sendto(message.encode("utf-8"), (SERVER_IP, SERVER_PORT))

            print(
                f"seq={sequence:05d} | "
                f"steer={steering: .4f} | "
                f"throttle={throttle:.4f} | "
                f"brake={brake:.4f}"
            )

            sequence += 1

            elapsed = time.time() - start_time
            sleep_time = period - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)

    except KeyboardInterrupt:
        print("\nStopped sender.")
    finally:
        sock.close()
        pygame.quit()


if __name__ == "__main__":
    main()