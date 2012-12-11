/**
 * (C) Copyright 2012 flemming.madsen at madsensoft.dk.
 **                                          MarkDown
 * Convert AWA & AWS to analog out
 * ===============================
 */
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avrx.h>
#include <stdio.h>
#include "hardware.h"
#include "Screen.h"
#include "util.h"

uint8_t state;
void Task_BattStat(void)
{
   for (;;) {
      delay_ms(125);
		state = (state+1) & 0x7;

		SBIT(LED_PORT, LED_DBG3) ^= 1;
		ScreenUpdate();
   }
}
// vim: set sw=3 ts=3 noet nu:
