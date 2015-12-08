
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

volatile char rx_water_level_data;


extern char paTable[]; // external arrays for radio settings
extern char paTableLen; // used by CC110L

char buttonPressed;
char txBuffer[MSGLEN];
char rxBuffer[MSGLEN];

char WaterLevelValid(void);



int main(void) {
    WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer
    /* Inputs
 	P2DIR = BIT2;
 	P1DIR = BIT3 | GDO0;
     */
    // Outputs
    P2DIR |= BIT3 | BIT4; // output to BLDC and LED
    //P2SEL |= BIT3;
    P2OUT &= ~BIT3; // make sure pump is off
    P2OUT &= ~BIT4; //make sure LED is off

    // Port 1 pushbutton config
    P1REN |= TI_CC_SW1; // enable PUR on switch1
    P1OUT |= TI_CC_SW1;
    P1IES |= TI_CC_SW1; // interrupt on falling edge
    P1IFG = 0; // clear all interurpt flags
    P1IE = TI_CC_SW1; // enable interrupts on pushbutton

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
	txBuffer[0] = MSGLEN-1;                        // Packet length -- this will get trimmed off.
	txBuffer[1] = 0x01;                     // Packet address - If this is 0xFF, it's an ack and not data.
	// Begin data
	// --------------------------------------------
	txBuffer[2] = OPERATE_SENSOR; // flag for other node to know that it needs to read switch state.
	txBuffer[3] = 0x30;
	txBuffer[4] = 0x31;
	txBuffer[5] = 0x34;
	txBuffer[6] = 0x35;
	txBuffer[7] = 0x36;
	txBuffer[8] = 0x37;
	txBuffer[9] = 0x38;
	txBuffer[10] = 0x39;		// the rest of this data isn't used in ths project.
	// ------
	// End Data
	txBuffer[11] = 0x00;					// terimate
	// --------------------------------------------

	_BIS_SR(GIE); // turn on interrupts. Initialization must be complete by this point
    while(1) {
    	if(buttonPressed) {
    		//_BIC_SR(GIE); 		// disable interrupts to avoid conflicts
    		if (WaterLevelValid()) {
    			// make sure red LED is off
    			P2OUT |= BIT3; // turn on pump
    			__delay_cycles(1000000); // delay a few cycles
    			P2OUT &= ~BIT3; // turn off pump
    			P2OUT &= ~BIT4; // turn off the LED
    		}
    		buttonPressed = 0; // clear flag
        	_BIS_SR(GIE); // re-enable interrupts
    	}
    }
}

#pragma vector=TIMER0_A1_VECTOR
__interrupt void TIMERA1_ISR(void) {
	// no interrupt necessary due to automatic TA1.0 operation
}

#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
	if(P1IFG & TI_CC_SW1) {
		__delay_cycles(10000); // debounce switch by waiting
		buttonPressed = 1;
	}
	P1IFG = 0;
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
		P2OUT |= BIT4;//(rxBuffer[1]) ? BIT4 : 0; // turn on an LED if the water level is valid
		rx_water_level_data = 1; // set flag to indicate that sensor data has been received
     }
  }

  TI_CC_GDO0_PxIFG &= ~TI_CC_GDO0_PIN;      // After pkt RX, this flag is set.
}

char WaterLevelValid(void) {
	// This function builds and transmits a request packet to the sensor module
	// It then waits until a flag is set within an RX interrupt
	//    that specifies a valid water level condition.
	// Build packet
	rx_water_level_data = 0;
	RFSendPacket(txBuffer, MSGLEN);
	while(rx_water_level_data == 0); // hold in loop until sensor data is received.
	return rxBuffer[WATER_DATA_RX_INDEX];
}
