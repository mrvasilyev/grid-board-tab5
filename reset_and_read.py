#!/usr/bin/env python3
import serial
import sys
import time

# Open serial port
port = '/dev/cu.usbmodem2101'
baudrate = 115200

try:
    # Open with DTR/RTS control for reset
    ser = serial.Serial(port, baudrate, timeout=1)
    
    # Toggle DTR to reset the device
    ser.dtr = False
    time.sleep(0.1)
    ser.dtr = True
    time.sleep(0.1)
    
    print(f"Reset and connected to {port} at {baudrate} baud")
    print("Reading serial output...\n")
    print("-" * 50)
    
    # Read for 30 seconds
    start_time = time.time()
    while time.time() - start_time < 30:
        if ser.in_waiting:
            data = ser.read(ser.in_waiting)
            try:
                print(data.decode('utf-8', errors='ignore'), end='')
            except:
                pass
                
    ser.close()
except serial.SerialException as e:
    print(f"Error: {e}")
    sys.exit(1)
except KeyboardInterrupt:
    print("\nExiting...")
    ser.close()