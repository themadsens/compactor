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

#include "Screen.h"
#include "Serial.h"
#include "SoftUart.h"
#include "WindOut.h"
#include "BattStat.h"
#include "util.h"
#include "hardware.h"

#define BAUD 38400
#include <util/setbaud.h>

#define TICKRATE 1000       // AvrX timer queue 1ms resolution
#define TCNT0_TOP (F_CPU/64/TICKRATE)

#define SOFTBAUD 4800
#define TCNT2_TOP (F_CPU/8/3/SOFTBAUD)

static int uart_putchar(char c, FILE *stream);
static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);

inline void WindPulse(void)
{
    if (!windPulse || msTick - lastPulse < windPulse)
        return;
    SBIT(AWS_PORT, AWS_PIN) ^= 1;
    lastPulse = msTick;
}

int16_t msTick;
// Use Timer0 for the 1/1000s tick rate
AVRX_SIGINT(TIMER0_COMP_vect)
{
   IntProlog();                // Switch to kernel stack/context
	BSET(LED_PORT, LED_DBG2);
	msTick++;
	WindPulse();                // Hook into wind square generation
   AvrXTimerHandler();         // Call Time queue manager
   Epilog();                   // Return to tasks
}


#if 0
AVRX_SIGINT(BADISR_vect)
{
   IntProlog();                // Switch to kernel stack/context
	DEBUG("\n  -- BAD INT --\n");
   Epilog();                   // Return to tasks
}
#endif

// Debug out (stdout)
uint8_t bufStdOut[200];
pAvrXFifo pStdOut = (pAvrXFifo) bufStdOut;

#if 0
static void Task_Idle(void)
{
	for (;;) {
      BCLR(LED_PORT, LED_DBG2);
		sleep_mode();
	}
}
AVRX_GCC_TASK( Task_Idle,        10,  9);
#endif

// Task declarations -- Falling priority order
AVRX_GCC_TASK( Task_SoftUartOut,  50,  1);
AVRX_GCC_TASK( Task_ScreenButton, 50,  2);
AVRX_GCC_TASK( Task_Serial,      150,  4);
AVRX_GCC_TASK( Task_WindOut,      50,  5);
AVRX_GCC_TASK( Task_BattStat,     50,  5);
AVRX_GCC_TASK( Task_Screen,      120,  6);

#define RUN(T) (AvrXRunTask(TCB(T)))

int main(void) 
{
 	AvrXSetKernelStack(0);
	pStdOut->size = (uint8_t) sizeof(bufStdOut) - sizeof(AvrXFifo) + 1;
	AvrXFlushFifo(pStdOut);

	set_sleep_mode(SLEEP_MODE_IDLE);
	BSET(ACSR, ACD);           // Save a little power

   // Timer0 heartbeat - CTC mode ensures equidistant ticks
   OCR0 = TCNT0_TOP;
   BSET(TIMSK, OCIE0);
   TCCR0 = BV(CS00)|BV(CS01)|BV(WGM01); // F_CPU/64 - CTC Mode

#if 1
   // Timer2 4800 baud soft uart timer
   OCR2 = TCNT2_TOP;
   BSET(TIMSK, OCIE2);
   TCCR2 = BV(CS21)|BV(WGM21); // F_CPU/8 - CTC Mode
#endif


   // USART Baud rate etc
   UBRRH = UBRRH_VALUE;
   UBRRL = UBRRL_VALUE;
#if USE2X
   UCSRA |= BV(U2X);			    //Might double the UART Speed
#endif
   UCSRB = BV(RXEN)|BV(TXEN)|BV(RXCIE);	 //Enable Rx and Tx in UART + IEN
   UCSRC = BV(UCSZ0)|BV(UCSZ1)|BV(URSEL);  //8-Bit Characters
   stdout = &mystdout; //Required for printf init

	BSET(LED_PORT, LED_DBG1);
	BSET(LED_PORT, LED_DBG2);
	BSET(LED_PORT, LED_DBG3);
	BSET(LED_DDR, LED_DBG1);
	BSET(LED_DDR, LED_DBG2);
	BSET(LED_DDR, LED_DBG3);

	// Initially free -- AvrX should do this !!
	extern Mutex EEPromMutex;
	AvrXSetSemaphore(&EEPromMutex);

	msTick = 30000;

	DEBUG("--> RUN");
	// Start tasks
   RUN(Task_Screen);
   RUN(Task_ScreenButton);
#if 0
   RUN(Task_Idle);
#endif
   RUN(Task_Serial);
   RUN(Task_SoftUartOut);
   RUN(Task_BattStat);
   RUN(Task_WindOut);

   Epilog();                   // Fall into kernel & start first task

	for (;;);
}


void delay_ms(int x)
{
	AvrXDelay(&AvrXSelf()->processDelay, x);
}

TimerControlBlock *processTimer(void)
{
	return &AvrXSelf()->processDelay;
}

static int uart_putchar(char c, FILE *stream)
{
#if 1
	if (c == '\n')
		uart_putchar('\r', stream);

	BeginCritical();
	AvrXPutFifo(pStdOut, c);
	EndCritical();
	if (SBIT(UCSRA, UDRE)) {
		UDR = AvrXPullFifo(pStdOut);
		BSET(UCSRB, TXCIE);                     //Enable TX empty Intr.
	}
#endif
	return 0;
}
// Hispeed RX ready
AVRX_SIGINT(USART_TXC_vect)
{
	IntProlog();

	int16_t ch = AvrXPullFifo(pStdOut);
	if (FIFO_ERR != ch)
		UDR = ch;
	else
		BCLR(UCSRB, TXCIE);                     //Enable TX empty Intr.

	Epilog();
}

// vim: set sw=3 ts=3 noet nu:
