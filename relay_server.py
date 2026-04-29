import socket

# Define where the server listens
UDP_IP = "0.0.0.0"
UDP_PORT = 4210

# Create the network socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

print(f"Relay server live on port {UDP_PORT}. Waiting for car heartbeat...")

car_address = None

while True:
    # Wait to receive a packet of data
    data, addr = sock.recvfrom(1024)
    
    try:
        # Convert bytes to text and STRIP invisible newlines/spaces
        message = data.decode('utf-8').strip()
        
        # DEBUG: Print exactly what we just received and from who
        print(f"DEBUG - Received: '{message}' from {addr}")
        
    except UnicodeDecodeError:
        message = ""
        # DEBUG: If it's raw bytes (like steering data)
        print(f"DEBUG - Received raw bytes from {addr}")

    # Check if the packet is from the ESP32 Car
    if message == "HEARTBEAT":
        car_address = addr
        print(f"✅ CAR CONNECTED! Logged IP: {addr}")
        
    # If we know where the car is, forward the data (make sure we don't bounce the heartbeat back to the car)
    elif car_address and addr != car_address:
        sock.sendto(data, car_address)