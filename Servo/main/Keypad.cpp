#include "Keypad.h"

const unsigned long Keypad::DEBOUNCE_MS = 30;

Keypad::Keypad()
    : _pin(A0),
      _stableKey('\0'),
      _lastReportedKey('\0'),
      _candidateKey('\0'),
      _candidateSince(0)
{
}

void Keypad::attach(uint8_t analogPin){// for changing pin
    _pin = analogPin;
}

int Keypad::readRaw(){
    return analogRead(_pin);
}

/*
    Resistor-bridge keypad layout:

        1  2  3
        4  5  6
        7  8  9
        *  0  #

        V(A0) = 5V * R_gnd / (R_gnd + R_5V)
*/
char Keypad::decode(int adcValue) const  {

    if (adcValue > 852) return '\0'; // idle 
    if (adcValue > 660) return '1';
    if (adcValue > 615) return '4';
    if (adcValue > 568) return '7';
    if (adcValue > 527) return '*';
    if (adcValue > 486) return '2';
    if (adcValue > 438) return '3';
    if (adcValue > 381) return '5';
    if (adcValue > 312) return '6';
    if (adcValue > 226) return '8';
    if (adcValue > 156) return '9';
    if (adcValue > 66)  return '0';
    return '#';
}

char Keypad::getKey() {

    int raw = analogRead(_pin);
    char key = decode(raw);

    //check if changed
    if (key != _candidateKey){
        _candidateKey = key;
        _candidateSince = millis();
        return '\0';
    }
    //check if stable over time
    if ((millis() - _candidateSince) < DEBOUNCE_MS){
        return '\0';
    }

    _stableKey = decoded;

    if (_stableKey != '\0' && _stableKey != _lastReportedKey) {
        // New key press (edge-triggered - fires once, not every loop while held)
        _lastReportedKey = _stableKey;
        return _stableKey;
    }

    if (_stableKey == '\0')
    {
        // Released 
        _lastReportedKey = '\0';
    }

    return '\0';
}
