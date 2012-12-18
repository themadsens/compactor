/**
 * (C) Copyright 2012 flemming.madsen at madsensoft.dk.
 **                                          MarkDown
 * Convert AWA & AWS to analog out
 * ===============================
 *
 * DEBUG: Echo NMEA to stdout
 *
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <avrx.h>
#include "trigint_sin8.h"
#include <AvrXFifo.h>
#include "hardware.h"
#include "Screen.h"
#include "util.h"

int16_t lastPulse;
int16_t windPulse;

void Task_WindOut(void)
{
	BSET(AWS_DDR, AWS_PIN);
	BeginCritical();
	// Fast PWM 8 bit
	TCCR1A = BV(COM1A1) | BV(COM1B1) | BV(WGM10);
	TCCR1B = BV(WGM12) | BV(CS11);
	// Enable PWM pins
	AWA_DDR |= BV(AWA_DDR_COS) | BV(AWA_DDR_SIN);
	EndCritical();

   for (;;) {
		delay_ms(80);
		//
		// Dimension for 5 ms pulses (100 Hz) at 60 Kts 
		// 60 kts - 100 revs /
		if (Nav_AWS < 5)
			windPulse = 0;
		else
			windPulse = 2000 / Nav_AWS;

		register int deg = Nav_AWA/10;
		BeginCritical();
		AWA_SIN = trigint_sin8u(deg * 45 + (deg * 51 / 100));
		deg = (90 - deg + 360) % 360;
		AWA_COS = trigint_sin8u(deg * 45 + (deg * 51 / 100));
		EndCritical();
	}
}

// vim: set sw=3 ts=3 noet nu:
