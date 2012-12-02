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
#include <avr/interrupt.h>

#define AVRX_USER_PROCESS_DATA struct TimerControlBlock processDelay
#include <avrx.h>

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
#define TCNT2_TOP (F_CPU/8/6/SOFTBAUD)


uint16_t msTick;
// Use Timer0 for the 1/1000s tick rate
AVRX_SIGINT(TIMER0_COMP_vect)
{
   IntProlog();                // Switch to kernel stack/context
	msTick++;
	SBIT(LED_PORT, LED_DBG1) = msTick&1;
   if (0 == msTick%10) {
      ScreenButtonTick();
   }
   AvrXTimerHandler();         // Call Time queue manager
   Epilog();                   // Return to tasks
}

// Hispeed RX ready
AVRX_SIGINT(USARC_RXC_vect)
{
	IntProlog();
	uint8_t ch = UDR;

	// Already in critical section
	SoftUartFifo(0xff); // HiSpeed
	SoftUartFifo(ch);

	Epilog();
}

// Hi speed timer for soft uart in & out at 4800 baud
uint8_t suTick;
extern uint8_t suEdgeTick[3];
AVRX_SIGINT(TIMER2_COMP_vect)
{
   IntProlog();                // Switch to kernel stack/context
   if (6 == ++suTick) {
      suTick = 0;
		if (CurUartOut)
			SoftUartOutKick();
	}
	else if (CurUartOut && 3 == suTick)
		SoftUartOutKick();
	if (suEdgeTick[0] || suEdgeTick[1] || suEdgeTick[2])
		SoftUartFifo((SBIT(SUART1_RXPORT, SUART1_RXPIN) << 4) |
		             (SBIT(SUART2_RXPORT, SUART2_RXPIN) << 5) |
		             (SBIT(SUART3_RXPORT, SUART3_RXPIN) << 6) |
						 (suTick&0x0f));
   Epilog();                   // Return to tasks
}
// Use GCC register saving in these; Miniscule w. no kernel calls or stack use
ISR(INT0_vect)
{
	suEdgeTick[0] = suTick + 1;
	BCLR(GICR, INT0);
}
ISR(INT1_vect)
{
	suEdgeTick[1] = suTick + 1;
	BCLR(GICR, INT1);
}
ISR(INT2_vect)
{
	suEdgeTick[2] = suTick + 1;
	BCLR(GICR, INT2);
}

// Task declarations
AVRX_GCC_TASK( Task_Screen,      50,  0);
AVRX_GCC_TASK( Task_Serial,      30,  0);
AVRX_GCC_TASK( Task_SoftUartIn,  20,  0);
AVRX_GCC_TASK( Task_SoftUartOut, 20,  0);
AVRX_GCC_TASK( Task_WindOut,     20,  0);
AVRX_GCC_TASK( Task_BattStat,    20,  0);

#define RUN(T) (memset(T##Stk, 0xAA, sizeof(T##Stk)), AvrXRunTask(TCB(T)))

int main(void) 
{
   BSET(MCUCR, SE);           // Sleep enable for idle'ing
	BSET(ACSR, ACD);           // Save a little power

   // Timer0 heartbeat - CTC mode ensures equidistant ticks
   OCR0 = TCNT0_TOP;
   BSET(TIMSK, OCIE0);
   TCCR0 = BV(CS00)|BV(CS01)|BV(WGM00); // F_CPU/64 - CTC Mode

   // Timer2 4800 baud soft uart timer
   OCR2 = TCNT2_TOP;
   BSET(TIMSK, OCIE2);
   TCCR2 = BV(CS21)|BV(WGM21); // F_CPU/8 - CTC Mode

	// Extern Ints 0,1,2
	MCUCR |= BV(ISC11)|BV(ISC01);
	//BCLR(MCUCSR, ISC2);
	GICR |= BV(INT0)|BV(INT1)|BV(INT2);

   // USART Baud rate etc
   UBRRH = UBRRH_VALUE;
   UBRRL = UBRRL_VALUE;
#if USE2X
   UCSRA |= BV(U2X);			    //Might double the UART Speed
#endif
   UCSRB = BV(RXEN)|BV(TXEN)|BV(RXCIE);	 //Enable Rx and Tx in UART + IEN
   UCSRC = BV(UCSZ0)|BV(UCSZ1)|BV(URSEL);  //8-Bit Characters

	BSET(LED_DDR, LED_DBG1);
	BSET(LED_DDR, LED_DBG2);
	BSET(LED_PORT, LED_DBG1);
	BSET(LED_PORT, LED_DBG2);

	// Start tasks
#if 0
   RUN(Task_Screen);
   RUN(Task_Serial);
   RUN(Task_SoftUartIn);
   RUN(Task_SoftUartOut);
   RUN(Task_WindOut);
#endif
   RUN(Task_BattStat);

   Epilog();                   // Fall into kernel & start first task
}


void delay_ms(int x)
{
	AvrXDelay(&AvrXSelf()->processDelay, x);
}

TimerControlBlock *processTimer(void)
{
	return &AvrXSelf()->processDelay;
}

// vim: set sw=3 ts=3 noet nu:
