
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
    if(outSerial) outSerial.begin(115200);
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
    
    // Activate full power transmission
    SX1272LoRaSetPa20dBm(true);

    // function for running LoRa
    switch( Radio->Process( ) )
    {
    case RF_RX_TIMEOUT:
    case RF_RX_DONE:
    case RF_TX_DONE:
        // Set led off to show end of transmission
        digitalWrite(PIN_LED, LOW);
        
        // Output for finished transmission
        if(outSerial) outSerial.println("TX done");
        
        if(TX_PAYLOAD_SIZE <= BUFFER_SIZE)
        {
            // Fill buffer with numbers
            for( i = 0; i < TX_PAYLOAD_SIZE; i++ )
            {
                Buffer[i] = i;
            }
        }
        
        // Delay between two transmissions
        delay(250);

        // Start transmission with LoRa
        Radio->SetTxPacket( Buffer, TX_PAYLOAD_SIZE );

        // Set led on to show transmission
        digitalWrite(PIN_LED, HIGH);

        break;
        
    default:
        break;
    }
}

