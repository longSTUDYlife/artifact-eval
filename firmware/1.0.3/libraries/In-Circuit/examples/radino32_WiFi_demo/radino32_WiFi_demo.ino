/*
        Radino-WiFi ESP based webserver for In-Circuit radino WiFi modules on Spider
        for more information: www.in-circuit.de or www.radino.cc

	Copyright (c) 2015 In-Circuit GmbH

        v0.3 - 2015.06.04
            Modifications for radino32

        v0.2 - 2015.04.10
	    Migration to In-Circuit ESP Library

        v0.1 - 2014.12.19
	    Basic implementation - functions and comments are subject to change

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
  This example demonstrates how to build a communcation partner for the ESP webserver.

  The ESP will open an access point with ssid "RADINO-WiFi" and password "12345678".
  In default configuration the IP adress of the ESP is 192.168.2.1

  The ESP will start an http server on port 80 and forward write and read requests to the Arduino.

  You can now access a simple website with your webbrowser, when entering your radino's IP adress.
  http://192.168.2.1/

  Further documentation is available on wiki.in-circuit.de

 */

// Use SPI-library for communication with ESP8266
#include <radino32_UART.h>
#include <IC_ESP.h>

IC_ESP esp = IC_ESP();

// Define Serial port that should be used for communication
// Valid values:
// USB-UART:       Serial
// Hardware-UART:  Serial1
#define dbgSerial Serial1
#define DBG_SERIAL_BAUDRATE 115200

/* Registers */
unsigned char reg1013 = 128;  //D13

// Setup Serial connection and init ESP8266-Wifi-Module
void setup()
{
  dbgSerial.begin(DBG_SERIAL_BAUDRATE);  // Init Serial for status messages

  /*
  //Wait for USB serial ready, so all output can be seen on terminal
  // !! If USB Serial is not opened the module will wait here forever
  //Has no effect on hardware serial
  while(!dbgSerial);
  */

  delay(100);  // Wait 100 ms

  if(dbgSerial) dbgSerial.println("Hello");  // Print something to show that Serial connection has established

  esp.setMode(ESP_IC_PROTOCOL_MODE);
  esp.init();  // Init ESP8266EX

  unsigned char cnt = 0;
  // Wait for ESP
  while(!esp.ready())
  {
    if(!(++cnt%10)) esp.init(); // Re-Init ESP
    if(dbgSerial) dbgSerial.println("ESP !ready...");
    delay(500);
  }

  //Set Pins
  pinMode(13, PWM);
  analogWrite(13, reg1013);

  if(dbgSerial) dbgSerial.print("ESP library firmware version: ");
  if(dbgSerial) dbgSerial.println(esp.getFirmwareVersion());

  /*
  //Starting as access point
  if(dbgSerial) dbgSerial.print("Set SSID:");
  if(esp.AP_setSSID("RADINO-WiFi")) if(dbgSerial) dbgSerial.println("ok");
    else if(dbgSerial) dbgSerial.println("er");

  if(dbgSerial) dbgSerial.print("Set PW:");
  if(esp.AP_setPW("12345678")) if(dbgSerial) dbgSerial.println("ok");
    else if(dbgSerial) dbgSerial.println("er");

  if(dbgSerial) dbgSerial.print("Set Mode:");
  if(esp.NET_mode(ESP_WIFI_AP)) if(dbgSerial) dbgSerial.println("ok");
    else if(dbgSerial) dbgSerial.println("er");

  if(dbgSerial) dbgSerial.print("starting Wifi:");
  if(esp.NET_wifiStart()) if(dbgSerial) dbgSerial.println("ok");
    else if(dbgSerial) dbgSerial.println("er");
  */
  /*
  if(dbgSerial) dbgSerial.print("Set SSID:");
  if(esp.ST_setSSID("RADINO-WiFi")) if(dbgSerial) dbgSerial.println("ok");
    else if(dbgSerial) dbgSerial.println("er");

  if(dbgSerial) dbgSerial.print("Set PW:");
  if(esp.ST_setPW("12345678")) if(dbgSerial) dbgSerial.println("ok");
    else if(dbgSerial) dbgSerial.println("er");
  
  if(dbgSerial) dbgSerial.print("Set Mode:");
  if(esp.NET_mode(ESP_WIFI_STATION)) if(dbgSerial) dbgSerial.println("ok");
    else if(dbgSerial) dbgSerial.println("er");

  if(dbgSerial) dbgSerial.print("starting Wifi:");
  if(esp.NET_wifiStart()) if(dbgSerial) dbgSerial.println("ok");
    else if(dbgSerial) dbgSerial.println("er");
  */
  //Wait until network established
  unsigned int wifiStarts = 0;
  unsigned long ip = 0;
  while(wifiStarts < 3) {
    //Wait until network established
    unsigned long startMillis = millis();
    while(!ip) {
      ip = esp.getIPAddress();
      if (millis() - startMillis > 10000) {
        wifiStarts++;
        if(dbgSerial) dbgSerial.print("starting Wifi:");
        if(esp.NET_wifiStart()) if(dbgSerial) dbgSerial.println("ok");
          else if(dbgSerial) dbgSerial.println("er");
        break;
      }
      delay(1000);
    }
    if (ip) break;
  }
  if (!ip) {
    if(dbgSerial) dbgSerial.println("no valid IP received");
  }
  //Print current IP
  if(dbgSerial) dbgSerial.print("IP Address: ");
  if(dbgSerial) dbgSerial.print((ip>>24)&0xFF);
  if(dbgSerial) dbgSerial.print(".");
  if(dbgSerial) dbgSerial.print((ip>>16)&0xFF);
  if(dbgSerial) dbgSerial.print(".");
  if(dbgSerial) dbgSerial.print((ip>>8)&0xFF);
  if(dbgSerial) dbgSerial.print(".");
  if(dbgSerial) dbgSerial.print((ip>>0)&0xFF);
  if(dbgSerial) dbgSerial.println();

  /*
  //Set http port. ESP defaults to port 80
  if(dbgSerial) dbgSerial.println("Setting HTTP-Port...");
  esp.HTTPD_setServerPort(81);
  */

  if(dbgSerial) dbgSerial.println("Starting HTTP-Server...");
  esp.HTTPD_startServer();

  //also uncomment char array declaration in file "radino32_WiFi_demo_website.ino"
  //or use your own char array
  extern char * website;
  //Update Website from string
  if(dbgSerial) dbgSerial.println("Updating Website...");
  esp.HTTPD_updateWebsite(website);
  if(dbgSerial) dbgSerial.println("ok");
  delay(100);

  if(dbgSerial) dbgSerial.println("Setup done");
}

// Wait for requests
void loop()
{
  unsigned short data = 0;

  /*
  // Upload website to ESP using Serial
  static unsigned long uartTimeout = 0;
  static char testBuf[9] = {0, };
  while (dbgSerial.available())
  {
    // Test if upload command started
    for(data=0;data<8;data++) testBuf[data]=testBuf[data+1];
    testBuf[8] = dbgSerial.read();
    if (strncmp(testBuf,"ICUPLOAD$",9)==0)
    {
      esp.disableInterrupt();
      delay(5);
      esp.write('I');esp.write('C');esp.write('U');esp.write('P');esp.write('L');esp.write('O');esp.write('A');esp.write('D');esp.write('$');
      uartTimeout = millis()+100;
      // Bridge data to ESP
      while(millis()<uartTimeout)
      {
        for ( data=dbgSerial.available() ; data || (data=dbgSerial.available()) ; data-- )
        {
          uartTimeout = millis()+100;
          esp.write(dbgSerial.read());
        }
        for ( data=esp.available() ; data || (data=esp.available()) ; data-- )
        {
          uartTimeout = millis()+100;
          if(dbgSerial) dbgSerial.write(esp.read());
        }
      }
      delay(5);
      esp.enableInterrupt();
      delay(5);
    }
  }
  */

  /*
  //Push value every 2.5s to TCP server
  static unsigned long pushTimeout = 0;
  static unsigned char pushCounter = 0;
  if ((millis()-pushTimeout)>2500)
  {
    if(dbgSerial) dbgSerial.print("Push ");
    if (esp.TCP_push(IP(192,168,2,2), 1022, String(pushCounter++)))
      if(dbgSerial) dbgSerial.println("OK");
    else
      if(dbgSerial) dbgSerial.println("ER");
    pushTimeout = millis();
  }
  */

  while(esp.newEventAvailable())
  {
    // new event arrived
    struct event e = esp.getEvent();

    switch (e.type) {
      case EVENT_HTTP_GET_REQUEST:
        switch(e.reg) {

          case 1013:
            esp.sendEventResponse(reg1013);
            break;

          default:
            if(dbgSerial) dbgSerial.print("unknown register: ");
            if(dbgSerial) dbgSerial.println(e.reg);

        }
        break;
      case EVENT_HTTP_SET_REQUEST:
        switch(e.reg) {

          case 1013:
            reg1013 = e.value;
            esp.sendEventResponse(reg1013);
            analogWrite(13, reg1013);
            break;

          default:
            if(dbgSerial) dbgSerial.print("unknown register: ");
            if(dbgSerial) dbgSerial.println(e.reg);

        }
        break;
      default:
        if(dbgSerial) dbgSerial.print("unknown event: ");
        if(dbgSerial) dbgSerial.println(e.type);
    }
  }
}

