#pragma once
#include <stdint.h>
#ifdef KEYPAD_HOST_TEST
#include "KeypadTestPlatform.h"
#else
#include <avr/io.h>
#include <util/delay.h>
uint32_t now();
static const uint8_t LOW=0, HIGH=1, INPUT=0, OUTPUT=1;
inline void digitalWrite(uint8_t pin,uint8_t value) {
    if(value) PORTD |= (1U<<pin); else PORTD &= ~(1U<<pin);
}
inline void pinMode(uint8_t pin,uint8_t mode) {
    if(mode) DDRD |= (1U<<pin); else DDRD &= ~(1U<<pin);
}
inline uint8_t digitalRead(uint8_t pin) {return (PIND & (1U<<pin)) ? HIGH : LOW;}
inline uint32_t millis() {return now();}
inline uint32_t micros() {return now()*1000UL; /* ms resolution sufficient for 2ms bus timeout */}
inline void delayMicroseconds(unsigned int us) {while(us--) _delay_us(1);}
#endif
