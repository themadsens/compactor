/**
 * (C) Copyright 2012 flemming.madsen at madsensoft.dk.
 **                                          MarkDown
 * Convert AWA & AWS to analog out
 * ===============================
 */
#include <avr/io.h>
#include "hardware.h"
#include "util.h"

void Task_BattStat(void)
{
   for (;;) {
      BCLR(LED_PORT, LED_DBG2);
      delay_ms(100);
      BSET(LED_PORT, LED_DBG2);
      delay_ms(900);
   }
}
// vim: set sw=3 ts=3 noet nu:
