/*
        Radino32 UART demo for In-Circuit radino modules
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
// Hardware-UART1:  Serial1
// Hardware-UART2:  Serial2
#define dbgSerial Serial1
#define SERIAL_BAUD_RATE 115200

// LED
const int ledPin =  13;
int ledState = LOW;
long previousMillis = 0;

void setup() {
  // setup serials
  dbgSerial.begin(SERIAL_BAUD_RATE);
  if(dbgSerial) dbgSerial.println("Hello!");
  Serial2.begin(SERIAL_BAUD_RATE);
  Serial2.println("Hello!");

  pinMode(ledPin, OUTPUT);

  if(dbgSerial) dbgSerial.println(F("Setup complete"));
}

void loop() {

  char data;
  unsigned long i = 0;
  unsigned long count = dbgSerial.available();

  // echo on dbgSerial
  for (i = 0; i < count; i++) {
    data = dbgSerial.read();
    if(dbgSerial) dbgSerial.write(data);
  }

  // echo on Serial2
  count = Serial2.available();
  for (i = 0; i < count; i++) {
    data = Serial2.read();
    Serial2.write(data);
  }

  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis > 1000) {
    previousMillis = currentMillis;
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }
    digitalWrite(ledPin, ledState);
  }
}
