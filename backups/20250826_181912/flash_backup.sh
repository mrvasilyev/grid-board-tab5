#!/usr/bin/env bash
set -euo pipefail
PORT="${1:-/dev/cu.usbmodem212301}"
BASEDIR="$(cd "$(dirname "$0")" && pwd)"
ESPIDF_PY="$HOME/.espressif/python_env/idf5.4_py3.13_env/bin/python"
ESP_IDF_TOOLS="$HOME/esp/esp-idf/components/esptool_py/esptool/esptool.py"
# Fallback to 'esptool.py' in PATH if not found
if [ ! -x "$ESPIDF_PY" ] || [ ! -f "$ESP_IDF_TOOLS" ]; then
  echo "Using esptool.py from PATH"
  ESPTOOL="esptool.py"
else
  ESPTOOL="$ESPIDF_PY"";""$ESP_IDF_TOOLS"
fi
# Flash
$ESPIDF_PY -m esptool --chip esp32p4 -p "$PORT" -b 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x2000 "$BASEDIR/bootloader.bin" \
  0x8000 "$BASEDIR/partition-table.bin" \
  0x10000 "$BASEDIR/m5stack_tab5.bin"
