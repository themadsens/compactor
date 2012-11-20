//*******************************************************
//					Nokia Shield
//*******************************************************
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "Screen.h"
#include "Serial.h"

#define BAUD 38400
#include <util/setbaud.h>

#define TICKRATE 1000       // AvrX timer queue 1ms resolution
#define TCNT0_TOP (F_CPU/64/TICKRATE)

#define SOFTBAUD 4800
#define TCNT2_TOP (F_CPU/8/6/SOFTBAUD)


uint8_t msTick;
// Use Timer0 for the 1/1000s tick rate
AVRX_SIGINT(TIMER0_COMP_vect)
{
   IntProlog();                // Switch to kernel stack/context
   AvrXTimerHandler();         // Call Time queue manager
   if (10 == ++msTick) {
      msTick = 0;
      ScreenButtonTick();
   }
   Epilog();                   // Return to tasks
}

// Hispeed RX ready
AVRX_SIGINT(USARC_RXC_vect)
{
	IntProlog();
	uint8_t ch = UDR;

	// Already in critical section
	UartFifo(9); // HiSpeed
	UartFifo(ch);

	Epilog()
}

// Hi speed timer for soft uart in & out at 4800 baud
uint8_t suTick;
uint8_t suMaskActive;
uint8_t suState[3];
AVRX_SIGINT(TIMER2_COMP_vect)
{
   IntProlog();                // Switch to kernel stack/context
   if (6 == ++suTick) {
      suTick = 0;
		if (CurNmeaOut)
			SoftUartOutKick();
	}
	if (suMaskActive)
		SoftUartFifo(((PIND<<2)&0x30) | ((PIND<<4)&0x40) | (suTick&0x0f));
   Epilog();                   // Return to tasks
}
// Use GCC register saving in these; Miniscule w. no kernel calls or stack use
ISR(INT0_vect)
{
	suState[0] = suTick;
	BSET(suMaskActive, 0);
	BCLR(GICR, INT0);
}
ISR(INT1_vect)
{
	suState[1] = suTick;
	BSET(suMaskActive, 1);
	BCLR(GICR, INT1);
}
ISR(INT2_vect)
{
	suState[2] = suTick;
	BSET(suMaskActive, 2);
	BCLR(GICR, INT2);
}

// Task declarations
AVRX_GCC_TASK(Task_Screen, 20, 0);
AVRX_GCC_TASK(Task_Uart, 20, 0);
AVRX_GCC_TASK(Task_SoftUart, 20, 0);

int main(void) 
{
   BSET(MCUCR, SE);           // Sleep enable for idle'ing

   // Timer0 heartbeat - CTC mode ensures equidistant ticks
   OCR0 = TCNT0_TOP;
   BSET(TIMSK, OCIE0);
   TCCR0 = BV(CS00)|BV(CS01)|BV(WGM01); // F_CPU/64 - CTC Mode

   // Timer2 4800 baud soft uart timer
   OCR2 = TCNT2_TOP;
   BSET(TIMSK, OCIE2);
   TCCR2 = BV(CS21)|BV(WGM21); // F_CPU/8 - CTC Mode

	// Extern Ints 0,1,2
	MCUCR |= BV(ISC11)|BV(ISC01)|
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

	// Start tasks
   AvrXRunTask(TCB(Task_Screen));
   AvrXRunTask(TCB(Task_Uart));
   AvrXRunTask(TCB(Task_SoftUart));

   Epilog();                   // Fall back into kernel & start first task
}

// vim: set sw=3 ts=3 noet:
