# IDP Locker: TCA8418 keypad integration

## Upload this file

**Open `LockerKeypad/LockerKeypad.ino` in Arduino IDE and upload to Arduino Uno.** Keep `Firmware.h`, `Sha256.h`, `Keypad.h`, `Keypad.cpp`, and `KeypadPlatform.h` in that same folder. No third-party libraries are required. The sketch targets an ATmega328P at 16 MHz. You can then transfer the programmed chip to your existing standalone circuit with its crystal, capacitors, reset and power wiring.

`main.cpp` and `build.sh` are optional standalone build files; do not upload those in Arduino IDE. The Python files are computer-side software tests.

## Connections

| Signal | Uno name | ATmega328P DIP pin | Connection |
|---|---|---|---|
| Software I2C clock | D2 | 4 | TCA8418 SCL, original wiring |
| Software I2C data | D3 | 5 | TCA8418 SDA, original wiring |
| Green LED | A0 / PC0 | 23 | Existing green LED and its resistor; moved from D3 |
| Red LED | D4 | 6 | Existing red LED and resistor |
| Amber LED | D5 | 11 | Existing amber LED and resistor |
| Buzzer signal | D6 | 12 | Existing buzzer circuit |
| Servo signal | D9 / OC1A | 15 | Existing servo signal |
| Ground | GND | 8, 22 | Common ground with keypad and servo supplies |

D3 cannot be both SDA and an LED output. Move only the green LED signal wire from D3 to A0. A0 is used as a digital output; the ADC stays off. Do not connect a door sensor to D2 in this version; D2 is the keypad clock. This integrates the latest basic serial-test firmware, which has no door sensor input or ESP32 behavior.

The keypad is a **4-row, 3-column / 12-key matrix**, connected to ROW0–ROW3 and COL0–COL2. Map: `123 / 456 / 789 / *0#`. The original address search (0x34–0x37) is retained; the TCA8418 normally responds at 0x34. Matrix wires must match the row/column order.

Keep the existing I2C pull-ups and keypad supply arrangement. The **bare TCA8418 supply is 1.65–3.6 V, not 5 V**; bus pull-ups must use a suitable keypad logic rail. The driver drives SDA/SCL LOW or releases them, without enabling AVR pull-ups to 5 V. Check your breakout's regulator/level-shifting arrangement before powering it. Use the existing servo supply; the code does not switch its power.

## Controls

Open Serial Monitor at **9600 baud** for prompts and results. Keypad digits are not printed. The original newline-terminated serial commands also remain available for diagnostics.

| Action | Key sequence |
|---|---|
| First enrollment | Enter 6–24 digits, `#`, repeat the same digits, `#` |
| Unlock | Enter saved PIN, then `#` |
| Lock | With entry empty and no password-change prompt active, press `#` |
| Clear/cancel | `*` |
| Start password change | `*#` |
| Complete change | Current PIN `#`, new PIN `#`, repeat new PIN `#` |

Example first enrollment: `123456#123456#`. Lock with `#`; unlock with `123456#`. Change with `*#123456#654321#654321#`.

After pressing `*`, the next `#` requests a password change; entering digits instead starts a fresh PIN entry. Pressing `*` cancels any enrollment confirmation or password-change workflow. A PIN longer than 24 digits is rejected in full on `#`. Input during servo motion or lockout is discarded. Three incorrect passwords trigger the existing 30-second lockout.

The minimum is **six digits**, inherited from the latest serial-test firmware. No new four-digit default or master override is introduced. Existing EEPROM credentials and their journal format remain unchanged. If a previously enrolled serial password contains letters or symbols, authenticate through Serial Monitor and use `CHANGE` to replace it with a numeric PIN before using keypad-only access. Do not erase EEPROM just to test the keypad.

On boot, the commanded servo position remains UNKNOWN. After enrolling, use `#` to command the initial locked position. The movement interval is 600 ms, with LOCK=1000 us and UNLOCK=2000 us, unchanged from the latest serial version. Calibrate these values for your mechanism. Completion is a timed command, not a measured physical position. There is no blockage detection.

## What changed

- Reused the software I2C transport, TCA8418 register setup, row/column mapping and decoder from `Servo/main/Keypad.cpp`.
- Corrected event polarity: bit 7 set means PRESS, cleared means RELEASE (datasheet section 8.4.2).
- Consume hardware-debounced FIFO events in order, returning one press at a time. This preserves quick taps and repeated digits even when press/release are already queued. Removed the old state-based 30 ms software debounce from the active input path.
- Enabled overflow detection with CFG bits 3 and 5 per the datasheet overflow errata. Bus errors or FIFO overflow clear partial keypad input and pending password changes. Retry initialization once per second; stale events are discarded during reinitialization. Re-enter the PIN after recovery.
- Added an AVR platform adapter so the original keypad transport can coexist with the serial firmware's own UART and timers. Arduino `millis()` and `Serial` are not used by the integrated firmware.
- Kept the existing salted SHA-256 storage, EEPROM journal, LED/buzzer patterns, timed servo control and serial commands.
- Keypad is polled every 5 ms. No TCA8418 INT wire is required. During IDLE, Timer2 remains active to wake the processor every 1 ms for continued polling. This is not Power-down and will draw more current than an interrupt-driven implementation. Partial entry and password-change authorization expire after 10 seconds without input.

The original `Servo/main/main.ino` was an angle-per-key demonstration with missing semicolons and switch fall-through. It is not used as the locker's state machine. The integrated sketch uses the latest `without Keypad/LockerSerial/Firmware.h` instead.

## Verification and bench test

The standalone AVR build passed with `avr-g++` for ATmega328P / 16 MHz, with warnings treated as errors. Flash text is 11,432 bytes; initialized data is 86 bytes and static BSS is 263 bytes (349 bytes static SRAM, excluding stack). The build outputs are included under `build/`. Prefer uploading the `.ino` through Arduino IDE; the standalone HEX has no bootloader bundled.

Software checks are supplied in `test_firmware.py` and `test_keypad_driver.py`:

```
python3 test_firmware.py
python3 test_keypad_driver.py
```

These compile the actual credential/parser/state and FIFO consumer code for the host, replacing hardware boundaries. They cover enrollment, EEPROM persistence and interrupted commits, SHA-256 reference vectors, password changes, lockout, timed motion, keypad controls, all 12 key mappings, press/release polarity, quick repeated taps, and bus/overflow rejection. They do not verify electrical signaling, AVR interrupt timing, sleep current, or physical servo movement.

Bench sequence:

1. Verify wiring and open Serial Monitor at 9600 baud. Confirm no keypad initialization failure.
2. Enroll a numeric PIN if EEPROM is blank; otherwise use the existing credential.
3. Command lock with empty `#`; authenticate and verify unlock movement and feedback.
4. Test a held digit, quick repeated digits, and `*` cancellation. Each press should enter once; release should enter nothing.
5. Change the PIN with `*#`; verify the old PIN fails and the new PIN succeeds.
6. Verify three failures cause a 30-second lockout and keypad input during motion is ignored.
7. Wait 10 seconds, then enter the complete PIN. The first key should be preserved after IDLE.
8. Power cycle and confirm PIN persistence. If the keypad disconnects, reconnect, wait for the recovery prompt, and re-enter the complete PIN.

No physical hardware was connected during preparation. Treat the above as the required bench validation before your demonstration.
