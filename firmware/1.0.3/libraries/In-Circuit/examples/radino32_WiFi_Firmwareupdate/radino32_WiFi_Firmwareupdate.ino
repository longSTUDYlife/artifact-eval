/*
	Radino-WiFi-Firmwareupdate Example for In-Circuit radino WiFi modules
        for more information: www.in-circuit.de or www.radino.cc

	Copyright (c) 2014 In-Circuit GmbH

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
/*  
  This example demonstrates how to build a WiFi-UART-Bridge with
  radino WiFi and setup the ESP8266 for a Firmwareupdate.
    
  This example enables you to directly communicate with the bootloader of the
  ESP8266 WiFi-chip which is integrated on the radino WiFi. It can be used to
  upload new firmware to the ESP8266 WiFi-module.
  
- Internal connections
  
  Arduino   ESP8266
  
  29        CH_PD
  36        RST
  30        GPIO0
  31        GPIO2
  
- Pin setup for ESP8266:

  Pin            normal operation        Firmware-Update-Mode
  
  CH_PD          HIGH                    HIGH
  GPIO0          HIGH                    LOW
  GPIO2          HIGH                    HIGH
  RST            HIGH                    HIGH
  
 */

//ESP82666 pins
#define  ESP_CH_PD      29  //A8
#define  ESP_CH_RST     36  //B15
#define  ESP_GPIO0_A      30  //A13 - HW RevA
#define  ESP_GPIO2_A      31  //A14 - HW RevA
#define  ESP_GPIO0_B      34  //B13 - HW RevB
#define  ESP_GPIO2_B      35  //B14 - HW RevB

// Define Serial port that should be used for communication
// Valid values:
// USB-UART:       Serial
// Hardware-UART:  Serial1
#define progSerial Serial1

//Set to 1 if updating ESP Firmware. If set to zero direct communication with the ESP is possible
#define ESP_UPDATE_MODE 1

void setup()
{
  progSerial.begin(115200);  // Start serial port at baudrate 57600
  Serial3.begin(115200);
  
  pinMode(BOARD_LED_PIN, OUTPUT);
  
  //init ESP8266
  pinMode(ESP_CH_PD, OUTPUT);  // Chip-Enable of ESP8266
  digitalWrite(ESP_CH_PD, LOW);  // HIGH
  pinMode(ESP_CH_RST,OUTPUT);
  digitalWrite(ESP_CH_RST, LOW);
  
  //Detect Hardware Revision
  pinMode(ESP_GPIO0_A, INPUT_PULLDOWN);
  pinMode(ESP_GPIO0_B, INPUT_PULLDOWN);
  pinMode(ESP_GPIO2_A, INPUT_PULLDOWN);
  pinMode(ESP_GPIO2_B, INPUT_PULLDOWN);
  
  if (digitalRead(ESP_GPIO0_B) && digitalRead(ESP_GPIO2_B))
  {
    pinMode(ESP_GPIO0_A, INPUT);
    pinMode(ESP_GPIO2_A, INPUT);
    pinMode(ESP_GPIO0_B, OUTPUT_OPEN_DRAIN);  // GPIO0 of ESP8266
#if ESP_UPDATE_MODE
    digitalWrite(ESP_GPIO0_B, LOW);  // LOW
#else
    digitalWrite(ESP_GPIO0_B, HIGH);  // HIGH
#endif
    pinMode(ESP_GPIO2_B, OUTPUT_OPEN_DRAIN);  // GPIO2 of ESP8266
    digitalWrite(ESP_GPIO2_B, HIGH);  // HIGH
  }
  else
  {
    pinMode(ESP_GPIO0_B, INPUT);
    pinMode(ESP_GPIO2_B, INPUT);
    pinMode(ESP_GPIO0_A, OUTPUT_OPEN_DRAIN);  // GPIO0 of ESP8266
#if ESP_UPDATE_MODE
    digitalWrite(ESP_GPIO0_A, LOW);  // LOW
#else
    digitalWrite(ESP_GPIO0_A, HIGH);  // HIGH
#endif
    pinMode(ESP_GPIO2_A, OUTPUT_OPEN_DRAIN);  // GPIO2 of ESP8266
    digitalWrite(ESP_GPIO2_A, HIGH);  // HIGH
  }
  
  // Start ESP8266
  digitalWrite(ESP_CH_PD, HIGH);
  delay(200);
  digitalWrite(ESP_CH_RST, HIGH);
}

/*
 * "Connect" external UART to ESP-UART
 * This enables direct communication with ESP8266 WiFi-Chip
*/
void loop()
{
  unsigned char ch;
  unsigned char cnt;
  
  // If data available from ESP8266EX, pass through to external UART
  if (Serial3.available())
  {
    ch = Serial3.read();
    if(progSerial) progSerial.write(ch);
  }
  
  // If data available from external UART, pass through to ESP8266EX
  if(progSerial.available())
  {
    ch = progSerial.read();
    Serial3.write(ch);
  }
}

