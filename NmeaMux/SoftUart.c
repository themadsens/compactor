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
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <stdlib.h>
#include <avrx.h>
#include <AvrXFifo.h>
#include "SoftUart.h"
#include "Serial.h"
#include "hardware.h"
#include "util.h"

#define NUMIN 3
#define NUMOUT 3

MessageQueue qOut;
static void *msgOut[NUMOUT];

int16_t actTimer;
static void bufPut(char *s, uint8_t c)
{
	register struct NmeaBuf *b = (struct NmeaBuf *) s;
#if 1
	if (0 == b->ix && c != LF)
		return;
	if (1 == b->ix && c != '$' && c != '!')  {
		b->ix = 0;
		return;
	}
	if (b->ix + b->off >= b->sz-1) {
		b->ix = 0; //reset
		return;
	}
#endif

	if (CR == c) {
		b->buf[b->ix++] = 0;
		b->off = b->ix;
		b->ix = 0;
		AvrXIntSendMessage(&SerialQ, (pMessageControlBlock) s);
		BCLR(LED_PORT, LED_ACT);
		actTimer = msTick ?: 1;
	}
	else
		b->buf[b->off + b->ix++] = c;
}

int16_t ovfTimer;
void NmeaPutFifo(uint8_t f, char *s)
{
	register pSysFifo pf = 0==f ? pNmeaLoOut : pNmeaHiOut;
	register char *e;

	for (e = s; *e; e++)
		;
	e[0] = CR;
	e[1] = LF;
	if (FIFO_ERR == SysPutFifoStr(pf, (uint8_t *) s, e - s + 2)) {
		puts("FIFO_ERR");
		BCLR(LED_PORT, LED_OVF);
		ovfTimer = msTick ?: 1;
	}
	*e = '\0'; // Restore end of string
	// msgOut offset is index into stOut[]
	AvrXSendMessage(&qOut, (pMessageControlBlock) msgOut + (0==f ? 1 : 0));
}

//
// IN
//

static struct SoftUartState 
{
	uint8_t state;
	uint8_t ch;
	char    *buf;
} stIn[NUMIN];
static uint8_t suEdgeTick[4];

static struct SoftUartState stOut[NUMOUT];

static char NmeaHiBuf[2][120];
static char NmeaLoBuf[NUMIN][120];

// Hispeed RX ready
AVRX_SIGINT(USART_RXC_vect)
{
	IntProlog();
	BSET(LED_PORT, LED_DBG);

	uint8_t ch = UDR;
	bufPut(NmeaHiBuf[0], ch);
	Epilog();
}

static inline void suartDo(struct SoftUartState *stIn, uint8_t pin)
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

		if (actTimer && msTick - actTimer > 50) {
			BSET(LED_PORT, LED_ACT);
			actTimer = 0;
		}
		if (ovfTimer && msTick - ovfTimer > 50) {
			BSET(LED_PORT, LED_OVF);
			ovfTimer = 0;
		}
	}
	
	register uint8_t pin;
	register uint8_t i = suTick;

	if (i > 1 && i < NUMOUT && stOut[i].state) {
		// Drive the output for 1&2, 0 is on the UART Tx

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
			AvrXSendMessage(&qOut, (pMessageControlBlock) msgOut + i);
			break;
		}
		if (1 == i)
			SBIT(SUART1_TXPORT, SUART1_TXPIN) = pin;
		else
			SBIT(SUART2_TXPORT, SUART2_TXPIN) = pin;
	}

	// Drive the three soft uarts
	for (i = 0; i < NUMIN; i++) {
		if (stIn[i].state && suTick == suEdgeTick[i]) {

			// The actual Soft UART
			switch (i) {
			 default: // Avoid "uninitialized pin" warning
			 case 0: pin = SBIT(SUART1_RXPORT, SUART1_RXPIN); break;
			 case 1: pin = SBIT(SUART2_RXPORT, SUART2_RXPIN); break;
			 case 2: pin = SBIT(SUART3_RXPORT, SUART3_RXPIN); break;
			}
			suartDo(&stIn[i], pin);

			if (0 == stIn[i].state) {
				// Re-enable edge trigger INT
				BSET(GICR, 0==i ? INT0 : 1==i ? INT1 : INT2);
			}
		}
	}
   Epilog();                   // Return to tasks
}

// Start a byte for the Lo-speed uart
ISR(INT0_vect)
{
	BSET(LED_PORT, LED_DBG);
	suEdgeTick[0] = (suTick+SUART_MULFREQ/2)%SUART_MULFREQ;
	stIn[0].state = 10;
	BCLR(GICR, INT0);
}
ISR(INT1_vect)
{
	BSET(LED_PORT, LED_DBG);
	suEdgeTick[1] = (suTick+SUART_MULFREQ/2)%SUART_MULFREQ;
	stIn[1].state = 10;
	BCLR(GICR, INT1);
}
ISR(INT2_vect)
{
	BSET(LED_PORT, LED_DBG);
	suEdgeTick[2] = (suTick+SUART_MULFREQ/2)%SUART_MULFREQ;
	stIn[2].state = 10;
	BCLR(GICR, INT2);
}

//
// OUT
//
static char      StdBufOut[100];
static pSysFifo pStdBufOut = (pSysFifo) StdBufOut;
static char      NmeaHiOut[200];
pSysFifo pNmeaHiOut = (pSysFifo) NmeaHiOut;
static char      NmeaLoOut[262];
pSysFifo pNmeaLoOut = (pSysFifo) NmeaLoOut;

// Hispeed TX ready
AVRX_SIGINT(USART_TXC_vect)
{
	IntProlog();

	int16_t ch = SysPullFifo(pNmeaHiOut);
	if (FIFO_ERR == ch) {
		BCLR(UCSRB, TXCIE);
		stOut[0].state = 0;
		AvrXIntSendMessage(&qOut, (pMessageControlBlock) msgOut + 0);
	}
	else
		UDR = ch;

	Epilog();
}

int out_putchar(char c, FILE *stream)
{
	register uint8_t i = stream==stderr ? 0:2;
	if (FIFO_ERR != SysPutFifo((pSysFifo) stOut[i].buf, c))
		// msgOut offset is index into stOut[]
		AvrXSendMessage(&qOut, (pMessageControlBlock) msgOut + i);
	return 0;
}

void Task_SoftUartOut(void)
{

	register uint8_t i;
	register void **msg;
	for (;;) {
		msg = (void **) AvrXWaitMessage(&qOut);
		i = msg - msgOut;

		if (0 == stOut[i].state &&
			 FIFO_ERR != SysPeekFifo((pSysFifo) stOut[i].buf))
		{
			stOut[i].state = 10;
			if (i > 0)
				// Lineup for a soft fifo
				stOut[i].ch = SysPullFifo((pSysFifo) stOut[i].buf);
			else {
				// Use the HW fifo
				if (SBIT(UCSRA, UDRE)) {
					UDR = SysPullFifo(pNmeaHiOut);
					BSET(UCSRB, TXCIE);                 //Enable TX empty Intr.
				}
			}
		}
	}
}

void SoftUartInInit(void)
{
	BSET(SUART1_RXPU, SUART1_RXPIN);
	BSET(SUART2_RXPU, SUART2_RXPIN);
	BSET(SUART3_RXPU, SUART3_RXPIN);

	NmeaHiBuf[0][2] = 1; NmeaHiBuf[0][3] = sizeof(NmeaHiBuf[0])-sizeof(NmeaBuf);
	NmeaHiBuf[1][2] = 2; NmeaHiBuf[1][3] = sizeof(NmeaHiBuf[1])-sizeof(NmeaBuf);
	NmeaLoBuf[0][2] = 3; NmeaLoBuf[0][3] = sizeof(NmeaLoBuf[0])-sizeof(NmeaBuf);
	NmeaLoBuf[1][2] = 4; NmeaLoBuf[1][3] = sizeof(NmeaLoBuf[1])-sizeof(NmeaBuf);
	NmeaLoBuf[2][2] = 5; NmeaLoBuf[2][3] = sizeof(NmeaLoBuf[2])-sizeof(NmeaBuf);
	stIn[0].buf = NmeaLoBuf[0];
	stIn[1].buf = NmeaLoBuf[1];
	stIn[2].buf = NmeaLoBuf[2];

#if 1
	// Extern Ints 0,1,2
	MCUCR |= BV(ISC11)|BV(ISC01);
	//BCLR(MCUCSR, ISC2);
	GICR |= BV(INT0)|BV(INT1)|BV(INT2);
#endif
}

void SoftUartOutInit(void) 
{
	stOut[0].buf = NmeaHiOut;
	SysFlushFifo(pNmeaHiOut);
	pNmeaHiOut->size = (uint16_t)(sizeof(NmeaHiOut) - sizeof(SysFifo) + 1);

	stOut[1].buf = NmeaLoOut;
	SysFlushFifo(pNmeaLoOut);
	pNmeaLoOut->size = (uint16_t)sizeof(NmeaLoOut) - sizeof(SysFifo) + 1;

	stOut[2].buf = StdBufOut;
	SysFlushFifo(pStdBufOut);
	pStdBufOut->size = (uint16_t)(sizeof(StdBufOut) - sizeof(SysFifo) + 1);

   OCR2 = TCNT2_TOP;
   BSET(TIMSK, OCIE2);
   TCCR2 = BV(CS21)|BV(WGM21); // F_CPU/8 - CTC Mode

   UCSRB = BV(TXEN)|BV(RXEN)|BV(RXCIE); //Enable Tx/Rx in UART + IEN

   // Timer2 4800 baud soft uart timer

	BSET(SUART1_TXPORT, SUART1_TXPIN);
	BSET(SUART2_TXPORT, SUART2_TXPIN);
	BSET(SUART1_TXDDR, SUART1_TXPIN);
	BSET(SUART2_TXDDR, SUART2_TXPIN);
}
//
// vim: set sw=3 ts=3 noet nu:
