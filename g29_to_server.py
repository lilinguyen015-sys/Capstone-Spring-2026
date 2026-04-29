"""
Logitech G29 UDP Streamer.

Captures input from a Logitech G29 racing wheel using Pygame and streams
the normalized axis data to a remote Oracle Cloud Server via UDP.

Safety Features:
- Sends STOP command on graceful exit
- Monitors joystick connection and triggers E-STOP if disconnected
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
JOYSTICK_TIMEOUT = 0.5  # Emergency stop if joystick disconnects for 500ms


def normalize_01_from_minus1_1(val: float) -> float:
    """Normalize a value from range [-1, 1] to [0, 1]."""
    return max(0.0, min(1.0, (val + 1.0) / 2.0))


def send_stop_command(sock, ip, port):
    """Send emergency STOP command to server"""
    stop_msg = "STOP;0;0.0;0.0;0.0"
    sock.sendto(stop_msg.encode("utf-8"), (ip, port))
    print("[EMERGENCY] STOP command sent to server")


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
    print(f"[SAFETY] Emergency STOP will trigger if joystick disconnects for {JOYSTICK_TIMEOUT}s")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sequence = 0
    period = 1.0 / REFRESH_HZ
    last_valid_input_time = time.time()
    estop_triggered = False

    try:
        while True:
            start_time = time.time()

            pygame.event.pump()

            # Check if joystick is still connected
            if pygame.joystick.get_count() == 0:
                elapsed_no_input = time.time() - last_valid_input_time
                if elapsed_no_input > JOYSTICK_TIMEOUT and not estop_triggered:
                    print("[EMERGENCY] Joystick disconnected! Sending STOP command.")
                    send_stop_command(sock, SERVER_IP, SERVER_PORT)
                    estop_triggered = True
                time.sleep(0.01)  # Prevent busy-waiting
                continue

            steering = joystick.get_axis(STEER_AXIS_INDEX)
            throttle = normalize_01_from_minus1_1(
                joystick.get_axis(THROTTLE_AXIS_INDEX)
            )
            brake = normalize_01_from_minus1_1(
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
            last_valid_input_time = time.time()
            estop_triggered = False

            elapsed = time.time() - start_time
            sleep_time = period - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)

    except KeyboardInterrupt:
        print("\n[SAFETY] Sending STOP command...")
        send_stop_command(sock, SERVER_IP, SERVER_PORT)
        print("Stopped sender.")
    finally:
        sock.close()
        pygame.quit()


if __name__ == "__main__":
    main()
