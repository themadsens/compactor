/**
 * (C) Copyright 2012 flemming.madsen at madsensoft.dk.
 **                                          MarkDown
 * UART Handler
 * ============
 */
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avrx.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hardware.h"
#include "util.h"
#include "SoftUart.h"
#include "Serial.h"

/**
 * --- Sample GPS
 * $GPRMC,152828.000,A,3646.8279,N,01432.7178,E,0.00,272.30,081212,,,A*68
 * $GPVTG,272.30,T,,M,0.00,N,0.0,K,A*09
 * $GPGGA,152829.000,3646.8280,N,01432.7178,E,1,09,0.9,0.2,M,39.0,M,,0000*59
 *
 * $GPRMC,152831.000,A,3646.8281,N,01432.7178,E,0.00,272.30,081212,,,A*67
 * $GPVTG,272.30,T,,M,0.00,N,0.0,K,A*09
 * $GPGGA,152832.000,3646.8282,N,01432.7179,E,1,09,0.9,-0.2,M,39.0,M,,0000*7D
 *
 * $GPRMC,152832.000,A,3646.8282,N,01432.7179,E,0.00,272.30,081212,,,A*66
 * $GPVTG,272.30,T,,M,0.00,N,0.0,K,A*09
 * $GPGGA,152833.000,3646.8282,N,01432.7180,E,1,09,0.9,-0.2,M,39.0,M,,0000*7A
 * $GPGSA,A,3,13,31,20,32,01,17,11,04,23,,,,1.8,0.9,1.5*37
 * $GPGSV,3,1,11,23,84,275,44,20,65,043,44,13,49,227,43,01,46,148,35*77
 * $GPGSV,3,2,11,32,38,071,31,17,30,271,38,11,25,159,39,04,20,314,36*79
 * $GPGSV,3,3,11,31,19,046,35,07,01,186,24,10,01,280,*42
 *
 * - Suppress redundant GPS messages on lower interface numbers.
 * - Mirror GPS messages from any hispeed on lospeed out
 * - Mirror Any message on hispeed1 on lospeed out
 * - Mirror all hi and lospeed messages on hispeed out
 * - Throttle repeat rate on message types on congestion
 *
 */


MessageQueue SerialQ;
static NmeaBuf *msg;
static TimerMessageBlock tmb;

void Task_Serial(void)
{
	uint8_t bit;
	for (;;) {
		
		AvrXStartTimerMessage(&tmb, 1000, &SerialQ);
		msg = (NmeaBuf *) AvrXWaitMessage(&SerialQ);
		if (msg == (NmeaBuf *) &tmb)
			continue;

		AvrXCancelTimerMessage(&tmb, &SerialQ);

		bit = 99;
#define DEF(a,b,c) ((a-1)<<14 | (b&0x7f)<<7 | (c&0x7f))


		switch (DEF(msg->buf[4], msg->buf[5], msg->buf[6])) {
		}

		// Move residual accumulated data thus far to front - Ie. Give buffer free
		BeginCritical();
		memmove(msg->buf, msg->buf + msg->off, msg->ix);
		msg->off = 0;
		EndCritical();

	}
}

// vim: set sw=3 ts=3 noet nu:
