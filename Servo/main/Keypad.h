#ifndef CUSTOM_KEYPAD_H
#define CUSTOM_KEYPAD_H

#include <Arduino.h>

class Keypad
{
public:
    Keypad();

    // pin defaults to A0 - this design only ever needs one analog input
    void attach(uint8_t analogPin = A0);

    // Returns the pressed key ('1'-'9', '*', '0', '#') exactly once per
    // press (debounced, edge-triggered). Returns 0 ('\0') if no new key
    // press is detected (idle, still held, or still bouncing).
    char getKey();

    // Raw ADC reading straight from the pin, 0-1023. Useful for
    // calibration - print this in a test sketch and press each button
    // to confirm/adjust the thresholds in decode() below.
    int readRaw();

private:
    uint8_t _pin;

    char _stableKey;          // last debounced key state ('\0' = idle)
    char _lastReportedKey;    // last key actually returned by getKey()
    char _candidateKey;       // most recent raw decode, awaiting debounce
    unsigned long _candidateSince;

    static const unsigned long DEBOUNCE_MS;

    char decode(int adcValue) const;
};

#endif
