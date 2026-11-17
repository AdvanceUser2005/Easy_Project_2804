# IDP Personal Locker — basic serial bench test

Target: standalone ATmega328P-PU (28-pin DIP), existing 16 MHz crystal and 5 V supply. The Arduino Uno provides USB serial and bench power.

Open `LockerSerial/LockerSerial.ino`. Serial Monitor: **9600 baud, 8-N-1, Newline or Both NL & CR**.

## START HERE — the main file to upload

**Open `locker_firmware/LockerSerial/LockerSerial.ino` in Arduino IDE and click Upload.** This is the main sketch. Arduino IDE compiles it together with `Firmware.h` and `Sha256.h`; keep all three files in the same `LockerSerial` folder. You do not upload the headers separately.

Your workflow uses the removable ATmega328P chip in an Arduino Uno:

1. Extract this ZIP completely. Open `LockerSerial.ino` from the extracted `LockerSerial` folder.
2. Leave the ATmega328P installed in the Uno for uploading. Disconnect external wiring from D0/D1, and remove any Uno RESET-to-GND jumper.
3. Connect the Uno over USB. In Arduino IDE select **Tools → Board → Arduino AVR Boards → Arduino Uno**, then select the Uno's port.
4. Click **Upload** and wait for it to finish successfully. Arduino IDE handles the compiler and uploader; no separate command-line AVR setup or ISP programmer is needed for your already bootloaded Uno chip.
5. Disconnect USB and all other power before gently removing the chip. Note its orientation and avoid bending its legs.
6. Put the chip in your existing breadboard circuit with the 16 MHz crystal, load capacitors, reset pull-up, supply decoupling, 5 V and common GND. Check pin 1 orientation before powering it.
7. Use the now-empty Uno as the USB serial bridge and bench supply, following the wiring below. With its ATmega removed, the Uno does **not** need a RESET-to-GND jumper.
8. Open Serial Monitor at **9600 baud** with **Newline**, then follow the first-test commands below. The same code runs on the breadboard; no second upload is needed.

**Ignore `main.cpp`, `build.sh`, `test_firmware.py`, `test_sha.cpp` and the `build` folder for Arduino IDE uploading.** They are optional build/test files. Do not copy `main.cpp` into the sketch folder; Arduino IDE supplies its own `main()`.

## What this version tests

- Serial password entry, initial enrollment and authenticated password changes.
- Salted SHA-256 password storage in EEPROM, retained after power removal.
- Timer/input-timing-derived 16-byte salt, regenerated when the password changes.
- Green, red and amber LEDs plus the existing passive piezo buzzer.
- Three consecutive incorrect passwords trigger a 30-second lockout.
- Nonblocking 50 Hz servo control for 600 ms per movement.
- Idle sleep after 10 seconds of inactivity; sending serial input wakes the MCU.

**No added hardware is required beyond the existing bare-chip circuit, Uno, servo, LEDs with their resistors, and passive piezo.** The servo power switch, current sensor and battery-sensing divider are removed. D8, A0 and A1 are unused. There is no keypad, ESP32, door sensor, solar circuit or separate wake button in this version.

The servo remains connected to its normal 5 V supply. After movement, the firmware stops its control pulses and drives its signal low; it does not disconnect servo power or guarantee zero servo current.

## Existing wiring

Keep your existing crystal, reset pull-up and supply-decoupling circuit. All physical pin numbers below are for the **28-pin DIP ATmega328P-PU**.

### Uno USB-to-serial bridge

For your standard Uno R3/compatible **after removing its ATmega chip**:

- Leave the Uno chip socket empty. No Uno RESET-to-GND jumper is needed.
- Uno **D0/RX → external ATmega RXD, physical pin 2**.
- External ATmega **TXD, physical pin 3 → Uno D1/TX**.
- Uno 5 V → existing external 5 V rail; Uno GND → common GND.
- Keep the **standalone ATmega's RESET pulled up**, not grounded.

These same-name RX/RX and TX/TX connections are intentional when using the Uno's USB bridge. A separate USB-UART adapter instead uses adapter TX → MCU RX and adapter RX ← MCU TX.

This serial bridge does not configure a blank chip, burn a bootloader or automatically provide an external-chip reset for uploading. Use your existing programming arrangement. If moving a bootloaded chip into the Uno socket for programming, disconnect power first and remove the Uno RESET-to-GND jumper before programming.

If you instead use a second Uno that still contains its own MCU as the bridge, hold that Uno's MCU in reset with Uno RESET → Uno GND. Never ground the external ATmega's RESET during normal operation.

### Bare chip and outputs

| Connection | Arduino name | Physical DIP pin | Wiring |
|---|---|---:|---|
| MCU supply | VCC | 7 | Existing 5 V rail |
| Analog supply | AVCC | 20 | Same 5 V rail, even with ADC unused |
| Ground | GND | 8, 22 | Common GND |
| Reset | RESET | 1 | Existing 10 kΩ pull-up; existing reset button if fitted |
| Clock | XTAL1/XTAL2 | 9, 10 | Existing 16 MHz crystal and load capacitors |
| Serial RX | D0 / PD0 | 2 | Uno D0/RX |
| Serial TX | D1 / PD1 | 3 | Uno D1/TX |
| Green LED | D3 / PD3 | 5 | Pin → existing LED resistor (e.g. 330 Ω) → anode; cathode → GND |
| Red LED | D4 / PD4 | 6 | Same LED/resistor arrangement |
| Amber LED | D5 / PD5 | 11 | Same LED/resistor arrangement |
| Passive piezo | D6 / PD6 / OC0A | 12 | Existing small, high-impedance passive piezo → GND |
| Servo signal | D9 / PB1 / OC1A | 15 | SG90 orange/yellow signal wire |
| Servo power | — | — | SG90 red → existing suitable 5 V supply; brown/black → common GND |

There is **nothing to connect to D8, A0 or A1**. Do not power the servo from a GPIO. Use your existing servo supply arrangement; supply voltage dips can reset the MCU. Do not connect two independent 5 V supply outputs together.

## First test

Send each line separately, waiting for the response:

```
SET myLocker27
myLocker27
LOCK
myLocker27
STATUS
```

The first two lines enroll and confirm the example password. `LOCK` establishes the commanded locked position; the password then requests unlocking. Choose your own password: **6–24 printable ASCII characters without spaces**, case-sensitive. `STATUS`, `CHANGE` and `CANCEL` are reserved.

Position starts **UNKNOWN** after reset because no position sensor is present. The displayed position reports the completed command interval, not a physical position measurement.

### Change password

```
CHANGE
myLocker27
myNewLocker28
myNewLocker28
```

Send command, current password, new password and confirmation as four separate lines. A mismatch leaves the existing credential unchanged. `CANCEL` or 10 seconds of inactivity clears an unfinished change.

| Input | Action |
|---|---|
| Password alone | Verify and request unlocking |
| `LOCK` | Request locking |
| `CHANGE` | Start an authenticated password change |
| `STATUS` | Show commanded state and whether a credential exists |
| `CANCEL` | Cancel enrollment confirmation/password change |
| `HELP` | Print command summary |

The old `RAW` sensor command is removed. During lockout, inputs are rejected for 30 seconds. Lockout resets on power cycling.

## LED and buzzer feedback

| Event or state | Feedback |
|---|---|
| Input character received | Brief amber flash/chirp |
| Correct password | Two green flashes/chirps |
| Wrong password | Three red flashes/chirps |
| Third consecutive wrong password | Six red flashes/chirps, then 30-second lockout |
| Password enrollment/change | Amber between event indications |
| Password saved | Two green flashes/chirps |
| Unlock movement finishes | Two green flashes/chirps; green while awake |
| Lock movement finishes | Two amber flashes/chirps; amber while awake |
| Lockout | Red between event indications |
| Sleep | LEDs and buzzer off |

Characters sent as one line can merge into one entry indication. Later events replace earlier patterns. The firmware does not echo passwords.

## Servo and sleep

`LOCK_US=1000`, `UNLOCK_US=2000`, `MOVE_MS=600`. Set the pulse widths to your previously tested mechanism positions before using the latch. The PWM period is 20 ms. Do not use the previously strained 350 µs endpoint.

Movement is enabled immediately, with no sensor calibration or power-enable circuit. The servo gets pulses for 600 ms, then the signal is held low. No blockage sensing or automatic reversal is performed.

After **10 seconds without activity**, provided movement, lockout and feedback have finished, the MCU clears partial password data, turns off feedback, stops its timer clocks and enters **Idle sleep**. USART reception remains enabled. Sending a line from Serial Monitor wakes it; typing into the monitor without sending does not.

Idle keeps the UART clock available so the first received byte can be buffered. No separate wake button or wake circuit is needed. The Uno and servo remain powered, so this is a functional sleep/wake test, not a battery-life validation.

## Password storage

The digest is `SHA256(16-byte salt || password)` with no terminating NUL. Only the salt and digest are stored, not the plaintext password. The salt uses Timer1, Timer2, uptime, input timing and credential generation; timer values are not cryptographically secure randomness. SHA-256 is fast, so short or predictable passwords can still be guessed from an EEPROM dump.

Two 56-byte EEPROM records at addresses 0 and 64 use sequence numbers, CRC-16 and a final commit marker. An interrupted update preserves the previous valid record. Corrupt, nonempty storage blocks credential commands and needs physical recovery through your programmer. CRC protects against accidental corruption, not intentional modification. No master password or serial factory-reset bypass is included.

## Build and software checks

**Arduino IDE / your method:** follow START HERE above. Upload `LockerSerial/LockerSerial.ino` to the chip while it is installed in your Uno, disconnect power, then transfer that same chip to the breadboard. The runtime uses custom register control and SHA-256 code, without third-party Servo, EEPROM, Serial, tone or crypto libraries.

The following is an **optional developer build/test route, not needed for your Arduino IDE workflow**. For a direct AVR toolchain build:

```
bash build.sh
python3 test_firmware.py
```

Outputs: `build/locker.elf` and `build/locker.hex`. Rebuild after changing endpoints. Timer0 belongs to the buzzer, Timer1 to the servo, and Timer2 to the clock. Do not mix Arduino timing/Serial/Servo APIs into this implementation without resolving ownership of peripherals and interrupts. The bootloader must leave the watchdog disabled.

Software checks cover ATmega328P compilation, SHA-256 against nine Python hashlib vectors, enrollment, password changes, reboot persistence, lockout, serial parsing, corrupt storage, interrupted EEPROM commits, timed movements and isolation of unused PORTB pins.

Physical testing is still needed for UART wake/first-byte reception, servo movement, LED/buzzer behavior and sleep current. This package intentionally tests the existing serial-powered setup; keypad, battery monitoring and blocked-servo recovery are outside this version.
