/*
        radino32 GPIO demo for In-Circuit radino32 modules
        for more information: www.in-circuit.de or www.radino.cc

	Copyright (c) 2015 In-Circuit GmbH

	Permission is hereby granted, free of charge, to any person obtaining a copy of this software
	and associated documentation files (the "Software"), to deal in the Software without restriction,
	including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
	and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
	LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
	WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

// Define Serial port that should be used for communication
// Valid values:
// USB-UART:        Serial
// Hardware-UART1:  Serial1
// Hardware-UART2:  Serial2
#define dbgSerial Serial1
#define SERIAL_BAUD_RATE 115200

const int pwmPin = 11;
unsigned int pwmState = 0;
const int digitalPin = 12;
unsigned int digitalState = LOW;
const int analogPin = A0;
const int dacPin = A2;

long previousMillis = 0;

void setup() {
  dbgSerial.begin(SERIAL_BAUD_RATE);
  if(dbgSerial) dbgSerial.println(F("Hello!"));

  //STM32 has separat registers for mode and value. Always set both.
  //Available Modes:
  // PWM on DAC capable Pin will output voltage set by analogWrite
  // PWM, PWM_OPEN_DRAIN on PWM capable Pin will output PWM value written by analogWrite
  // OUTPUT, OUTPUT_OPEN_DRAIN will output value set by digitalWrite
  // INPUT_ANALOG is used to read voltage on ADC Pins
  // INPUT, INPUT_PULLUP, INPUT_PULLDOWN for digitalRead

  pinMode(pwmPin, PWM);
  pinMode(dacPin, PWM);
  pinMode(digitalPin, OUTPUT);
  pinMode(analogPin, INPUT_ANALOG);

  if(dbgSerial) dbgSerial.println(F("v001"));
  if(dbgSerial) dbgSerial.println(F("Setup complete"));
}

void loop() {

  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis > 100) {
    previousMillis = currentMillis;

    // toggle pwm pin
    pwmState++;
    if (pwmState > 255) {
      pwmState = 0;
    }
    analogWrite(pwmPin, pwmState);
    analogWrite(dacPin, pwmState);

    // toggle digital pin
    if (digitalState == LOW) {
      digitalState = HIGH;
    } else {
      digitalState = LOW;
    }
    digitalWrite(digitalPin, digitalState);

    // read analog Pin, scaled to 3.3 V
    int val = analogRead(analogPin);
    if(dbgSerial) dbgSerial.print("analog value: ");
    if(dbgSerial) dbgSerial.println(val*3.3/1023);
  }

}
