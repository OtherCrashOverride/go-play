#!/bin/bash
set -euo pipefail

#                    Copyright 2018 23pieces
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

# Please change the values below to match your setup

export PATH="$PATH:$HOME/esp/xtensa-esp32-elf/bin"
export IDF_PATH=~/esp/esp-idf
SERIAL_PORT="/dev/cu.SLAB_USBtoUART"

# You don't need to change the values below this line

PATH_ORIG=`pwd`
GO_PLAY_ORIG_FW_PATH="Go-Play.fw"
EFWT_PATH="efwt"
GOPLAY_SOURCE_PATH="."
MKFW_PATH="odroid-go-firmware/tools/mkfw"

echo "Compiling mkfw"
cd $MKFW_PATH
make
cd $PATH_ORIG

echo "Compiling Gnuboy"
cd $GOPLAY_SOURCE_PATH/gnuboy-go
make all
echo ""
cp ./build/gnuboy-go.bin $PATH_ORIG

echo "Compiling Nesemu"
cd ../nesemu-go
make all
echo ""
cp ./build/nesemu-go.bin $PATH_ORIG

echo "Compiling Smsplusgx"
cd ../smsplusgx-go
make all
echo ""
cp ./build/smsplusgx-go.bin $PATH_ORIG

cd $PATH_ORIG

if [ ! -f springboard.bin ]; then
	echo "Reading springboard from flash"
	./esptool.py --port $SERIAL_PORT --baud 921600 read_flash 0x100000 0x100000 springboard.bin
fi

echo "Building mkfw"
cd $MKFW_PATH
make

if [ ! -f tile.raw ]; then

    echo "Downloading the latest precompiled version of go-play to extract the tile..."
    cd $PATH_ORIG

    # TODO: This might break when the latest release contains no .fw file...
    curl https://api.github.com/repos/OtherCrashOverride/go-play/releases/latest | python -c "import sys, json; print json.load(sys.stdin)['assets'][0]['browser_download_url']" | xargs curl -o $GO_PLAY_ORIG_FW_PATH

	cd $EFWT_PATH
	make
	chmod +x efwt

	cd $PATH_ORIG

	echo "Extracting tile from original go-play.fw"
	$EFWT_PATH/efwt $GO_PLAY_ORIG_FW_PATH tile.raw

    echo "Done. Deleting prebuilt go-play.fw..."

	rm $GO_PLAY_ORIG_FW_PATH
fi

cd $PATH_ORIG
$MKFW_PATH/mkfw Go-Play tile.raw 0 16 1048576 springboard springboard.bin 0 17 1048576 nesemu nesemu-go.bin 0 18 1048576 gnuboy gnuboy-go.bin 0 19 2097152 smsplusgx smsplusgx-go.bin
mv firmware.fw Go-Play-New.fw

#To convert the picture into a png run:
#ffmpeg -f rawvideo -pixel_format rgb565 -video_size 86x48 -i tile.raw output.png
#display output.png