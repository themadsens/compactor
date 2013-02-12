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

#include "trigint_sin8.h"
#include "Screen.h"
#include "Serial.h"
#include "SoftUart.h"
#include "WindOut.h"
//#include "BattStat.h"
#include "util.h"
#include "hardware.h"

#define BAUD 38400
#include <util/setbaud.h>

#define TICKRATE 1000       // AvrX timer queue 1ms resolution
#define TCNT0_TOP (F_CPU/64/TICKRATE)

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
		if (0 == (secTick % 3600)) {
			 memmove(maxWind+1, maxWind, sizeof(maxWind) - sizeof(uint16_t));
			 maxWind[0] = Nav_AWS;
			 maxWindCur = 0;
			 for (register u8 i = 0; i < 24; i++) {
				 if (maxWind[i] > maxWindCur) {
					  maxWindCur = maxWind[i];
					  maxWindAgo = i;
				  }
			  }
		 }
	}
	WindPulse();                // Hook into wind square generation
   AvrXTimerHandler();         // Call Time queue manager
   Epilog();                   // Return to tasks
}


// Debug out (stdout)
uint8_t bufStdOut[90];
pAvrXFifo pStdOut = (pAvrXFifo) bufStdOut;

// Task declarations -- Falling priority order
AVRX_GCC_TASK( Task_SoftUartOut,  20,  1);
AVRX_GCC_TASK( Task_ScreenButton, 20,  2);
AVRX_GCC_TASK( Task_Serial,      100,  4);
AVRX_GCC_TASK( Task_WindOut,      20,  5);
//AVRX_GCC_TASK( Task_BattStat,     20,  5);
AVRX_GCC_TASK( Task_Screen,      100,  6);

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

   // USART Baud rate etc
   UBRRH = UBRRH_VALUE;
   UBRRL = UBRRL_VALUE;
#if USE2X
   BSET(UCSRA, U2X);			       //Might double the UART Speed
#endif
   UCSRB = BV(RXEN)|BV(RXCIE);	 //Enable Rx in UART + IEN
   UCSRC = BV(UCSZ0)|BV(UCSZ1)|BV(URSEL);  //8-Bit Characters
	BSET(UCSRB, TXEN);                      //Hand over pin to Uart
   stdout = &mystdout; //Required for printf init

	BSET(LED_PORT, LED_DBG);
	BSET(LED_DDR, LED_DBG);

	// Initially free -- AvrX should do this !!
	extern Mutex EEPromMutex;
	AvrXSetSemaphore(&EEPromMutex);

	msTick = 30000;

	DEBUG("--> RUN");
	// Start tasks
   RUN(Task_Screen);
   RUN(Task_ScreenButton);
   RUN(Task_Serial);
   RUN(Task_SoftUartOut);
//   RUN(Task_BattStat);
   RUN(Task_WindOut);

   Epilog();                   // Fall into kernel & start first task

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
	if (FIFO_ERR == ch) {
		BCLR(UCSRB, TXCIE);
	}
	else
		UDR = ch;

	Epilog();
}

#define TRIGINT_PI2 0x4000

long CalcRngBrg(long lat1, long lat2, long lon1, long lon2, uint16_t *Brg)
{
	register int16_t x;
	register uint16_t lonDist, latDist;
	
	// TRIGINT_PI2 / 360 ~= 45.51 -- Keep it in 16bit!

	latDist = abs(lat2 - lat1);
	x = trigint_sin8(TRIGINT_PI2/4 - muldiv(abs((lat2+lat1)/2), 4551, 100));
	lonDist = muldiv(abs(lon2 - lon1), abs(x), 128);

	if (Brg) {
		if (latDist > lonDist)
			x = muldiv(lonDist, 100, latDist);
		else
			x = muldiv(latDist, 100, lonDist);
		// See http://www.convict.lu/Jeunes/Math/arctan.htm for atan on the RCX
		// and the polynomial approximation in short integers.
		*Brg =(-150 + 310*x - (x*x / 2) - (x*x / 3)) / 50;
		if (lonDist > latDist)
			*Brg = 900 - *Brg;
		if (lat1 > lat2)
			*Brg = 1800 - *Brg;
		if (lon1 > lon2)
			*Brg = 3600 - *Brg;
	}

	return round(sqrt(latDist * lonDist));
}

// vim: set sw=3 ts=3 noet nu:
