import pygame
import time

pygame.init()
pygame.joystick.init()

# Check for wheel
if pygame.joystick.get_count() == 0:
    print("❌ No joystick found. Check USB connection & G Hub.")
    exit()

# Connect to the first joystick (G29)
wheel = pygame.joystick.Joystick(0)
wheel.init()

print(f"✅ Connected to: {wheel.get_name()}")
print(f"👉 This wheel has {wheel.get_numaxes()} axes.")
print("Press Ctrl+C to stop.")
print("------------------------------------------------")

try:
    while True:
        pygame.event.pump()
        
        # This loop reads every single axis (0, 1, 2, 3...)
        # and prints them all on one line so you can see what changes.
        axes_values = []
        for i in range(wheel.get_numaxes()):
            val = wheel.get_axis(i)
            axes_values.append(f"Ax{i}: {val:.2f}")
            
        print(" | ".join(axes_values), end="\r") # \r overwrites the line
        time.sleep(0.1)

except KeyboardInterrupt:
    print("\nDone.")