/**
 * (C) Copyright 2012 flemming.madsen at madsensoft.dk.
 **                                          MarkDown
 * Convert AWA & AWS to analog out
 * ===============================
 */
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avrx.h>
#include <math.h>
#include <stdio.h>
#include "hardware.h"
#include "Screen.h"
#include "util.h"

#define VREF_LADJ (BV(REFS1)|BV(REFS0)|BV(ADLAR))

uint16_t Batt_AH_micro10;

uint8_t state;
void Task_BattStat(void)
{
	ADCSRA =  BV(ADEN) | BV(ADIE) | BV(ADPS2) | BV(ADPS1) | BV(ADPS0);
	state = 11;
   for (;;) {
      delay_ms(75);
		state = (state+1) % 12;

		if (state < 10 && (state&1)) {
			SBIT(ADCSRA, ADSC) = 1;
			continue;
		}
		//else
		switch (state/2) {
		 case 0:
			ADMUX = VREF_LADJ | VHOUSE;
			break;
		 case 1:
			ADMUX = VREF_LADJ | VSTART;
			break;
		 case 2:
			ADMUX = VREF_LADJ | IHOUSE;
			break;
		 case 3:
			ADMUX = VREF_LADJ | ISOLAR;
			break;
		 case 4:
			ADMUX = VREF_LADJ | TBATT;
			break;
		 case 5:
			// Every 0.9 sec -> 3600 / 0.9 = 4000
			Batt_AH_micro10 += pow(Batt_IHOUSE * 2.5, Cnfg_PEUKERT / 1000.0);
			if (Batt_AH_micro10 > 10000) {
				Batt_HOUSEAH += Batt_AH_micro10 / 10000;
				Batt_AH_micro10 = Batt_AH_micro10 % 10000;
				Batt_redraw |= BV(BATT_HOUSEAH);
				ScreenUpdate();
			}
			break;
		}

   }
}

AVRX_SIGINT(ADC_vect)
{
   IntProlog();                // Switch to kernel stack/context
	
	// Left adjust -> div4 (1024/256)
	switch (state/2) {
	 case 0:
		Batt_VHOUSE = muldiv(ADCH, 1000, 4500/4);
		break;
	 case 1:
		Batt_VENGINE = muldiv(ADCH, 1000, 4500/4);
		break;
	 case 2:
		Batt_IHOUSE = muldiv(ADCH, 1234, 100/4);
		break;
	 case 3:
		Batt_ISOLAR = muldiv(ADCH, 1234, 100/4);
		break;
	 case 4:
		Batt_TEMP = 583 - muldiv(ADCH, 185, 400/4);
		break;
	}
	Batt_redraw |= BV(state);
	ScreenUpdateInt();

	
   Epilog();                   // Return to tasks
}
// vim: set sw=3 ts=3 noet nu:
