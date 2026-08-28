
/*
 * Example code for reciver on radino32 SX1272
 *
 * for more information: www.in-circuit.de or www.radino.cc
 */

#include <SPI.h>
#include <stm32/l1/iwdg.h>
#include <radino32_sx1272.h>

// Define led pin
#define PIN_LED   13

// Serial for USB-UART, Serial1 for Hardware-UART
#define outSerial Serial
// Baudrate for serial communication
#define SERIALSPEED 115200

// Payloadsize for transmission with LoRa
#define TX_PAYLOAD_SIZE                             10
// Define buffer with max buffersize for LoRa
#define BUFFER_SIZE                                 RF_BUFFER_SIZE_MAX 
static unsigned char Buffer[BUFFER_SIZE];	    // RF buffer

tRadioDriver *Radio = NULL;


void setup ( void )
{
    // Init led
    pinMode(PIN_LED, OUTPUT);

    // Set led on
    digitalWrite(PIN_LED, HIGH);
    
    // Start serial communication
    if(outSerial) outSerial.begin(SERIALSPEED);
    if(outSerial) outSerial.print("Setup ");
    
    // Start init for RF-chip
    BoardInit( );
    if(outSerial) outSerial.print("-");
    delay(1);
    
    Radio = RadioDriverInit( );
    if(outSerial) outSerial.print("-");
    delay(1);

    Radio->Init( );
    if(outSerial) outSerial.print("-");
    delay(1);

    Radio->StartRx( );
    if(outSerial) outSerial.print("-");
    delay(1);

    delay(500);
    // Set led off
    digitalWrite(PIN_LED, LOW);

    if(outSerial) outSerial.println(" pass");
}


void loop( void )
{
    int i;

    // Init RF received byte counter for payload
    int rxCnt = 0;
    	
    // function for running LoRa
    switch( Radio->Process( ) )
    {
        // Timeout 500ms after Radio->GetRxPacket (LoRaSettings.RxPacketTimeout in radino32_sx1272.c)
    case RF_RX_TIMEOUT:
        // Serial output for receiver timeout
        if(outSerial) outSerial.println("RX timeout");
        delay(1);

        // Start receive with LoRa
        Radio->StartRx( ); 
        break;
        
    case RF_RX_DONE:
        // Set tx led on to show receive
        digitalWrite(PIN_LED, HIGH);
        
        // Clear Buffer for receive (set all to 0)
        for (i = 0; i < BUFFER_SIZE; i++)
        {
            Buffer[i] = 0;
        }
        
        // Get payload from receive
        Radio->GetRxPacket( Buffer, ( short unsigned int* )&rxCnt );

        // Request for payload on packet
        // More than 0, lesser than BUFFER_SIZE (RF_BUFFER_SIZE_MAX)
        if((rxCnt > 0) && (rxCnt <= BUFFER_SIZE))
        {
            // NOTE: all fallowing only for compare with dummydata
            // Compare reviced packet with dummydata form example
            i = 0;
            while((i < rxCnt) && (Buffer[i] == i))
            {
                i++;
            }
            // Serial output for received data           
            if (i == rxCnt)
            {
                if (rxCnt == TX_PAYLOAD_SIZE)
                {
                    // Found dummydata on packet payload
                    if(outSerial) outSerial.println("dummydata");
                    delay(1);
                }
                else
                {
                    // Found something else
                    if(outSerial) outSerial.println("unknown data");
                    delay(1);
                }
            }
            else
            {
                // Found something else
                if(outSerial) outSerial.println("unknown data");
                delay(1);
            }
        }

        // Set rx led off to wait for new receive
        digitalWrite(PIN_LED, LOW);
        
        // Start receive with LoRa
        Radio->StartRx( );       
        break;
        
    case RF_TX_DONE:
    default:
        break;
    }
}

