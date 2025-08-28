#!/usr/bin/env python3
"""
ESP32-C6 Firmware Flasher for M5Stack Tab5
Multiple approaches to flash the C6 co-processor
"""

import subprocess
import time
import sys
import os

def check_esptool():
    """Check if esptool is available"""
    try:
        # Try ESP-IDF environment first
        result = subprocess.run([
            '/Users/a.vasilyev/.espressif/python_env/idf5.5_py3.13_env/bin/python',
            '-m', 'esptool', '--version'
        ], capture_output=True, text=True)
        if result.returncode == 0:
            print(f"✓ esptool found: {result.stdout.strip()}")
            return '/Users/a.vasilyev/.espressif/python_env/idf5.5_py3.13_env/bin/python'
    except FileNotFoundError:
        pass
    
    print("✗ esptool not found. Please source ESP-IDF environment")
    return None

def try_m5burner_method():
    """Try using M5Burner firmware if available"""
    print("\n🔥 Method 1: M5Burner firmware")
    
    # Check if we can find the correct firmware in M5Burner
    m5_fw_path = "/Applications/M5Burner.app/Contents/Resources/packages/firmware/54caf67e0320eb26ca61fa356c61ae83.bin"
    
    if os.path.exists(m5_fw_path):
        print(f"Found M5Burner firmware: {m5_fw_path}")
        return m5_fw_path
    
    print("M5Burner firmware not found")
    return None

def flash_c6_direct(python_cmd, port, firmware_path):
    """Try to flash C6 directly via USB"""
    print(f"\n📡 Flashing C6 firmware: {firmware_path}")
    print(f"🔌 Port: {port}")
    
    cmd = [
        python_cmd, '-m', 'esptool',
        '--chip', 'esp32c6',
        '--port', port,
        '--baud', '115200',
        '--before', 'default_reset',
        '--after', 'hard_reset',
        'write_flash',
        '--flash_mode', 'dio',
        '--flash_size', '4MB',
        '--flash_freq', '40m',
        '0x0', firmware_path
    ]
    
    print(f"Command: {' '.join(cmd)}")
    
    try:
        result = subprocess.run(cmd, timeout=120)
        return result.returncode == 0
    except Exception as e:
        print(f"Flash failed: {e}")
        return False

def main():
    print("="*60)
    print("ESP32-C6 Firmware Flasher for M5Stack Tab5")
    print("="*60)
    
    # Check if firmware file exists
    if not os.path.exists(FIRMWARE_FILE):
        print(f"Error: Firmware file not found: {FIRMWARE_FILE}")
        return 1
    
    print(f"Firmware file: {FIRMWARE_FILE}")
    print(f"File size: {os.path.getsize(FIRMWARE_FILE)} bytes")
    
    # Method 1: Try to find C6 serial port
    print("\nMethod 1: Direct serial connection")
    c6_port = find_c6_port()
    
    if c6_port:
        print(f"Found potential C6 port: {c6_port}")
        if flash_with_esptool(c6_port, FIRMWARE_FILE):
            print("Success! C6 firmware flashed.")
            return 0
    else:
        print("No C6 serial port found")
    
    # Method 2: Try via P4 control
    print("\nMethod 2: Control via ESP32-P4")
    if reset_c6_via_p4():
        # Check again for serial port
        time.sleep(2)
        c6_port = find_c6_port()
        if c6_port:
            if flash_with_esptool(c6_port, FIRMWARE_FILE):
                print("Success! C6 firmware flashed via P4 control.")
                return 0
    
    # Method 3: SDIO method
    print("\nMethod 3: SDIO flash")
    if try_flash_via_sdio():
        print("Success! C6 firmware flashed via SDIO.")
        return 0
    
    print("\n" + "="*60)
    print("IMPORTANT: ESP32-C6 firmware could not be flashed automatically")
    print("\nManual flashing options:")
    print("1. Check if there are test points on the PCB for C6 UART")
    print("2. Contact M5Stack support for factory flashing procedure")
    print("3. The C6 might be accessible via USB-C in download mode")
    print("\nTo manually flash if you find the port:")
    print(f"  esptool.py --chip esp32c6 -p PORT write_flash 0x0 {FIRMWARE_FILE}")
    print("="*60)
    
    return 1

if __name__ == "__main__":
    sys.exit(main())