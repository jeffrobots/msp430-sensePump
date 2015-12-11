// Jeff Baatz
// sensePump
// https://github.com/jeffrobots/msp430-sensePump.git
/*
 * main_sense.c
 * Handles initialization and control for a water sensor node
 * Idles until a data request packet is received, then checks
 * the status of a water reservoir and reports back to the sender
 * with a
 *
 */


#include "include.h"
#define OPERATE_SENSOR 1 // Just to make packets a little more readable
#define VALID_LEVEL 1 // This will come back from the sensor node in rxBuffer

#define WATER_LEVEL_VALID 1 	// Definition of what constitutes a valid level data entry.
#define DATA_REQUEST_RX_INDEX 1 // Index for checking received packet
#define WATER_DATA_TX_INDEX 2   // Index for transmitting water level data
#define MSGLEN 12 				// Size of data packet to look for (before trimming in radio drivers)

extern char paTable[]; // external arrays for radio settings
extern char paTableLen; // used by CC110L drivers

volatile char water_level_request;	// Flag specifying a request has been received
char txBuffer[MSGLEN];				// Char array to hold a standard message structure
char rxBuffer[MSGLEN];				// Char array to hold received radio data


int main(void) {
    WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

    //P2REN |= BIT3; // enable pull up resistor on p2.3
    //P2OUT |= BIT3; // This does not seem to be working as expected.

    // Initialize radio and SPI
    TI_CC_SPISetup();                         // Initialize SPI port
	TI_CC_PowerupResetCCxxxx();               // Reset CCxxxx
	writeRFSettings();                        // Write RF settings to config reg
	TI_CC_SPIWriteBurstReg(TI_CCxxx0_PATABLE, paTable, paTableLen);//Write PATABLE
	// Enable interrupts from radio module
	TI_CC_GDO0_PxIES |= TI_CC_GDO0_PIN;       // Int on falling edge (end of pkt)
	TI_CC_GDO0_PxIFG &= ~TI_CC_GDO0_PIN;      // Clear flag
	TI_CC_GDO0_PxIE |= TI_CC_GDO0_PIN;        // Enable int on end of packet
	// Enable radio
	TI_CC_SPIStrobe(TI_CCxxx0_SRX);           // Initialize CCxxxx in RX mode.

    // configure packet
    // --------------------------------------------
	txBuffer[0] = MSGLEN-1;                        // Packet length
	txBuffer[1] = 0x01;                     // Packet address - If this is 0xFF, it's an ack and not data.
	// Begin data
	// --------------------------------------------
	txBuffer[2] = VALID_LEVEL; // default flag is valid. This is set on request from other module.
	txBuffer[3] = 0x30;
	txBuffer[4] = 0x31;
	txBuffer[5] = 0x34;
	txBuffer[6] = 0x35;
	txBuffer[7] = 0x36;
	txBuffer[8] = 0x37;
	txBuffer[9] = 0x38;
	txBuffer[10] = 0x39;		// the rest of this data is used as filler.
	// ------
	// End Data
	txBuffer[11] = 0x00;					// terimate
	// --------------------------------------------

	_BIS_SR(GIE); // turn on interrupts. Initialization must be complete by this point
    while(1) {
    	if(water_level_request) {
    		// Ideally, P2.3 should be pulled high. This is causing strange behavior
    		// 		on the pin that should be investigated.
    		// Check the status of P2.3 and set the r
    		txBuffer[WATER_DATA_TX_INDEX] = (P2IN & BIT3) ? 0 : VALID_LEVEL;// turn on the pump if the water level is valid.
    		RFSendPacket(txBuffer, MSGLEN);		// Send water level data back to controller
    		__delay_cycles(450000); 			// delay a few cycles. Note that this means requests that take place during
    											// a pump cycle will be ignored completely.
    		water_level_request = 0; // clear requested data flag
      	}
    }
}

// CC110L received a packet (Port 2 interrupted on GDO0)
#pragma vector=PORT2_VECTOR
__interrupt void PORT2_ISR()
{
   // if GDO fired
  if(TI_CC_GDO0_PxIFG & TI_CC_GDO0_PIN)
  {
    char len=11;                            // Len of rx packet expected
    if (RFReceivePacket(rxBuffer,&len)) 	// Check for a received packet
    {
        if(rxBuffer[DATA_REQUEST_RX_INDEX] == OPERATE_SENSOR){// if the packet indicates that the sensor should read data...
        	water_level_request = 1; // set flag to indicate a request is received.
        }
     }
    TI_CC_GDO0_PxIFG &= ~TI_CC_GDO0_PIN;      // After pkt RX, this flag is set.
  }
}
