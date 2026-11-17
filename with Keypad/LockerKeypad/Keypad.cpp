#include "Keypad.h"

namespace {
const uint8_t SCL_PIN = 2;
const uint8_t SDA_PIN = 3;

const uint8_t TCA8418_ADDRESS_FIRST = 0x34;
const uint8_t TCA8418_ADDRESS_LAST = 0x37;

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

const unsigned long I2C_CLOCK_TIMEOUT_US = 2000;
const unsigned int I2C_HALF_PERIOD_US = 5;

void releaseLine(uint8_t pin) {
    digitalWrite(pin, LOW);
    pinMode(pin, INPUT);
}

void pullLow(uint8_t pin) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
}

bool releaseClock() {
    releaseLine(SCL_PIN);
    const unsigned long started = micros();
    while (digitalRead(SCL_PIN) == LOW) {
        if (micros() - started >= I2C_CLOCK_TIMEOUT_US) {
            return false;
        }
    }
    delayMicroseconds(I2C_HALF_PERIOD_US);
    return true;
}

bool startCondition() {
    releaseLine(SDA_PIN);
    if (!releaseClock() || digitalRead(SDA_PIN) == LOW) {
        return false;
    }
    pullLow(SDA_PIN);
    delayMicroseconds(I2C_HALF_PERIOD_US);
    pullLow(SCL_PIN);
    return true;
}

bool repeatedStartCondition() {
    releaseLine(SDA_PIN);
    if (!releaseClock()) {
        return false;
    }
    pullLow(SDA_PIN);
    delayMicroseconds(I2C_HALF_PERIOD_US);
    pullLow(SCL_PIN);
    return true;
}

void stopCondition() {
    pullLow(SDA_PIN);
    delayMicroseconds(I2C_HALF_PERIOD_US);
    if (releaseClock()) {
        releaseLine(SDA_PIN);
        delayMicroseconds(I2C_HALF_PERIOD_US);
    } else {
        releaseLine(SDA_PIN);
    }
}

bool writeByte(uint8_t value) {
    for (uint8_t bit = 0; bit < 8; ++bit) {
        if (value & 0x80) {
            releaseLine(SDA_PIN);
        } else {
            pullLow(SDA_PIN);
        }
        delayMicroseconds(I2C_HALF_PERIOD_US);
        if (!releaseClock()) {
            return false;
        }
        pullLow(SCL_PIN);
        value <<= 1;
    }

    releaseLine(SDA_PIN);
    delayMicroseconds(I2C_HALF_PERIOD_US);
    if (!releaseClock()) {
        return false;
    }
    const bool acknowledged = digitalRead(SDA_PIN) == LOW;
    pullLow(SCL_PIN);
    return acknowledged;
}

bool readByte(uint8_t &value) {
    value = 0;
    releaseLine(SDA_PIN);
    for (uint8_t bit = 0; bit < 8; ++bit) {
        value <<= 1;
        if (!releaseClock()) {
            return false;
        }
        if (digitalRead(SDA_PIN) == HIGH) {
            value |= 1;
        }
        pullLow(SCL_PIN);
        delayMicroseconds(I2C_HALF_PERIOD_US);
    }

    releaseLine(SDA_PIN);
    delayMicroseconds(I2C_HALF_PERIOD_US);
    if (!releaseClock()) {
        return false;
    }
    pullLow(SCL_PIN);
    return true;
}
}

Keypad::Keypad()
    : _address(0),
      _initialized(false),
      _lastRawEvent(0)
{
}

bool Keypad::attach(uint8_t analogPin) {
    (void)analogPin;

    _initialized = false;
    _address = 0;
    _lastRawEvent = 0;
    releaseLine(SCL_PIN);
    releaseLine(SDA_PIN);
    delayMicroseconds(I2C_HALF_PERIOD_US);

    for (uint8_t address = TCA8418_ADDRESS_FIRST;
         address <= TCA8418_ADDRESS_LAST;
         ++address) {
        if (startCondition()) {
            const bool acknowledged = writeByte(address << 1);
            stopCondition();
            if (acknowledged) {
                _address = address;
                break;
            }
        }
    }

    if (_address == 0) {
        return false;
    }

    if (!writeRegister(REG_GPIO_DIR_1, 0x00) ||
        !writeRegister(REG_GPIO_DIR_2, 0x00) ||
        !writeRegister(REG_GPIO_DIR_3, 0x00) ||
        !writeRegister(REG_KP_GPIO_1, 0x0F) ||
        !writeRegister(REG_KP_GPIO_2, 0x07) ||
        !writeRegister(REG_KP_GPIO_3, 0x00) ||
        !writeRegister(REG_DEBOUNCE_DIS_1, 0x00) ||
        !writeRegister(REG_DEBOUNCE_DIS_2, 0x00) ||
        !writeRegister(REG_DEBOUNCE_DIS_3, 0x00) ||
        !writeRegister(REG_INT_STAT, 0x03)) {
        _address = 0;
        return false;
    }

    uint8_t configuration;
    if (!readRegister(REG_CFG, configuration) ||
        !writeRegister(REG_CFG, configuration | 0x29)) {
        _address = 0;
        return false;
    }

    uint8_t count;
    if (!readRegister(REG_KEY_LCK_EC, count)) return false;
    for (uint8_t i = 0; i < 10 && (count & 15); ++i) {
        uint8_t ignored;
        if (!readRegister(REG_KEY_EVENT_A, ignored) ||
            !readRegister(REG_KEY_LCK_EC, count)) return false;
    }
    if ((count & 15) || !writeRegister(REG_INT_STAT, 0x1F)) return false;
    _initialized = true;
    return true;
}

int Keypad::readRaw() {
    return _initialized ? _lastRawEvent : -1;
}

bool Keypad::writeRegister(uint8_t reg, uint8_t value) {
    if (!startCondition()) {
        return false;
    }

    const bool written = writeByte(_address << 1) &&
                         writeByte(reg) &&
                         writeByte(value);
    stopCondition();
    return written;
}

bool Keypad::readRegister(uint8_t reg, uint8_t &value) {
    if (!startCondition()) {
        return false;
    }
    if (!writeByte(_address << 1) || !writeByte(reg) ||
        !repeatedStartCondition() || !writeByte((_address << 1) | 1)) {
        stopCondition();
        return false;
    }

    const bool read = readByte(value);
    stopCondition();
    return read;
}

char Keypad::decode(uint8_t eventCode) const {
    if (eventCode == 0) {
        return '\0';
    }

    const uint8_t keyIndex = eventCode - 1;
    const uint8_t row = keyIndex / 10;
    const uint8_t column = keyIndex % 10;
    if (row >= 4 || column >= 3) {
        return '\0';
    }

    static const char keys[4][3] = {
        {'1', '2', '3'},
        {'4', '5', '6'},
        {'7', '8', '9'},
        {'*', '0', '#'}
    };
    return keys[row][column];
}

char Keypad::getKey() {
    if (!_initialized) return '\0';
    uint8_t status, count;
    if (!readRegister(REG_INT_STAT, status) || (status & 0x08) ||
        !readRegister(REG_KEY_LCK_EC, count) || (count & 15) > 10) {
        _initialized = false;
        return '\0';
    }
    count &= 15;
    while (count--) {
        uint8_t event;
        if (!readRegister(REG_KEY_EVENT_A, event)) {
            _initialized = false;
            return '\0';
        }
        _lastRawEvent = event;
        // Hardware debounce remains enabled. Consume FIFO in order, one press per call.
        // Bit 7 = 1 is PRESS, bit 7 = 0 is RELEASE (datasheet section 8.4.2).
        const char key = decode(event & 0x7F);
        if ((event & 0x80) && key) return key;
    }
    if (!writeRegister(REG_INT_STAT, 0x01)) _initialized = false;
    return '\0';
}
