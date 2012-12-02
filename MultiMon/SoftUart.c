/**
 * (C) Copyright 2012 flemming.madsen at madsensoft.dk.
 **                                          MarkDown
 *
 * Soft UART in & out
 * ==================
 */

#include <avr/io.h>
#include <avrx.h>
#include <AvrXFifo.h>
#include "SoftUart.h"
#include "Serial.h"
#include "hardware.h"
#include "util.h"

//
// IN
//

// 0,1: Queue next pointer. 2: Index. 2: Length

uint8_t WindBuf[50];
uint8_t GpsBuf[70];
uint8_t MuxBuf[70];
uint8_t DepthBuf[50];

static void bufPut(uint8_t *s, uint8_t c)
{
	if (4 == s[3] && c != LF)
		return;
	if (5 == s[3] && c != '$')  {
		s[3] = 4;
		return;
	}

	s[s[3]++] = c;
	if (CR == c) {
		s[s[3]++] = 0;
		s[3] = 4; // Length reset to 0
		AvrXSendMessage(&SerialQ, (pMessageControlBlock) s);
	}
}

char BitFifoBuf[sizeof(AvrXFifo) + 5];
pAvrXFifo BitFifo = (pAvrXFifo) BitFifoBuf;
void SoftUartFifo(uint8_t spec)
{
	AvrXPutFifo(BitFifo, spec);
}

struct SoftUartState 
{
	uint8_t state;
	uint8_t ch;
	uint8_t *buf;
} stIn[4];
uint8_t suEdgeTick[3];

void Task_SoftUartIn(void)
{
	uint8_t i;

	SBIT(SUART1_RXPU, SUART1_RXPIN);
	SBIT(SUART2_RXPU, SUART2_RXPIN);
	SBIT(SUART3_RXPU, SUART3_RXPIN);

	WindBuf[2] = 1; WindBuf[3] = 4;
	DepthBuf[2]= 2; DepthBuf[3]= 4;
	MuxBuf[2]  = 3; MuxBuf[3]  = 4;
	GpsBuf[2]  = 4; GpsBuf[3]  = 4;
	stIn[0].buf = WindBuf;
	stIn[1].buf = DepthBuf;
	stIn[2].buf = MuxBuf;
	for (i = 0; i < 3; i++) 
		stIn[i].state = 10;

	for (;;) {
		uint8_t spec = AvrXWaitPullFifo(BitFifo);

		uint8_t i;
		if (0xff == spec) {
			bufPut(GpsBuf, AvrXPullFifo(BitFifo));
		}
		else {
			uint8_t tck = spec & 0xf;
			for (i = 0; i < 3; i++) {
				if ((tck+3) % 6 == suEdgeTick[i] - 1) {

					// The actual Soft UART
					uint8_t pin = spec & (1<<(i+4));
					switch (stIn[i].state--) {
					 case 9:
						if (pin == 0)     // start bit valid
							break;
						stIn[i].state = 0;
						break;

					 default:
						// LSB first
						stIn[i].ch = (stIn[i].ch >> 1) | (spec&(1<<i)) ? 0x80 : 0;
						break;

					 case 0:
						if (pin == 1) {   // stop bit valid
							bufPut(stIn[i].buf, stIn[i].ch);
						}
					}
					if (!stIn[i].state)
					{
						// Re-enable edge trigger INT
						GICR |= BV(0==i ? INT0 : 1==i ? INT1 : INT2);
						// Ready for next time
						stIn[i].state = 10;
						suEdgeTick[i] = 0;
					}
				}
			}
		}
	}
}

//
// OUT
//
uint8_t NmeaFiOut[400];
pAvrXFifo pNmeaFiOut = (pAvrXFifo) NmeaFiOut;
uint8_t NmIIFiOut[56];
pAvrXFifo pNmIIFiOut = (pAvrXFifo) NmIIFiOut;

uint8_t CurUartOut;
uint8_t RadioOn;

Mutex suOut;
void SoftUartOutKick(void )
{
	AvrXSetSemaphore(&suOut);
}

void NmeaPutFifo(uint8_t f, char *s)
{
	pAvrXFifo pf = 0==f ? pNmeaFiOut : pNmIIFiOut;

	CurUartOut |= BV(f);
	while (*s && *s != CR) 
		AvrXPutFifo(pf, *s++);
	AvrXPutFifo(pf, CR);
	AvrXPutFifo(pf, LF);
}

struct SoftUartState stOut[2];
void Task_SoftUartOut(void)
{

	SBIT(SUART1_TXDDR, SUART1_TXPIN);
	SBIT(SUART2_TXDDR, SUART2_TXPIN);
	SBIT(SUART3_TXDDR, SUART3_TXPIN);
	stOut[0].buf = NmeaFiOut;
	stOut[1].buf = NmIIFiOut;

	static uint8_t ix;
	for (;;) {
		AvrXWaitSemaphore(&suOut);
		ix = ix+1 % 2;

		if (0 == stOut[ix].state) {
			if (FIFO_ERR != AvrXPeekFifo((pAvrXFifo) stOut[ix].buf)) {
				stOut[ix].ch = AvrXPullFifo((pAvrXFifo) stOut[ix].buf);
				stOut[ix].state = 10;
			}
			else
				CurUartOut &= ~BV(ix);
		}
		switch (stOut[ix].state--) {
		 case 9:
			// Bit clr'd already
			break;

		 default:
			// LSB first
			SBIT(ix, 7) = stIn[ix].ch & 1;
			stIn[ix].ch >>= 1;
			break;

		 case 0:
			BSET(ix, 7);
		}
		if (ix) {
			SBIT(SUART2_TXPORT, SUART1_TXPIN) = ix >> 7;
			if (0 == SBIT(VHFSENSE_PORT, VHFSENSE_PIN))
				SBIT(SUART3_TXPORT, SUART1_TXPIN) = ix >> 7;
		}
		else
			SBIT(SUART1_TXPORT, SUART1_TXPIN) = ix >> 7;

		BCLR(ix, 7);
	}
}

// vim: set sw=3 ts=3 noet nu:
