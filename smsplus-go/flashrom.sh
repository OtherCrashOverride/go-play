#!/bin/bash
. ${IDF_PATH}/add_path.sh
esptool.py --chip esp32 --port "/dev/ttyUSB0" --baud $((230400*4)) --after no_reset write_flash -fs detect 0x200000 "$1"
file=$(basename "$1")
echo -en "${file}\0" > temp.bin
esptool.py --chip esp32 --port "/dev/ttyUSB0" --baud $((230400*4)) write_flash -fs detect 0x300000 temp.bin
