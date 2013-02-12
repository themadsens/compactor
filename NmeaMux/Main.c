/**
 * (C) Copyright 2012 flemming.madsen at madsensoft.dk.
 **                                          MarkDown
 *
 * MultiMon main
 * =============
 *
 * * Initialize HW
 * * Start tasks
 * * Service most interrupts
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#define AVRX_USER_PROCESS_DATA struct TimerControlBlock processDelay
#include <avrx.h>
#include <AvrXFifo.h>

#include "Serial.h"
#include "SoftUart.h"
#include "util.h"
#include "hardware.h"

#define BAUD 38400
#include <util/setbaud.h>

#define TICKRATE 1000       // AvrX timer queue 1ms resolution
#define TCNT0_TOP (F_CPU/64/TICKRATE)

static FILE mystdout = FDEV_SETUP_STREAM(dbg_putchar, NULL, _FDEV_SETUP_WRITE);

int16_t msTick;
int16_t msAge;
uint16_t secTick;
uint16_t hdayTick;
// Use Timer0 for the 1/1000s tick rate
AVRX_SIGINT(TIMER0_COMP_vect)
{
   IntProlog();                // Switch to kernel stack/context
	BSET(LED_PORT, LED_DBG);
	msTick++;
	if (msTick - msAge >= 1000) {
		msAge = msTick;
		secTick++;
		if (secTick >= (uint16_t)3600*12) {
			secTick = 0;
			hdayTick++;
		}
	}
   AvrXTimerHandler();         // Call Time queue manager
   Epilog();                   // Return to tasks
}

struct TimerEnt *TimerQ;

MessageQueue TimerWQ;
static pMessageControlBlock msg;
static TimerMessageBlock dly;
static void *intr;
uint16_t curTimerStart;

static uint16_t waited;

void Task_Timer(void)
{
	for (;;) {
		BeginCritical();
		if (TimerQ) {
			curTimerStart = msTick;
			AvrXStartTimerMessage(&dly, TimerQ->count, &TimerWQ);
		}
		curTimerStart = msTick;
		EndCritical();
		msg = AvrXWaitMessage(&TimerWQ);

		BeginCritical();
		waited = msTick - curTimerStart;
		for (struct TimerEnt *cur = TimerQ; cur; cur = cur->next) {
			if (cur->count > waited)
				cur->count -= waited;
			else
				cur->count = 0;
		}


		if (msg == (pMessageControlBlock) &intr) {
			AvrXCancelTimerMessage(&dly, &TimerWQ);
			EndCritical();
			continue;
		}
		if (TimerQ) {
			TimerQ->handler(TimerQ, TimerQ->instanceP);
			TimerQ = TimerQ->next;
		}
		EndCritical();
	}
}

void AddTimer(struct TimerEnt *ent, uint16_t count,
				  TimerHandler handler, void *instanceP, uint8_t isInt)
{
	ent->count = count;
	ent->handler = handler;
	ent->instanceP = instanceP;
	ent->next = NULL;
	BeginCritical();
	if (!TimerQ) {
		TimerQ = ent;
	}
	else {
		struct TimerEnt *prev = NULL;
		for (struct TimerEnt *cur = TimerQ; cur; cur = cur->next) {
			if (cur->count > ent->count) {
				ent->next = cur;
				if (cur == TimerQ)
					TimerQ = ent;
				else
					prev->next = ent;
				break;
			}
			prev = cur;
		}
	}
	EndCritical();

	if (isInt)
		AvrXIntSendMessage(&TimerWQ, (pMessageControlBlock)&intr);
	else
		AvrXSendMessage(&TimerWQ, (pMessageControlBlock)&intr);
}

void ClrTimer(struct TimerEnt *ent, uint8_t isInt)
{
	if (TimerQ == ent) {
		TimerQ = ent->next;
		if (isInt)
			AvrXIntSendMessage(&TimerWQ, (pMessageControlBlock)&intr);
		else
			AvrXSendMessage(&TimerWQ, (pMessageControlBlock)&intr);
		return;
	}
	for (struct TimerEnt *cur = TimerQ; cur; cur = cur->next) {
		if (cur->next == ent) {
			cur->next = ent->next;
			return;
		}
	}
}

// Task declarations -- Falling priority order
AVRX_GCC_TASK( Task_SoftUartOut,  20,  1);
AVRX_GCC_TASK( Task_Serial,      100,  4);
AVRX_GCC_TASK( Task_Timer,        20,  4);

#define RUN(T) (AvrXRunTask(TCB(T)))


int main(void) 
{
 	AvrXSetKernelStack(0);

	set_sleep_mode(SLEEP_MODE_IDLE);
	BSET(ACSR, ACD);           // Save a little power

   // Timer0 heartbeat - CTC mode ensures equidistant ticks
   OCR0 = TCNT0_TOP;
   BSET(TIMSK, OCIE0);
   TCCR0 = BV(CS00)|BV(CS01)|BV(WGM01); // F_CPU/64 - CTC Mode

   // USART Baud rate etc
   UBRRH = UBRRH_VALUE;
   UBRRL = UBRRL_VALUE;
#if USE2X
   UCSRA |= BV(U2X);			       //Might double the UART Speed
#endif
   UCSRB = BV(RXEN)|BV(RXCIE);	 //Enable Rx in UART + IEN
   UCSRC = BV(UCSZ0)|BV(UCSZ1)|BV(URSEL);  //8-Bit Characters

   stdout = &mystdout; //Required for printf init

	BSET(LED_PORT, LED_DBG);
	BSET(LED_PORT, LED_ACT);
	BSET(LED_PORT, LED_OVF);
	BSET(LED_DDR, LED_DBG);
	BSET(LED_DDR, LED_ACT);
	BSET(LED_DDR, LED_OVF);

	// Initially free -- AvrX should do this !!
	extern Mutex EEPromMutex;
	AvrXSetSemaphore(&EEPromMutex);

	msTick = 30000;

	DEBUG("--> RUN");
	// Start tasks
   RUN(Task_Timer);
   RUN(Task_Serial);
   RUN(Task_SoftUartOut);

   Epilog();                   //Fall into kernel & start first task

	for (;;);
}
void BeginIdleHook(void) { BCLR(LED_PORT, LED_DBG); }

void delay_ms(int x)
{
	AvrXDelay(&AvrXSelf()->processDelay, x);
}

TimerControlBlock *processTimer(void)
{
	return &AvrXSelf()->processDelay;
}

// vim: set sw=3 ts=3 noet nu:
