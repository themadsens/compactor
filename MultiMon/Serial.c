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
#include "hardware.h"
#include "util.h"
#include "SoftUart.h"
#include "Screen.h"
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
 * --- Sample Wind
 * $YXXDR,C,11.1,C,WND
 * $WIMWV,14.2,R,0.0,N,A
 *
 */


MessageQueue SerialQ;

static uint8_t NmeaParse_P(const char *spec, const char *str, uint16_t *res)
{
	register char c;
	register uint8_t i = 0; // Bail out after first digit after '.'
	register uint8_t n = 0;
	register char *s;

	while ((c = pgm_read_byte(spec++))) {
		switch (c) {
		 case ',':
			while (*str && *str != ',')
				str++;
			if (*str)
				str++;
			break;

		 case 'F':  // Field
			for (s = (char *) *res; *str && ',' != *str; str++)
				*s++ = *str;
			*s++ = 0;
			res++;
			n++;
			break;

		 case 'd': // Digit field
			for (s = (char *) *res; *str && isdigit(*str); str++)
				*s++ = *str;
			*s++ = 0;
			res++;
			n++;
			break;

		 case 'L': //Lat / Lon
		   // $GPRMC,152828.000,A,3646.8279,N,01432.7178,E,0.00,272.30,081212,,,A*68
			i = 0;
			for (s = (char *) *res; *str && ',' != *str; str++) {
				if (i && ++i > 6) // Two decimals copied
					continue;
				*s++ = *str;
				if (str[3] == '.') { // Next is minutes
					*s++ = '\'';
					i++;
				}
			}
			str++;
			*s++ = *str++; // N,S,E,W indicator
			*s++ = 0;
			res++;
			n++;
			break;

		 case 'D': // Decimal
		 case 'I': // Fixed point Integer 1/10'ths
			*res = 0;
			i = 0;
			while (isdigit(*str) || ('I'==c && '.'==*str)) {
				if ('.'==*str) {
					if(i++ > 1) 
						break;
				}
				else
					*res = *res * 10 + (*str - '0');
				str++;
				if (i && i++ > 1)
					break;
			}
			if ('I'==c && i < 2)
				*res = *res * 10;
			n++;
			res++;
			break;

		 case 'C':
			*res = *str++;
			n++;
			res++;
			break;
		}
	}
	return n;
}

#define DEF(a,b,c) ((a-1)<<14 | (b&0x7f)<<7 | (c&0x7f))

static NmeaBuf *msg;
static TimerMessageBlock tmb;
static int16_t windTick;
static int16_t gsvTick;
static int16_t atmpTick;
static int16_t wtmpTick;
static int16_t dptTick;
static int16_t stwTick;
static int16_t sumTick;
static int16_t hdgTick;
static int16_t gsvTick;
void Task_Serial(void)
{
	uint8_t bit = 0xff;
	for (;;) {
		static uint16_t num[6];
		static uint8_t n;
		
		AvrXStartTimerMessage(&tmb, 1000, &SerialQ);
		msg = (NmeaBuf *) AvrXWaitMessage(&SerialQ);
		if (msg == (NmeaBuf *) &tmb)
			continue;

		AvrXCancelTimerMessage(&tmb, &SerialQ);
		LED_ActivityMask |= BV(msg->id - 1);

		if (msTick - windTick > 30000) {
			Nav_AWA = Nav_AWS = NO_VAL;
			bit = NAV_WIND;
		}
		if (msTick - atmpTick > 30000) {
			Nav_ATMP = NO_VAL;
			bit = NAV_ATMP;
		}
		if (msTick - dptTick > 30000) {
			Nav_DBS = NO_VAL;
			bit = NAV_DEPTH;
		}
		if (msTick - stwTick > 30000) {
			Nav_STW = NO_VAL;
			bit = NAV_HDG;
		}
		if (msTick - wtmpTick > 30000) {
			Nav_WTMP = NO_VAL;
			bit = NAV_DEPTH;
		}
		if (msTick - sumTick > 30000) {
			Nav_SUM = NO_VAL;
			bit = NAV_HDG;
		}
		if (msTick - hdgTick > 30000) {
			Nav_HDG = NO_VAL;
			bit = NAV_HDG;
		}
		if (msTick - gsvTick > 30000) {
			SatHDOP = NO_VAL;
			memset(SatSNR, 0, 12*sizeof(*SatSNR));
		}

		switch (DEF(msg->id, msg->buf[4], msg->buf[5])) {
		 case DEF(1, 'M', 'W'):
			// Wind: AWA, AWS
		   //	$WIMWV,199.7,R,0.0,N,A

			n = NmeaParse_P(PSTR(",I,C,I,C"), msg->buf, num);
			if (4 == n && 'R' == num[1] && 'N' == num[3]) {
				Nav_AWA = (num[0] + AvrXReadEEPromWord(&Cnfg_AWAOFF)) % 3600;
				Nav_AWS = muldiv(num[2], AvrXReadEEPromWord(&Cnfg_AWSFAC), 1000);
				windTick = msTick;
				sprintf_P(msg->buf+4, PSTR("MWV,%d.%d,R,%d.%d,N,A"),
							 Nav_AWA/10, Nav_AWA%10,
							 Nav_AWS/10, Nav_AWS%10);
				NmeaPutFifo(0, msg->buf+1);
				bit = NAV_WIND;
			}
			break;

		 case DEF(1, 'X', 'D'):
			// Wind: Air temp
		   //	$YXXDR,C,23.6,C,WND
			n = NmeaParse_P(PSTR(",,I,C"), msg->buf, num);
			if (2 == n && 'C' == num[1]) {
				atmpTick = msTick;
				Nav_ATMP = num[0];
				sprintf_P(msg->buf+4, PSTR("MDA,,,,,%d.%d,,,,,,,,,,,,,,,"), Nav_ATMP/10, Nav_ATMP%10);
				NmeaPutFifo(0, msg->buf+1);
				bit = NAV_ATMP;
			}
			break;

		 case DEF(2, 'D', 'B'):
		   // -- Triducer: Depth
		   // $SDDBT,14.0,f,4.2,M,2.3,F*34
			n = NmeaParse_P(PSTR(",,,I,C"), msg->buf, num);
			if (2 == n && 'M' == num[1]) {
				dptTick = msTick;
				Nav_DBS = num[0] + Cnfg_DBSOFF;
				sprintf_P(msg->buf+4, PSTR("DBS,,,%d.%d,M,,"), Nav_DBS/10, Nav_DBS%10);
				NmeaPutFifo(0, msg->buf);
				// Depth-as-waypoint to VDO Compass
				sprintf_P(msg->buf+4, PSTR("BOD,,,,,DPT:%d.%dm,"), Nav_DBS/10, Nav_DBS%10);
				NmeaPutFifo(1, msg->buf+1);
				bit = NAV_DEPTH;
			}
			break;

		 case DEF(2, 'M', 'T'):
		   // -- Triducer: Water temp
		   // $YXMTW,18.5,C*1E
			n = NmeaParse_P(PSTR(",,I,C"), msg->buf, num);
			if (2 == n && 'C' == num[1]) {
				wtmpTick = msTick;
				Nav_WTMP = num[0];
				NmeaPutFifo(0, msg->buf+1);
				bit = NAV_DEPTH;
			}
			break;

		 case DEF(2, 'V', 'H'):
		   // -- Triducer: Water speed
		   // $SDVHW,,,,,8.7,N,15.2,K*hh<CR><LF>
			n = NmeaParse_P(PSTR(",,,,,I,C"), msg->buf, num);
			if (1 == n && 'N' == num[1]) {
				stwTick = msTick;
				Nav_STW = muldiv(num[0], Cnfg_STWFAC, 1000);
				sprintf_P(msg->buf+4, PSTR("VHW,,,,,%d.%d,N,,"), Nav_STW/10, Nav_STW%10);
				NmeaPutFifo(0, msg->buf+1);
				bit = NAV_HDG;
			}
			break;

		 case DEF(2, 'V', 'L'):
		   // -- Triducer: Log
		   // $SDVLW,999.9,N,777.7,N*hh
			n = NmeaParse_P(PSTR(",I,C"), msg->buf+4, num);
			if (1 == n && 'N' == num[1]) {
				sumTick = msTick;
				Nav_SUM = num[0];
				NmeaPutFifo(0, msg->buf+1);
			}
			break;

		 case DEF(3, 'H', 'D'):
		   // -- Compass: Heading
		   // $IIHDM,999.9,M*hh
			n = NmeaParse_P(PSTR(",I,C"), msg->buf+4, num);
			if (1 == n && 'M' == num[1]) {
				hdgTick = msTick;
				Nav_HDG = num[0];
				NmeaPutFifo(0, msg->buf+1);
			}
			break;

		 case DEF(4, 'T', 'L'):
		   // -- Radar Target Lat Lon
		   // $--TLL,xx,llll.lll,a,yyyyy.yyy,a,c--c,hhmmss.ss,a,a*hh<CR><LF>
			NmeaPutFifo(0, msg->buf+1);
			break;

		 case DEF(5,'G','S'):
			// GPS in panel
			if (msg->buf[6] == 'A') {
				if (2 == NmeaParse_P(PSTR(",,D,,,,,,,,,,,,,,I,"), msg->buf+4, num)) {
					SatMODE = num[0];
					SatHDOP = num[1];
				}
			}
			if (msg->buf[6] == 'V') {
				memset(num, 0, sizeof(num));
				if (NmeaParse_P(PSTR(",,D,D,,,,D,,,,D,,,,D,,,,D,"), msg->buf+4, num) > 2 && num[0] <= 3) {
					for (uint8_t i = 0; i < 4; i++)
						SatSNR[(num[0]-1)*4 + i] = num[i+2];
					BSET(Gps_redraw, GPS_SAT);
					if (curScreen == SCREEN_GPS) 
						ScreenUpdate();
				}
			}
			gsvTick = msTick;
			NmeaPutFifo(0, msg->buf+1);
			break;
		 case DEF(5,'R','M'):
		   // $GPRMC,152828.000,A,3646.8279,N,01432.7178,E,0.00,272.30,081212,,,A*68
			if (msTick - gsvTick < 100) 
			 break;
			NmeaPutFifo(0, msg->buf+1);
			if (curScreen == SCREEN_GPS) {
				if (NmeaParse_P(PSTR(",d,,L,L,I,I"), msg->buf+4, (uint16_t *)Gps_STR) == 5) {
					ScreenUpdate();
					BSET(Gps_redraw, GPS_POS);
				}
			}
			break;
		}

		// Move residual accumulated data thus far to front - Ie. Give buffer free
		BeginCritical();
		memmove(msg->buf, msg->buf + msg->off, msg->ix);
		msg->off = 0;
		EndCritical();

		if (bit < 8) {
			Nav_redraw |= BV(bit);
			if (curScreen == SCREEN_NAV) 
				ScreenUpdate();
		}
	}
}

// vim: set sw=3 ts=3 noet nu:
