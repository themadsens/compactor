/**
 * (C) Copyright 2014 flemming.madsen at madsensoft.dk.
 **                                                   MarkDown
 * NMEA Wind
 * =========
 * Convert VHW wind speed & direction sentences to analog
 * output suitable for VDO series instrument
 *
 * Timer0
 * ------
 * Timer 0 at baudrate => Top = CPU_FREQ / BAUDRATE
 *
 * Timer
 * -----
 * App timer at half second resolution. Updated each BAUDRATE / 100
 *
 */

#include <stdint.h>
#include <string.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
//#include <avr/cpufunc.h>
#include "util.h"
#include <util/delay.h>
#include "trigint_sin8.h"
#define CPU_FREQ    F_CPU

#define _NOP() __asm__ __volatile__("nop")

#if defined (__AVR_ATtiny24__) || defined (__AVR_ATtiny44__)

#define WIND_PIN    SBIT(PORTA, PA1)
#define ACT_PIN     SBIT(PORTA, PA7)
#define ACT_OUT     SBIT(DDRA,  PA7)
#define SRXD_PIN    SBIT(PINA,  PA0)
#define SRXD_PU     SBIT(PORTA, PA0)
#define SRXD_IEN    SBIT(GIMSK, INT0)
#define SER_SCALE   1
#define TIMER_SCALE 1
#define ISR_LEAD    4 // Determined empiric @ 12 MHz FIXME

#else
#error "Must be one of ATtiny24 ATtiny44"
#endif

#define DBG_PIN    SBIT(PORTA, PA5)
#define DBG_OUT    SBIT(DDRA,  PA5)
#ifdef DEBUG
#define DBG_SET(x) (DBG_PIN = x)
#define DBG_INIT(x) (DBG_OUT = x)
#else
#define DBG_SET(x)
#define DBG_INIT(x)
#endif

// Phase correct PWM, TOP = FF -> WGM = 1, Prescale = 1 
#define TIMER_TOP   255
// @ 12 MHz = 23437
#define TIMER_PULSE (CPU_FREQ / (TIMER_TOP + 1) / 2) 
#define CSEC_TOP    (TIMER_PULSE / 100)

#define BAUDRATE     4800
// @ 12 MHz = 2500
#define SERIAL_TOP  (((CPU_FREQ + BAUDRATE / 2) / BAUDRATE / SER_SCALE) - 1)

uint8_t nmeaState; // Counter in NMEALead or state
uint8_t actTimer; // Minimum activity light pulse width

// Sentence: $WIMWV,30.0,R,1.2,N,A
// Example:  $WIMWV,214.8,R,0.1,K,A*28
//
//Field# 	Example 	Description
//     1 	214.8 	Wind Angle
//     2 	R 	      Reference: R = Relative, T = True
//     3 	0.1    	Wind Speed
//     4 	K 	      Units: M = m/s, N = Knots, K = km per hour

const char NMEALead[] PROGMEM = "\n$\a\aMWV,";

enum  {
	NMEA_LEAD_MAX      = 8 ,
	NMEA_ST_ANGLE_INT  = 9 ,
	NMEA_ST_ANGLE_FRAC = 10,
	NMEA_ST_ANGLE_UNIT = 11,
	NMEA_ST_WIND_INT   = 12,
	NMEA_ST_WIND_FRAC1 = 13,
	NMEA_ST_WIND_FRAC  = 14,
	NMEA_ST_WIND_UNIT  = 15,
	NMEA_ST_WIND_VAL   = 16,
};

uint16_t windAngle = 75;
uint16_t windSpeed = 100; // In 1/10 knots

uint16_t windAngle_;
uint16_t windSpeed_;


void NMEADecode(int ch)
{
	volatile uint32_t num;
	//num = (uint16_t) strchr_P(NMEALead, ch); //FIXME: Seems to fix compiler error
#if 0
			//DEBUG
			if (NULL != strchr_P(NMEALead, ch)) {
				num = ch;
				DBG_SET(1);
			}
#endif
	num = ch - '0';
	if (nmeaState < NMEA_LEAD_MAX) {
		char test = pgm_read_byte(&NMEALead[nmeaState]);
		if (test == '\a' || ch == test) {
			nmeaState++;
		}
		else {
			nmeaState = 0;
		}
		if (nmeaState == NMEA_LEAD_MAX) {
			ACT_PIN = 0;                 // Activity light ON
			actTimer = 1;
			windAngle_ = 0;
		}
	}
	else if (nmeaState == NMEA_ST_ANGLE_INT) {
		if (num < 10)
			windAngle_ = windAngle_ * 10 + num;
		else if (ch == '.')
			nmeaState = NMEA_ST_ANGLE_FRAC;
		else if (ch == ',') {
			nmeaState = NMEA_ST_ANGLE_UNIT;
		}
	}
	else if (nmeaState == NMEA_ST_ANGLE_FRAC) {
		if (ch == ',')
			nmeaState = NMEA_ST_ANGLE_UNIT;
		else if (num >= 10)
			nmeaState = 0; // Start over
	}
	else if (nmeaState == NMEA_ST_ANGLE_UNIT) {
		if (ch == ',')
			nmeaState = NMEA_ST_WIND_INT;
		else if (ch != 'R')
			nmeaState = 0; // Start over
	}
	else if (nmeaState == NMEA_ST_WIND_INT) {
		if (num < 10)
			windSpeed_ = windSpeed_ * 10 + num;
		else if (ch == '.')
			nmeaState = NMEA_ST_WIND_FRAC1;
		else if (ch == ',') {
			windAngle_ = windSpeed_ * 10;
			nmeaState = NMEA_ST_WIND_UNIT;
		}
		else
			nmeaState = 0; // Start over
	}
	else if (nmeaState == NMEA_ST_WIND_FRAC1) {
		if (num < 10) {
			windSpeed_ = windSpeed_ * 10 + num;
			nmeaState = NMEA_ST_WIND_FRAC;
		}
		else
			nmeaState = 0;
	}
	else if (nmeaState == NMEA_ST_WIND_FRAC) {
		if (ch == ',')
			nmeaState = NMEA_ST_WIND_UNIT;
		else if (num >= 10)
			nmeaState = 0; // Start over
	}
	else if (nmeaState == NMEA_ST_WIND_UNIT) {
		if (ch == ',')
			nmeaState = NMEA_ST_WIND_VAL;
		else if (ch == 'K')
			windSpeed_ = windSpeed_ * 1000 / 1852;
		else if (ch == 'M')
			windSpeed_ = windSpeed_ * 3600 / 1852;
		else if (ch == 'N')
			_NOP();
		else
			nmeaState = 0; // Start over
	}
	else if (nmeaState == NMEA_ST_WIND_VAL) {
		if (ch == 'A') {
			windSpeed = windSpeed_;
			windAngle = windAngle_;
		}
	}
	else
		nmeaState = 0; // Start over
}

uint16_t csecCnt = 0;

static uint8_t srx_cur;
static uint8_t srx_pending;
static uint8_t srx_data;
static uint8_t srx_state;

ISR(TIM1_COMPA_vect)
{
	register uint8_t i;

	srx_state--;
	switch (srx_state)
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
		}
	}
	if (!srx_state)
	{
		// Start bit interrupt enable.
		// Ie. Cancel any half-received on next level change
      SRXD_IEN = 1;
		// Stop timer
		TCCR1B &= ~BIT(CS10);
		DBG_SET(1);
	}
}

// Start bit detect ISR
ISR(INT0_vect)
{
	// Scan again in 0.5 bit time. Scan in middle of bit
#define LEAD ((SERIAL_TOP / 2) + ISR_LEAD)
   TCNT1H = LEAD >> 8;
   TCNT1L = LEAD  & 0xff;
	DBG_SET(0);

   SRXD_IEN = 0;                // Disable this interrupt until char RX
	srx_state = 10;
	// Start timer
	TCCR1B |= BIT(CS10);
}

ISR(TIM0_OVF_vect)
{
	csecCnt++;
}

static inline u8 ugetchar(void)			// wait until byte received
{
   while (!srx_pending)
      sleep_cpu();
   srx_pending = 0;
   return srx_cur;
}

uint16_t windPulse;
uint16_t curAngle;
int main(void)
{
	// Might save a few bytes here with a single assignment
	SRXD_PU    = 1;
	ACT_OUT    = 1;
	DBG_INIT(1);

	// 1. Pin change interrupt enable
	// 2. Timer 0 connected to main clock. Prescale=1 PHC-PWM TOP=FF
	// 3. Timer 1 running at baud rate  -- with interrupt enabled
	MCUCR = BIT(ISC01);    // 1.
	TCCR0A = BIT(COM0A1) | BIT(COM0B1) | BIT(WGM00);   // 2.
	TCCR0B = BIT(CS00);    // 2. 
	TCCR1A = 0;            // 2.
	TCCR1B = BIT(WGM12);   // 2.
	OCR1AH = SERIAL_TOP>>8;// 4.
	OCR1AL = SERIAL_TOP&0xff;// 4.
	TIMSK0 = BIT(TOIE0);  // 4.
	TIMSK1 = BIT(OCIE1A);  // 4.

	SRXD_IEN = 1;       // Arm start bit interrupt

	set_sleep_mode(SLEEP_MODE_IDLE);
   sei();                       // Rock & roll
	while(1) {
		// Rest here until next timer overrun
		sleep_enable();
	   sleep_cpu();
		sleep_disable();


		// Read NMEA stram
		if (srx_pending) {
			NMEADecode(ugetchar());
		}

		if (windSpeed > 2 && 0 == --windPulse) {
			WIND_PIN ^= 1;
			// 100 hz at 60 kts. 60 kts = 600
			windPulse = TIMER_PULSE * 600 / 100 / 2 / windSpeed;
		}


		// Main timer
		if (csecCnt >= CSEC_TOP) {
			csecCnt -= CSEC_TOP;
		}
		else
			continue;

		/**
		 * Henceforth every 1/100 second
		 */

		register int incr = 1;
		if (curAngle > windAngle) {
			if (curAngle - windAngle < 180)
				incr = -1;
		}
		else 
			if (windAngle - curAngle > 180)
				incr = -1;
		curAngle = (curAngle + incr + 360) % 360;

		// TRIGINT_MAX / 360 ~= 45.51 -- Keep it in 16bit!
		register int deg = curAngle;
		register int8_t sin = trigint_sin8(deg * 45 + (deg * 51 / 100));
		deg = (90 - deg + 360) % 360;
		register int8_t cos = trigint_sin8(deg * 45 + (deg * 51 / 100));
		OCR0A = sin;
		OCR0B = cos;

		actTimer++;
		if (actTimer > 5)
			ACT_PIN = 1;                 // Activity light OFF
	}
}

// vim: set sw=3 ts=3 noet nu:
