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
#include <string.h>
#include "util.h"
#include "SoftUart.h"
#include "Screen.h"
#include "Serial.h"
#include "MultiMon.h"


MessageQueue SerialQ;

static uint8_t NmeaParse_P(const char *spec, const char *str, uint16_t *res)
{
	char c;
	uint8_t i = 0;
	uint8_t n = 0;

	while ((c = pgm_read_byte(spec++))) {
		switch (c) {
		 case ',':
			while (*str && *str != ',')
				str++;
			if (*str)
				str++;
			break;

		 case 'D':
		 case 'I':
			*res = 0;
			i = 0;
			while (isdigit(*str) || ('I'==c && '.'==*str)) {
				if ('.'==*str) {
					if(++i > 1) 
						break;
				}
				*res = *res * 10 + (*str - '0');
			}
			n++;
			res++;
			break;

		 case 'C':
			*res++ = *str++;
			n++;
			break;
		}
	}
	return n;
}

#define DEF(a,b,c) ((a-1)<<14 | (b&0x7f)<<7 | (c&0x7f))

static char *buf;
static int16_t windTick;
void Task_Serial(void)
{
	uint8_t bit = 0xff;
	for (;;) {
		static uint16_t num[5];
		static uint8_t n;
		
		buf = (char *) AvrXWaitMessage(&SerialQ);
		LED_ActivityMask |= BV(buf[2] - 1);

		switch (DEF(buf[2], buf[7], buf[8])) {
		 case DEF(1, 'M', 'W'):
			// Wind: AWA, AWS
		   //	$WIMWV,199.7,R,0.0,N,A

			n = NmeaParse_P(PSTR(",I,C,I,C"), buf+6, num);
			if (4 == n && 'R' == num[1] && 'N' == num[3]) {
				Nav_AWA = (num[0] + AvrXReadEEPromWord(&Cnfg_AWAOFF)) % 3600;
				Nav_AWS = muldiv(num[2], AvrXReadEEPromWord(&Cnfg_AWSFAC), 1000);
				bit = NAV_WIND;
				if ((int16_t)msTick - windTick > 350) {
					windTick = msTick;
					sprintf_P(buf+7, PSTR("MWV,%d.%d,R,%d.%d,N,A"), Nav_AWA/10, Nav_AWA%10,
								 Nav_AWS/10, Nav_AWS%10);
					NmeaPutFifo(0, buf+4);
				}
			}
			break;

		 case DEF(1, 'X', 'D'):
			// Wind: Air temp
		   //	$YXXDR,C,23.6,C,WND
			n = NmeaParse_P(PSTR(",,I,C"), buf+6, num);
			if (1 == n && 'C' == num[1]) {
				Nav_ATMP = num[0];
				sprintf_P(buf+7, PSTR("MDA,,,,,%d.%d,,,,,,,,,,,,,,,"), Nav_ATMP/10, Nav_ATMP%10);
				NmeaPutFifo(0, buf+4);
				bit = NAV_WIND;
			}
			break;

		 case DEF(2, 'D', 'B'):
		   // -- Triducer: Depth
		   // $SDDBT,14.0,f,4.2,M,2.3,F*34
			n = NmeaParse_P(PSTR(",,,I,C"), buf+6, num);
			if (1 == n && 'M' == num[1]) {
				Nav_DBS = num[0] + Cnfg_DBSOFF;
				sprintf_P(buf+7, PSTR("DBS,,,%d.%d,M,,"), Nav_DBS/10, Nav_DBS%10);
				NmeaPutFifo(0, buf+4);
				// Depth-as-waypoint to VDO Compass
				sprintf_P(buf+7, PSTR("BOD,,,,,DPT:%d.%dm,"), Nav_DBS/10, Nav_DBS%10);
				NmeaPutFifo(1, buf+4);
				bit = NAV_DEPTH;
			}
			break;

		 case DEF(2, 'M', 'T'):
		   // -- Triducer: Water temp
		   // $YXMTW,18.5,C*1E
			n = NmeaParse_P(PSTR(",,I,C"), buf+6, num);
			if (1 == n && 'C' == num[1]) {
				Nav_WTMP = num[0];
				NmeaPutFifo(0, buf+4);
				bit = NAV_DEPTH;
			}
			break;

		 case DEF(2, 'V', 'H'):
		   // -- Triducer: Water speed
		   // $SDVHW,,,,,8.7,N,15.2,K*hh<CR><LF>
			n = NmeaParse_P(PSTR(",,,,,I,C"), buf+6, num);
			if (1 == n && 'N' == num[1]) {
				Nav_STW = muldiv(num[0], Cnfg_STWFAC, 1000);
				sprintf_P(buf+7, PSTR("VHW,,,,,%d.%d,N,,"), Nav_STW/10, Nav_STW%10);
				NmeaPutFifo(0, buf+4);
				bit = NAV_DEPTH;
			}
			break;

		 case DEF(2, 'V', 'L'):
		   // -- Triducer: Log
		   // $SDVLW,999.9,N,777.7,N*hh
			n = NmeaParse_P(PSTR(",I,C"), buf+6, num);
			if (1 == n && 'N' == num[1]) {
				Nav_SUM = num[0];
				NmeaPutFifo(0, buf+4);
			}
			break;

		 case DEF(4,'G','S'):
			// GPS in panel
			if (buf[8] == 'A') {
				if (2 == NmeaParse_P(PSTR(",,D,,,,,,,,,,,,,I,"), buf+6, num)) {
					SatMODE = num[0];
					SatHDOP = num[1];
				}
			}
			if (buf[8] == 'V') {
				memset(num, 0, sizeof(num));
				if (NmeaParse_P(PSTR(",,D,,,,,D,,,,D,,,,D,,,,D,"), buf+6, num) > 2) {
					for (uint8_t i = 0; i < 4; i++)
						SatSNR[(num[0]-1)*4 + i] = num[i+1];
				}
			}
			// No "break" here
		 case DEF(4,'R','M'):
			NmeaPutFifo(0, buf+4);
			break;
		}
		if (bit < 8) {
			Nav_redraw |= BV(bit);
			if (curScreen == SCREEN_NAV) 
				ScreenUpdate();
		}
	}
}

// vim: set sw=3 ts=3 noet nu:
