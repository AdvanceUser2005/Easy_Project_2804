#include "CustomServo.h"

Servo myServo;

void setup()
{
    myServo.attach();
}

void loop()
{
    myServo.write(180);
    delay(1500);

    myServo.write(90);
    delay(1500);

    myServo.write(0);
    delay(1500);
    
    myServo.write(90);
    delay(1500);

}