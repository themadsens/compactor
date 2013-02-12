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
#include <BigFifo.h>
#include "SoftUart.h"
#include "Serial.h"
#include "hardware.h"
#include "util.h"
//
// IN
//

struct TimerEnt actTimer;
void actOff(struct TimerEnt *ent, void *inst)
{
	BSET(LED_PORT, LED_ACT);
}
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
		BCLR(LED_PORT, LED_ACT);
		ClrTimer(&actTimer, 1);
		AddTimer(&actTimer, 50, actOff, 0, 1);
	}
}

static struct SoftUartState 
{
	uint8_t state;
	uint8_t ch;
	char    *buf;
} stIn[4];
static uint8_t suEdgeTick[4];

static struct SoftUartState stOut[3];
static Mutex suOut;

static char NmeaHiBuf[2][99];
static char NmeaLoBuf[3][70];

static void SoftUartInInit(void)
{
	BSET(SUART1_RXPU, SUART1_RXPIN);
	BSET(SUART2_RXPU, SUART2_RXPIN);
	BSET(SUART3_RXPU, SUART3_RXPIN);
	BSET(SUARTHI_RXPU, SUARTHI_RXPIN);

	NmeaHiBuf[0][2] = 1; NmeaHiBuf[0][3] = sizeof(NmeaHiBuf[0])-sizeof(NmeaBuf);
	NmeaHiBuf[1][2] = 2; NmeaHiBuf[1][3] = sizeof(NmeaHiBuf[1])-sizeof(NmeaBuf);
	NmeaLoBuf[0][2] = 3; NmeaLoBuf[0][3] = sizeof(NmeaLoBuf[0])-sizeof(NmeaBuf);
	NmeaLoBuf[1][2] = 4; NmeaLoBuf[1][3] = sizeof(NmeaLoBuf[1])-sizeof(NmeaBuf);
	NmeaLoBuf[2][2] = 5; NmeaLoBuf[2][3] = sizeof(NmeaLoBuf[2])-sizeof(NmeaBuf);
	stIn[0].buf = NmeaLoBuf[0];
	stIn[1].buf = NmeaLoBuf[1];
	stIn[2].buf = NmeaLoBuf[2];
	stIn[3].buf = NmeaHiBuf[1];

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
	bufPut(NmeaHiBuf[0], ch);
	Epilog();
}

uint16_t vhfChanged;
uint8_t vhfLast;

static void inline suartDo(struct SoftUartState *stIn, uint8_t pin)
{
	stIn->state--;
	switch (stIn->state) {
	 case 9:
		if (pin == 0)     // start bit valid
			break;
		stIn->state = 0;
		break;

	 default:
		// LSB first
		stIn->ch = (stIn->ch >> 1) | (pin ? 0x80 : 0);
		break;

	 case 0:
		if (pin == 1) {   // stop bit valid
			bufPut(stIn->buf, stIn->ch);
		}
	}
}

// Hi speed timer for soft uart in & out at 4800 baud
uint8_t suTick;
AVRX_SIGINT(TIMER2_COMP_vect)
{
   IntProlog();                // Switch to kernel stack/context
	BSET(LED_PORT, LED_DBG);

	if (++suTick >= SUART_MULFREQ) {
		suTick = 0;

	}
	
	register uint8_t pin;
	register uint8_t i = suTick;

	if (i > 0 && i < 3 && stOut[i].state) {
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
		if (1 == i)
			SBIT(SUART1_TXPORT, SUART1_TXPIN) = pin;
		else
			SBIT(SUART2_TXPORT, SUART2_TXPIN) = pin;
	}

	// Drive the three soft uarts
	for (i = 0; i < 3; i++) {
		if (stIn[i].state && suTick == suEdgeTick[i]) {

			// The actual Soft UART
			switch (i) {
			 default: // Avoid "uninitialized pin" warning
			 case 0: pin = SBIT(SUART1_RXPORT, SUART1_RXPIN); break;
			 case 1: pin = SBIT(SUART2_RXPORT, SUART2_RXPIN); break;
			 case 2: pin = SBIT(SUART3_RXPORT, SUART3_RXPIN); break;
			}
			if (stIn[i].state == 99 && 0 == pin) {
				// SUART3 has no interrupt -> polled
				stIn[2].state = 10;
				suEdgeTick[2] = (suTick+SUART_MULFREQ/2)%SUART_MULFREQ;
				continue;
			}

			suartDo(&stIn[i], pin);

			if (0 == stIn[i].state) {
				// Re-enable edge trigger INT
				if (i == 2)
					stIn[i].state = 99;
				else
					BSET(GICR, 0==i ? INT0 : INT1);
			}
		}
	}
   Epilog();                   // Return to tasks
}

// Start a byte for the Lo-speed uart
AVRX_SIGINT(INT0_vect)
{
   IntProlog();                // Switch to kernel stack/context
	BSET(LED_PORT, LED_DBG);
	suEdgeTick[0] = (suTick+SUART_MULFREQ/2)%SUART_MULFREQ;
	stIn[0].state = 10;
	BCLR(GICR, INT0);
   Epilog();                   // Return to tasks
}
AVRX_SIGINT(INT1_vect)
{
   IntProlog();                // Switch to kernel stack/context
	BSET(LED_PORT, LED_DBG);
	suEdgeTick[1] = (suTick+SUART_MULFREQ/2)%SUART_MULFREQ;
	stIn[1].state = 10;
	BCLR(GICR, INT1);
   Epilog();                   // Return to tasks
}

// Start a byte for the Hi-speed semi-soft uart on timer1
#define ISR_LEAD 1
AVRX_SIGINT(INT2_vect)
{
   IntProlog();                // Switch to kernel stack/context
	BSET(LED_PORT, LED_DBG);

	// Scan again in 0.5 bit time. Scan in middle of bit
   TCNT1 = (SERIAL_TOP / 2) + ISR_LEAD;
	// Start timer
	BSET(TCCR1B, CS10);

	stIn[3].state = 10;
	BCLR(GICR, INT2);

   Epilog();                   // Return to tasks
}
AVRX_SIGINT(TIMER1_COMPA_vect)
{
   IntProlog();                // Switch to kernel stack/context
	BSET(LED_PORT, LED_DBG);
	suartDo(&stIn[3], SBIT(SUARTHI_RXPORT, SUARTHI_RXPIN));
	if (0 == stIn[3].state) {
		BSET(GICR, INT2);
		TCNT1 = 0;
	}
   Epilog();                   // Return to tasks
}

//
// OUT
//
static char      StdBufOut[150];
static pBigFifo pStdBufOut = (pBigFifo) StdBufOut;
static char      NmeaHiOut[400];
static pBigFifo pNmeaHiOut = (pBigFifo) NmeaHiOut;
static char      NmeaLoOut[500];
static pBigFifo pNmeaLoOut = (pBigFifo) NmeaLoOut;

// Hispeed TX ready
AVRX_SIGINT(USART_TXC_vect)
{
	IntProlog();

	int16_t ch = BigPullFifo(pNmeaHiOut);
	if (FIFO_ERR == ch) {
		BCLR(UCSRB, TXCIE);
		stOut[0].state = 0;
	}
	else
		UDR = ch;

	Epilog();
}

int dbg_putchar(char c, FILE *stream)
{
	if (FIFO_ERR != BigPutFifo(pStdBufOut, c))
		AvrXSetSemaphore(&suOut);
	return 0;
}

struct TimerEnt ovfTimer;
void ovfOff(struct TimerEnt *ent, void *inst)
{
	BSET(LED_PORT, LED_OVF);
}
void NmeaPutFifo(uint8_t f, char *s)
{
	register pBigFifo pf = 0==f ? pNmeaLoOut : pNmeaHiOut;
	register char *e;

	for (e = s; *e; e++)
		;
	*e++ = CR;
	*e++ = LF;
	if (FIFO_ERR == BigPutFifoStr(pf, (uint8_t *) s, e - s)) {
		puts("FIFO_ERR");
		BCLR(LED_PORT, LED_OVF);
		ClrTimer(&ovfTimer, 0);
		AddTimer(&ovfTimer, 50, ovfOff, 0, 0);
	}
	AvrXSetSemaphore(&suOut);
}

void Task_SoftUartOut(void)
{
   // Timer2 4800 baud soft uart timer
   OCR2 = TCNT2_TOP;
   BSET(TIMSK, OCIE2);
   TCCR2 = BV(CS21)|BV(WGM21); // F_CPU/8 - CTC Mode

   SoftUartInInit();

	BSET(TCCR1B, WGM12);
	OCR1A = SERIAL_TOP;

	BSET(SUART1_TXDDR, SUART1_TXPIN);
	BSET(SUART2_TXDDR, SUART2_TXPIN);

	stOut[0].buf = NmeaHiOut;
	BigFlushFifo(pNmeaHiOut);
	pNmeaHiOut->size = (uint8_t)(sizeof(NmeaHiOut) - sizeof(BigFifo) + 1);

	stOut[1].buf = NmeaLoOut;
	BigFlushFifo(pNmeaLoOut);
	pNmeaLoOut->size = (uint8_t)sizeof(NmeaLoOut) - sizeof(BigFifo) + 1;

	stOut[2].buf = StdBufOut;
	BigFlushFifo(pStdBufOut);
	pStdBufOut->size = (uint8_t)(sizeof(StdBufOut) - sizeof(BigFifo) + 1);

	register uint8_t i;
	for (;;) {
		AvrXWaitSemaphore(&suOut);
		for (i = 0; i < 3; i++) {
			if (0 == stOut[i].state &&
				 FIFO_ERR != BigPeekFifo((pBigFifo) stOut[i].buf))
			{
				stOut[i].state = 10;
				if (i > 0)
					// Lineup for a soft fifo
					stOut[i].ch = BigPullFifo((pBigFifo) stOut[i].buf);
				else {
					// Use the HW fifo
					if (SBIT(UCSRA, UDRE)) {
						UDR = BigPullFifo(pNmeaHiOut);
						BSET(UCSRB, TXCIE);                 //Enable TX empty Intr.
					}
				}
			}
		}
	}
}
// vim: set sw=3 ts=3 noet nu:
