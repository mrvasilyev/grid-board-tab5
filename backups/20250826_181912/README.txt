Backup firmware bundle
======================

Files
- m5stack_tab5.bin: App image
- m5stack_tab5.elf: ELF (symbols for debugging)
- bootloader.bin: Bootloader image
- partition-table.bin: Partition table
- sdkconfig.backup: Configuration snapshot
- SHA256SUMS.txt: Checksums
- flash_backup.sh: Script to flash these images

Flash
- Default serial port: /dev/cu.usbmodem212301
- Usage: ./flash_backup.sh [PORT]

Example:
  ./flash_backup.sh /dev/cu.usbmodem212301

