"""
UDP Relay Server.

Runs on an Oracle Cloud instance and acts as a middleman between the
operator laptop and the ESP32 in the vehicle.  The ESP32 registers
itself by sending periodic HEARTBEAT packets; the server forwards all
other incoming packets to the last registered ESP32 address.
"""

import socket

UDP_IP = "0.0.0.0"
UDP_PORT = 4210


def main():
    """Start the relay server and forward packets indefinitely."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))

    print(
        f"Relay server live on port {UDP_PORT}. Waiting for car heartbeat..."
    )

    car_address = None

    while True:
        data, addr = sock.recvfrom(1024)

        try:
            message = data.decode("utf-8").strip()
            print(f"DEBUG - Received: '{message}' from {addr}")
        except UnicodeDecodeError:
            message = ""
            print(f"DEBUG - Received raw bytes from {addr}")

        if message == "HEARTBEAT":
            car_address = addr
            print(f"✅ CAR CONNECTED! Logged IP: {addr}")
        elif car_address and addr != car_address:
            # Forward CMD packets from the operator to the car.
            # Do not bounce HEARTBEAT back to the ESP32.
            sock.sendto(data, car_address)


if __name__ == "__main__":
    main()
