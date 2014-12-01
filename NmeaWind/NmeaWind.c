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

#if defined (__AVR_ATtiny24__) || defined (__AVR_ATtiny44__) || defined (__AVR_ATtiny84__)

#define WIND_PIN    SBIT(PORTA, PA1)
#define WIND_OUT    SBIT(DDRA,  PA1)
#define ACT_PIN     SBIT(PORTA, PA2)
#define ACT_OUT     SBIT(DDRA,  PA2)
#define SIN_OUT     SBIT(DDRA,  PA7)
#define COS_OUT     SBIT(DDRB,  PA2)
#define SRXD_PIN    SBIT(PINA,  PA0)
#define SRXD_PU     SBIT(PORTA, PA0)
#define SRXD_IEN    SBIT(PCMSK0,PCINT0)
#define SER_SCALE   1
#define TIMER_SCALE 1
#define ISR_LEAD    4 // Determined empiric @ 12 MHz FIXME

#else
#error "Must be one of ATtiny24 ATtiny44 ATtiny84"
#endif

#define DEBUG
#define DBG_PIN    SBIT(PORTA, PA3)
#define DBG_OUT    SBIT(DDRA,  PA3)
#ifdef DEBUG
#define DBG_SET(x) (DBG_PIN = x)
#define DBG_INIT(x) (DBG_OUT = x)
#else
#define DBG_SET(x)
#define DBG_INIT(x)
#endif

// Phase correct PWM, TOP = FF -> WGM = 1, Prescale = 1 
#define TIMER_TOP   255
// @ 16 MHz = 23437
#define TIMER_PULSE (CPU_FREQ / (TIMER_TOP + 1) / 2) 
#define CSEC_TOP    (TIMER_PULSE / 100)

// @ 12 MHz = 2500
#define SERIAL_TOP  (((CPU_FREQ + BAUDRATE / 2) / BAUDRATE / SER_SCALE) - 1)

uint8_t nmeaState; // Counter in NMEALead or state
int8_t  actTimer; // Minimum activity light pulse width

uint16_t csecCnt;     // < 1000 => Since boot; > 1000 Since last NMEA Msg
uint16_t csecInc = 0; // When to tick csecCnt

//           0       10         20   26
//           +--------+----------+----+----
// Example:  $WIMWV,214.8,R,20.1,K,A*28   
//
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
	NMEA_ST_WIND_END   = 17,
};

uint16_t windAngle = 75;
uint16_t windSpeed = 100; // In 1/10 knots

uint16_t windAngle_;
uint16_t windSpeed_;


void NMEADecode(int ch)
{
	volatile uint32_t num;
	char c = ch;
	num = (uint16_t) strchr_P(NMEALead, ch); //FIXME: Seems to fix compiler error
	num = c - '0';
	if (nmeaState < NMEA_LEAD_MAX) {
		char test = pgm_read_byte(&NMEALead[nmeaState]);
		if (test == '\a' || ch == test) {
			nmeaState++;
			if (nmeaState == 1) {
				ACT_PIN = 0;                 // Activity light ON
				actTimer = 0;
			}
		}
		else {
			nmeaState = 0;
		}
		if (nmeaState == NMEA_LEAD_MAX) {
			windAngle_ = 0;
			windSpeed_ = 0;
			nmeaState = NMEA_ST_ANGLE_INT;
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
			nmeaState = 0; // Error
	}
	else if (nmeaState == NMEA_ST_ANGLE_UNIT) {
		if (ch == ',')
			nmeaState = NMEA_ST_WIND_INT;
		else if (ch != 'R')
			nmeaState = 0; // Error
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
			nmeaState = 0; // Error
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
			nmeaState = 0; // Error
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
			nmeaState = 0; // Error
	}
	else if (nmeaState == NMEA_ST_WIND_VAL) {
		if (ch == 'A') {
			windSpeed = windSpeed_;
			windAngle = windAngle_;
			csecCnt = 1000;
			nmeaState = NMEA_ST_WIND_END;
		}
		else
			nmeaState = 0;               // Error
	}
	else if (nmeaState == NMEA_ST_WIND_END) {
		if (ch == '\r') {
			nmeaState = 0;               // Start over
			return;
		}
		else if (num > 9 && ch != '*')
			nmeaState = 0;               // Error
	}
	else
		nmeaState = 0; // Start over
	if (0 == nmeaState) {
		ACT_PIN = 1;  // Error -- Activity light OFF
	}
}

static uint8_t srx_cur;
static uint8_t srx_pending;
static uint8_t srx_data;
static uint8_t srx_state;

ISR(TIM1_COMPA_vect)
{
	register uint8_t i;

	//DBG_PIN ^= 1;
	srx_state--;
	switch (srx_state)
	{

	 case 9:
		if (SRXD_PIN == 0)     // start bit valid
			break;
		srx_state = 0;
		break;

	 case 7:
		DBG_SET(0);
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
			DBG_SET(1);
		}
	}
	if (!srx_state)
	{
		// Start bit interrupt enable.
		// Ie. Cancel any half-received on next level change
      SRXD_IEN = 1;
		// Stop timer
		TCCR1B &= ~BIT(CS10);
	}
}

// Start bit detect ISR
ISR(PCINT0_vect)
{
	// Scan again in 0.5 bit time. Scan in middle of bit
#define LEAD ((SERIAL_TOP / 2) + ISR_LEAD)
   TCNT1H = LEAD >> 8;
   TCNT1L = LEAD  & 0xff;

	if (srx_cur != 'U')
	DBG_SET(0);

   SRXD_IEN = 0;                // Disable this interrupt until char RX
	srx_state = 10;
	// Start timer
	TCCR1B |= BIT(CS10);
}

ISR(TIM0_OVF_vect)
{
	csecInc++;
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
	SRXD_PU   = 1;
	ACT_OUT   = 1;
	WIND_OUT  = 1;
	SIN_OUT   = 1;
	COS_OUT   = 1;
	// TODO Do we need to set dir. for pwm outputs
	DBG_INIT(1);

	// 1. Pin change interrupt enable
	// 2. Timer 0 connected to main clock. Prescale=1 PHC-PWM TOP=FF
	// 3. Timer 1 running at baud rate  -- with interrupt enabled
	MCUCR = BIT(ISC01);                              // 1.
	GIMSK = BIT(PCIE0);                              // 1.
	TCCR0A = BIT(COM0A1) | BIT(COM0B1) | BIT(WGM00); // 2.
	TCCR0B = BIT(CS00);                              // 2.
	TIMSK0 = BIT(TOIE0);                             // 2.
	TCCR1A = 0;                                      // 3.
	TCCR1B = BIT(WGM12);                             // 3.
	OCR1AH = SERIAL_TOP>>8;                          // 3.
	OCR1AL = SERIAL_TOP&0xff;                        // 3.
	TIMSK1 = BIT(OCIE1A);                            // 3.

	ACT_PIN = 0;        // Activity light ON for 1 sec on boot
	DBG_SET(1);         // Debug LED off initially
	actTimer = -50;

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
		if (csecInc >= CSEC_TOP) {
			csecInc -= CSEC_TOP;
		}
		else
			continue;

		/**
		 * Henceforth every 1/100 second
		 */
		if (csecCnt < 60000)
			csecCnt++;

		if (csecCnt < 3000) {
			register int incr = 1;
			if (curAngle > windAngle) {
				if (curAngle - windAngle < 180)
					incr = -1;
			}
			else if (windAngle - curAngle > 180)
				incr = -1;
			curAngle = (curAngle + incr + 360) % 360;
		}
		else {
			curAngle = 0;
			windSpeed = 0;
		}

		// TRIGINT_MAX / 360 ~= 45.51 -- Keep it in 16bit!
		register int deg = curAngle;
		register uint8_t sin = trigint_sin8u(deg * 45 + (deg * 51 / 100));
		deg = (90 - deg + 360) % 360;
		register uint8_t cos = trigint_sin8u(deg * 45 + (deg * 51 / 100));
		OCR0A = (sin * 145 / 255) + 110;
		OCR0B = (cos * 145 / 255) + 110;

		if (actTimer < 100)
			actTimer++;
		if (csecCnt > 100 && actTimer == 9) {
			ACT_PIN = 1;                 // Activity light OFF
		}
		if (csecCnt == 100) {
			ACT_PIN = 1;                 // Activity light OFF
			SRXD_IEN = 1;       // Arm start bit interrupt
		}
	}
}

// vim: set sw=3 ts=3 noet nu:
