#!/usr/bin/env python3
"""
Delete C6 firmware backup file from SD card to exit bridge mode
"""

import os
import sys

backup_file = "/Volumes/SD32G/c6_firmware_backup.bin"

if os.path.exists(backup_file):
    try:
        os.remove(backup_file)
        print(f"✅ Deleted {backup_file}")
        print("Bridge mode will be disabled on next boot")
    except Exception as e:
        print(f"❌ Failed to delete {backup_file}: {e}")
        sys.exit(1)
else:
    print(f"⚠️  File not found: {backup_file}")
    print("The backup file may have already been deleted or SD card not mounted")