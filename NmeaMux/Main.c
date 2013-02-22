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

void Task_Debug(void)
{
	delay_ms(100);
	DEBUG("--> RUN");
	delay_ms(100);
	NmeaPutFifo(0, "$GPTST,LO1");
	delay_ms(100);
	NmeaPutFifo(1, "$GPTST,HI1");
	delay_ms(100);
	NmeaPutFifo(0, "$GPTST,LO2");
	delay_ms(100);
	NmeaPutFifo(1, "$GPTST,HI2");
	for (;;) {
		//DEBUG
		delay_ms(500);
		SBIT(LED_PORT, LED_OVF) ^= 1;
	}
}
// Task declarations -- Falling priority order
AVRX_GCC_TASK( Task_SoftUartOut, 60,  1);
AVRX_GCC_TASK( Task_Serial,      60,  4);
AVRX_GCC_TASK( Task_Debug,       20,  2);

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

#if 1
   // USART Baud rate etc
   UBRRH = UBRRH_VALUE;
   UBRRL = UBRRL_VALUE;
#if USE2X
   UCSRA |= BV(U2X);			       //Might double the UART Speed
#endif
   UCSRC = BV(UCSZ0)|BV(UCSZ1)|BV(URSEL);  //8-Bit Characters
#endif

   stdout = &mystdout; //Required for printf init

	SoftUartInInit();
	SoftUartOutInit();

	BSET(LED_PORT, LED_DBG);
	BSET(LED_PORT, LED_ACT);
	BSET(LED_PORT, LED_OVF);
	BSET(LED_DDR, LED_DBG);
	BSET(LED_DDR, LED_ACT);
	BSET(LED_DDR, LED_OVF);

	// Initially free -- AvrX should do this !!
	extern Mutex EEPromMutex;
	AvrXSetSemaphore(&EEPromMutex);

	// Start tasks
   RUN(Task_SoftUartOut);
	RUN(Task_Serial);
   RUN(Task_Debug);

   Epilog();                   //Fall into kernel & start first task

	for (;;);
}
void BeginIdleHook(void) { BCLR(LED_PORT, LED_DBG); }

void delay_ms(int x)
{
	AvrXDelay(&AvrXSelf()->processDelay, x);
}

TimerControlBlock *myTimer(void)
{
	return &AvrXSelf()->processDelay;
}

// vim: set sw=3 ts=3 noet nu:
