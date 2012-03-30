/**
 * (C) Copyright 2012 flemming.madsen at madsensoft.dk. See libuevent.h
 **                                                   MarkDown
 * AIS Sentinel
 * ============
 * Fires the buzzer and LED if AIS type 14 or MMSI prefix 970 is seen.
 * Both signifies a transmitting AIS SART
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
 *
 */


#define CPU_FREQ    (9600 * 1000UL)
#define BAUDRATE    38400
#define CSEC_TOP    (BAUDRATE / 100)
#define TIMER0_TOP  (F_CPU / BAUDRATE)
#define BUZZER1_TOP (BAUDRATE / 2800)
#define BUZZER2_TOP (BAUDRATE / 2200)
#define AIS_TIMEOUT (600 * 10)

#include <stdint.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include "util.h"

#if defined (__AVR_ATtiny13__)

#define BUZZER_PIN SBIT(PORTB, PB3)
#define BUZZER_OUT SBIT(DDRB,  PB3)
#define LED_PIN    SBIT(PORTB, PB4)
#define LED_OUT    SBIT(DDRB,  PB4)
#define DBG_PIN    SBIT(PORTB, PB1)
#define DBG_OUT    SBIT(DDRB,  PB1)
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
uint16_t upTime;

char AISLead[] = "\n!AIVDM,1,1,,\a,";

// Nonzero is active alarm. 
// Updated on every message
// Zeroed when no message seen in 10 minutes
uint16_t alarmStart;

void alarmCond(void);

#define AIST_MMSI_ABOVE 60
#define AIST_MMSI_BELOW 70
#define AIST_LEAD_MAX 16
uint8_t aisState; // Counter in AISLead or state

/*
 * MMSI High 970999999 == 0b 111001 111000 000100 100010 111111
 * MMSI Low  970000000 == 0b 111001 110100 010000 011010 000000
 */
//                111001 111000 000100 100010 111111 EOT
char MMSIHi[] = { 'q',   'p',   '4',   'R',   'w',   '\0'};
//                111001 110100 010000 011010 000000 EOT
char MMSILo[] = { 'q',   'l',   '@',   'J',   '0',   '\x7f'};

void AISDecode(int ch)
{
	register int t;
	if (aisState < AIST_LEAD_MAX) {
		char test = AISLead[aisState];
		if (test == '\a' || ch == test)
			aisState++;
		else
			aisState = 0;
	}
	else if (aisState == AIST_LEAD_MAX) {
		if (ch == '>') {          // Type 14: Safety related broadcast
			alarmCond();
			// Skip rest of message
			aisState = 0;
		}
		else if (ch == '1') {     // Type 01: Position update
			aisState = AIST_MMSI_ABOVE; // Pick one
		}
	}
	else if (aisState / 10 == AIST_MMSI_ABOVE) {
		t = MMSILo[aisState - AIST_MMSI_ABOVE];
		if (ch > t) {
			aisState = aisState - AIST_MMSI_ABOVE + AIST_MMSI_BELOW;
         AISDecode(ch);         // Must be below max as well
		}
		else if (ch < t)
			aisState = 0;
		else
			aisState++;
	}
	else if (aisState / 10 == AIST_MMSI_BELOW) {
		t = MMSIHi[aisState - AIST_MMSI_ABOVE];
		if (ch > t)
			aisState = 0;
		else
			aisState++;
	}
	else
		aisState = 0; // Start over

	if (aisState == AIST_MMSI_ABOVE + 5 || aisState == AIST_MMSI_BELOW + 5) {
		aisState = 0;
		alarmCond();
	}
}

// Timer0 ISR
uint8_t buzzerOn   = 0;
uint8_t buzzerHigh = 0;
#define BuzzerTop  (buzzerHigh ? BUZZER1_TOP : BUZZER2_TOP)
uint16_t buzzerCnt = 0;

uint16_t csecCnt = 0;
int16_t  csecErr = 0;

static uint8_t srx_cur;
static uint8_t srx_pending;
static uint8_t srx_data;
static uint8_t srx_state;

#define SRXD_READ_LEAD 33 // 33 Cycles from TCNT0 set to read
ISR(TIM0_COMPA_vect)
{
	DBG_PIN = SRXD_PIN;
   if (srx_state)
   {
      register uint8_t i;

	BUZZER_PIN ^= 1;
      switch (--srx_state)
      {

       case 9:
         if (SRXD_PIN == 0)     // start bit valid
            goto end_uart;
         break;

		 case 3:
			//if (!alarmStart) LED_PIN = 0; // Activity light
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

		if (!alarmStart) LED_PIN = 0; // Activity light
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
	csecCnt += 1 + (csecErr / TIMER0_TOP);
	csecErr %= TIMER0_TOP;
}

// Start bit detect ISR
ISR(PCINT0_vect)
{
	DBG_PIN = SRXD_PIN;
	register int tsave = TCNT0;
   TCNT0 = (TIMER0_TOP / 2) + SRXD_READ_LEAD; // scan at 0.5 bit time
	csecErr += ((TIMER0_TOP / 2) + SRXD_READ_LEAD) - tsave;

   SRXD_IEN = 0;                // Disable this interrupt until char RX
	srx_state = 10;
	//if (!alarmStart) LED_PIN = 1; // Activity light
}

static inline u8 ugetchar( void )			// wait until byte received
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

uint8_t dsecCnt = 0;
uint8_t btnPressed = 0;
int main(void)
{
	// Might save a few bytes here with a single assignment
	BUZZER_OUT = 1;
	LED_OUT    = 1;
	DBG_OUT    = 1;

	// Pin change interrupt enable
	GIMSK = (1 << PCIE);
	// Waveform Generation is CTC (Clear timer on compare) WGM2:0 == 2 == 0b010
	TCCR0A = (1 << WGM01);
	// Timer connected to main clock. No prescale: CS2:0 == 1 == 0b001
	TCCR0B = (1 << CS00);
	// Timer0 running at baud rate  -- with interrupt enabled
	OCR0A = TIMER0_TOP;
	TIMSK0 = (1 << OCIE0A);

	SRXD_IEN   = 1;       // Arm start bit interrupt

	set_sleep_mode(SLEEP_MODE_IDLE);


   sei();                       // Rock & roll
	while(1) {
		// Rest here until next timer overrun
		sleep_enable();
	   sleep_cpu();
		sleep_disable();


		// Read AIS stram
		if (srx_pending) {
			AISDecode(ugetchar());
		}

		// Main timer
		if (csecCnt >= CSEC_TOP) {
			csecCnt = 0;
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
		
		dsecCnt++;
		//DBG_PIN ^= 1;
		if (dsecCnt >= 10)
			dsecCnt = 0;
		else
			continue;

		/**
		 * Henceforth every 1/10 second
		 */

		upTime++;

		// Isophase heartbeat
		if (dsecCnt == 0) {
			LED_PIN = 0;
			buzzerHigh = 0;
			if ((upTime - alarmStart) > AIS_TIMEOUT) {
				alarmStart = 0;
			}
		}
		else if (dsecCnt == 5 && (alarmStart || btnPressed)) {
			LED_PIN = 1;
			buzzerHigh = 1;
		}

	}
}

// vim: set sw=3 ts=3 noet nu:
