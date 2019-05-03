/**********************************************************************
 *
 * XMEGA TWI driver example applied to the TCS34725 RGB Sensor.
 * This file was derived from the XMEGA TWI_Master software from Atmel
 * application note AVR1308: Using the XMEGA TWI.  This file uses only
 * TWI Master driver as a prototype for the Poseidon Project at SVVSD
 * Innovation Center.
 *
 * Original code is Copyright (c) 2008, Atmel Corporation All rights reserved.
 * The Atmel code is found in twi_master_driver.h and twi_master_driver.c
 * Created: 2/5/2017 7:20:50 PM
 * Author : Snoopy Brown
 *
 *****************************************************************************/
// The following line has been moved into the compiler symbols (Project->properties->toolchain)
//#define F_CPU 32000000UL		// set CPU variable so delays are correct
/********************************************************************************
						Includes
********************************************************************************/
#include "poseidon.h"
// #include "avr_compiler.h"
#include "twi_master_driver.h"
#include "twiForPoseidon.h"
#include "colorSensor.h"
#include "clocks_and_counters.h"
#include "xmega_uarte0.h"

/********************************************************************************
						Macros and Defines
********************************************************************************/

/********************************************************************************
						Global Variables
********************************************************************************/
volatile unsigned char temp1;
//Test string to verify UART is working
char String[]="Hello World!! The serial port is working!";
char DOdata[32];
// This next line opens a virtual file that writes to the serial port
static FILE mystdout = FDEV_SETUP_STREAM(USARTE0_Transmit_IO, NULL, _FDEV_SETUP_WRITE);
extern volatile uint8_t ItsTime;
uint16_t seconds;


/********************************************************************************
						Functions
********************************************************************************/


/********************************************************************************
						Main
********************************************************************************/
int main(void)
{
	// DEBUG: Provide some data to print out...(erase these lines when done testing)
	//uint16_t u16data = 10;
	ItsTime = 0;
	
	// Set up the serial interface output file (to computer)
	stdout = &mystdout;
	
	// *************************************************************************
	//        Initialization code & device configuration
	// *************************************************************************
	// Set up the system clock for 32MHz with FLL...
	system_clock_init();
	
	//Set up the timers and counters to control ESCs and Servos
	timer_counter_C0_C1_D0_init(ESC_TOP_COUNT);
	
	//Set up the 1Hz interrupt
	timer_counter_E0_init();
	
	//Set up the general purpose IO pins
	GPIO_init();
	
	/* Set the baud rate to parameters set in the xmega_uartd.h file. */
	USARTE0_init(MyBSEL, MyBSCALE);
	
	// Configure the hardware, pins, and interrupt levels for the TWI I/F on Port E
	TWIE_initialization();

	//blink an LED for the fun of it....
	ClearBit(xplained_red_LED_port, xplained_red_LED);
	_delay_ms(250);
	SetBit(xplained_red_LED_port, xplained_red_LED);
	//
	ClearBit(xplained_yellow_LED_port, xplained_yellow_LED_0);
	_delay_ms(250);
	SetBit(xplained_yellow_LED_port, xplained_yellow_LED_0);
	
	// enable global interrupts
	sei();
	//
	// Initialize the fake sensors used to drive the PI interface
	DO_init();
	//
	// Now set up the RGB sensor
	xmega_RGBsensor_init();
	// If we survived that, we're ready for the main loop
	
	//but first some UART set up
	// enable interrupts---------------------------------------------------
	// Enable all low level interrupts
	PMIC_CTRL |= PMIC_LOLVLEN_bm;
	
	// Test the interrupt isn't blocking 
	ClearBit(xplained_yellow_LED_port, xplained_yellow_LED_0);
	_delay_ms(1000);
	SetBit(xplained_yellow_LED_port, xplained_yellow_LED_0);
	// Interrupts should be good now -----------------------------------
	
	// *************************************************************************
	// main loop
	// *************************************************************************
	while (1) 
	{
		//Wait for the 1Hz interrupt...
		if (ItsTime == 1){ //wait for our 1Hz flag
			ItsTime = 0; 
			seconds++;
			printf("\nSeconds = %u", seconds);
			DO_read(DOdata);
			printf("\n%s", DOdata);
			TWI_XFER_STATUS = xmega_read_RGB_values();
			printf("\nraw clear = %u", raw_clear);
			printf("\nraw red   = %u", raw_red);
			printf("\nraw green = %u", raw_green);
			printf("\nraw blue  = %u", raw_blue);
			printf("\n=================");
		} else {}
		
		// The next few lines test the UART interface.  Uncomment the UARTE0 line below to test.
		// Echo the received character:  with a terminal window open on your PC,
		// you should see everything you type echoed back to the terminal window
		//USARTE0_TransmitByte(USARTE0_ReceiveByte());
	}
	return(0);
}

/********************************************************************************
*********************************************************************************
						Interrupt Service Routines
*********************************************************************************
********************************************************************************/

ISR(TWIE_TWIM_vect)  // TWIE Master Interrupt vector.
{
	TWI_MasterInterruptHandler(&twiMaster);
}
