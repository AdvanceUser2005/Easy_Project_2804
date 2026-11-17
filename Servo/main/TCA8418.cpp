#include "TCA8418.h"
#include <Wire.h>

namespace {
const uint8_t REG_CFG = 0x01;
const uint8_t REG_INT_STAT = 0x02;
const uint8_t REG_KEY_LCK_EC = 0x03;
const uint8_t REG_KEY_EVENT_A = 0x04;
const uint8_t REG_KP_GPIO_1 = 0x1D;
const uint8_t REG_KP_GPIO_2 = 0x1E;
const uint8_t REG_KP_GPIO_3 = 0x1F;
const uint8_t REG_GPIO_DIR_1 = 0x23;
const uint8_t REG_GPIO_DIR_2 = 0x24;
const uint8_t REG_GPIO_DIR_3 = 0x25;
const uint8_t REG_DEBOUNCE_DIS_1 = 0x29;
const uint8_t REG_DEBOUNCE_DIS_2 = 0x2A;
const uint8_t REG_DEBOUNCE_DIS_3 = 0x2B;
const uint8_t CFG_KE_IEN = 0x01;
}

TCA8418::TCA8418()
    : _address(0x34),
      _initialized(false)
{
}

bool TCA8418::begin(uint8_t address) {
    _initialized = false;
    _address = address;
    Wire.begin();

    Wire.beginTransmission(_address);
    if (Wire.endTransmission() != 0) {
        return false;
    }

    if (!writeRegister(REG_GPIO_DIR_1, 0x00) ||
        !writeRegister(REG_GPIO_DIR_2, 0x00) ||
        !writeRegister(REG_GPIO_DIR_3, 0x00) ||
        !writeRegister(REG_INT_STAT, 0x03)) {
        return false;
    }

    uint8_t configuration;
    if (!readRegister(REG_CFG, configuration) ||
        !writeRegister(REG_CFG, configuration | CFG_KE_IEN)) {
        return false;
    }

    _initialized = true;
    return true;
}

bool TCA8418::matrix(uint8_t rows, uint8_t columns) {
    if (!_initialized || rows == 0 || rows > 8 ||
        columns == 0 || columns > 10) {
        return false;
    }

    uint8_t rowMask = 0;
    for (uint8_t row = 0; row < rows; ++row) {
        rowMask = static_cast<uint8_t>((rowMask << 1) | 1);
    }

    uint8_t columnMask = 0;
    for (uint8_t column = 0; column < columns && column < 8; ++column) {
        columnMask = static_cast<uint8_t>((columnMask << 1) | 1);
    }

    uint8_t upperColumnMask = 0;
    if (columns > 8) {
        upperColumnMask = columns == 9 ? 0x01 : 0x03;
    }

    return writeRegister(REG_KP_GPIO_1, rowMask) &&
           writeRegister(REG_KP_GPIO_2, columnMask) &&
           writeRegister(REG_KP_GPIO_3, upperColumnMask);
}

bool TCA8418::enableDebounce() {
    return _initialized &&
           writeRegister(REG_DEBOUNCE_DIS_1, 0x00) &&
           writeRegister(REG_DEBOUNCE_DIS_2, 0x00) &&
           writeRegister(REG_DEBOUNCE_DIS_3, 0x00);
}

bool TCA8418::flush() {
    if (!_initialized) {
        return false;
    }

    uint8_t eventCount;
    if (!available(eventCount)) {
        return false;
    }

    while (eventCount > 0) {
        uint8_t event;
        if (!getEvent(event)) {
            return false;
        }
        --eventCount;
    }

    return writeRegister(REG_INT_STAT, 0x03);
}

bool TCA8418::available(uint8_t &eventCount) {
    if (!_initialized || !readRegister(REG_KEY_LCK_EC, eventCount)) {
        return false;
    }
    eventCount &= 0x0F;
    return true;
}

bool TCA8418::getEvent(uint8_t &event) {
    return _initialized && readRegister(REG_KEY_EVENT_A, event);
}

bool TCA8418::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(_address);
    if (Wire.write(reg) != 1 || Wire.write(value) != 1) {
        Wire.endTransmission();
        return false;
    }
    return Wire.endTransmission() == 0;
}

bool TCA8418::readRegister(uint8_t reg, uint8_t &value) {
    Wire.beginTransmission(_address);
    if (Wire.write(reg) != 1 || Wire.endTransmission(false) != 0) {
        return false;
    }

    if (Wire.requestFrom(_address, static_cast<uint8_t>(1)) != 1 ||
        Wire.available() < 1) {
        while (Wire.available()) {
            Wire.read();
        }
        return false;
    }

    value = static_cast<uint8_t>(Wire.read());
    return true;
}
