/**
 * (C) Copyright 2012 flemming.madsen at madsensoft.dk.
 **                                                     MarkDown
 * windlassRemote
 * ==============
 * Reads On, Up ^ Down buttons.
 * - On for 1 min after On press
 * - On signified by LED
 * - Up & Down drives a PNP -> Hexfet
 *
 * Timer0
 * ------
 * Make main do a roundtrip every milliSec
 *
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/cpufunc.h>
#include "util.h"

#if defined (__AVR_ATtiny13__)

#define CPU_FREQ (F_CPU / 8)
#define ON_PIN SBIT(PINB,  PB5)
#define ON_PU  SBIT(PORTB, PB5)
#define UP_PIN SBIT(PINB,  PB4)
#define UP_PU  SBIT(PORTB, PB4)
#define DN_PIN SBIT(PINB,  PB3)
#define DN_PU  SBIT(PORTB, PB3)
#define ON_OUT SBIT(PINB,  PB2)
#define ON_EN  SBIT(DDRB,  PB2)
#define UP_OUT SBIT(PINB,  PB1)
#define UP_EN  SBIT(DDRB,  PB1)
#define DN_OUT SBIT(PINB,  PB0)
#define DN_EN  SBIT(DDRB,  PB0)
#else
#error "Must be a ATtiny13"
#endif

#define TIMER0_TOP  (CPU_FREQ  / 8 / 1000 )

#include <util/delay.h>

// Maintained by timer interrupt. 
uint16_t upTime = 0;
uint16_t boot = 1;


ISR(TIM0_COMPA_vect)
{
    return;
}

uint8_t secCnt = 0;
uint8_t btnPressed = 0;
int main(void)
{
	ON_PU = 1;
	UP_PU = 1;
	DN_PU = 1;
	ON_EN = 1;
	UP_EN = 1;
	DN_EN = 1;

	ON_OUT = 0; // Light up for half a sec on boot (plugged in)

	// 1. Waveform Generation is CTC (Clear timer on compare) 
	// 2. Timer connected to main clock. Prescale = 8
	// 3. Timer0 running at baud rate  -- with interrupt enabled
	TCCR0A = BIT(WGM01);   // 1.
	TCCR0B = BIT(CS01);    // 2.
	TIMSK0 = BIT(OCIE0A);  // 3.
	OCR0A =  TIMER0_TOP;   // 3.

	set_sleep_mode(SLEEP_MODE_IDLE);
   sei();                       // Rock & roll
	while(1) {
		// Rest here until next timer overrun
		sleep_enable();
	   sleep_cpu();
		sleep_disable();

		secCnt++;

		if (!upTime) {

			if (0 == ON_PIN)
				btnPressed++;
			else
				btnPressed = 0;
			if (btnPressed > 50) {
				boot = 0;
				upTime = 1;
			}
			else if (boot && secCnt == 500) {
				ON_OUT = 1;
				boot = 0;
			}
			else if (!boot)
				ON_OUT = 1;
			continue;
		}

		ON_OUT = 0; // Lit the led

		if (0 == UP_PIN || 0 == DN_PIN)
			btnPressed++;
		else
			btnPressed = 0;

		if (btnPressed > 50) {
			if (0 == UP_PIN)
				UP_OUT = 1;
			else
				DN_OUT = 1;
		}
		else {
			UP_OUT = 1;
			DN_OUT = 1;
		}

		if (1000 == secCnt)
			secCnt = 0;
		else
			continue;

		upTime++;

		if (60 == upTime)
			upTime = 0; // Off
	}
}

// vim: set sw=3 ts=3 noet nu:
