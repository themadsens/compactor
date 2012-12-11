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
#include <AvrXFifo.h>
#include "hardware.h"
#include "Screen.h"
#include "util.h"

int16_t lastPulse;
int16_t windPulse;

void Task_WindOut(void)
{
	BSET(AWS_DDR, AWS_PIN);
   for (;;) {
		delay_ms(90);
		// Dimension for 5 ms pulses (100 Hz) at 60 Kts 
		// 60 kts - 100 revs /
		if (Nav_AWS < 20)
			windPulse = 0;
		else
			windPulse = 3000 / Nav_AWS;

	}
}

// vim: set sw=3 ts=3 noet nu:
