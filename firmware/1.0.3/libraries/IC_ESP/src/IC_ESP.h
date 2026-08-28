/*
        Pin, register and other definitions for ESP8266EX chip on In-Circuit radino WiFi modules
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

#ifndef IC_ESP_h
#define IC_ESP_h

#include "Arduino.h"

#define EVENT_HEADER_SIZE 8
#ifdef RADINO32
  #define EVENT_BUFFER_SIZE 1024
  #include <radino32_UART.h>
  #if defined ESP_GPIO0
   #undef ESP_GPIO0
  #endif
  #if defined ESP_GPIO2
    #undef ESP_GPIO2
  #endif
  // Pin definitions of ESP8266EX
  //       Pin-Name   Arduino-Pin-Number
  #define  CH_PD      29
  #define  CH_RST     36
  #define  ESP_GPIO0_A    30  //A13 - HW RevA
  #define  ESP_GPIO2_A    31  //A14 - HW RevA
  #define  ESP_GPIO0_B    34  //B13 - HW RevB
  #define  ESP_GPIO2_B    35  //B14 - HW RevB
#else
  #define EVENT_BUFFER_SIZE 128
  #include <SPI_UART.h>
  #define  CH_PD      A4
  #define  CH_RST     4
  #define  ESP_GPIO0      9
  //       Pin-Name   SC16IS750-GPIO-Number
  #define  ESP_GPIO2      0
#endif

#define IC_ESP_VERSION 5

enum ESP_modes{
	ESP_RAW_MODE = 0,
	ESP_IC_PROTOCOL_MODE,
	ESP_FIMWAREUPDATE_MODE,
	ESP_MODE_POWERDOWN,
	ESP_MODE_UNCHANGED,
	};

enum ESP_parser_states{
	IC_PARSER_IDLE = 0, // Idle, waiting for new command
	IC_PARSER_ICSTART, // 'I'
	
	// Got Event
	IC_PARSER_PARA_1, // register number / event number
	IC_PARSER_PARA_2, // DataInfo High Byte / Error-Code High Byte
	IC_PARSER_PARA_3, // DataInfo Low Byte / Error-Code Low Byte
	
	IC_PARSER_COPYTOWORKER,

	IC_PARSER_CHECKANDLEAVE, // Check Checksum & done
	IC_PARSER_CHECKANDCOPY, // Check checksum & update data registers
	IC_PARSER_DATA, // Data
	IC_PARSER_DATA_STRING_READY, // Datatype: String - data ready
//	IC_PARSER_STRINGBURST,
//	IC_PARSER_EXEC
	IC_PARSER_INACTIVE = 0x80,
	IC_PARSER_DISABLE_PARSER,
	};

enum ESP_wifi_modes{
	ESP_WIFI_STATION = 1,
	ESP_WIFI_AP = 2,
	ESP_WIFI_DUAL = 3,
};

enum ESP_read_modes{
	READ_BLOCKING,
	READ_ASYNC,	
	};
	
enum ESP_write_modes{
	WRITE_BLOCKING,
	WRITE_ASYNC,
	WRITE_IGNORE_RESPONSE,
};

enum ESP_IC_registers{

	// WiFi settings
	WIFI_MODE =			0x01,
	WIFI_SSID_AP =			0x02,
	WIFI_PASSWD_AP =		0x03,
	WIFI_CHANNEL_AP =		0x05,
	WIFI_SSID_JOIN =		0x06,
	WIFI_PASSWD_JOIN =		0x07,

	// IP-Settings
	IP_ADDR_READ =			0x10,
	IP_ADDR_AP = 			0x11,
	// not yet implemented on ESP
	//IP_ADDR_JOIN =			0x16,

	// TCP-Settings
	TCP_STATUS =			0x40,
	TCP_STROBE =			0x41,
	TCP_CLIENT_TARGETIP =		0x44,
	TCP_CLIENT_TARGETPORT = 	0x45,
	TCP_CLIENT_SEND_TARGETSOCKET =	0x48,
	TCP_CLIENT_SEND =		0x49,
	TCP_CLOSE_SOCKET =		0x4A,
	TCP_PUSH =			0x4B,

	TELNET_STATUS =			0x50,
	TELNET_STROBE =			0x51,
	TELNET_PORT_TCP =		0x52,
	TELNET_PORT_UDP =		0x53,
	TELNET_PORT_UDP_BROADCAST =	0x54,
	TELNET_TARGET_SOCKET =		0x55,

	TELNET_SOCKET_SEND =		0x56,
	TELNET_UDP_SEND =		0x57,
	TELNET_BROADCAST_SEND =		0x58,

	// HTTP-Settings
	HTTP_STATUS = 			0x60,
	HTTP_STROBE = 			0x61,
	HTTP_SERVERPORT =		0x62,
	HTTP_UPLOAD =			0x63,

	// System-Settings
	SYSTEM_STROBE =		0x72,
	STROBE_STORE =		0x7A,
	FORCE_APIP =		0x7B,
	USER_DATA =		0x7E,
	FIRMWARE_VERSION =	0x7F,
	};

enum ESP_event_types{
	EVENT_NONE = 0,
	EVENT_TCP_NEWDATA = 1,
	EVENT_UDP_NEWDATA = 2,
	EVENT_HTTP_SET_REQUEST = 3,
	EVENT_HTTP_SET_RESPONSE = 4,
	EVENT_HTTP_GET_REQUEST = 5,
	EVENT_HTTP_GET_RESPONSE = 6,
	EVENT_HTTP_AUTH_REQUEST = 7,
	EVENT_HTTP_AUTH_RESPONSE = 8,
	EVENT_HTTP_USERLEVEL_REQUEST = 9,
	EVENT_HTTP_USERLEVEL_RESPONSE = 10,
	EVENT_TCP_CLIENT_DATA_RECEIVED = 11,
	EVENT_HTTP_CLIENT_DATA_RECEIVED = 12,
	EVENT_TELNET_GOT_TCP_DATA = 13,
	EVENT_TELNET_GOT_UDP_DATA = 14,
	EVENT_TELNET_TCP_CONNECTED = 15,
	EVENT_TELNET_TCP_DISCONNECTED = 16,
	};

enum TCP_SOCKET_STATUS {
	CLOSED = 0,
	CONNECTING = 1,
	LISTENING = 2,
	OPEN = 3,

	ERROR = 0xFF,
};

enum ESP_event_modes{
	EVENT_MODE_IGNORE = 0,
	EVENT_MODE_HANDLE_ASYNC,
	};

union ip {
	unsigned long ip;
	unsigned char b[sizeof(unsigned long)];
};

union port {
	unsigned short port;
	unsigned char b[sizeof(unsigned short)];
};

struct event {
	// IE event type
	ESP_event_types type;
	unsigned char socket;
	// remote/client ip & port
	unsigned long ip;
	unsigned short port;
	// IE GET / SET register
	unsigned long reg;
	unsigned long value;
	// client data (tcp/telnet)
	unsigned char* data;
	unsigned short datalen;
};


struct event_state {
	/* interface */
	unsigned short data_available;
	// set to 1 by newEventAvailable, reset by sendEventResponse
	// if newEventAvailable if called again and value is still 1
	// we know that this is a request the user sketch won't handle
	unsigned short data_read;
	// to indicate the parser that data can be overwritten
	unsigned short data_handled;

	struct event event;
	/* data buffer for IE requests */
	unsigned char buffer_header[EVENT_HEADER_SIZE];
	unsigned char buffer[EVENT_BUFFER_SIZE];
	unsigned long buffer_index;
	unsigned long buffer_len;
};

struct command_state {
	// TODO store header etc.
	/* interface */
	unsigned short data_available;
	unsigned short data_handled;

	//cmd;
	// small buffer for read/write requests
	unsigned long buffer_small;
};

// General macros
#define IP(a,b,c,d)	(((unsigned long)a<<24)|((unsigned long)b<<16)|((unsigned long)c<<8)|((unsigned long)d<<0)) // Build 32Bit IP value from seperate ip segments

// Parser help-defines
#define	IC_PARSER_SHIFT_DATATYPE	13	// Shift DataInfo for DataType
#define IC_PARSER_MASK_DATALEFT		0x1FFF // Cut top 3 bits
#define IC_PARSER_MASK_BAD_STATE    0x80 // Mask for states where normal operation is not possible

#define IC_PARSER_RESET_TIMEOUT 50 // ms

// DataTypes
#define IC_DT_STRING	  0x00
#define IC_DT_UINT32      0x01
#define IC_DT_SINT32      0x02
#define IC_DT_FLOAT       0x03
#define IC_DT_STRINGBURST 0x04

// Handshake-Line
// ESP Drives high when busy with httpd requests
#define HARDWARE_HANDSHAKE _gpio0	// Arduino Pin 9 / ESP8266EX GPIO0
#define CHECK_ESPBUSYLINE() digitalRead(_gpio0)
class IC_ESP
{	
	public:
		// Variable to remember own instance to refer to callback-functions
		static IC_ESP* callback_instance;
		IC_ESP();
		// Init ESP8266EX
		unsigned char init(unsigned char mode = ESP_MODE_UNCHANGED);
		// Check if ESP is ready for communication; 0: not ready - 1: ready
		unsigned char ready();
		// Set mode to normal / firmwareUpdate
		unsigned char setMode(unsigned char mode);
		// function which handles SC16 interrupts
		static unsigned char handleInterrupt(unsigned char val);
		
		// main parser
		unsigned char parser(unsigned char chr);
		// Add next char to checksum
		void parser_addCheck(unsigned char newChar);
		// returns True if "checksum" equals last calculated checksum
		unsigned char parser_checkChecksum(unsigned char checksum);

		// Read Register of ESP8266EX; Returns success or Error Code
		unsigned char writeRegister(unsigned char reg, String data, unsigned char writeMode = WRITE_BLOCKING, unsigned short timeoutms = 500);
		unsigned char writeRegister(unsigned char reg, unsigned char *data, unsigned char writeMode = WRITE_BLOCKING, unsigned short timeoutms = 500);
		unsigned char writeRegister(unsigned char reg, unsigned long data, unsigned char writeMode = WRITE_BLOCKING, unsigned short timeoutms = 500);

		// returns status: 1 - ok, 0 - error
		unsigned char readRegister(unsigned char reg, unsigned char readMode = READ_BLOCKING, unsigned short timeoutms = 500);
		
		void SPI_setBaudrate(unsigned long baudrate); // Set baudrate of SC16IS750
		
		void write(unsigned char character); // Directly write to ESP8266EX UART
		unsigned char read(); // Directly read from ESP8266EX UART
		unsigned char available(); // Data available on ESP8266EX UART?
				
		// Application Layer Functions

		// WiFi-Settings
		unsigned char AP_setSSID(String ssid); // Set SSID for AP-Mode -  max. 32 chars
		unsigned char AP_setPW(String pw); // Set PW for AP-mode - max. 64 chars
		unsigned char AP_setChannel(unsigned long channel);
		unsigned char ST_setSSID(String ssid); // Set SSID for JOIN-Mode / station -  max. 32 chars		
		unsigned char ST_setPW(String pw); // Set PW for JOIN-mode / station - max. 64 chars
		unsigned char NET_mode(ESP_wifi_modes mode); // select network mode Station/AP
		// System-Settings
		unsigned char NET_wifiStart();
		unsigned char NET_wifiReset();

		// IP-Settings
		// currently used IP address
		unsigned long getIPAddress();
		unsigned long ST_getIPAddress();
		void getIPAddress(unsigned char *buf, unsigned long ip);
		unsigned long AP_getIPAddress();
		unsigned char AP_setIPAddress(unsigned long ip);
		unsigned char AP_forceIPAddress(unsigned long ip);

		// TCP-Settings
		unsigned long TCP_status(unsigned long *status);
		TCP_SOCKET_STATUS TCP_status(unsigned long socket);
		unsigned char TCP_serverRunning(unsigned char *running);
		unsigned char TCP_serverConnected(unsigned char *connected);
		// opens a TCP connection as configured with functions below
		unsigned char TCP_openClientSocket();
		// set/get target ip
		unsigned char TCP_setClientTargetIP(unsigned long ip);
		unsigned long TCP_clientTargetIP();
		// set/get target port
		unsigned char TCP_setClientTargetPort(unsigned short port);
		unsigned short TCP_clientTargetPort();
		// set/get currently used socket
		unsigned char TCP_setClientSocket(unsigned long clientSocket);
		unsigned char TCP_clientSocket(unsigned long *clientSocket);
		// send data to TCP client
		unsigned char TCP_clientSend(unsigned char *data);
		unsigned char TCP_clientSend(const char *data);
		// close socket
		unsigned char TCP_closeSocket();
		// Send "data" to target IP & port via TCP and don't wait for response
		unsigned char TCP_push(unsigned long ip, unsigned short port, String data);

		// Telnet Settings
		unsigned long Telnet_status(unsigned long *status);
		unsigned char Telnet_start();
		unsigned char Telnet_stop();
		unsigned long Telnet_TCPPort();
		unsigned char Telnet_setTCPPort(unsigned long port);
		unsigned long Telnet_UDPPort();
		unsigned char Telnet_setUDPPort(unsigned long port);
		unsigned long Telnet_UDPBroadcastPort();
		unsigned char Telnet_setUDPBroadcastPort(unsigned long port);
		unsigned char Telnet_targetSocket(unsigned long *socket);
		unsigned char Telnet_setTargetSocket(unsigned long socket);
		unsigned char Telnet_socketSend(String data);
		void Telnet_UDPSend(String data);
		void Telnet_broadcastSend(String data);

		// HTTP-Settings
		unsigned char HTTPD_getStatus(unsigned long *data);
		unsigned char HTTPD_setServerPort(unsigned short port); // set HTTP server port, default 80
		unsigned char HTTPD_startServer(); // start HTTP server
		unsigned char HTTPD_updateWebsite(const char *data);
		unsigned char HTTPD_updateWebsiteProgMem(char *data);
		//unsigned char HTTPD_updateWebsite(String *data); // Upload new website to internal webserver

		// store all ESP settings
		unsigned char StoreSettings_All();
		// store AP IP address
		unsigned char StoreSettings_APIP();
		// reset all settings to factory values
		unsigned char StoreSettings_FactoryReset();
		// read user data
		unsigned char userData(unsigned long *data);
		// write user data
		unsigned char setUserData(unsigned long data);
		// ESP version
		unsigned long getFirmwareVersion();
		// IC_ESP version
		unsigned long getVersion();

    // Go to deepsleep
    unsigned char goToDeepsleep();
    // Wakeup from deepsleep
    unsigned char wakeupFromDeepsleep();

		// Debugging only !!!
		unsigned char debugInit(unsigned char mode);
		unsigned char debugHandle(char);
		
		// \/------------ not implemented yet ---------------\/
		void setBaudRate(unsigned long baudrate); // Set Baudrate of ESP8266 via command
		void factoryReset(); // Tell ESP8266 to load factory settings

		// Event Interface
		// check if event available
		unsigned newEventAvailable();
		// return event type; only valid if new event is available
		struct event getEvent();
		// send response; only valid if new event is available
		void sendEventResponse(unsigned long val);
		void sendEventResponse(const char* str, unsigned long length);
    void sendEventResponse(const String & val);

		// disable SC16 interrupts
		// used when sending large data, e.g. website upload
		void disableInterrupt();
		void enableInterrupt();

// debug
unsigned char* getBuffer();
unsigned char* getBufferHeader();
unsigned short getBufferLength();
unsigned short getBufferIndex();

	private:
    uint8_t _gpio0;
    uint8_t _gpio2;
    
		unsigned char handshakeAlreadyActive(); // Read Handshake-Line and return status (1 == HIGH == used; 0 == LOW == FREE)
		void setHandshake(); // Set Handshake-line HIGH
		void releaseHandshake(); // Let go of Handshake-line
		
		void initVariables(); // Init all variables
		
		void clearParserReceiveRegs(); // Reset Receive buffers; set "_worker_data_valid" to 0

#ifdef RADINO32
		radino32_UART _espUART; // Hardware UART3 for ESP8266EX in radino32 Wifi
#else
        SPI_UART _espUART; // SPI-UART for ESP8266EX in radino Wifi
#endif
		
		// Settings
		unsigned long _baudrate; // Baudrate for SPI-UART Bridge / current Baudrate with ESP8266
		unsigned char _mode; // ESP current mode
		unsigned char _event_mode; // Event Mode
		
		// Flags
//		volatile unsigned char _got_Response;
//		volatile unsigned char _waiting_for_response; // TRUE while waiting for response, false if not waiting for response
		volatile unsigned char _event_handle_required; // set to Event Type if event was received and needs to be read -> 
		
		// States -> Parser
		volatile unsigned char  _parser_state; // State for parser
		unsigned char  _parser_cmd_type; // command type
		unsigned char  _parser_reg; // register number / IE: event type
		unsigned short _parser_cmd_dataHeader; // 2 Bytes 'dataInfo' / Error Code
		unsigned char  _parser_cmd_dataType; // DataType
		unsigned short _parser_cmd_dataLeft; // Data amount
		volatile unsigned long  _parser_data_buffer; // 32 Bit data of non-String-Commands
		unsigned char  _parser_checksum; // Checksum
		
		//unsigned char _parser_ie_http_socketid; // buffer for socketID
		//unsigned long _parser_ie_http_reg; // register number -  16 bit
		unsigned long _parser_timeout;
		
		// Flags for worker
		// Parser READ-only; Worker WRITE-only:
		volatile unsigned char _worker_handling_active; // True while worker is accessing _worker_* registers
		// Parser WRITE-only; Worker READ-only:
		volatile unsigned char _worker_data_valid; // True after Parser has written new data to the registers
		
		// States / Data for worker
		volatile unsigned char  _worker_cmd_type; // command type
		volatile unsigned char  _worker_reg; // register number
		unsigned short _worker_cmd_dataHeader; // 2 Bytes 'dataInfo' / Error Code
		unsigned char  _worker_cmd_dataType; // DataType
		volatile unsigned short _worker_cmd_dataLeft; // Data amount
		unsigned long  _worker_data_buffer; // 32 Bit data of non-String-Commands

		// worker state
		struct command_state _cs;
		struct event_state _es;
		// reset state
		void initStates();
		void initEventState();
		void initCommandState();

	};
		
#endif

