#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
AVR_CXX="${AVR_CXX:-avr-g++}"
AVR_OBJCOPY="${AVR_OBJCOPY:-avr-objcopy}"
AVR_SIZE="${AVR_SIZE:-avr-size}"
"$AVR_CXX" -mmcu=atmega328p -DF_CPU=16000000UL -Os -std=gnu++11 -Wall -Wextra -Werror -Wno-misleading-indentation -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti main.cpp -Wl,--gc-sections -o build/locker.elf
"$AVR_OBJCOPY" -O ihex -R .eeprom build/locker.elf build/locker.hex
"$AVR_SIZE" -B build/locker.elf
