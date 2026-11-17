#ifndef CUSTOM_KEYPAD_H
#define CUSTOM_KEYPAD_H

#include <Arduino.h>

class Keypad
{
public:
    Keypad();

    // Initializes the TCA8418 on D2 (SCL) and D3 (SDA).
    // The argument is retained for compatibility and is ignored.
    bool attach(uint8_t analogPin = A0);

    // Returns the pressed key ('1'-'9', '*', '0', '#') exactly once per
    // press (debounced, edge-triggered). Returns 0 ('\0') if no new key
    // press is detected (idle, still held, or still bouncing).
    char getKey();

    // Returns the last raw TCA8418 event byte, or -1 before initialization.
    int readRaw();

private:
    uint8_t _address;
    bool _initialized;
    int _lastRawEvent;

    char _stableKey;          // last debounced key state ('\0' = idle)
    char _lastReportedKey;    // last key actually returned by getKey()
    char _candidateKey;       // most recent raw decode, awaiting debounce
    unsigned long _candidateSince;

    static const unsigned long DEBOUNCE_MS;

    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegister(uint8_t reg, uint8_t &value);
    char decode(uint8_t eventCode) const;
};

#endif
