#include "CustomServo.h"

// Experimentally tested limits
const uint16_t Servo::MIN_PULSE_US = 400;
const uint16_t Servo::MAX_PULSE_US = 2600;

// Timer1 OC1A output
const uint8_t Servo::SERVO_PIN = 9;


Servo::Servo()
{
}


void Servo::attach()
{
    pinMode(SERVO_PIN, OUTPUT);

    // Reset Timer1
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;

    /*
        Arduino Uno clock = 16 MHz

        Prescaler = 8
        Timer frequency = 2 MHz

        One timer count = 0.5 us

        Servo period = 20 ms = 20,000 us

        20,000 / 0.5 = 40,000 counts

        Timer counts from 0 to 39,999
    */

    ICR1 = 39999;

    // Fast PWM Mode 14
    // TOP = ICR1
    TCCR1A |= (1 << WGM11);
    TCCR1B |= (1 << WGM12);
    TCCR1B |= (1 << WGM13);

    // Non-inverting output on OC1A
    // OC1A = Arduino Pin 9
    TCCR1A |= (1 << COM1A1);

    // Prescaler = 8
    TCCR1B |= (1 << CS11);

    // Start servo in center position
    write(90);
}


void Servo::write(int angle)
{
    // Prevent commands outside 0-180 degrees
    if (angle < 0)
    {
        angle = 0;
    }

    if (angle > 180)
    {
        angle = 180;
    }

    /*
        Convert:

        0 degrees   -> 400 us
        90 degrees  -> 1500 us
        180 degrees -> 2600 us
    */

    long pulseWidth =
        MIN_PULSE_US +
        ((long)angle * (MAX_PULSE_US - MIN_PULSE_US)) / 180;

    writeMicroseconds(pulseWidth);
}


void Servo::writeMicroseconds(int pulseWidth)
{
    // Clamp to experimentally tested range
    if (pulseWidth < MIN_PULSE_US)
    {
        pulseWidth = MIN_PULSE_US;
    }

    if (pulseWidth > MAX_PULSE_US)
    {
        pulseWidth = MAX_PULSE_US;
    }

    /*
        Timer resolution = 0.5 us

        Therefore:

        timer counts = pulse width * 2
    */

    OCR1A = pulseWidth * 2;
}


void Servo::detach()
{
    // Disconnect Timer1 output from pin 9
    TCCR1A &= ~(1 << COM1A1);

    // Stop Timer1
    TCCR1B &= ~((1 << CS12) | (1 << CS11) | (1 << CS10));

    digitalWrite(SERVO_PIN, LOW);
}