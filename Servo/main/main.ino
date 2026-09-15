#include "CustomServo.h"

Servo myServo;

void setup()
{
    myServo.attach();
}

void loop()
{
    myServo.write(80);
    delay(1500);

    myServo.write(90);
    delay(1500);

    myServo.write(100);
    delay(1500);
}