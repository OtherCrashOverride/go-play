# go-play

Emulator firmware for ODROID Go.

## Setup

1. Setup esp-idf according to [the official documentation](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html)
2. Delete esp-idf directory (run in a terminal): `cd ~/esp && rm -rf esp-idf`
3. Clone the ODROID version of esp-idf: `git clone https://github.com/OtherCrashOverride/esp-idf --recursive ~/esp/esp-idf`
4. You'll also need to have your ODROID Go with go-play firmware connected to your pc (switched on) so we can extract `springboard.bin` which is currently not open source.

## Building

1. Clone this repository with `--recursive` and `cd` into it
2. Open `build.sh` and edit the `SERIAL_PORT` variable to match your setup
3. Run this in a terminal: `./build.sh`
4. If the build ran through successfully, you should now have the `Go-Play-New.fw` file that you can copy to your SD Card and flash using the ODROID Go "Bios"

## Troubleshooting

- If the build fails with `[Errno 2] No such file or directory: '/dev/ttyUSB0'`
    1. Make sure your ODROID Go is plugged in and switched on
    2. Restart the build
- If the build fails with `A fatal error occurred: Invalid head of packet (0x21)`:
    1. Make sure your ODROID Go is plugged in and switched on
    2. Restart the build