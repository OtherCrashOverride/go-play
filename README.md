# go-play

Emulator firmware for ODROID Go.

## Setup

1. Setup esp-idf according to [the official documentation](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html)
2. Delete esp-idf directory (run in a terminal): `cd ~/esp && rm -rf esp-idf`
3. Clone the ODROID version of esp-idf: `git clone https://github.com/OtherCrashOverride/esp-idf --recursive ~/esp/esp-idf`
4. You'll also need to have your ODROID Go with go-play firmware connected to your pc (switched on) so we can extract `springboard.bin` which is currently not open source.

## Building

1. Clone this repository with `--recursive` and `cd` into it
2. Run this in a terminal: `./make.sh`