#include "CustomServo.h"
#include "Keypad.h"

Servo myServo;
Keypad myKeypad;

int i=0;

void setup() {

    Serial.begin(9600);
    myKeypad.attach(A0);
    myServo.attach();

}

void loop()
{
       char key = myKeypad.getKey();

    if (key != '\0')
    {
        Serial.print("Key pressed: ");
        Serial.println(key);
    }
    switch (key) {
    case '0':
        i=0
    case '1':
        i=20
    case '2':
        i=40
    case '3':
        i=60
    case '4':
        i=80
    case '5':
        i=100
    case '6':
        i=120
    case '7':
        i=140
    case '8':
        i=160
    case '9':
        i=180;
    default:
        myServo.write(i);
    }

}