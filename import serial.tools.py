import serial
import serial.tools.list_ports
import time

def get_esp32_mac():
    # 1. Find the ESP32 Port automatically
    ports = list(serial.tools.list_ports.comports())
    esp_port = None
    
    # Look for common ESP32 USB names
    for p in ports:
        if "SLAB" in p.hwid or "USBtoUART" in p.description or "CP210" in p.hwid or "CH340" in p.hwid:
            esp_port = p.device
            break
    
    if not esp_port:
        # Fallback: Just grab the first USB serial device found
        usb_ports = [p.device for p in ports if "Bluetooth" not in p.device]
        if usb_ports:
            esp_port = usb_ports[0]
        else:
            print("❌ No ESP32 found. Plug it in!")
            return

    print(f"🔌 Connecting to {esp_port}...")

    try:
        # 2. Open Connection
        ser = serial.Serial(esp_port, 115200, timeout=2)
        
        # 3. THE MAGIC: Reset the board via software (DTR/RTS)
        # This is equivalent to pressing the "EN" button on the board
        print("🔄 Resetting board to read boot data...")
        ser.dtr = False
        ser.rts = False
        time.sleep(0.1)
        ser.dtr = True
        ser.rts = True
        
        # 4. Read the output
        print("📥 Listening for MAC Address...\n")
        start_time = time.time()
        
        while time.time() - start_time < 5: # Listen for 5 seconds
            if ser.in_waiting:
                try:
                    # Read line and decode to text
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        print(f"   [ESP32 SAYS]: {line}")
                        # Check if it looks like a MAC address (checking for colons)
                        if line.count(':') == 5: 
                            print(f"\n✅ POTENTIAL MAC FOUND: {line}")
                except Exception:
                    pass
        
        ser.close()
        print("\n✅ Done.")

    except Exception as e:
        print(f"❌ Error: {e}")

if __name__ == "__main__":
    get_esp32_mac()