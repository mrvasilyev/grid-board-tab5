#!/usr/bin/env python3
import serial
import sys
import time

# Open serial port
port = '/dev/cu.usbmodem2101'
baudrate = 115200

try:
    ser = serial.Serial(port, baudrate, timeout=1)
    print(f"Connected to {port} at {baudrate} baud")
    print("Reading serial output... Press Ctrl+C to exit\n")
    
    # Read for 20 seconds
    start_time = time.time()
    while time.time() - start_time < 20:
        if ser.in_waiting:
            data = ser.readline()
            try:
                print(data.decode('utf-8', errors='ignore').strip())
            except:
                pass
                
    ser.close()
except serial.SerialException as e:
    print(f"Error: {e}")
    sys.exit(1)
except KeyboardInterrupt:
    print("\nExiting...")
    ser.close()