
/*
 * main.c
 * Handles initialization for the main project
 * Handles main control loop
 * Handles interrupts (from switch and eventually RF module)
 *
 */


#include <msp430.h>
#include <intrinsics.h>
#include "utils.h"
#include "include.h"


#define OPERATE_SENSOR 1 // Just to make packets a little more readable
#define VALID_LEVEL 1 // This will come back from the sensor node in rxBuffer

#define TI_CC_SW1 BIT3
#define WATER_LEVEL_VALID 1
#define WATER_DATA_RX_INDEX 1 // Data will be received in the second entry in rxBuffer due to trimming.
#define MSGLEN 12
//#define PWM 1 // uncomment for PWM options

#ifdef PWM // In case PWM is desired, these can be used to set it up.
#define MCU_CLK 1000000
#define PWM_FREQ 2 // 10k PWM frequency
#define PWM_DUTY 50 // percentage duty cycle
#define PWM_PERIOD (MCU_CLK/PWM_FREQ) // time between pulses
#define ONTIME_CYCLES 5000000 // 5 seconds ontime
#endif

extern char paTable[]; // external arrays for radio settings
extern char paTableLen; // used by CC110L

volatile char water_level_request;
char txBuffer[MSGLEN];
char rxBuffer[MSGLEN];


int main(void) {
    WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

    //P2REN |= BIT3; // enable pull up resistor on p2.3
    //P2OUT |= BIT3;

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
    		// Ideally, P2.3 should be pulled high. This is causing strange behavior on the pin
    		//    that should be investigated.
    		txBuffer[2] = (P2IN & BIT3) ? 0 : 1; // if the switch is low (open), turn on the pump
    		RFSendPacket(txBuffer, MSGLEN);
    		__delay_cycles(450000); // delay a few cycles. Note that this means requests that take place during
    								// a pump cycle will be ignored completely.
    		//\P2OUT &= ~BIT4; // turn off the LED
    		water_level_request = 0; // clear flag
        	_BIS_SR(GIE); // re-enable interrupts
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
    if (RFReceivePacket(rxBuffer,&len))
    {
        // Fetch packet from CC110
		//P2OUT |= (rxBuffer[1]) ? BIT4 : 0; // turn on LED if we need to read data
		water_level_request = 1; // set flag to indicate a request is received.
     }
  }

  TI_CC_GDO0_PxIFG &= ~TI_CC_GDO0_PIN;      // After pkt RX, this flag is set.
}
