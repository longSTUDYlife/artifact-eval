/*
        ESP8266EX Chip on In-Circuit radino WiFi modules
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

#include "Arduino.h"
#include "IC_ESP.h"

#ifdef RADINO32
#include <radino32_UART.h>
#else
// Use SPI-library for communication with SC16IS750
#include <SPI.h>

//
#include <avr/io.h>
#endif

// Debug mode
static unsigned char debugMode;

IC_ESP * IC_ESP::callback_instance = NULL;

// Constructor
IC_ESP::IC_ESP()
{
	_baudrate = 115200; // Standard baudrate = 115200
	_mode = ESP_RAW_MODE;
	_event_mode = EVENT_MODE_IGNORE;
#ifdef RADINO32
  _gpio0 = 0xFF;
  _gpio2 = 0xFF;
  _espUART = radino32_UART();
#else
  _gpio0 = ESP_GPIO0;
  _gpio2 = ESP_GPIO2;
  _espUART = SPI_UART();
#endif
	
	initVariables();
	
	// Init SPI-UART
	_espUART.init();
}

// Init
unsigned char IC_ESP::init(unsigned char mode)
{
	callback_instance = this;	// Remember this
	
	// Init SPI-UART
	_espUART.init();
	_espUART.setINT_None(); // Disable interrupts here
	_espUART.enableSleeping();  // Sleep when idle
	_espUART.setBaudrate(_baudrate); // Set Baudrate of SPI-UART-Module
	
	initVariables();
  
#ifdef RADINO32
  //Detect Hardware Revision
  pinMode(ESP_GPIO0_A, INPUT_PULLDOWN);
  pinMode(ESP_GPIO0_B, INPUT_PULLDOWN);
  pinMode(ESP_GPIO2_A, INPUT_PULLDOWN);
  pinMode(ESP_GPIO2_B, INPUT_PULLDOWN);
  if (digitalRead(ESP_GPIO0_B) && digitalRead(ESP_GPIO2_B))
  {
    pinMode(ESP_GPIO0_A, INPUT);
    pinMode(ESP_GPIO2_A, INPUT);
    _gpio0 = ESP_GPIO0_B;
    _gpio2 = ESP_GPIO2_B;
  } else {
    pinMode(ESP_GPIO0_B, INPUT);
    pinMode(ESP_GPIO2_B, INPUT);
    _gpio0 = ESP_GPIO0_A;
    _gpio2 = ESP_GPIO2_A;
  }
#endif
  if (mode!=ESP_MODE_UNCHANGED)
    setMode(mode); // Set mode for ESP8266EX
  else
    setMode(_mode); // Init with former selected mode
	
	delay(250);
	
	_espUART.flush(); // Clear buffers
	_espUART.attachInterruptCallback(IC_ESP::callback_instance->handleInterrupt); // Connect "handleInterrupt" with SC16 interrupt
	//_espUART.attachInterruptCallback(IC_ESP::callback_instance->debugHandle);
  if (_mode==ESP_IC_PROTOCOL_MODE)
  {
		_espUART.setINT_RXlvl(); // Enable RXlvl interrupt
	}
	
  return 1;
}


// Check if ESP is ready for communication; 0: not ready - 1: ready
unsigned char IC_ESP::ready()
{
	if(_parser_state & IC_PARSER_MASK_BAD_STATE) return 0; // ESP8266EX not ready yet for communication
		else return 1;
}


// Set mode to normal / FirmwareUpdate
// Tested: Normal Mode; Firmware-Update-Mode
unsigned char IC_ESP::setMode(unsigned char mode)
{
	if(debugMode) Serial1.print("Set ESP Mode: ");
  if (_gpio0==0xFF || _gpio2==0xFF)
  {
    if (mode==ESP_RAW_MODE || mode==ESP_IC_PROTOCOL_MODE || mode==ESP_FIMWAREUPDATE_MODE || mode==ESP_MODE_POWERDOWN)
    {
      _mode = mode;
    }
    if(debugMode) Serial1.println("ERROR");
    return 0;
  }
	switch(mode)
	{
    case ESP_MODE_POWERDOWN:
      digitalWrite(CH_PD, LOW);  // LOW
      pinMode(CH_PD, OUTPUT);  // Chip-Enable of ESP8266EX
      digitalWrite(CH_PD, LOW);  // LOW
      pinMode(_gpio0, INPUT);  // GPIO0 of ESP8266EX has Pullup
      _espUART.GPIO_pinMode(_gpio2, SC16_GPIO_INPUT);  // GPIO2 of ESP8266EX has Pullup
      digitalWrite(CH_RST, LOW);  // Pull RESET of WIFI-Chip
      pinMode(CH_RST,OUTPUT);
      digitalWrite(CH_RST, LOW);  // Pull RESET of WIFI-Chip
      break;
      
		case ESP_RAW_MODE:
      if(debugMode) Serial1.println("raw");
			_espUART.setINT_None();
      
		case ESP_IC_PROTOCOL_MODE:

			if(debugMode) Serial1.println("normal");

			/*if (mode == ESP_IC_PROTOCOL_MODE) {
				_espUART.setINT_RXlvl();
			}*/
      
      digitalWrite(CH_PD, HIGH);  // HIGH
			pinMode(CH_PD, OUTPUT);  // Chip-Enable of ESP8266EX
			digitalWrite(CH_PD, HIGH);  // HIGH
			
			pinMode(_gpio0, INPUT);  // GPIO0 of ESP8266EX has Pullup
			//pinMode(_gpio0, OUTPUT);  // GPIO0 of ESP8266EX has Pullup
			//digitalWrite(_gpio0, HIGH);  // HIGH
			
			// GPIO of SC16 is connected to GPIO2 of ESP8266EX

			_espUART.GPIO_pinMode(_gpio2, SC16_GPIO_INPUT);  // GPIO2 of ESP8266EX has Pullup
			//_espUART.GPIO_pinMode(_gpio2, SC16_GPIO_OUTPUT);  // GPIO2 of ESP8266EX
			//_espUART.GPIO_digitalWrite(_gpio2, 1);  // HIGH
			
			// Perform RESET of ESP8266EX
			pinMode(CH_RST,OUTPUT);
			digitalWrite(CH_RST, LOW);  // Pull RESET of WIFI-Chip
			delay(200);
			digitalWrite(CH_RST, HIGH);  // Disable RESET of WIFI-Chip

			break;
			
		case ESP_FIMWAREUPDATE_MODE:

			if(debugMode) Serial1.println("Firmwareupdate");
      
      digitalWrite(CH_PD, HIGH);  // HIGH
			pinMode(CH_PD, OUTPUT);  // Chip-Enable of ESP8266EX
			digitalWrite(CH_PD, HIGH);  // HIGH
			
      digitalWrite(_gpio0, LOW);  // LOW
			pinMode(_gpio0, OUTPUT);  // GPIO0 of ESP8266EX
			digitalWrite(_gpio0, LOW);  // LOW
			
			// GPIO of SC16 is connected to GPIO2 of ESP8266EX
			_espUART.GPIO_pinMode(_gpio2, SC16_GPIO_INPUT);  // GPIO2 of ESP8266EX  has Pullup
			//_espUART.GPIO_pinMode(_gpio2, SC16_GPIO_OUTPUT);  // GPIO2 of ESP8266EX
			//_espUART.GPIO_digitalWrite(_gpio2, 1);  // HIGH
			
			// Perform RESET of ESP8266EX
      digitalWrite(CH_RST, LOW);
			pinMode(CH_RST,OUTPUT);
			digitalWrite(CH_RST, LOW);
			delay(200);
			digitalWrite(CH_RST, HIGH);

			break;
					
		default:
			if(debugMode) Serial1.println("ERROR");
			return 0; // mode-Value not valid
	}
	_mode = mode; // Save new setting
	
	return 1; // Success
}

// Tested: handleInterrupt called on INT6 interrupt successfully
unsigned char IC_ESP::handleInterrupt(unsigned char val)
{
	// Debug: Call Debug Interrupt Handler
	if(debugMode == 1){
      IC_ESP::callback_instance->debugHandle(val);
      return 0;
	}
//	if(callback_instance->_mode == ESP_FIMWAREUPDATE_MODE) callback_instance->_espUART.setINT_DisableAll();
	
	// Call parser
	if(callback_instance->_mode == ESP_IC_PROTOCOL_MODE)
  {
		IC_ESP::callback_instance->parser(val); // Call parser	
	}
	return 0;
}

// Init all variables
void IC_ESP::initVariables()
{
	// Init flags & values
	debugMode = 0;
	_parser_state = IC_PARSER_INACTIVE;
	_parser_cmd_type = 0;
	_parser_cmd_dataHeader = 0;
	_parser_cmd_dataLeft = 0;
	_parser_data_buffer = 0;
	_parser_checksum = 0;

	_parser_timeout = 0;
	
	// Flags for worker
	_worker_handling_active = 0; // True while worker is accessing _worker_* registers
	_worker_data_valid = 0; // True after Parser has written new data to the registers

	// States / Data for worker
	_worker_cmd_type = 0; // command type
	_worker_reg = 0; // register number
	_worker_cmd_dataHeader = 0; // 2 Bytes 'dataInfo' / Error Code
	_worker_cmd_dataType = 0; // DataType
	_worker_cmd_dataLeft = 0; // Data amount
	_worker_data_buffer = 0; // 32 Bit data of non-String-Commands

	initStates();

	releaseHandshake();
}

// Reset Receive buffers; set "_worker_data_valid" to 0
void IC_ESP::clearParserReceiveRegs()
{
	// Clear all information
	_worker_cmd_type = 0; // command type
	_worker_data_valid = 0; // Data not valid beginning here	
}

// Parser
unsigned char IC_ESP::parser(unsigned char chr)
{
	// Variables for sync command
	static unsigned long dollartimestamp = 0;
	static unsigned char dollarCounter = 0;
	
	{

		unsigned long timediff = millis() - _parser_timeout;
		if ((_parser_timeout != 0) && (timediff > IC_PARSER_RESET_TIMEOUT) && (_parser_state != IC_PARSER_IDLE) && (_parser_state != IC_PARSER_INACTIVE)) {
			// do not show message for first call after startup/reset
			Serial1.println(F("PARSER TIMEOUT - RESET!"));
			Serial1.print(_parser_state);
			Serial1.print(" ");
			Serial1.println(timediff);
			_parser_state = IC_PARSER_IDLE;
			initStates();
		}

		_parser_timeout = millis(); // Set parser timeout
#ifndef RADINO32
		chr = _espUART.read();
#endif

		// Check for sync command 4x $$$$ + 100ms timeout
		if(chr == '$' || dollartimestamp!=0)
		{
			dollarCounter++;

			switch(dollarCounter) // got 4x $
			{
				case 1:
				case 2:
				case 3:
					break;
				case 4:
					dollartimestamp = millis();
					break;
				case 5:
					if((millis()-dollartimestamp) >= 0) // Check for ~100ms timeout after 4x $ // ToDo: set to 80-100 !!!
					{
						// Reset parser
						clearParserReceiveRegs();
						_parser_state = IC_PARSER_IDLE;
					}
					dollartimestamp = 0;
					dollarCounter = 0;
					break;
				default:
					dollartimestamp = 0;
					dollarCounter = 0;
					break;
			}
		}

		switch (_parser_state)
		{
			case IC_PARSER_INACTIVE:
			break;
			
			
			case IC_PARSER_DISABLE_PARSER: // Got '#' -> disable parser when you get '####'
			static unsigned char rauteCounter = 1; // When entering this state, you always got 1 '#' already
			if(chr == '#') 
			{
				rauteCounter++;
				break;
			}
			else
			{
				// got different char
				rauteCounter = 1; // Reset counter
				_parser_state = IC_PARSER_IDLE; // Go back to idle state
			}
			if(rauteCounter>=4)
			{
				rauteCounter = 1; // Reset counter
				_parser_state = IC_PARSER_INACTIVE; // deactivate parser
				break;
			}			
			// no 'break;' here: continue with IC_PARSER_IDLE with current char


			case IC_PARSER_IDLE:

			switch (chr)
			{
				case 'I': // new command begins
				case 'i':
				_parser_state = IC_PARSER_ICSTART;
				_parser_checksum = chr; // Begin checksum
				break;
				
				case '#': // First of 4 '#':
				_parser_state = IC_PARSER_DISABLE_PARSER;
				break;
			}
			break;

			case IC_PARSER_ICSTART:
			switch(chr)
			{
				case 'E': // EVENT-Request
				case 'e':
				_parser_cmd_type = chr;
				_parser_cmd_dataHeader = 0;
				parser_addCheck(chr);
				_parser_state = IC_PARSER_PARA_1;
				break;
				
				case 'R': // READ-response
				case 'r':
				_parser_cmd_type = chr;
				_parser_cmd_dataHeader = 0;
				parser_addCheck(chr);
				_parser_state = IC_PARSER_PARA_1;
				break;

				case 'W': // WRITE-response
				case 'w':
				_parser_cmd_type = chr;
				_parser_cmd_dataHeader = 0;
				parser_addCheck(chr);
				_parser_state = IC_PARSER_PARA_1;
				break;
				
				case 'N': // ERROR-response
				case 'n':
				_parser_cmd_type = chr;
				_parser_cmd_dataHeader = 0;
				parser_addCheck(chr);
				_parser_state = IC_PARSER_PARA_1;
				break;


				default:
				_parser_state=IC_PARSER_IDLE;
				break;
			}
			break;

			// Got Request / Response
			// Get register number / event number
			case IC_PARSER_PARA_1:
			_parser_state = IC_PARSER_PARA_2;
			_parser_reg = chr;
			parser_addCheck(chr);
			break;

			// get DataInfo High Byte / Error-Code High Byte
			case IC_PARSER_PARA_2:
			_parser_state = IC_PARSER_PARA_3;
			_parser_cmd_dataHeader = chr;
			parser_addCheck(chr);
			break;
			
			// get DataInfo Low Byte / Error-Code Low Byte
			case IC_PARSER_PARA_3:
			_parser_cmd_dataHeader <<= 8;
			_parser_cmd_dataHeader |= chr;
			parser_addCheck(chr);
			// Decide what to do with following bytes
			switch (_parser_cmd_type)
			{
				case 'E': // EVENT-Request
				case 'e':
				// DataHeader: 2 Bytes DataInfo
				_parser_cmd_dataType = _parser_cmd_dataHeader>>IC_PARSER_SHIFT_DATATYPE; // Get DataType
				_parser_cmd_dataLeft = _parser_cmd_dataHeader & IC_PARSER_MASK_DATALEFT; // Get DataCount

				switch(_parser_reg) // Check event type
				{
					case EVENT_NONE:
					case EVENT_TCP_NEWDATA:
					case EVENT_UDP_NEWDATA:
					case EVENT_HTTP_SET_REQUEST:
					case EVENT_HTTP_GET_REQUEST:
					//case EVENT_HTTP_AUTH_REQUEST:
					//case EVENT_HTTP_USERLEVEL_REQUEST:
					case EVENT_TCP_CLIENT_DATA_RECEIVED:
					//case EVENT_HTTP_CLIENT_DATA_RECEIVED:
					case EVENT_TELNET_GOT_TCP_DATA:
					case EVENT_TELNET_GOT_UDP_DATA:
					case EVENT_TELNET_TCP_CONNECTED:
					case EVENT_TELNET_TCP_DISCONNECTED:

					// executing interrupt; cannot wait for worker to finish
					// TODO more buffers for concurrent requests
					/*
					if (_es.data_available) {
						Serial1.print(F("TODO concurrent events?"));
						Serial1.print(_es.event.type);
						Serial1.print(" ");
						Serial1.println(_parser_reg);
					}
					*/

					// Copy Data
					_worker_cmd_type = _parser_cmd_type; // command type
					_worker_reg = _parser_reg; // event type
					_worker_cmd_dataHeader = _parser_cmd_dataHeader; // 2 Bytes 'dataInfo'
					_worker_cmd_dataType = _parser_cmd_dataType; // DataType
					_worker_cmd_dataLeft = _parser_cmd_dataLeft; // Data amount
					_worker_data_buffer = 0; // no data in buffer
					
					_worker_data_valid = 1;
					
					_es.buffer_index = 0;
					_es.buffer_len = _parser_cmd_dataLeft;
					_es.event.type = (ESP_event_types) _parser_reg;

					_parser_state = IC_PARSER_COPYTOWORKER;

					// debug
					_es.buffer_header[0] = 'I';
					_es.buffer_header[1] = 'E';
					_es.buffer_header[2] = _parser_reg;
					_es.buffer_header[3] = _parser_cmd_dataType;
					_es.buffer_header[4] = _parser_cmd_dataLeft;

					break;
					default:
						Serial1.print("unhandled event: ");
						Serial1.println(_parser_reg);
						_parser_state = IC_PARSER_IDLE;
					break;
				}
				
				break;
				
				case 'R': // READ-response
				case 'r':
				// DataHeader: 2 Bytes DataInfo
				_parser_cmd_dataType = _parser_cmd_dataHeader>>IC_PARSER_SHIFT_DATATYPE; // Get DataType
				_parser_cmd_dataLeft = _parser_cmd_dataHeader & IC_PARSER_MASK_DATALEFT; // Get DataCount
				
				switch(_parser_cmd_dataType)
				{
					case IC_DT_STRING:
	//					_parser_state=IC_PARSER_DATA_STRING_READY;
	//					break;
					case IC_DT_STRINGBURST:
	//					_parser_state=IC_PARSER_STRINGBURST;
	//					break;
					case IC_DT_UINT32:
					case IC_DT_SINT32:
					case IC_DT_FLOAT:
						_parser_state=IC_PARSER_DATA;
						break;
					default:
						_parser_state=IC_PARSER_DATA;
					break;
				}

				//_parser_state=IC_PARSER_DATA;
				break;

				case 'W': // WRITE-response
				case 'w':
				// DataHeader: 2 Bytes Write-Response Additional Info
				_parser_state=IC_PARSER_CHECKANDCOPY; // Next byte: Checksum
				break;
				
				case 'N': // ERROR-response
				case 'n':
				// DataHeader: 2 Bytes Error-Code
				_parser_state=IC_PARSER_CHECKANDCOPY; // Next byte: Checksum
				break;
				
				default:
				_parser_state=IC_PARSER_IDLE;
				break;
			}	
			break;
					
			
//			case IC_PARSER_STRINGBURST:	// ToDo !!!
			case IC_PARSER_DATA:
			// Read next of 4 DataBytes			
			_parser_data_buffer<<=8; // Add next byte to buffer
			_parser_data_buffer |= chr;
			_parser_cmd_dataLeft--;
						
			parser_addCheck(chr);
			if (0==_parser_cmd_dataLeft)
				_parser_state = IC_PARSER_CHECKANDCOPY;
			break;
			
			//case IC_PARSER_IE_HTTP_DATA_READY: // ToDo !!!
			case IC_PARSER_DATA_STRING_READY: // ToDo !!! // Only landing here if no one picks up data
			case IC_PARSER_COPYTOWORKER:

			parser_addCheck(chr);

			_es.buffer[_es.buffer_index] = chr;
			_es.buffer_index++;

			if(_worker_cmd_dataLeft) _worker_cmd_dataLeft--;
			
			if (0==_worker_cmd_dataLeft) {
				_parser_state = IC_PARSER_CHECKANDLEAVE;
			}
			break;
		
			//case IC_PARSER_DATA_STRING_READY: // ToDo !!!
			//_parser_cmd_dataLeft--;
			//
			//if (0==_parser_cmd_dataLeft)
				//_parser_state = IC_PARSER_CHECKANDLEAVE;
			//break;
			
			
			// Check checksum and finish
			case IC_PARSER_CHECKANDLEAVE: // ToDo !!!
			if(parser_checkChecksum(chr)) // Check Checksum
			{
				_es.data_available = 1;
				_es.data_handled = 0;

			}
			_parser_state = IC_PARSER_IDLE;

			break;
			
			
			// Check checksum and copy information to worker-registers
			case IC_PARSER_CHECKANDCOPY: // ToDo !!!
			if(parser_checkChecksum(chr)) // Check Checksum
			{
				_cs.data_available = 1;
				_cs.data_handled = 0;
				_cs.buffer_small = _parser_data_buffer; // 32 Bit data of non-String-Commands
				// TODO copy rest of data to _es structure
				{
					
					// Copy values
					_worker_cmd_type = _parser_cmd_type; // command type
					_worker_reg = _parser_reg; // register number
					_worker_cmd_dataHeader = _parser_cmd_dataHeader; // 2 Bytes 'dataInfo' / Error Code
					_worker_cmd_dataType = _parser_cmd_dataType; // DataType
					_worker_cmd_dataLeft = _parser_cmd_dataLeft; // Data amount
					
					_worker_data_valid = 1;
				}
			}
			
			
			// ToDo: copy info to target registers
			_parser_state = IC_PARSER_IDLE;
			break;

			default:
				_parser_state = IC_PARSER_IDLE;
			break;
		}
		
		//Serial1.write(_parser_state+'0');
	}
	
	return 1;
}

// Add next char to checksum
void IC_ESP::parser_addCheck(unsigned char newChar)
{
	_parser_checksum = 0x00;	// Dummy Checksum
}

// Check Checksum
// returns True if "checksum" equals last calculated checksum
unsigned char IC_ESP::parser_checkChecksum(unsigned char checksum) // ToDo !!!
{
	// TODO do real check once implemented
	return 1;
	//return (_parser_checksum == checksum);
	if((checksum!=0) && !(checksum&0x80)) return 0;
	return 1;
}

unsigned char IC_ESP::writeRegister(unsigned char reg, String data, unsigned char writeMode, unsigned short timeoutms)
{
	if(!ready() || (_parser_state!=IC_PARSER_IDLE)) return 0; // ESP not ready for communication / busy
	
	clearParserReceiveRegs(); // Reset Receive buffers

	unsigned long timeoutTarget = 0;

	unsigned short count = data.length();	
	
	_espUART.write('I');
	_espUART.write('W');
	_espUART.write(reg); // register number
	_espUART.write(count>>8 & 0xFF); // DataInfo High Byte
	_espUART.write(count & 0xFF); // DataInfo Low Byte
	for (unsigned short i = 0; i<data.length(); i++) // Data
	{
		_espUART.write(data.charAt(i));
	}
	_espUART.write(0x00); // Checksum
	
	if(writeMode == WRITE_BLOCKING)
	{
		timeoutTarget = millis() + timeoutms; // timeout

		while(!_worker_data_valid || (_worker_cmd_type == 0)) // Wait for new data to arrive
		{
			if(millis() > timeoutTarget)
			{
				return 0;	// Timeout-Error -> Return!
			}
		}
		// Data now available!
		//_worker_handling_active = 1; // Start handling
		if((_worker_cmd_type != 'W') && (_worker_cmd_type != 'w'))
		  return 0; // Error-Response / collision
	}
	
	return 1;
}

unsigned char IC_ESP::writeRegister(unsigned char reg, unsigned char *data, unsigned char writeMode, unsigned short timeoutms)
{
	if(!ready() || (_parser_state!=IC_PARSER_IDLE)) return 0; // ESP not ready for communication / busy
	
	clearParserReceiveRegs(); // Reset Receive buffers

	unsigned long timeoutTarget = 0;

	short count = strlen((const char*)data)+1;

	_espUART.write('I');
	_espUART.write('W');
	_espUART.write(reg); // register number
	_espUART.write(count>>8 & 0xFF); // DataInfo High Byte
	_espUART.write(count & 0xFF); // DataInfo Low Byte
	for (unsigned short i = 0; i<=count; i++) // Data
	{
		_espUART.write(data[i]);
	}
	_espUART.write(0x00); // Checksum
	
	if(writeMode == WRITE_BLOCKING)
	{
		timeoutTarget = millis() + timeoutms; // timeout

		while(!_worker_data_valid || (_worker_cmd_type == 0)) // Wait for new data to arrive
		{
			if(millis() > timeoutTarget)
			{
				return 0;	// Timeout-Error -> Return!
			}
		}
		// Data now available!
		//_worker_handling_active = 1; // Start handling
		if((_worker_cmd_type != 'W') && (_worker_cmd_type != 'w'))
		return 0; // Error-Response / collision
	}
	
	return 1;
}

unsigned char IC_ESP::writeRegister(unsigned char reg, unsigned long data, unsigned char writeMode, unsigned short timeoutms)
{
	if(!ready() || (_parser_state!=IC_PARSER_IDLE)) return 0; // ESP not ready for communication / busy
	
	clearParserReceiveRegs(); // Reset Receive buffers
	
	//unsigned long commandHeader = 0; // Command Header to send
	
	unsigned long timeoutTarget = 0;

	_espUART.write('I');
	_espUART.write('W');
	_espUART.write(reg); // register number
	_espUART.write(0x20); // DataInfo High Byte -> DataType: uInt32
	_espUART.write(0x04); // DataInfo Low Byte -> 4 Bytes of data
	_espUART.write(data>>24 & 0xFF); // Send data bytes
	_espUART.write(data>>16 & 0xFF);
	_espUART.write(data>>8 & 0xFF);
	_espUART.write(data & 0xFF);
	_espUART.write(0x00); // Checksum
	
	
	if(writeMode == WRITE_BLOCKING)
	{
		timeoutTarget = millis() + timeoutms; // 500ms timeout
		while(!_worker_data_valid || (_worker_cmd_type == 0)) // Wait for new data to arrive
		{
			if(millis() > timeoutTarget)
			{
				//releaseHandshake();
				return 0;	// Timeout-Error -> Return!
			}
		}
		// Data now available!
		if((_worker_cmd_type != 'W') && (_worker_cmd_type != 'w'))
		return 0; // Error-Response / collision
	}
	
	return 1;
}


// returns status -> "Data now ready" or "got ERROR"; data itself must be read with "readNewData()"
// 0: Error
// 1: dataNowReady
unsigned char IC_ESP::readRegister(unsigned char reg, unsigned char readMode, unsigned short timeoutms)
{
	if(!ready() || (_parser_state!=IC_PARSER_IDLE)) return 0; // ESP not ready for communication / busy
	
	clearParserReceiveRegs(); // Reset Receive buffers
	
	unsigned long timeoutTarget = 0;
	
	_espUART.write('I');
	_espUART.write('R');
	_espUART.write(reg);
	_espUART.write(0x00); // Checksum

	if(readMode == READ_BLOCKING)
	{
		timeoutTarget = millis() + timeoutms; // 500ms timeout

		while(!_worker_data_valid || (_worker_cmd_type == 0)) // Wait for new data to arrive
		{
			if(millis() > timeoutTarget)
			{
				return 0;	// Timeout-Error -> Return!
			}
		}
		// Data now available!
		if((_worker_cmd_type != 'R') && (_worker_cmd_type != 'r')) {

			return 0; // Error-Response / collision
		}
	}	
	
	return 1;
}

// Set SSID for AP-Mode -  max. 32 chars
unsigned char IC_ESP::AP_setSSID(String ssid)
{
	return writeRegister(WIFI_SSID_AP, ssid, WRITE_BLOCKING, 1000); // 1 s timeout
}

// Set PW for AP-mode - max. 64 chars
unsigned char IC_ESP::AP_setPW(String pw)
{
	return writeRegister(WIFI_PASSWD_AP, pw, WRITE_BLOCKING, 1000);
}

unsigned char IC_ESP::AP_setChannel(unsigned long channel)
{
	return writeRegister(WIFI_CHANNEL_AP, channel, WRITE_BLOCKING, 1000);
}

// Set SSID for JOIN-Mode / station -  max. 32 chars
unsigned char IC_ESP::ST_setSSID(String ssid)
{
	return writeRegister(WIFI_SSID_JOIN, ssid, WRITE_BLOCKING, 1000); // 1 s timeout
}

// Set PW for JOIN-mode / station - max. 64 chars
unsigned char IC_ESP::ST_setPW(String pw)
{
	return writeRegister(WIFI_PASSWD_JOIN, pw, WRITE_BLOCKING, 1000);
}

// select network mode Station/AP
unsigned char IC_ESP::NET_mode(ESP_wifi_modes mode)
{
	if (mode>3 || mode<1) return 0;
	return writeRegister(WIFI_MODE, (unsigned long)mode);
}

unsigned char IC_ESP::NET_wifiStart()
{
	return writeRegister(SYSTEM_STROBE, (unsigned long)0x01, WRITE_BLOCKING, 2000);
}

unsigned char IC_ESP::NET_wifiReset() {
	unsigned char ret = writeRegister(SYSTEM_STROBE, (unsigned long) (1<<8), WRITE_BLOCKING, 2000);
	if (ret) {
		// reset parser, wait for ESP
		_parser_timeout = 0;
		_parser_state = IC_PARSER_INACTIVE;
	}
	return ret;
}

unsigned long IC_ESP::getIPAddress() {
	if (readRegister(IP_ADDR_READ, READ_BLOCKING)) {
		unsigned long buf = _cs.buffer_small;
		initCommandState();
		return buf;
	} else {
		return 0;
	}
}
/* not yet implemented on ESP
unsigned long IC_ESP::ST_getIPAddress() {
	if (readRegister(IP_ADDR_JOIN, READ_BLOCKING)) {
		unsigned long buf = _cs.buffer_small;
		initCommandState();
		return buf;
	} else {
		return 0;
	}
}*/

void IC_ESP::getIPAddress(unsigned char *buf, unsigned long ip) {
	buf[0] = (ip >> 24) & 0xff;
	buf[1] = (ip >> 16) & 0xff;
	buf[2] = (ip >> 8) & 0xff;
	buf[3] = ip & 0xff;
}

unsigned long IC_ESP::AP_getIPAddress() {
	if (readRegister(IP_ADDR_AP, READ_BLOCKING)) {
		unsigned long buf = _cs.buffer_small;
		initCommandState();
		return buf;
	} else {
		return 0;
	}
}

unsigned char IC_ESP::AP_setIPAddress(unsigned long ip) {
	return writeRegister(IP_ADDR_AP, ip, WRITE_BLOCKING, 1000);
}

unsigned char IC_ESP::AP_forceIPAddress(unsigned long ip) {
	return writeRegister(FORCE_APIP, ip, WRITE_BLOCKING, 1000);
}

unsigned long IC_ESP::TCP_status(unsigned long *status) {
	if (readRegister(TCP_STATUS, READ_BLOCKING)) {
		unsigned long buf = _cs.buffer_small;
		initCommandState();
		*status = buf;
		return 1;
	} else {
		return 0;
	}
}

TCP_SOCKET_STATUS IC_ESP::TCP_status(unsigned long socket) {
	unsigned long s;
	if (TCP_status(&s)) {
		if (socket < 8) {
			// socket state is two bits
			unsigned long t = 0x03;
			// socket states are at the MSBs 31 downto 16
			// 8 sockets
			t = t << 30;
			// socket 0 at 31:30
			t = t >> socket;
			// select socket state
			t = t & s;
			t = t << socket;
			t = t >> 30;
			switch(t) {
			case 0:
				return CLOSED;
				break;
			case 1:
				return CONNECTING;
				break;
			case 2:
				return LISTENING;
				break;
			case 3:
				return OPEN;
				break;
			default:
				return ERROR;
				break;
			}
		} else {
			return ERROR;
		}
	}
	return ERROR;
}

unsigned char IC_ESP::TCP_serverRunning(unsigned char *running) {
	if (readRegister(TCP_STATUS, READ_BLOCKING)) {
		unsigned long buf = _cs.buffer_small;
		initCommandState();
		*running = (unsigned char) buf & 0x01;
		return 1;
	} else {
		return 0;
	}
}

unsigned char IC_ESP::TCP_serverConnected(unsigned char *connected) {
	if (readRegister(TCP_STATUS, READ_BLOCKING)) {
		unsigned long buf = _cs.buffer_small;
		initCommandState();
		*connected = (unsigned char) buf & 0x02;
		return 1;
	} else {
		return 0;
	}
}

unsigned char IC_ESP::TCP_openClientSocket() {
	return writeRegister(TCP_STROBE, (unsigned long)0x04, WRITE_BLOCKING, 1000);
}

unsigned char IC_ESP::TCP_setClientTargetIP(unsigned long ip) {
	return writeRegister(TCP_CLIENT_TARGETIP, (unsigned long)ip, WRITE_BLOCKING, 1000);
}

unsigned long IC_ESP::TCP_clientTargetIP() {
	if (readRegister(TCP_CLIENT_TARGETIP, READ_BLOCKING)) {
		unsigned long buf = _cs.buffer_small;
		initCommandState();
		return buf;
	} else {
		return 0;
	}
}

unsigned char IC_ESP::TCP_setClientTargetPort(unsigned short port) {
	return writeRegister(TCP_CLIENT_TARGETPORT, (unsigned long)0x0000ffff&port, WRITE_BLOCKING, 1000);
}

unsigned short IC_ESP::TCP_clientTargetPort() {
	if (readRegister(TCP_CLIENT_TARGETPORT, READ_BLOCKING)) {
		unsigned short buf = (unsigned short) 0x0000ffff&_cs.buffer_small;
		initCommandState();
		return buf;
	} else {
		return 0;
	}
}

unsigned char IC_ESP::TCP_setClientSocket(unsigned long clientSocket) {
	return writeRegister(TCP_CLIENT_SEND_TARGETSOCKET, clientSocket, WRITE_BLOCKING, 1000);
}

unsigned char IC_ESP::TCP_clientSocket(unsigned long *clientSocket) {
	if (readRegister(TCP_CLIENT_SEND_TARGETSOCKET, READ_BLOCKING)) {
		unsigned long buf = _cs.buffer_small;
		initCommandState();
		*clientSocket = buf;
		return 1;
	} else {
		return 0;
	}
}

unsigned char IC_ESP::TCP_clientSend(unsigned char *data) {
	return writeRegister(TCP_CLIENT_SEND, data, WRITE_BLOCKING, 1000);
}

unsigned char IC_ESP::TCP_clientSend(const char *data) {
	return writeRegister(TCP_CLIENT_SEND, (unsigned char*) data, WRITE_BLOCKING, 1000);
}

unsigned char IC_ESP::TCP_closeSocket() {
	// TODO: atm expects client socket; later version will use socket set
	// by register TCP_CLIENT_SEND_TARGETSOCKET
	unsigned long socket;
	if (TCP_clientSocket(&socket)) {
		return writeRegister(TCP_CLOSE_SOCKET, socket, WRITE_BLOCKING, 1000);
	} else {
		return 0;
	}
}

// Send "data" to target IP & port via TCP and don't wait for response
unsigned char IC_ESP::TCP_push(unsigned long ip, unsigned short port, String data)
{
	if(!ready() || (_parser_state!=IC_PARSER_IDLE)) return false; // ESP not ready for communication / busy
	short count = data.length() + 6; // String + 6bytes IP/port

	_espUART.write('I');
	_espUART.write('W');
	_espUART.write(TCP_PUSH); // register number: TCP_PUSH
	_espUART.write(count>>8 & 0xFF); // DataInfo High Byte
	_espUART.write(count & 0xFF); // DataInfo Low Byte

	_espUART.write(ip>>24 & 0xFF); // IP
	_espUART.write(ip>>16 & 0xFF);
	_espUART.write(ip>>8  & 0xFF);
	_espUART.write(ip>>0  & 0xFF);

	_espUART.write(port>>8 & 0xFF); // Port
	_espUART.write(port>>0 & 0xFF);

	for (unsigned char i = 0; i<data.length(); i++) // Data
	{
		_espUART.write(data.charAt(i));
	}
	_espUART.write(0x00); // Checksum
}

unsigned long IC_ESP::Telnet_status(unsigned long *status) {
	if (readRegister(TELNET_STATUS, READ_BLOCKING)) {
		unsigned long buf = _cs.buffer_small;
		initCommandState();
		*status = buf;
		return 1;
	} else {
		return 0;
	}
}

unsigned char IC_ESP::Telnet_start() {
	return writeRegister(TELNET_STROBE, 0x01);
}

unsigned char IC_ESP::Telnet_stop() {
	return writeRegister(TELNET_STROBE, 0x02);
}

unsigned long IC_ESP::Telnet_TCPPort() {
	if (readRegister(TELNET_PORT_TCP, READ_BLOCKING)) {
		unsigned long buf = _cs.buffer_small;
		initCommandState();
		return buf;
	} else {
		return 0;
	}
}

unsigned char IC_ESP::Telnet_setTCPPort(unsigned long port) {
	return writeRegister(TELNET_PORT_TCP, port);
}

unsigned long IC_ESP::Telnet_UDPPort() {
	if (readRegister(TELNET_PORT_UDP, READ_BLOCKING)) {
		unsigned long buf = _cs.buffer_small;
		initCommandState();
		return buf;
	} else {
		return 0;
	}
}

unsigned char IC_ESP::Telnet_setUDPPort(unsigned long port) {
	return writeRegister(TELNET_PORT_UDP, port);
}

unsigned long IC_ESP::Telnet_UDPBroadcastPort() {
	if (readRegister(TELNET_PORT_UDP_BROADCAST, READ_BLOCKING)) {
		unsigned long buf = _cs.buffer_small;
		initCommandState();
		return buf;
	} else {
		return 0;
	}
}

unsigned char IC_ESP::Telnet_setUDPBroadcastPort(unsigned long port) {
	return writeRegister(TELNET_PORT_UDP_BROADCAST, port);
}

unsigned char IC_ESP::Telnet_targetSocket(unsigned long *socket) {
	if (readRegister(TELNET_TARGET_SOCKET, READ_BLOCKING)) {
		unsigned long buf = _cs.buffer_small;
		initCommandState();
		*socket = buf;
		return 1;
	} else {
		return 0;
	}
}

unsigned char  IC_ESP::Telnet_socketSend(String data) {
  return writeRegister(TELNET_SOCKET_SEND, data, WRITE_BLOCKING, 1000);
}

void IC_ESP::Telnet_UDPSend(String data) {
	if(!ready() || (_parser_state!=IC_PARSER_IDLE)) return; // ESP not ready for communication / busy
	short count = data.length();

	_espUART.write('I');
	_espUART.write('W');
	_espUART.write(TELNET_UDP_SEND); // register number: TCP_PUSH
	_espUART.write(count>>8 & 0xFF); // DataInfo High Byte
	_espUART.write(count & 0xFF); // DataInfo Low Byte

	for (unsigned char i = 0; i<data.length(); i++) // Data
	{
		_espUART.write(data.charAt(i));
	}
	_espUART.write(0x00); // Checksum
}

void IC_ESP::Telnet_broadcastSend(String data) {
	if(!ready() || (_parser_state!=IC_PARSER_IDLE)) return; // ESP not ready for communication / busy
	short count = data.length();

	_espUART.write('I');
	_espUART.write('W');
	_espUART.write(TELNET_BROADCAST_SEND); // register number: TCP_PUSH
	_espUART.write(count>>8 & 0xFF); // DataInfo High Byte
	_espUART.write(count & 0xFF); // DataInfo Low Byte

	for (unsigned char i = 0; i<data.length(); i++) // Data
	{
		_espUART.write(data.charAt(i));
	}
	_espUART.write(0x00); // Checksum
}

unsigned char IC_ESP::Telnet_setTargetSocket(unsigned long socket) {
	return writeRegister(TELNET_TARGET_SOCKET, socket, WRITE_BLOCKING, 1000);
}

unsigned char IC_ESP::HTTPD_getStatus(unsigned long *data) {
	if (readRegister(HTTP_STATUS, READ_BLOCKING)) {
		unsigned long buf = _cs.buffer_small;
		*data = buf;
		return 1;
	} else {
		return 0;
	}
}

// Add additional HTTP server port
unsigned char IC_ESP::HTTPD_setServerPort(unsigned short port)
{
	return writeRegister(HTTP_SERVERPORT, (unsigned long) port);
}

unsigned char IC_ESP::HTTPD_startServer()
{
	// set first bit to start server
	return writeRegister(HTTP_STROBE, (unsigned long) 1);
}

// Upload new website to internal webserver
unsigned char IC_ESP::HTTPD_updateWebsite(const char *data)
{
	// Disable Interrupts
	_espUART.setINT_None();
	_espUART.print("ICUPLOAD$");
	_espUART.print(data);
	_espUART.flush();
	// Enable Interrupts again
	_espUART.setINT_RXlvl();
	delay(50);

	return 1;
}

unsigned char IC_ESP::HTTPD_updateWebsiteProgMem(char *data)
{
	// Disable Interrupts
	_espUART.setINT_None();
	_espUART.print("ICUPLOAD$");
	char buf;
	unsigned i = 0;
	while(1) {
		buf = (char) pgm_read_byte_near(data+i);
		if (buf) {
			_espUART.write(buf);
			i++;
		} else {
			break;
		}
	}
	_espUART.flush();
	// Enable Interrupts again
	_espUART.setINT_RXlvl();
	delay(50);
	return 1;
}

/*// Upload new website to internal webserver
unsigned char IC_ESP::HTTPD_updateWebsite(String *data)
{
	// Disable Interrupts
	_espUART.setINT_None();
	_espUART.print("ICUPLOAD$");
	_espUART.flush();
	// Enable Interrupts again
	_espUART.setINT_RXlvl();
	delay(50);
	return 1;
}*/

unsigned char IC_ESP::StoreSettings_All() {
	return writeRegister(STROBE_STORE, (unsigned long) 0x01, WRITE_BLOCKING, 1000);
}

unsigned char IC_ESP::StoreSettings_APIP() {
	return writeRegister(STROBE_STORE, (unsigned long) 0x02, WRITE_BLOCKING, 1000);
}

unsigned char IC_ESP::StoreSettings_FactoryReset() {
	return writeRegister(STROBE_STORE, (unsigned long) (0x01UL << 24), WRITE_BLOCKING, 1000);
}

unsigned char IC_ESP::userData(unsigned long *data) {
	if (readRegister(USER_DATA, READ_BLOCKING)) {
		unsigned long buf = _cs.buffer_small;
		initCommandState();
		*data = buf;
		return 1;
	} else {
		return 0;
	}
}

unsigned char IC_ESP::setUserData(unsigned long data) {
	return writeRegister(USER_DATA, data, WRITE_BLOCKING, 1000);
}

unsigned long IC_ESP::getFirmwareVersion() {
	if (readRegister(FIRMWARE_VERSION, READ_BLOCKING)) {
		unsigned long buf = _cs.buffer_small;
		initCommandState();
		return buf;
	} else {
		return 0;
	}
}

unsigned long IC_ESP::getVersion() {
	return IC_ESP_VERSION;
}

unsigned char IC_ESP::goToDeepsleep() {
  return writeRegister(SYSTEM_STROBE, (unsigned long)(1<<11), WRITE_BLOCKING, 2000);
}

unsigned char IC_ESP::wakeupFromDeepsleep() {
  // Falling Edge on Reset pin will wakeup ESP8266 from deepsleep
	digitalWrite(CH_RST, HIGH);  // Disable RESET of WIFI-Chip
  pinMode(CH_RST,OUTPUT);
  digitalWrite(CH_RST, LOW);  // Pull RESET of WIFI-Chip
	digitalWrite(CH_RST, HIGH);  // Disable RESET of WIFI-Chip
  return 0;
}

// Set Baudrate of SC16IS750
// Tested
void IC_ESP::SPI_setBaudrate(unsigned long baudrate)
{
	_baudrate = baudrate;
	_espUART.setBaudrate(_baudrate);
}

// Read Handshake-Line and return status (1 == HIGH == used; 0 == LOW == FREE)
unsigned char IC_ESP::handshakeAlreadyActive()
{
	return digitalRead(HARDWARE_HANDSHAKE);
}

// Set Handshake-line HIGH
void IC_ESP::setHandshake()
{
	pinMode(HARDWARE_HANDSHAKE, OUTPUT);
	digitalWrite(HARDWARE_HANDSHAKE, LOW);
}

// Let go of Handshake-line
void IC_ESP::releaseHandshake()
{
	pinMode(HARDWARE_HANDSHAKE, INPUT);	
}


// Function for debugging only
unsigned char IC_ESP::debugInit(unsigned char mode)
{
	Serial1.begin(57600);
	debugMode = mode;
	
	if(debugMode) Serial1.print("DebugMode:");
	if(debugMode) Serial1.println(debugMode);
	
	return 1;
}

// Loop data to Serial1
// Tested: successfully called from handleInterrupt() && loops data through
unsigned char IC_ESP::debugHandle(char)
{
	unsigned char cnt = 0;
	unsigned char chr = 0;
	if(debugMode) // Debugging only
	{		
		for (cnt=_espUART.available(); cnt>0; cnt--)
		{
			chr = _espUART.read();
			Serial1.write(chr);
		}
		return 0; // Break here
	}
	//if(debugMode) Serial1.println("p");
	return 0;
}

// Directly write to ESP8266EX UART
// only used in Firmwareupdatemode & for debugging
// Tested
void IC_ESP::write(unsigned char character)
{
	//if(debugMode || (_mode == ESP_FIMWAREUPDATE_MODE)) _espUART.write(character);
	_espUART.write(character);
}

// Directly read from ESP8266EX UART 
// only used in Firmwareupdatemode & for debugging
// Tested
unsigned char IC_ESP::read()
{
	//if(debugMode || (_mode == ESP_FIMWAREUPDATE_MODE)) return _espUART.read();
	//return 0;
	return _espUART.read();
}

// Data available on ESP8266EX UART?
// only used in Firmwareupdatemode & for debugging
// Tested
unsigned char IC_ESP::available()
{
	return _espUART.available();
}

void IC_ESP::initStates() {
	initEventState();
	initCommandState();
}

void IC_ESP::initEventState() {
	_es.data_available = 0;
	_es.data_handled = 1;
	_es.data_read = 0;
	_es.event.type = EVENT_NONE;
}

void IC_ESP::initCommandState() {
	_cs.data_available = 0;
	_cs.data_handled = 0;
}

unsigned IC_ESP::newEventAvailable() {

	if (_es.data_read == 1) {
		// we were called before, but user did not handle data
		// so we ignore it
		initEventState();
		return 0;
	}

	if (_es.data_available) {

		_es.data_read = 1;

		// parse message

		unsigned long x = 0, i = 0, j = 0;
		unsigned char len;

		switch(_parser_reg) {
		
		case EVENT_HTTP_GET_REQUEST:

			// start after socket
			i = 1;
			for (; i < _es.buffer_len;) {
				x = x*10 + (_es.buffer[i] - '0');
				i++;
				if (_es.buffer[i] == 0xFF) {
					break;
				}
			}
			// malformed request
			if (i >= _es.buffer_len) {
				initEventState();
				return 0;
			}
			_es.event.type = (ESP_event_types) _parser_reg;
			_es.event.socket = _es.buffer[0];
			_es.event.reg = x;

			// read ip
			x = 0;
			i++;
			for (j = 0; j < 4; j++) {
				x = x << 8;
				x += _es.buffer[i+j];
			}
			_es.event.ip = x;
			break;

		case EVENT_HTTP_SET_REQUEST:

			// start after socket
			i = 1;
			for (; i < _es.buffer_len;) {
				x = x*10 + (_es.buffer[i] - '0');
				i++;
				if (_es.buffer[i] == 0xFF) {
					break;
				}
			}
			// malformed request
			if (i >= _es.buffer_len) {
				initEventState();
				return 0;
			}
			_es.event.type = (ESP_event_types) _parser_reg;
			_es.event.socket = _es.buffer[0];
			_es.event.reg = x;

			// read value
			x = 0;
			i++;
			_es.event.data = &(_es.buffer[i]);
			_es.event.datalen = 0;

			for (; i < _es.buffer_len;) {
				if (_es.buffer[i] == 0xFF) {
					_es.buffer[i] = '\0';
					break;
				}
				x = x*10 + (_es.buffer[i] - '0');
				i++;
				_es.event.datalen++;
			}
			// malformed request
			if (i >= _es.buffer_len) {
				initEventState();
				return 0;
			}
			_es.event.value = x;

			// read ip
			x = 0;
			i++;
			for (j = 0; j < 4; j++) {
				x = x << 8;
				x += _es.buffer[i+j];
			}
			_es.event.ip = x;

			break;

		case EVENT_TCP_CLIENT_DATA_RECEIVED:

			_es.event.type = (ESP_event_types) _parser_reg;
			_es.event.socket = _es.buffer[0];
			_es.event.data = &(_es.buffer[1]);
			len = _es.buffer_len - 1;
			_es.event.datalen = len;
			//Serial1.print(sizeof(_es.buffer));
			if (len + 1 < sizeof(_es.buffer)) {
				// null terminate
				_es.event.data[len] = 0;
			}
			break;

		case EVENT_TELNET_GOT_TCP_DATA:

			_es.event.type = (ESP_event_types) _parser_reg;
			_es.event.socket = _es.buffer[0];
			_es.event.data = &(_es.buffer[1]);
			len = _es.buffer_len - 1;
			_es.event.datalen = len;
			if (len + 1 < sizeof(_es.buffer)) {
				// null terminate
                                _es.event.data[len] = 0;
			}

			break;

		case EVENT_TELNET_GOT_UDP_DATA:

			_es.event.type = (ESP_event_types) _parser_reg;
			// buffer[0] is dummy byte
			_es.event.data = &(_es.buffer[1]);
			len = _es.buffer_len - 1;
			_es.event.datalen = len;
			if (len + 1 < sizeof(_es.buffer)) {
                                // null terminate
                                _es.event.data[len] = 0;
                        }
			break;

		case EVENT_TELNET_TCP_CONNECTED:
			{
				_es.event.type = (ESP_event_types) _parser_reg;
				_es.event.socket = _es.buffer[0];
				union ip ip;
				for (i = 0; i < 4; i++) {
					ip.b[3-i] = _es.buffer[i+1];
				}
				_es.event.ip = ip.ip;
				union port port;
				port.b[1] = _es.buffer[5];
				port.b[0] = _es.buffer[6];
				_es.event.port = port.port;
			}
			break;

		case EVENT_TELNET_TCP_DISCONNECTED:
			_es.event.type = (ESP_event_types) _parser_reg;
			_es.event.socket = _es.buffer[0];
			break;

		default:
			// unknown or not yet implemented
			Serial1.print(F("unknown event type: "));
			Serial1.println(_parser_reg);
			break;
		}
		return 1;
	} // data available

	return 0;
}

// return event type
struct event IC_ESP::getEvent() {
	return _es.event;
}

// send value as integer, convert to char
void IC_ESP::sendEventResponse(unsigned long val) {

	unsigned short i = 0;
	unsigned short len = 0;
	unsigned long temp = val;
	for (i = 0; ; i++) {
		temp /= 10;
		len++;
		if (temp == 0) break;
	} 
	// max length of decimal number is 10 for 32 bit integer
        char valc[11];
	temp = val;
        for (i = 0; i < len; i++) {
                valc[len-i-1] = temp%10 + '0';
                temp /= 10;
        }
	valc[len] = 0;
	sendEventResponse((char*)&valc, len);
}

void IC_ESP::sendEventResponse(const String & val)
{
  sendEventResponse(val.c_str(),val.length());
  return;
}

// send IE response
// copy needed data from request
// at the moment only 4 bytes are used
void IC_ESP::sendEventResponse(const char* str, unsigned long len) {

	unsigned short i = 0;
	// packet length
	unsigned short length = 0;
	// data length
	unsigned short count = 0;

	unsigned char buf_send[EVENT_BUFFER_SIZE];

	unsigned short reglen = 0;

	// register starts after socket id
	i = 1;
	while(_es.buffer[i] != 0xFF) {
		i++;
		reglen++;
	}
	i = 0;

        // sse below: socket, register, 0xff, value, 0xff, ip
	// count does not include checksum
	switch(_es.event.type) {
	case EVENT_HTTP_GET_REQUEST:
		count = 1 + reglen + 1 + len + 1 + 4;
		break;
	case EVENT_HTTP_SET_REQUEST:
		count = 1 + reglen + 1 + len + 1 + 4;
		break;
	default:
		// TODO
		break;
	}

	buf_send[length++] = 'I';
	buf_send[length++] = 'E';
	switch(_es.event.type) {
	case EVENT_HTTP_GET_REQUEST:
		buf_send[length++] = EVENT_HTTP_GET_RESPONSE;
		break;
	case EVENT_HTTP_SET_REQUEST:
		buf_send[length++] = EVENT_HTTP_SET_RESPONSE;
		break;
	default:
		// TODO
		break;
	}
	// DataInfo High Byte
	buf_send[length++] = count>>8 & 0xFF;
	// DataInfo Low Byte
	buf_send[length++] = count & 0xFF;

	// socket
	buf_send[length++] = _es.buffer[0];
	// reg
	for (i = 0; i < reglen; i++) {
		buf_send[length++] = _es.buffer[i+1];
	}
	buf_send[length++] = 0xFF;
	// value
	for (i = 0; i < len; i++) {
		buf_send[length++] = str[i];
        }

	buf_send[length++] = 0xFF;
	// user ip
	char* ip = (char*) &(_es.event.ip);
	for (i = 0; i < 4; i++) {
		buf_send[length++] = ip[3-i];
	}
	// checksum
	buf_send[length++] = 0x00;

	initEventState();

	// send buffer
	for (i = 0; i < length; i++) {
		_espUART.write(buf_send[i]);
	}
}

void IC_ESP::disableInterrupt() {
	_espUART.setINT_None();
}

void IC_ESP::enableInterrupt() {
	_espUART.flush();
	_espUART.setINT_RXlvl();
}

// for debug
unsigned char* IC_ESP::getBuffer() {
  return (unsigned char*) &_es.buffer;
}

unsigned char* IC_ESP::getBufferHeader() {
  uint8_t irt = _espUART.readRegister(IER);
  // debug: print uart interrupt setting
  Serial1.print("irt: ");
  Serial1.print(irt);
  Serial1.print(" _parser_reg: ");
  Serial1.print(_parser_reg);
  Serial1.print(" 7(sc-irq): ");
  Serial1.print(digitalRead(7));
  Serial1.print(" debugMode: ");
  Serial1.print(debugMode);
  Serial1.print(" _mode: ");
  Serial1.print(_mode);
  Serial1.print(" MCUCR: ");
  Serial1.print(MCUCR);
  Serial1.print(" EICRB:");
  Serial1.print(EICRB);
  Serial1.print(" EIMSK:");
  Serial1.print(EIMSK);
  Serial1.print(" SREG:");
  Serial1.print(SREG);
  Serial1.println("");
  return (unsigned char*)&_es.buffer_header;
}

unsigned short IC_ESP::getBufferLength() {
  return _es.buffer_len;
}

unsigned short IC_ESP::getBufferIndex() {
  return _es.buffer_index;
}

