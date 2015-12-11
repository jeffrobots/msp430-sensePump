// Jeff Baatz
// sensePump
// https://github.com/jeffrobots/msp430-sensePump.git
/*
 * main_control.c
 * Handles initialization and control for a water pump node.
 * Idles until a request to turn on the pump is received.
 * Upon request, the controller sends a packet over sub 1GHz RF
 * 	to check the water level at the reservoir.
 * It then turns on the pump once a packet is received back
 * 	saying that it is safe to turn on.
 *
 */

#include "include.h"


#define OPERATE_SENSOR 1 // Just to make packets a little more readable
#define VALID_LEVEL 1 // This will come back from the sensor node in rxBuffer

#define TI_CC_SW1 BIT3 			// Port location of user switch (P1.3)
#define WATER_LEVEL_VALID 1 	// Label to make code more readable - signifies valid water level
#define WATER_DATA_RX_INDEX 1 	// Data will be received in the second entry in rxBuffer due to trimming.
#define MSGLEN 12 				// Length of message BEFORE first byte is trimmed by CC110L on receipt

extern char paTable[];			// external arrays for radio settings
extern char paTableLen; 		// used by CC110L

char buttonPressed; 				// Global flag for processing user interrupts
char txBuffer[MSGLEN]; 				// Char array for storing transmission data
char rxBuffer[MSGLEN];				// Char array for storing received data
volatile char rx_water_level_data; 	// flag for detecting when new data has been received.

char WaterLevelValid(void);



int main(void) {
    WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer
    P2DIR |= BIT3 | BIT4; 		// output to BLDC and LED
    P2OUT &= ~BIT3; 			// make sure pump is off
    P2OUT &= ~BIT4; 			//make sure LED is off

    // Port 1 pushbutton config
    P1REN |= TI_CC_SW1; 		// enable pullup/down on switch1
    P1OUT |= TI_CC_SW1; 		// configure PUR as pull down (active low)
    P1IES |= TI_CC_SW1; 		// interrupt on falling edge
    P1IFG = 0; // clear all interurpt flags
    P1IE = TI_CC_SW1; // enable interrupts on pushbutton

    // Initialize radio and SPI - This code is derived from TI radio drivers
    TI_CC_SPISetup();                         // Initialize SPI port
	TI_CC_PowerupResetCCxxxx();               // Reset CCxxxx
	writeRFSettings();                        // Write RF settings to config reg
	TI_CC_SPIWriteBurstReg(TI_CCxxx0_PATABLE, paTable, paTableLen);//Write PATABLE
	// Enable interrupts from radio module
	P2IES |= TI_CC_GDO0_PIN;       // Int on falling edge (end of pkt)
	P2IFG &= ~TI_CC_GDO0_PIN;      // Clear flag
	P2IE |= TI_CC_GDO0_PIN;        // Enable int on end of packet
	// Enable radio
	TI_CC_SPIStrobe(TI_CCxxx0_SRX);           // Initialize CCxxxx in RX mode.

    // configure packet
    // --------------------------------------------
	txBuffer[0] = MSGLEN-1;                        // Packet length -- this will get trimmed off.
	txBuffer[1] = 0x01;                     // Packet address - If this is 0xFF, it's an ack and not data.
	// Begin data
	// --------------------------------------------
	txBuffer[2] = OPERATE_SENSOR; 	// flag for other node to know that it needs to read switch state.
	txBuffer[3] = 0x30; 			// Filler data to be used later on
	txBuffer[4] = 0x31;
	txBuffer[5] = 0x34;
	txBuffer[6] = 0x35;
	txBuffer[7] = 0x36;
	txBuffer[8] = 0x37;
	txBuffer[9] = 0x38;
	txBuffer[10] = 0x39;		// the rest of this data isn't used in ths project
	// Extra data could be used in the event that more controller nodes are added
	// ------
	// End Data
	txBuffer[11] = 0x00;					// terimate
	// --------------------------------------------

	_BIS_SR(GIE); // turn on interrupts. Initialization is now complete.
    while(1) {
    	if(buttonPressed) { 		// buttonPressed is a user interrupt flag
    		P1IE &= ~TI_CC_SW1; 	// disable user interrupts to avoid conflicting pump cycles
    		if (WaterLevelValid()) { // WaterLevelValid() will retrieve water level status from the other node
    			P2OUT |= BIT3; // turn on pump
    			__delay_cycles(1000000); // delay for a second while pump runs
    			P2OUT &= ~BIT3; // turn off pump
    			P2OUT &= ~BIT4; // turn off LED
    		}
    		buttonPressed = 0; 	// clear flag
    		P1IE = TI_CC_SW1; 	// Enable user interrupts (accept a new pump cycle)
    	}
    }
}
// PORT1_ISR handles all interrupts from PORT1. In this case
// This is just the user switch to active a pump cycle.
#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
	if(P1IFG & TI_CC_SW1) {
		__delay_cycles(10000); 	// Debounce switch by waiting for 1ms
		buttonPressed = 1;		// Set flag for main() processing
	}
	P1IFG = 0; // clear interrupt flag and return
}

// CC110L received a packet (Port 2 interrupted on GDO0)
#pragma vector=PORT2_VECTOR
__interrupt void PORT2_ISR()
{
   // if GDO fired
  if(TI_CC_GDO0_PxIFG & TI_CC_GDO0_PIN) 	// Verify that the interrupt is from the radio
  {
    char len=11;                            // Len of rx packet expected
    if (RFReceivePacket(rxBuffer,&len)) 	// RFReceive checks the radio for new data and fills rxBuffer
    {
        // Fetch packet from CC110
		P2OUT |= (rxBuffer[1]) ? BIT4 : 0; // turn on an LED if the water level is valid
		rx_water_level_data = 1; // set flag to indicate that sensor data has been received
     }
  }

  TI_CC_GDO0_PxIFG &= ~TI_CC_GDO0_PIN;      // After pkt RX, this flag is set.
}

char WaterLevelValid(void) {
	// This function builds and transmits a request packet to the sensor module
	// It then waits until a flag is set within an RX interrupt
	//    that specifies a valid water level condition.
	rx_water_level_data = 0; 				// Set new data flag low
	RFSendPacket(txBuffer, MSGLEN); 		// Send a request for water level data
	while(rx_water_level_data == 0); 		// Hold in loop until sensor data is received.
	return rxBuffer[WATER_DATA_RX_INDEX];	// Return the byte corresponding to water level data
}
