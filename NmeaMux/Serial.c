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
#include "BigFifo.h"
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
 * Types on Summertime:
 *	 1. $WIMWV Wind speed / angle
 *	 2. $YXXDR Air temp
 *  3. $WIMDA Meteorological composite
 *  4. $SDDBT Depth below Transducer
 *  4. $SDDBS Depth below Surface
 *  6. $YXMTW Water temp
 *  7. $SDVHW Water speed
 *  8. $SDVLW Water log
 *  9. $IIHDM Heading magnetic
 * 10. $IIHDT Heading true
 * 11. $IIHDG Heading Unspecified
 * 12. $RRTLL Radar target lat lon
 * 13. $GPRMC GPS Recommended minimum
 * 14. $GPVTG GPS 
 * 15. $GPGGA GPS
 * 16. $GPGLL GPS
 * 17. $GPVTG GPS
 * 18. $GPZDA GPS
 * 19. $GPGSA GPS Satellite report
 * 20. $GPGSV GPS Satellite S/N ratios
 * 21. !AIVDM AIS Message
 * 22. !AIVDO AIS Own ship
 */

#define NMEATYPE(a,b,c) (((((a)-'A')&0x1f)<<10) | \
								 ((((b)-'A')&0x1f)<<5) | \
								 (((c)-'A')&0x1f))
#define GPS1 NMEATYPE('R','M','C')
#define GPS2 NMEATYPE('V','T','G')
#define GPS3 NMEATYPE('G','G','A')
#define GPS4 NMEATYPE('G','L','L')
#define GPS5 NMEATYPE('V','T','G')
#define GPS6 NMEATYPE('Z','D','A')
#define GPS7 NMEATYPE('G','S','A')
#define GPS8 NMEATYPE('G','S','V')
#define GPS_TRIG NMEATYPE('G','S','V')
#define IS_GPSTYPE(t) ((t)==GPS1 || (t)==GPS2 || (t)==GPS3 || (t)==GPS4 || \
                       (t)==GPS5 || (t)==GPS6 || (t)==GPS7 || (t)==GPS8)
#define IS_GPSTYPE3(a,b,c) IS_GPSTYPE(NMEATYPE((a),(b),(c)))
#define AIS1 NMEATYPE('V','D','M')
#define AIS2 NMEATYPE('V','D','O')
#define IS_AISTYPE(t) ((t)==AIS1 || (t)==AIS2)
#define HISPEED(m) (m->id < 3)
#define AISPORT(m) (m->id < 2)

#define HI_ONLY_PORT 2
#define HI_SPEEDCNV_PORT 1


typedef struct NmeaDef {
	uint16_t type;
	int16_t  when;
	uint8_t  where;
} NmeaDef;

#define NUM_SEEN 20
NmeaDef msgSeen[NUM_SEEN];
uint8_t gpsPort;
int16_t gpsLast;

MessageQueue SerialQ;
static NmeaBuf *msg;
static TimerMessageBlock tmb;
static uint16_t msgType;

static uint8_t xmitDecide(uint16_t type, uint8_t  where)
{
	register uint8_t i, xmit = 1;
	BeginCritical();
	register int16_t curTime = msTick;
	EndCritical();

	for (i = 0; i < NUM_SEEN; i++) {
		if (curTime -  msgSeen[i].when > 300)
			 msgSeen[i].when = 0;
	}
	if (0 == type)
		return 1;

	register uint16_t bufFree = BigFifoFree(pNmeaLoOut);
	for (i = 0; i < NUM_SEEN; i++) {
		register NmeaDef *m = msgSeen+i;
		if (type == m->type) {
			if (m->when &&
				 ((bufFree < 200 && curTime - m->when < 500) ||
			     (bufFree < 150 && curTime - m->when < 2000  && where > m->where) ||
			     (bufFree < 70  && curTime - m->when < 5000)))
				xmit = 0;
			else {
				m->when = curTime ?: 1;
				m->where = where;
			}
			break;
		}
	}
	if (NUM_SEEN == i) {
		for (i = 0; i < NUM_SEEN; i++) {
			if (0 == msgSeen[i].when) {
				msgSeen[i].type = type;
				msgSeen[i].when = curTime ?: 1;
				msgSeen[i].where = where;
				break;
			}
		}
	}
	return xmit;
}

void Task_Serial(void)
{
	for (;;) {
		
		AvrXStartTimerMessage(&tmb, 5000, &SerialQ);
		msg = (NmeaBuf *) AvrXWaitMessage(&SerialQ);
		if (msg == (NmeaBuf *) &tmb)
			continue;

		BeginCritical();
		register int16_t curTime = msTick;
		EndCritical();

		AvrXCancelTimerMessage(&tmb, &SerialQ);
		if (msg == (NmeaBuf *) &tmb) {
			if (curTime - gpsLast > 5000)
				gpsLast = 0;
			xmitDecide(0, 0); // Update timestamps
			continue;
		}

		if (HI_ONLY_PORT == msg->id) {
			xmitDecide(0, 0); // Update timestamps
			NmeaPutFifo(1, msg->buf+1);
			continue;
		}

		msgType = NMEATYPE(msg->buf[4], msg->buf[5], msg->buf[6]);
		if (GPS_TRIG == msgType) {
			if (!gpsLast || curTime - gpsLast > 5000)
				gpsPort = msg->id;
			gpsLast = curTime ?: 1;
		}
		if (IS_GPSTYPE(msgType)) {
			if (!gpsLast || curTime - gpsLast > 5000)
				gpsLast = 0;
			if (0 == gpsLast || gpsPort == msg->id) {
				NmeaPutFifo(0, msg->buf+1);
				NmeaPutFifo(1, msg->buf+1);
			}
			continue;
		}
		if (xmitDecide(msgType, msg->id))
			NmeaPutFifo(0, msg->buf+1);
		NmeaPutFifo(1, msg->buf+1);

		// Move residual accumulated data thus far to front - Ie. Give buffer free
		BeginCritical();
		memmove(msg->buf, msg->buf + msg->off, msg->ix);
		msg->off = 0;
		EndCritical();

	}
}

// vim: set sw=3 ts=3 noet nu:
