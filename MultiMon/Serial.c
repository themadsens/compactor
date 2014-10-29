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

		 case 'f': // Fractional part in 1/10000'ths
		 {
			register uint16_t fac = 1000;
			*res = 0;
			for (; *str && isdigit(*str); str++) {
				*res = (*str-'0') * fac;
				fac/=10;
			}
			res++;
			n++;
			break;
		 }

		 case 'd': // Digit field
			s = (char *) *res;
			if ('-' == *str)
				*s++ = *str;
			for (; *str && isdigit(*str); str++)
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
		 case 'i': // Fixed point Integer 1/100'ths
			*res = 0;
			i = 0;
			register int8_t sign = 1;
			if ('-' == *str) {
				sign = -1;
				str++;
			}
			while (isdigit(*str) || ('I'==toupper(c) && '.'==*str)) {
				if ('.'==*str) {
					if(i++ > ('i'==c ? 2 : 1)) 
						break;
				}
				else
					*res = *res * 10 + (*str - '0');
				str++;
				if (i && i++ > ('i'==c ? 2 : 1))
					break;
			}
			if (('I'==c && i < 2) || ('i'==c && i < 3))
				return n; // No dec point -> ERROR; Alternatively: *res = *res * 10;
			*res *= sign;
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

static uint8_t bitLen;
static void stuffBits(char *buf, int bits, long val)
{
	register uint8_t cur = 0;
	while (bits > 0) {
		register uint8_t rest = 6 - (bitLen%6);
		if (rest >= bits) {
			cur |= val>>(bits-rest);
			val &= 0xffff >> (32-bits);
			buf[bitLen/6] = cur + (cur > 32 ? 48 : 40);
			cur = 0;
			bitLen += rest;
		}
		else {
			cur |= val << (rest-bits);
			bitLen += bits;
		}
		bits -= rest;
	}
}

static uint16_t num[7];
static uint8_t n;

static long oldLat;
static long oldLon;
static uint16_t oldPosSec;
static inline void TllToVDM(char *buf)
{
	// $--TLL,xx,llll.lll,a,yyyyy.yyy,a,c--c,hhmmss.ss,a,a*hh<CR><LF>
	register long sog = 0;
	register long cog = 0;
	register long lat;
	register long lon;

	if (NmeaParse_P(PSTR(",,Df,C,Df,C"), buf+5, num) != 6)
		return;

	lat = num[0]/100 * 600000L + num[0]%100 * 10000 + num[1] * ('S'==num[2] ? -1 : 1);
	lon = num[3]/100 * 600000L + num[3]%100 * 10000 + num[4] * ('W'==num[5] ? -1 : 1);

	// TODO Calc sog,cog from old*.
	// See http://www.movable-type.co.uk/scripts/latlong.html

	strcpy_P(buf, PSTR("$AIVDM,1,1,,A,")),
	bitLen = 14*6;

	stuffBits(buf,  6, 1);          // 0..5     Type 1
	stuffBits(buf,  2, 0);          // 6..7     Repeat count
	stuffBits(buf, 30, 1234567890); // 8..37    MMSI
	stuffBits(buf,  4, 0);          // 38..41   Nav Status: Underway
	stuffBits(buf,  8, 0);          // 42..49   ROT
	stuffBits(buf, 10, sog);        // 50..59   SOG
	stuffBits(buf, 28, lon);        // 61..88   Pos lon
	stuffBits(buf, 27, lat);        // 89..115  Pos lat
	stuffBits(buf, 12, cog);        // 116..127 COG
	stuffBits(buf,  9, 0);          // 128..136 HDG
	stuffBits(buf,  6, 62);         // 137..142 Time -> Dead reckoning
	stuffBits(buf,  2, 0);          // 143..144 Maneuver N/A
	stuffBits(buf, 23, 0);          // 145..167 Spares

	oldLat = lat;
	oldLon = lon;
	oldPosSec = secTick;
}

static uint16_t m1,m2,m3;
static uint16_t medianFilter(uint16_t v)
{
	m1 = m2;
	m2 = m3;
	m3 = v;

	if ((m1 > m2 && m1 < m3) || (m1 < m2 && m1 > m3))
		return m1;
	else if ((m2 > m1 && m2 < m3) || (m2 < m1 && m2 > m3))
		return m2;
	else
		return m3;
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
static inline void NmeaGsvFifo(uint8_t f, char *s)
{
	if (msTick - gsvTick > 300 || msTick - gsvTick < 0) 
		NmeaPutFifo(f, s);
}

void Task_Serial(void)
{
	uint8_t bit;
	for (;;) {
		
		AvrXStartTimerMessage(&tmb, 1000, &SerialQ);
		msg = (NmeaBuf *) AvrXWaitMessage(&SerialQ);
		if (msg == (NmeaBuf *) &tmb)
			continue;

		AvrXCancelTimerMessage(&tmb, &SerialQ);
		LED_ActivityMask |= BV(msg->id - 1);

		bit = 0;

		if (msTick - windTick > 5000) {
			Nav_AWA = Nav_AWS = NO_VAL;
			bit |= BV(NAV_WIND);
		}
		if (msTick - atmpTick > 5000) {
			Nav_ATMP = NO_VAL;
			bit |= BV(NAV_ATMP);
		}
		if (msTick - dptTick > 5000) {
			Nav_DBS = NO_VAL;
			bit |= BV(NAV_DEPTH);
		}
		if (msTick - stwTick > 5000) {
			Nav_STW = NO_VAL;
			bit |= BV(NAV_HDG);
		}
		if (msTick - wtmpTick > 5000) {
			Nav_WTMP = NO_VAL;
			bit |= BV(NAV_DEPTH);
		}
		if (msTick - sumTick > 5000) {
			Nav_SUM = NO_VAL;
			bit |= BV(NAV_HDG);
		}
		if (msTick - hdgTick > 5000) {
			Nav_HDG = NO_VAL;
			bit |= BV(NAV_HDG);
		}
		if (msTick - gsvTick > 5000) {
			SatHDOP = NO_VAL;
			memset(SatSNR, 0, 12*sizeof(*SatSNR));
		}
		if (bit) {
			Nav_redraw |= bit;
			if (curScreen == SCREEN_NAV) 
				ScreenUpdate();
		}
		if (msg == (NmeaBuf *) &tmb)
			continue;

		bit = 99;

		switch (DEF(msg->id, msg->buf[4], msg->buf[5])) {
		 case DEF(1, 'M', 'W'):
			// Wind: AWA, AWS
		   //	$WIMWV,199.7,R,0.0,N,A

			n = NmeaParse_P(PSTR(",I,C,I,C"), msg->buf, num);
			if (4 == n && 'R' == num[1] && 'N' == num[3]) {
				Nav_AWA = (num[0] + (AvrXReadEEPromWord(&Cnfg_AWAOFF)*10)) % 3600;
				Nav_AWS = muldiv(num[2], AvrXReadEEPromWord(&Cnfg_AWSFAC), 1000);
				if (abs(msTick - windTick) >= 250) {
					sprintf_P(msg->buf+4, PSTR("MWV,%d.%d,R,%d.%d,N,A"),
								 Nav_AWA/10, Nav_AWA%10,
								 Nav_AWS/10, Nav_AWS%10);
					NmeaPutFifo(0, msg->buf+1);
					windTick = msTick;
					bit = NAV_WIND;

					uint16_t f = medianFilter(Nav_AWS);
					if (f > maxWind[0])
						maxWind[0] = f;
					if (f > maxWindCur) {
						maxWindCur = f;
						maxWindAgo = 0;
					}
				}
			}
			break;

		 case DEF(1, 'X', 'D'):
			// Wind: Air temp
		   //	$YXXDR,C,23.6,C,WND
			n = NmeaParse_P(PSTR(",,I,C"), msg->buf, num);
			if (2 == n && 'C' == num[1]) {
				atmpTick = msTick;
				Nav_ATMP = num[0];
				sprintf_P(msg->buf+2, PSTR("WIMDA,,,,,%d.%d,C,,,,,,,,,,,,,,"), Nav_ATMP/10, Nav_ATMP%10);
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
				Nav_DBS = num[0] + (AvrXReadEEPromWord(&Cnfg_DBSOFF)+5)/10;
				sprintf_P(msg->buf+4, PSTR("DPT,%d.%d,"), Nav_DBS/10, Nav_DBS%10);
				NmeaPutFifo(0, msg->buf+1);
				// Depth-as-waypoint to VDO Compass
				sprintf_P(msg->buf+4, PSTR("HSC,%d.0,T,,,"), Nav_DBS);
				NmeaPutFifo(1, msg->buf+1);
				bit = NAV_DEPTH;
			}
			break;

		 case DEF(2, 'M', 'T'):
		   // -- Triducer: Water temp
		   // $YXMTW,18.5,C*1E
			n = NmeaParse_P(PSTR(",I,C"), msg->buf, num);
			if (2 == n && 'C' == num[1]) {
				wtmpTick = msTick;
				Nav_WTMP = num[0];
				NmeaPutFifo(0, msg->buf+1);
				bit = NAV_DEPTH;
			}
			break;

		 case DEF(2, 'V', 'H'):
		   // -- Triducer: Water speed
		   // $VWVHW,,T,,M,2.98,N,5.51,K*hh<CR><LF>
			n = NmeaParse_P(PSTR(",,,,,i,C,i,C"), msg->buf, num);
			if (4 == n && 'N' == num[1]) {
				stwTick = msTick;
				Nav_STW = muldiv(num[0], Cnfg_STWFAC, 1000);
				uint16_t kmh = muldiv(num[0], Cnfg_STWFAC, 1000);
				sprintf_P(msg->buf+4, PSTR("VHW,,T,,M,%d.%d,N,%d.%d,K"),
							 Nav_STW/100, Nav_STW%100,
							 kmh/100, kmh%100);
				NmeaPutFifo(0, msg->buf+1);
				bit = NAV_HDG;
			}
			break;

		 case DEF(2, 'V', 'L'):
		   // -- Triducer: Log
		   // $SDVLW,999.9,N,777.7,N*hh
			n = NmeaParse_P(PSTR(",I,C"), msg->buf+4, num);
			if (2 == n && 'N' == num[1]) {
				sumTick = msTick;
				Nav_SUM = num[0];
				NmeaPutFifo(0, msg->buf+1);
				bit = NAV_HDG;
			}
			break;

		 case DEF(3, 'H', 'D'):
		   // -- Compass: Heading
		   // $IIHDM,999.9,M*hh
			n = NmeaParse_P(PSTR(",I,C"), msg->buf+4, num);
			if (n < 1) {
				n = NmeaParse_P(PSTR(",D,C"), msg->buf+4, num);
				if (2 == n && 'M' == num[1]) {
					num[0] *= 10;
				}
			}
			if (2 == n && 'M' == num[1]) {
				hdgTick = msTick;
				Nav_HDG = num[0];
				sprintf_P(msg->buf+4, PSTR("HDG,%d.%d,,,,"), Nav_HDG/10, Nav_HDG%10);
				NmeaPutFifo(0, msg->buf+1);
				bit = NAV_HDG;
			}
			break;

		 case DEF(4, 'T', 'L'):
		   // -- Radar Target Lat Lon
		   // $--TLL,xx,llll.lll,a,yyyyy.yyy,a,c--c,hhmmss.ss,a,a*hh<CR><LF>
			TllToVDM(msg->buf);
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
					if (curScreen == SCREEN_GPS || curScreen == SCREEN_ANCH) 
						ScreenUpdate();
				}
			}
			gsvTick = msTick;
			NmeaPutFifo(2, msg->buf+1);
			break;
		 case DEF(5,'R','M'):
		   // $GPRMC,152828.000,A,3646.8279,N,01432.7178,E,0.00,272.30,081212,,,A*68
			NmeaGsvFifo(2, msg->buf+1);
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
