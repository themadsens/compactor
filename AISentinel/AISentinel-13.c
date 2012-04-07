/**
 * (C) Copyright 2012 flemming.madsen at madsensoft.dk. See libuevent.h
 **                                                   MarkDown
 * AIS Sentinel
 * ============
 * Fires the buzzer and LED if AIS MMSI prefix 97 is seen.
 * Both signifies a transmitting AIS SART
 * Test mode (nav state 15) only if button recently pressed
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
 * App timer at half second resolution. Updated each BAUDRATE / 100
 * Toggle buzzer freq every 500 ms if buzzerOn
 * Toggle LED every 500 ms if ledOn
 *
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/cpufunc.h>
#include "util.h"

#if defined (__AVR_ATtiny13__)

#define CPU_FREQ    (9600 * 1000UL)
#define BUZZER_PIN SBIT(PORTB, PB3)
#define BUZZER_OUT SBIT(DDRB,  PB3)
#define LED_PIN    SBIT(PORTB, PB4)
#define LED_OUT    SBIT(DDRB,  PB4)
#define ACT_PIN    SBIT(PORTB, PB1)
#define ACT_OUT    SBIT(DDRB,  PB1)
#define DBG_PIN    SBIT(PORTB, PB1)
#define DBG_OUT    SBIT(DDRB,  PB1)
#define DBG_SET(x) 
#define BUTTON_PIN SBIT(PINB,  PB2)
#define BUTTON_PU  SBIT(PORTB, PORTB2)
#define SRXD_PIN   SBIT(PINB,  PB0)
#define SRXD_PU    SBIT(PORTB,  PB0)
#define SRXD_IEN   SBIT(PCMSK, PCINT0)
#elif defined (__AVR_ATmega16__)
#define CPU_FREQ    (16 * 1000 * 1000UL)
#define BUZZER_PIN SBIT(PORTD, PD5)
#define BUZZER_OUT SBIT(DDRD,  PD5)
#define LED_PIN    SBIT(PORTB, PB3)
#define LED_OUT    SBIT(DDRB,  PB3)
#define ACT_PIN    SBIT(PORTD, PD7)
#define ACT_OUT    SBIT(DDRD,  PD7)
#define DBG_OUT    SBIT(DDRD,  PD4)
#define DBG_PIN    SBIT(PORTD, PD4)
#define DBG_SET(x)(DBG_PIN = x)
#define BUTTON_PIN SBIT(PIND,  PD6)
#define BUTTON_PU  SBIT(PORTD, PORTD6)
#define SRXD_PIN   SBIT(PIND,  PD3)
#define SRXD_PU    SBIT(PORTD, PD3)
#define SRXD_IEN   SBIT(GICR,  INT1)
#else
#error "Must be one of ATtiny13 ATtiny24 ATtiny44 ATmega16"
#endif

#define BAUDRATE    38400
#define CSEC_TOP    (BAUDRATE / 100)
#define TIMER0_TOP  (CPU_FREQ / BAUDRATE / 8)
#define BUZZER1_TOP (BAUDRATE / 3200 / 2)
#define BUZZER2_TOP (BAUDRATE / 2400 / 2)
#define AIS_TIMEOUT (600 * 2)

#define F_CPU CPU_FREQ
#include <util/delay.h>

// Maintained by timer interrupt. 
uint16_t upTime;

char AISLead[] PROGMEM = "\n!AIVDM,1,1,,\a,";

// Nonzero is active alarm. 
// Updated on every message
// Zeroed when no message seen in 10 minutes
uint16_t alarmStart = 0;
#define ALARM_ON (alarmStart != 0)

void alarmCond(void);

#define AIST_LEAD_MAX 15
#define AIST_CHECK_NAVSTATE 20
#define AIST_MMSI_ABOVE 32
#define AIST_MMSI_BELOW 40
#define AIST_MMSI_MASK  0x7
uint8_t aisState; // Counter in AISLead or state
uint16_t recentButtonPress = 0;

/*
 * MMSI High 979999999 == 0b 1110 100110 100110 011100 111111 11
 * MMSI Low  970000000 == 0b 1110 011101 000100 000110 100000 00
 * NEAT: Two LSB bits with navstate turns out as dont cares :-)
 */
//                          1110 100110 100110 011100 111111
uint8_t MMSIHi[] PROGMEM = {0x0E,  0x26,  0x26,  0x1C,  0x3F};
//                          1110 011101 000100 000110 100000
uint8_t MMSILo[] PROGMEM = {0x0E,  0x1D,  0x04,  0x06,  0x20};

void AISDecode(int ch)
{
	register uint8_t t;
	register uint8_t num = ch - (ch > 'W' ? 56 : 48);
	if (0 == (aisState & AIST_MMSI_MASK)) // First char in sequence 
		num &= 0xf;                        // Clear repeat indicator
	if (aisState < AIST_LEAD_MAX) {
			char test = pgm_read_byte(&AISLead[aisState]);
		if (test == '\a' || ch == test) {
			aisState++;
		}
		else {
			if (aisState > 10)
				_NOP();
			aisState = 0;
			ACT_PIN = 1;                 // Activity light
			DBG_SET(0);
		}
		if (aisState > 4) {
			ACT_PIN = 0;                 // Activity light
			DBG_SET(1);
		}
	}
	else if (aisState == AIST_LEAD_MAX) {
		if (ch == '1') {     // Type 01: Position update
			aisState = AIST_MMSI_ABOVE; // Pick one
		}
	}
	else if ((aisState & ~AIST_MMSI_MASK) == AIST_MMSI_ABOVE) {
		t = pgm_read_byte(&MMSILo[aisState - AIST_MMSI_ABOVE]);
		if (num > t) {
			aisState = aisState - AIST_MMSI_ABOVE + AIST_MMSI_BELOW;
         AISDecode(ch);         // Must be below max as well
		}
		else if (ch < t)
			aisState = 0;
		else
			aisState++;
	}
	else if ((aisState & ~AIST_MMSI_MASK) == AIST_MMSI_BELOW) {
		t = pgm_read_byte(&MMSIHi[aisState - AIST_MMSI_BELOW]);
		if (ch > t)
			aisState = 0;
		else
			aisState++;
	}
	else if (aisState == AIST_CHECK_NAVSTATE) {
		if ((num & 0xf) != 15 || recentButtonPress) // Test message only on
			alarmCond();                             // recent button press
		aisState = 0; // Start over
	}
	else
		aisState = 0; // Start over

	if (aisState == AIST_MMSI_ABOVE + 5 || aisState == AIST_MMSI_BELOW + 5) {
		aisState = AIST_CHECK_NAVSTATE;
	}
}

// Timer0 ISR
uint8_t buzzerOn   = 0;
#define BuzzerTop  ((upTime & 1) ? BUZZER1_TOP : BUZZER2_TOP)
uint16_t buzzerCnt = 0;

uint16_t csecCnt = 0;
int16_t  csecErr = 0;

static uint8_t srx_cur;
static uint8_t srx_pending;
static uint8_t srx_data;
static uint8_t srx_state;

#define SRXD_READ_LEAD 33 // 33 Cycles from TCNT0 set to read
#if defined (__AVR_ATtiny13__)
ISR(TIM0_COMPA_vect)
#elif defined (__AVR_ATmega16__)
ISR(TIMER0_COMP_vect)
#endif
{
   if (srx_state)
   {
      register uint8_t i;

      switch (--srx_state)
      {

       case 9:
         if (SRXD_PIN == 0)     // start bit valid
            break;
			srx_state = 0;
         break;

       default:
         i = srx_data >> 1;     // LSB first
         if (SRXD_PIN == 1)
            i |= 0x80;          // data bit = 1
         srx_data = i;
			break;

       case 0:
         if (SRXD_PIN == 1) {   // stop bit valid
				srx_cur = srx_data;
				srx_pending = 1;
				if (srx_cur >= 'A' && srx_cur <= 'z')
					_NOP();
         }
      }
   }
	if (!srx_state)
	{
		// Start bit interrupt enable.
		// Ie. Cancel any half-received on next level change
      SRXD_IEN = 1;
	}

	if (buzzerCnt++ >= BuzzerTop) {
		buzzerCnt = 0;
		if (buzzerOn)
			BUZZER_PIN ^= 1;
		else
			BUZZER_PIN = 0;
	}

	csecCnt++;
}

// Start bit detect ISR
#if defined (__AVR_ATtiny13__)
ISR(PCINT0_vect)
#elif defined (__AVR_ATmega16__)
ISR(INT1_vect)
#endif
{
	// Scan at 0.5 bit time.
	// This should also even out the timer error
	// Provided char interleave is truly random. Fat chance ..
	// Accumulating the timer error eats up too many instructions
   TCNT0 = (TIMER0_TOP / 2);

   SRXD_IEN = 0;                // Disable this interrupt until char RX
	srx_state = 10;
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
	if (!ALARM_ON) {
		buzzerOn = 1;
	}
	alarmStart = upTime ?: 1;
}

uint8_t hsecCnt = 0;
uint8_t btnPressed = 0;
int main(void)
{
	// Might save a few bytes here with a single assignment
	BUZZER_OUT = 1;
	LED_OUT    = 1;
	ACT_OUT    = 1;
	BUTTON_PU  = 1;
#if ! defined (__AVR_ATtiny13__)
	SRXD_PU    = 1;
	DBG_OUT    = 1;
#endif

	ACT_PIN = 1;

	// 1. Pin change interrupt enable
	// 2. Waveform Generation is CTC (Clear timer on compare) 
	// 3. Timer connected to main clock. Prescale = 8
	// 4. Timer0 running at baud rate  -- with interrupt enabled
#if defined (__AVR_ATtiny13__)
	GIMSK = BIT(PCIE);     // 1.
	TCCR0A = BIT(WGM01);   // 2.
	TCCR0B = BIT(CS01);    // 3.
	TIMSK0 = BIT(OCIE0A);  // 4.
	OCR0A = TIMER0_TOP;    // 4.
#elif defined (__AVR_ATmega16__)
	MCUCR = 0;//BIT(ISC10);    // 1.
	TCCR0 = BIT(WGM01);    // 2.
	TCCR0 |= BIT(CS01);    // 3.
	TIMSK = BIT(OCIE0);    // 4.
	OCR0 = TIMER0_TOP;     // 4.
#endif

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
		if (0 == BUTTON_PIN) {
			if (btnPressed == 5) {
				if (!ALARM_ON) {
					buzzerOn = 1;
					recentButtonPress = upTime ?: 1;
				}
				else
					buzzerOn ^= 1;
				hsecCnt = 0;          // Synchronize tone alternate interval
			}
			if (btnPressed < 100)
				btnPressed++;
		}
		else {
			if (btnPressed >= 5) {
				if (!ALARM_ON) {
					buzzerOn = 0;
				}
			}
			btnPressed = 0;
		}
		
		hsecCnt++;
		if (hsecCnt >= 50)
			hsecCnt = 0;
		else
			continue;

		/**
		 * Henceforth every 1/2 second
		 */
		upTime++;

		if ((upTime - alarmStart) >= AIS_TIMEOUT) {
			alarmStart = 0;
		}
		if ((upTime - recentButtonPress) >= AIS_TIMEOUT) {
			recentButtonPress = 0;
		}


		// Isophase heartbeat
		if ((upTime & 0x1) == 0) {
			LED_PIN = 1;
		}
		else {
			if (ALARM_ON || btnPressed >= 5)
				LED_PIN = 0;
		}

	}
}

// vim: set sw=3 ts=3 noet nu:
