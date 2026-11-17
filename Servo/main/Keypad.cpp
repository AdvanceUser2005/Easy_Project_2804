#include "Keypad.h"

const unsigned long Keypad::DEBOUNCE_MS = 30;

Keypad::Keypad()
    : _initialized(false),
      _lastRawEvent(0),
      _stableKey('\0'),
      _lastReportedKey('\0'),
      _candidateKey('\0'),
      _candidateSince(0)
{
}

bool Keypad::attach(uint8_t analogPin) {
    (void)analogPin;

    _initialized = false;
    _lastRawEvent = 0;
    if (!_tca.begin() || !_tca.matrix(3, 4) ||
        !_tca.enableDebounce() || !_tca.flush()) {
        return false;
    }
    _stableKey = '\0';
    _lastReportedKey = '\0';
    _candidateKey = '\0';
    _candidateSince = millis();
    _initialized = true;
    return true;
}

int Keypad::readRaw() {
    return _initialized ? _lastRawEvent : -1;
}

char Keypad::decode(uint8_t eventCode) const {
    if (eventCode == 0) {
        return '\0';
    }

    const uint8_t keyIndex = eventCode - 1;
    const uint8_t row = keyIndex / 10;
    const uint8_t column = keyIndex % 10;
    if (row >= 3 || column >= 4) {
        return '\0';
    }

    static const char keys[3][4] = {
        {'*', '7', '4', '1'},
        {'0', '8', '5', '2'},
        {'#', '9', '6', '3'}
    };
    return keys[row][column];
}

char Keypad::getKey() {
    if (!_initialized) {
        return '\0';
    }

    uint8_t eventCount;
    if (!_tca.available(eventCount)) {
        _initialized = false;
        return '\0';
    }

    if (eventCount > 0) {
        uint8_t event;
        if (!_tca.getEvent(event)) {
            _initialized = false;
            return '\0';
        }
        if (event != 0) {
            _lastRawEvent = event;

            const char eventKey = decode(event & 0x7F);
            if (eventKey != '\0') {
                const char candidateKey = (event & 0x80) ? '\0' : eventKey;
                if (candidateKey != _candidateKey) {
                    _candidateKey = candidateKey;
                    _candidateSince = millis();
                }
            }
        }
    }

    if (_candidateKey != _stableKey) {
        if (millis() - _candidateSince < DEBOUNCE_MS) {
            return '\0';
        }
        _stableKey = _candidateKey;
        if (_stableKey == '\0') {
            _lastReportedKey = '\0';
        }
    }

    if (_stableKey != '\0' && _stableKey != _lastReportedKey) {
        _lastReportedKey = _stableKey;
        return _stableKey;
    }
    return '\0';
}
