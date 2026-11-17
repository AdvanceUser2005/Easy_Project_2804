#ifndef CUSTOM_TCA8418_H
#define CUSTOM_TCA8418_H

#include <Arduino.h>

class TCA8418 {
public:
    TCA8418();

    bool begin(uint8_t address = 0x34);
    bool matrix(uint8_t rows, uint8_t columns);
    bool enableDebounce();
    bool flush();
    bool available(uint8_t &eventCount);
    bool getEvent(uint8_t &event);

private:
    uint8_t _address;
    bool _initialized;

    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegister(uint8_t reg, uint8_t &value);
};

#endif
