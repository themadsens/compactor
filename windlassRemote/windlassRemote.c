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

#define CKDIV 8
#define CPU_FREQ (F_CPU / CKDIV)
#define ON_PIN SBIT(PINB,  PB3)
#define ON_PU  SBIT(PORTB, PB3)
#define UP_PIN SBIT(PINB,  PB4)
#define UP_PU  SBIT(PORTB, PB4)
#define DN_PIN SBIT(PINB,  PB5)
#define DN_PU  SBIT(PORTB, PB5)
#define ON_OUT SBIT(PORTB, PB2)
#define ON_EN  SBIT(DDRB,  PB2)
#define UP_OUT SBIT(PORTB, PB1)
#define UP_EN  SBIT(DDRB,  PB1)
#define DN_OUT SBIT(PORTB, PB0)
#define DN_EN  SBIT(DDRB,  PB0)
#else
#error "Must be a ATtiny13"
#endif

#define PRESCALE 8
#define TIMER0_TOP  (CPU_FREQ / PRESCALE / 1000 )

#include <util/delay.h>

// Maintained by timer interrupt. 
int16_t upTime = 0;
uint16_t boot = 1;


ISR(TIM0_COMPA_vect)
{
    return;
}

uint16_t secCnt = 0;
uint16_t btnPressed = 0;
int main(void)
{
	ON_PU = 1;
	UP_PU = 1;
	DN_PU = 1;
	ON_EN = 1;
	UP_EN = 0;
	DN_EN = 0;

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

		if (upTime <= 0) {
			UP_OUT = 0;
			DN_OUT = 0;
			UP_EN = 0;
			DN_EN = 0;

			if (upTime < 0 && 0 == ON_PIN)
				continue; // Wait for button release

			if (upTime == -2) {
				// Going ON
				upTime = 1;
				UP_OUT = 0;
				DN_OUT = 0;
				UP_EN = 1;
				DN_EN = 1;
				btnPressed = 0;
				continue;
			}

			upTime = 0;

			if (0 == ON_PIN && btnPressed < 9999)
				btnPressed++;
			else
				btnPressed = 0;
			if (btnPressed > 50) {
				boot = 0;
				upTime = -2;
				ON_OUT = 0; // Lit the led
			}
			else if (boot && secCnt == 500) {
				ON_OUT = 1;
				boot = 0;
			}
			else if (!boot)
				ON_OUT = 1;
			continue;
		}


		if (0 == UP_PIN || 0 == DN_PIN || 0 == ON_PIN) {
			if (btnPressed < 9999)
				btnPressed++;
		}
		else
			btnPressed = 0;

		if (btnPressed > 50) {
			if (0 == UP_PIN)
				UP_OUT = 1;
			else if (0 == DN_PIN)
				DN_OUT = 1;
			upTime = 1;
			if (0 == ON_PIN && btnPressed > 1000) {
				upTime = -1; // Off
				ON_OUT = 1; // Off the led
				continue;
			}
		}
		else {
			UP_OUT = 0;
			DN_OUT = 0;
		}

		if (secCnt >= 500 && upTime > 50)
			ON_OUT = 0; // Blink the led

		if (secCnt >= 1000)
			secCnt = 0;
		else
			continue;

		upTime++;
		//DN_OUT ^= 1;


		if (upTime >= 60) {
			upTime = 0; // Off
			ON_OUT = 1; // Off the led
		}
		else if (upTime > 50) {
			ON_OUT = 1; // Blink the led
		}
		else
			ON_OUT = 0; // Lit the led
	}
}

// vim: set sw=3 ts=3 noet nu:
