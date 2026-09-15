#ifndef CUSTOM_SERVO_H
#define CUSTOM_SERVO_H

#include <Arduino.h>

class Servo
{
public:
    Servo();

    void attach();
    void detach();

    void write(int angle);
    void writeMicroseconds(int pulseWidth);

private:
    static const uint16_t MIN_PULSE_US;
    static const uint16_t MAX_PULSE_US;
    static const uint8_t SERVO_PIN;
};

#endif