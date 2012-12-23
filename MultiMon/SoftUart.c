/**
 * (C) Copyright 2012 flemming.madsen at madsensoft.dk.
 **                                          MarkDown
 *
 * Soft UART in & out
 * ==================
 * Mostly int driven stuff.
 * One task for pulling chars for the Out UART's
 */

#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <avrx.h>
#include <AvrXFifo.h>
#include "SoftUart.h"
#include "Serial.h"
#include "hardware.h"
#include "util.h"

//
// IN
//

static char WindBuf[50];
static char DepthBuf[50];
static char VdoBuf[40];
static char RdrBuf[60];
static char GpsBuf[99];

static void bufPut(char *s, uint8_t c)
{
	register struct NmeaBuf *b = (struct NmeaBuf *) s;
	if (0 == b->ix && c != LF)
		return;
	if (1 == b->ix && c != '$')  {
		b->ix = 0;
		return;
	}
	if (b->ix + b->off >= b->sz-1) {
		b->ix = 0; //reset
		return;
	}

	b->buf[b->off + b->ix++] = c;
	if (CR == c) {
		b->buf[b->ix++] = 0;
		b->off = b->ix;
		b->ix = 0;
		AvrXIntSendMessage(&SerialQ, (pMessageControlBlock) s);
	}
}

static struct SoftUartState 
{
	uint8_t state;
	uint8_t ch;
	char    *buf;
} stIn[4];
static uint8_t suEdgeTick[4];

static struct SoftUartState stOut[2];
static Mutex suOut;

static void SoftUartInInit(void)
{
	BSET(SUART1_RXPU, SUART1_RXPIN);
	BSET(SUART2_RXPU, SUART2_RXPIN);
	BSET(SUART3_RXPU, SUART3_RXPIN);
	BSET(SUART4_RXPU, SUART4_RXPIN);

	WindBuf[2] = 1; WindBuf[3] = sizeof(WindBuf)  - sizeof(NmeaBuf);
	DepthBuf[2]= 2; DepthBuf[3]= sizeof(DepthBuf) - sizeof(NmeaBuf);
	VdoBuf[2]  = 3; VdoBuf[3]  = sizeof(VdoBuf)   - sizeof(NmeaBuf);
	RdrBuf[2]  = 4; RdrBuf[3]  = sizeof(RdrBuf)   - sizeof(NmeaBuf);
	GpsBuf[2]  = 5; GpsBuf[3]  = sizeof(GpsBuf)   - sizeof(NmeaBuf);
	stIn[0].buf = WindBuf;
	stIn[1].buf = DepthBuf;
	stIn[2].buf = VdoBuf;

#if 1
	// Extern Ints 0,1,2
	MCUCR |= BV(ISC11)|BV(ISC01);
	//BCLR(MCUCSR, ISC2);
	GICR |= BV(INT0)|BV(INT1)|BV(INT2);
#endif
}

// Hispeed RX ready
AVRX_SIGINT(USART_RXC_vect)
{
	IntProlog();
	BSET(LED_PORT, LED_DBG);

	uint8_t ch = UDR;
	bufPut(GpsBuf, ch);
	Epilog();
}

static int16_t vhfOnTime;

// Hi speed timer for soft uart in & out at 4800 baud
uint8_t suTick;
AVRX_SIGINT(TIMER2_COMP_vect)
{
   IntProlog();                // Switch to kernel stack/context
	BSET(LED_PORT, LED_DBG);

	DoBuzzer();

	if (++suTick >= 3) {
		suTick = 0;

		if (SBIT(VHF_ON_RXPORT, VHF_ON_RXPIN))
			vhfOnTime = secTick;
	}
	
	register uint8_t pin;
	register uint8_t i = suTick;
	if (i < 2 && stOut[i].state) {
		// Drive the output

		stOut[i].state--;
		switch (stOut[i].state) {
		 case 9:
			pin = 0;
			break;

		 default:
			// LSB first
			pin = (stOut[i].ch & BV(8-stOut[i].state)) ? 1 : 0;
			break;

		 case 0:
			pin = 1;
			AvrXIntSetSemaphore(&suOut);
			break;
		}
		if (0 == i) {
			SBIT(SUART1_TXPORT, SUART1_TXPIN) = pin;
			if (abs(secTick - vhfOnTime) > 5)
				SBIT(VHF_PORT, VHF_UART) = pin;
		}
		else
			SBIT(SUART2_TXPORT, SUART2_TXPIN) = pin;
	}

	// Drive the three soft uarts
	for (i = 0; i < 4; i++) {
		if (stIn[i].state && suTick == suEdgeTick[i]) {

			// The actual Soft UART
			switch (i) {
			 default: // Avoid "uninitialized pin" warning
			 case 0: pin = SBIT(SUART1_RXPORT, SUART1_RXPIN); break;
			 case 1: pin = SBIT(SUART2_RXPORT, SUART2_RXPIN); break;
			 case 2: pin = SBIT(SUART3_RXPORT, SUART3_RXPIN); break;
			 case 3: pin = SBIT(SUART4_RXPORT, SUART4_RXPIN); break;
			}
			if (stIn[i].state == 99 && 0 == pin) { // SUART4 has no interrupt -> polled
				stIn[3].state = 10;
				suEdgeTick[3] = (suTick+1)%3;
				continue;
			}
			stIn[i].state--;
			switch (stIn[i].state) {
			 case 9:
				if (pin == 0)     // start bit valid
					break;
				stIn[i].state = 0;
				break;

			 default:
				// LSB first
				stIn[i].ch = (stIn[i].ch >> 1) | (pin ? 0x80 : 0);
				break;

			 case 0:
				if (pin == 1) {   // stop bit valid
					bufPut(stIn[i].buf, stIn[i].ch);
				}
			}
			if (0 == stIn[i].state) {
				// Re-enable edge trigger INT
				if (i == 3)
					stIn[i].state = 99;
				else
					GICR |= BV(0==i ? INT0 : 1==i ? INT1 : INT2);
			}
		}
	}
   Epilog();                   // Return to tasks
}

AVRX_SIGINT(INT0_vect)
{
   IntProlog();                // Switch to kernel stack/context
	BSET(LED_PORT, LED_DBG);
	suEdgeTick[0] = (suTick+1)%3;
	stIn[0].state = 10;
	BCLR(GICR, INT0);
   Epilog();                   // Return to tasks
}
AVRX_SIGINT(INT1_vect)
{
   IntProlog();                // Switch to kernel stack/context
	BSET(LED_PORT, LED_DBG);
	suEdgeTick[1] = (suTick+1)%3;
	stIn[1].state = 10;
	BCLR(GICR, INT1);
   Epilog();                   // Return to tasks
}
AVRX_SIGINT(INT2_vect)
{
   IntProlog();                // Switch to kernel stack/context
	BSET(LED_PORT, LED_DBG);
	suEdgeTick[2] = (suTick+1)%3;
	stIn[2].state = 10;
	BCLR(GICR, INT2);
   Epilog();                   // Return to tasks
}

//
// OUT
//
static char      NmeaFiOut[260];
static pAvrXFifo pNmeaFiOut = (pAvrXFifo) NmeaFiOut;
static char      NmIIFiOut[70];
static pAvrXFifo pNmIIFiOut = (pAvrXFifo) NmIIFiOut;

void NmeaPutFifo(uint8_t f, char *s)
{
	register pAvrXFifo pf = 0==f ? pNmeaFiOut : pNmIIFiOut;
	register uint8_t err = 0;

	while (*s && *s != CR)  {
		if (FIFO_ERR == AvrXPutFifo(pf, *s++))
			err = 1;
	}
	if (err)
		puts("FIFO_ERR");
	AvrXPutFifo(pf, CR);
	AvrXPutFifo(pf, LF);
	AvrXSetSemaphore(&suOut);
}

void Task_SoftUartOut(void)
{
   SoftUartInInit();

	BSET(SUART1_TXDDR, SUART1_TXPIN);
	BSET(SUART2_TXDDR, SUART2_TXPIN);
	BSET(VHF_ON_RXPU, VHF_ON_RXPIN);
	BSET(VHF_DDR, VHF_UART);

	stOut[0].buf = NmeaFiOut;
	AvrXFlushFifo(pNmeaFiOut);
	pNmeaFiOut->size = (uint8_t)(sizeof(NmeaFiOut) - sizeof(AvrXFifo) + 1);

	stOut[1].buf = NmIIFiOut;
	AvrXFlushFifo(pNmIIFiOut);
	pNmIIFiOut->size = (uint8_t)sizeof(NmIIFiOut) - sizeof(AvrXFifo) + 1;

	register uint8_t i;
	for (;;) {
		AvrXWaitSemaphore(&suOut);
		for (i = 0; i < 2; i++) {
			if (0 == stOut[i].state && FIFO_ERR!= AvrXPeekFifo((pAvrXFifo) stOut[i].buf)) {
				stOut[i].ch = AvrXPullFifo((pAvrXFifo) stOut[i].buf);
				stOut[i].state = 10;
			}
		}
	}
}
// vim: set sw=3 ts=3 noet nu:
