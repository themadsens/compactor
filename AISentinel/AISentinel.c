/**
 * (C) Copyright 2012 flemming.madsen at madsensoft.dk. See libuevent.h
 **                                                   MarkDown
 * AIS Sentinel
 * ============
 * Fires the buzzer and LED if AIS type 14 or MMSI prefix 970 is seen.
 * Both signifies a tranmitting AIS SART
 *
 * AIS decoding: http://gpsd.berlios.de/AIVDM.txt
 *
 * - Buzzer is driven from a square on a port pin.
 * - A button press toggles buzzer, but keeps LED blinking
 * - LED blinks with 1 second period in isophase.
 * - No alarm triggers in 10 minutes clears alarm condition
 *
 * Timer0
 * ------
 * Timer 0 at baudrate => Top = CPU_FREQ / BAUDRATE
 * Drive buzzer at alternating frequencies
 *
 * Timer
 * -----
 * App timer at centisecond resolution. Each BAUDRATE / 100
 * Toggle buzzer freq every 500 ms if buzzerOn
 * Toggle LED every 500 ms if ledOn
 */


#define CPU_FREQ    (9600 * 1000UL)
#define BAUDRATE    38400
#define CSEC_TOP    (BAUDRATE / 100)
#define TIMER0_TOP  (F_CPU / BAUDRATE)
#define BUZZER1_TOP (BAUDRATE / 2800)
#define BUZZER2_TOP (BAUDRATE / 2200)
#define AIS_TIMEOUT (600 * 100)

#include <stdint.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "util.h"

#if defined (__AVR_ATtiny13__)

#define BUZZER_PIN SBIT(PORTB, PB3)
#define BUZZER_OUT SBIT(DDRB,  PB3)
#define LED_PIN    SBIT(PORTB, PB4)
#define LED_OUT    SBIT(DDRB,  PB4)
#define BUTTON_PIN SBIT(PINB,  PB2)
#define SRXD_PIN   SBIT(PINB,  PB0)
#define SRXD_IEN   SBIT(PCMSK, PCINT0)
//#elif defined (__AVR_ATtiny44__)
//#elif defined (__AVR_ATmega16__)
#else
#error "Must be one of ATtiny13 ATtiny24 ATtiny44 ATmega16"
#endif

#define F_CPU CPU_FREQ
#include <util/delay.h>

// Maintained by timer interrupt. 
uint32_t upTime;

char AISLead[] PROGMEM = "\n!AIVDM,1,1,,\a,";

// Nonzero is active alarm. 
// Updated on every message
// Zeroed when no message seen in 10 minutes
uint32_t alarmStart;

void alarmCond(void);

#define AIST_MMSI_OFF 50
#define AIST_LEAD_MAX 15
uint8_t aisState; // Counter in AISLead or state
uint32_t mmsi;

void AISDecode(int ch)
{
	if (aisState < AIST_LEAD_MAX) {
		char test = pgm_read_byte(&AISLead[aisState]);
		if (test == '\a' || ch == test)
			aisState++;
		else
			aisState = 0;
	}
	else if (aisState == AIST_LEAD_MAX) {
		if (ch == '>') {      // Type 14: Safety related broadcast
			alarmCond();
			// Skip rest of message
			aisState = 0;
		}
		else if (ch == '1') { // Type 01: Position update
			aisState = AIST_MMSI_OFF;
			mmsi = 0;
		}
	}
	else if (aisState >= AIST_MMSI_OFF) {
		int code = ch - (ch > 'W' ? 56 : 48);
		switch (aisState - AIST_MMSI_OFF) {
		 case 0:
			// Two topmost bits are the repeat indicator
			mmsi = (uint32_t)(code & 0xf) << 26; // 26 bits left
			break;
		 case 1:
		 case 2:
		 case 3:
		 case 4:
			mmsi |= code << (20 - (aisState - AIST_MMSI_OFF - 1) * 6);
			break;
		 case 5:
			mmsi |= code >> 4; // 4 LSB bits are navigation status
			// MMSI complete - Check decimal prefix
			if ((mmsi / 1000000L) == 970)
				alarmCond();
			// Skip rest of message
			aisState = 0;
			break;
		 default:
			aisState = 0;
		}
	}
	else
		aisState = 0;
}

// Timer0 ISR
uint8_t buzzerOn   = 0;
uint8_t buzzerHigh = 0;
#define BuzzerTop  (buzzerHigh ? BUZZER1_TOP : BUZZER2_TOP)
uint16_t buzzerCnt = 0;

uint16_t csecCnt = 0;

static uint8_t srx_cur;
static uint8_t srx_pending;
static uint8_t srx_data;
static uint8_t srx_state;

ISR(TIM0_OVF_vect)
{
   if (srx_state)
   {
      u8             i;

      switch (--srx_state)
      {

       case 9:
         if (SRXD_PIN == 0)     // start bit valid
            goto end_uart;
         break;

       default:
         i = srx_data >> 1;     // LSB first
         if (SRXD_PIN == 1)
            i |= 0x80;          // data bit = 1
         srx_data = i;
			goto end_uart;

       case 0:
         if (SRXD_PIN == 1) {   // stop bit valid
				srx_cur = srx_data;
				srx_pending = 1;
         }
      }
		// Start bit interrupt enable.
		// Ie. Cancel any half-received on next level change
      SRXD_IEN = 1;
		srx_state = 0;
   }
end_uart:

	if (buzzerOn) {
		if (buzzerCnt++ >= BuzzerTop) {
			BUZZER_PIN ^= 1;
			buzzerCnt = 0;
		}
	}
	else {
		BUZZER_PIN = 0;
	}
	csecCnt++;
}

// Start bit detect ISR
ISR(PCINT0_vect)
{
   TCNT0 = TIMER0_TOP / 2;      // scan at 0.5 bit time
   SRXD_IEN = 0;                // Disable this interrupt until char RX
	srx_state = 10;
}

u8 ugetchar( void )			// wait until byte received
{
   while (!srx_pending)
      sleep_cpu();
   srx_pending = 0;
   return srx_cur;
}

void alarmCond(void)
{
	if (!alarmStart) {
		buzzerOn = 1;
	}
	alarmStart = upTime ?: 1;
}

uint8_t halfSec = 0;
uint8_t btnPressed = 0;
int main(void)
{
	BUZZER_OUT = 1;
	LED_OUT    = 1;

	// Pin change interrrupt enable
	GIMSK = (1 << PCIE);
	// Waveform Generation is CTC (Clear timeron compare) WGM2:0 == 2 == 0b010
	TCCR0A = (1 << WGM01);
	// Timer connected tomain clock. No prescale: CS2:0 == 1 == 0b001
	TCCR0A = (1 << CS00);
	// Timer0 running at baud rate  -- with interrupt enabled
	OCR0A = TIMER0_TOP;
	TIMSK0 = (1 << TOIE0);

	SRXD_IEN   = 1;       // Arm start bit interrupt

	set_sleep_mode(SLEEP_MODE_IDLE);
	sleep_enable();

   sei();                       // Rock & roll
	while(1) {
		// Rest here until next timer overrun
	   sleep_cpu();

		// Read AIS stram
		if (srx_pending) {
			AISDecode(ugetchar());
		}

		// Main timer
		if (csecCnt >= CSEC_TOP) {
			upTime++;
			csecCnt = 0;
			halfSec = (halfSec + 1) % 100;
		}
		else
			continue;

		/**
		 * Henceforth every 1/100 second
		 */

		// Handle pushbutton
		if (BUTTON_PIN) {
			if (btnPressed == 5) {
				if (!alarmStart)
					buzzerOn = 1;
				else
					buzzerOn ^= 1;
			}
			else btnPressed++;
		}
		else {
			if (btnPressed >= 5) {
				if (!alarmStart) {
					buzzerOn = 0;
				}
			}
			btnPressed = 0;
		}

		// Isophase heartbeat
		if (halfSec == 0) {
			LED_PIN = 0;
			buzzerHigh = 0;
			if ((upTime - alarmStart) > AIS_TIMEOUT) {
				alarmStart = 0;
			}
		}
		else if (halfSec == 50 && (alarmStart || btnPressed)) {
			LED_PIN = 1;
			buzzerHigh = 1;
		}

	}
}

// vim: set sw=3 ts=3 noet nu:
