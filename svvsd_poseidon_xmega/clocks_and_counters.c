/******************************************************************************
 * clocks_and_counters.c
 * 
 * The functions in this file perform several functions:
 *   1) Set the 256A3BU clock for high accuracy 32MHz
 *   2) Set the Timer/counters on PORT C to PWM periods and pulse widths
 *         to control Electronic Speed Controllers (ESCs) or Servos.
 *  3) Set up a timer/counter on PORT ? to enable a 1Hz interrupt...(NOT WRITTEN)
 * 
 * Created: 8/16/2017 11:04:10 AM
 * Author : Craig
 *
********************************************************************************/

/********************************************************************************
						Includes
********************************************************************************/
#include "poseidon.h"
#include "clocks_and_counters.h"
#include "motors.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <avr/interrupt.h>
//#include "xmega_uartd.h"
//#include <stdbool.h>
//#include <avr/eeprom.h>
//#include <stdio.h>
//#include <string.h>
//#include <avr/pgmspace.h>
//#include <asf.h>
//#include <util/twi.h>

/********************************************************************************
						Variables
********************************************************************************/
uint8_t ItsTime;


/********************************************************************************
						Functions
********************************************************************************/
void system_clock_init(void) {
	//*****************************************************************************************
	//
	//  Several things we have to do to set up the system clocks
	//	1 - Enable the external oscillator (32768Hz)
	//			(Used for the RTC and as a DFLL reference)
	//	2 - Hookup the RTC to the external oscillator
	//  3 - Enable the 32MHz internal clock
	//  4 - Set up the Digital Frequency-Locked-Loop (DFLL)
	//			(this improves frequency accuracy)
	// 
	// Enable the external oscillator...
	//		First we need to tell the chip what kind of oscillator is out there
	//		In our case, it is a high accuracy 32.768kHz oscillator
	//		Set low power mode and select 32.768kHz TOSC
	OSC_XOSCCTRL = OSC_FRQRANGE_04TO2_gc | OSC_X32KLPM_bm | OSC_XOSCSEL_32KHz_gc; 
	//		We need to enable the external oscillator in VBAT land, too.
	//		To do this, we need to first enable access to the registers...
	VBAT_CTRL |= VBAT_ACCEN_bm;
	//		Now enable the oscillator
	VBAT_CTRL |= VBAT_XOSCEN_bm;
	//		New we want to enable the oscillator in the OSC control register
	//		NOTE: Many OSC and CLK registers are protected, so writing to 
	//			the CCP register is required to unlock them.  See 7.9-7.11.
	CPU_CCP = CCP_IOREG_gc; // unlock critical IO registers for four clock cycles
	OSC_CTRL |= OSC_XOSCEN_bm; // Write the control register to enable external 32,768Hz clock
	//		Clock should be enabled.  Wait for it to stabilize...
	do {} while BitIsClear(OSC_STATUS,OSC_XOSCRDY_bp);
	//
	// Now we can hook up the RTC to the external oscillator...
	//		Select the external 32.768kHz external crystal oscillator for RTC source
	CPU_CCP = CCP_IOREG_gc; // unlock critical IO registers for four clock cycles
	CLK_RTCCTRL = CLK_RTCSRC_TOSC32_gc | CLK_RTCEN_bm;
	//
	// Now enable the 32MHz internal clock
	//		Enable the 32MHz internal clock in the Oscillator Control Register...
	CPU_CCP = CCP_IOREG_gc; // unlock critical IO registers for four clock cycles
	SetBit(OSC_CTRL, OSC_RC32MEN_bp); // Write the control register to enable internal 32MHz clock	
	//		and wait for the clock to come up to stabilize:
	do {} while BitIsClear(OSC_STATUS,OSC_RC32MRDY_bp);
	//
	// Now that it is enabled and stable, select it as the clock source...
	CPU_CCP = CCP_IOREG_gc; // unlock critical IO registers for four clock cycles
	CLK_CTRL = CLK_SCLKSEL_RC32M_gc; // Write the control register to select internal 32MHz clock
	//
	// Now, to improve accuracy, we want to enable the Digital Frequency 
	// Locked Loop (DFLL) for the 32MHz clock and using the external 32.768kHz
	// clock as the reference.  First, select the 32.768kHz external oscillator as the reference source
	OSC_DFLLCTRL = OSC_RC32MCREF_XOSC32K_gc;
	// Now enable the auto-calibration
	SetBit(DFLLRC32M_CTRL, DFLL_ENABLE_bp); // Enable auto-calibration
	// 
	// At this point, we should have all the new clocks set up.
	// Before returning, disable the 2MHz clock to save power
	CPU_CCP = CCP_IOREG_gc; // unlock critical IO registers for four clock cycles
	ClearBit(OSC_CTRL, OSC_RC2MEN_bp); // Clear the 2MHz enable bit to disable 2MHz clock	
	//
	// All done!
	//
}

void timer_counter_C0_C1_D0_init(uint16_t topcount) {
	//
	// This routine initializes the timer counters on PORT C to support driving
	// Electronic Speed Controllers (ESCs) via the single slope PWM operating mode
	//
	// From the XMEGA data sheet, to make the waveform visible on the
	// connected port pin, the following requirements must be fulfilled
	// 1. A waveform generation mode must be selected.
	// 2. Event actions must be disabled.
	// 3. The CC channels used must be enabled. This will override the
	//		corresponding port pin output register.
	// 4. The direction for the associated port pin must be set to output.
	// ...so, let's do it...
	// Set the data direction register for the PORT C PWM pins.
	// The output compare register pins are c0 through C5, set them to output:
	PORTC_DIR=0b00111111;
	// Camera servos on on port D pins 0, 1, and 2:
	PORTD_DIR=0b00000111;
	//
	// Set the clock prescaler to 64.  From table 14-3:
	TCC0_CTRLA = TC_CLKSEL_DIV64_gc;
	TCC1_CTRLA = TC_CLKSEL_DIV64_gc;
	TCD0_CTRLA = TC_CLKSEL_DIV64_gc;
	//
	// Enable the compare enable functions: See 14.12.2
	// Set WGM=0b011, which is single slope PWM.  See 14.12.2
	TCC0_CTRLB = TC0_CCAEN_bm | TC0_CCBEN_bm  | TC0_CCCEN_bm | TC0_CCDEN_bm | TC_WGMODE_SINGLESLOPE_gc;
	TCC1_CTRLB = TC1_CCAEN_bm | TC1_CCBEN_bm | TC_WGMODE_SINGLESLOPE_gc;
	TCD0_CTRLB = TC0_CCAEN_bm | TC0_CCBEN_bm | TC0_CCCEN_bm | TC0_CCDEN_bm | TC_WGMODE_SINGLESLOPE_gc;
	//
	// Ensure event actions are turned OFF
	TCC0_CTRLD = TC_EVACT_OFF_gc | TC_EVSEL_OFF_gc;
	TCC1_CTRLD = TC_EVACT_OFF_gc | TC_EVSEL_OFF_gc;
	TCD0_CTRLD = TC_EVSEL_OFF_gc | TC_EVACT_OFF_gc;
	//
	// Nothing to set in TCCx control registers C, and E.
	//
	// Ensure timer/counters are enabled (See 8.7.3)
	ClearBit(PR_PRPC, PR_TC0_bp); //Port C TC0
	ClearBit(PR_PRPC, PR_TC1_bp); //Port C TC1
	ClearBit(PR_PRPD, PR_TC0_bp); //Port D TC0
	//
	// Set the TOP count in the Input Compare Register PER registers
	// This is a 16 bit register, so we should block interrupts when
	// writing to it, but interrupts should not yet be enabled...
	//
	TCC1_PER = topcount;
	TCC0_PER = topcount;
	TCD0_PER = topcount;
	//
	// We now need to set the output compare registers to a value that will
	// prevent the motors from spinning.  To do this. we need calibrated values
	// from a dataset loaded somewhere...like EEPROM.  FOr now, we'll use the
	// defaults calculated in the global variables above.
	// Again, we should write these after disabling interrupts, but since this
	// should run BEFORE interrupts are enabled, we should be OK.
	TCC0_CCA = motor_1_neutral_setting;
	TCC0_CCB = motor_2_neutral_setting;
	TCC0_CCC = motor_3_neutral_setting;
	TCC0_CCD = motor_4_neutral_setting;
	TCC1_CCA = motor_5_neutral_setting;
	TCC1_CCB = motor_6_neutral_setting;
	//
	// Servos
	TCD0_CCA = servo_1_neutral_setting;
	TCD0_CCB = servo_2_neutral_setting;
	TCD0_CCC = servo_3_neutral_setting;
	// Timer Counters 0 and 1 should now be set up on PORTC
	// Timer Counter 0 should be set up on Port D
}

void timer_counter_C0_init(uint16_t topcount) {
	//
	// This routine is the testbed version of the light initialization. It uses pins 0-2 on port C.
	// *********This routine needs to be moved to port F timer counter 0 for production version. **************
	PORTC_DIR=0b00000111;
	//
	// Set the clock prescaler to 64. From table 14-3:
	TCC0_CTRLA = TC_CLKSEL_DIV64_gc;
	//
	// Enable the compare enable functions: See 14.12.2
	// Set WGM=0b011, which is single slope PWM.  See 14.12.2
	TCC0_CTRLB = TC0_CCAEN_bm | TC0_CCBEN_bm  | TC0_CCCEN_bm | TC0_CCDEN_bm | TC_WGMODE_SINGLESLOPE_gc;
	//
	// Ensure event actions are turned OFF
	TCC0_CTRLD = TC_EVSEL_OFF_gc | TC_EVACT_OFF_gc;
	//
	// Nothing to set in TCCx control registers C, and E.
	//
	// Ensure timer/counters are enabled for port C (See 8.7.3)
	ClearBit(PR_PRPC, PR_TC0_bp);
	//
	// Set the TOP count in the Input Compare Register PER registers
	// This is a 16 bit register, so we should block interrupts when
	// writing to it, but interrupts should not yet be enabled...
	//
	TCC0_PER = topcount;
	//
	// We now need to set the output compare registers to a value that will
	// prevent the motors from spinning.  To do this. we need calibrated values
	// from a dataset loaded somewhere...like EEPROM.  FOr now, we'll use the
	// defaults calculated in the global variables above.
	// Again, we should write these after disabling interrupts, but since this
	// should run BEFORE interrupts are enabled, we should be OK.
	
	//runs through each of the light cycles on all lights to test that all settings are working
	for (char i=0;i<3;i++)
	{
		TCC0_CCA = lights_off;
		TCC0_CCB = lights_off;
		TCC0_CCC = lights_off;
		_delay_ms(1000);
		TCC0_CCA = light_lowest_setting;
		TCC0_CCB = light_lowest_setting;
		TCC0_CCC = light_lowest_setting;
		_delay_ms(1000);
		TCC0_CCA = light_middle_setting;
		TCC0_CCB = light_middle_setting;
		TCC0_CCC = light_middle_setting;
		_delay_ms(1000);
		TCC0_CCA = light_highest_setting;
		TCC0_CCB = light_highest_setting;
		TCC0_CCC = light_highest_setting;
		_delay_ms(1000);
	}
	
	//sets the lights to the off setting for the final initialization state
	TCC0_CCA = lights_off;
	TCC0_CCB = lights_off;
	TCC0_CCC = lights_off;
	
	/*// We are assigning only one of each setting to each pin, however
	// the structure will have to be altered later so that each pin can access
	// each setting.
	TCF0_CCA = light_lowest_setting;
	TCF0_CCB = light_middle_setting;
	TCF0_CCC = light_highest_setting;
	//*/
	// Timer Counters 0 and 1 should now be set up on PORTC
}

void timer_counter_E0_init() {
	// This routine initializes the 1Hz interrupt.  It does not use any external pins.
	// THIS IS NOT YET WRITTEN>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	// System clock is 32,000,000Hz.  We want to trigeer every second.
	// Max count is 65535.  32M/65536=488.  If we divide the clock by 1024, 
	// we need the top count to be 31250 (0x7A12)
	// Set the clock prescaler to 1024. From table 14-3:
	TCE0_CTRLA = TC_CLKSEL_DIV1024_gc;
	//
	// Enable the compare enable functions: See 14.12.2
	// Set WGM=0b011, which is single slope PWM.  See 14.12.2
	TCE0_CTRLB = TC_WGMODE_SINGLESLOPE_gc;
	//
	// Ensure event actions are turned OFF
	TCE0_CTRLD = TC_EVSEL_OFF_gc | TC_EVACT_OFF_gc;
	//
	// Nothing to set in TCCx control registers C, and E.
	//
	// Enable timer interrupt / interrupt level
	TCE0_INTCTRLA = TC_OVFINTLVL_LO_gc;
	//
	// Ensure timer/counters are enabled for port C (See 8.7.3)
	ClearBit(PR_PRPC, PR_TC0_bp);
	//
	// Set the TOP count in the Input Compare Register PER registers
	// This is a 16 bit register, so we should block interrupts when
	// writing to it, but interrupts should not yet be enabled...
	//
	TCE0_PER = 31250;
	//
}

/********************************************************************************
*********************************************************************************
						Interrupt Service Routines
*********************************************************************************
********************************************************************************/


ISR(TCE0_OVF_vect)
{
	ToggleBit(xplained_yellow_LED_port, xplained_yellow_LED_1); //for now, just toggle the yellow status LED
	ItsTime = 1;
}
