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

//FIXME Relay only activated on every other test button press

#include <stdint.h>
#include <string.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
//#include <avr/cpufunc.h>
#include "util.h"
#include <util/delay.h>
#define CPU_FREQ    F_CPU

#define _NOP() __asm__ __volatile__("nop")

#if defined (__AVR_ATtiny24__) || defined (__AVR_ATtiny44__)

#define BUZZER_PIN SBIT(PORTA, PA2)
#define BUZZER_OUT SBIT(DDRA,  PA2)
#define LED_ON(x) (SBIT(PORTB, PB2) = x ? 0 : 1)
#define LED_OUT    SBIT(DDRB,  PB2)
#define ACT_PIN    SBIT(PORTA, PA7)
#define ACT_OUT    SBIT(DDRA,  PA7)
#define RELAY_PIN  SBIT(PORTA, PA3)
#define RELAY_OUT  SBIT(DDRA,  PA3)
#define PATRN_PIN  SBIT(PINA,  PA4)
#define PATRN_PU   SBIT(PORTA, PA4)
#define PULSE_PIN  SBIT(PINA,  PA5)
#define PULSE_PU   SBIT(PORTA, PA5)
#define BUTTON_PIN SBIT(PINA,  PA1)
#define BUTTON_PU  SBIT(PORTA, PA1)
#define SRXD_PIN   SBIT(PINA,  PA0)
#define SRXD_PU    SBIT(PORTA, PA0)
#define SRXD_IEN   SBIT(PCMSK0,PCINT0)
#define SER_SCALE  1
#define ISR_LEAD   4 // Determined empiric @ 12 MHz FIXME

#else
#error "Must be one of ATtiny24 ATtiny44"
#endif

#define DBG_PIN    SBIT(PORTA, PA5)
#define DBG_OUT    SBIT(DDRA,  PA5)
#ifdef DEBUG
#define DBG_SET(x) (DBG_PIN = x)
#define DBG_INIT(x) (DBG_OUT = x)
#else
//#define DBG_SET(x) LED_ON(x)
#define DBG_SET(x)
#define DBG_INIT(x)
#endif

#define BAUDRATE     38400
#define SERIAL_TOP  (((CPU_FREQ + BAUDRATE / 2) / BAUDRATE / SER_SCALE) - 1)
#define BUZZER_PULSE 19200 // Plays nize with buzzer frequencies
#define CSEC_TOP    (BUZZER_PULSE / 100)

#define BUZZER_TOP  ((CPU_FREQ + BUZZER_PULSE / 2) / BUZZER_PULSE / 8) - 1 
#define BUZZER1_TOP (BUZZER_PULSE / 2400 / 2)
#define BUZZER2_TOP (BUZZER_PULSE / 3200 / 2)
#define AIS_TIMEOUT (600 * 2)


// Maintained by timer interrupt. 
uint16_t upTime;

// Nonzero is active alarm. 
// Updated on every message
// Zeroed when no message seen in 10 minutes
uint16_t alarmStart = 0;
#define ALARM_ON (alarmStart != 0)
#define BUTTON_ON ((btnPressed & 0x7fff) > 5)

void alarmCond(void);

#define AIST_LEAD_MAX 14
#define AIST_MMSI_CHECK 40 // Must divide with 8
#define AIST_CHECK_NAVSTATE 30
#define AIST_MMSI_MASK  0x7
uint8_t aisState; // Counter in AISLead or state
uint8_t actTimer; // Minimum activity light pulse width
uint8_t testTimer; // Flash red when test message received
uint16_t armTestPressed = 0;

const char AISLead[] PROGMEM = "!AIVDM,1,1,,\a,1";
#define MMSI_HIGH 979999999
#define MMSI_LOW  970000000
uint32_t MmsiNo;

void AISDecode(int ch)
{
	volatile uint32_t num;
	num = (uint16_t) strchr_P(AISLead, ch); //FIXME: Seems to fix compiler error
#if 0
			//DEBUG
			if (NULL != strchr_P(AISLead, ch)) {
				num = ch;
				DBG_SET(1);
			}
#endif
	num = ch - (ch > 'W' ? 56 : 48);
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
		}
		if (aisState > 3) {
			ACT_PIN = 0;                 // Activity light ON
			actTimer = 1;
		}
	}
	else if (aisState == AIST_LEAD_MAX) {
		if (ch == '1') {     // Type 01: Position update
			aisState = AIST_MMSI_CHECK;
			MmsiNo = 0;
		}
		else {
			aisState = 0;
		}
	}
	else if (aisState == AIST_MMSI_CHECK) {
		MmsiNo = (num & 0xF) << 26;
		aisState++;
	}
	else if (aisState == AIST_MMSI_CHECK + 5) {
		MmsiNo |= num >> 4;
		if (MmsiNo >= MMSI_LOW && MmsiNo <= MMSI_HIGH) {
			if ((num & 0xf) != 15 || armTestPressed) // Test message only issues alarm
				alarmCond();                          // after recent button press
			else {
				LED_ON(1);
				testTimer = 1;
			}
		}
		aisState = 0; // Start over
	}
	else if ((aisState & ~AIST_MMSI_MASK) == AIST_MMSI_CHECK) {
		MmsiNo |= num << (((AIST_MMSI_CHECK + 5 - aisState) * 6) - 4);
		aisState++;
	}
	else
		aisState = 0; // Start over

}

// Timer0 ISR
uint8_t buzzerOn   = 0;
#define BuzzerTop  ((upTime & 1) ? BUZZER1_TOP : BUZZER2_TOP)
uint16_t buzzerCnt = 0;

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
ISR(PCINT0_vect)
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

ISR(TIM0_COMPA_vect)
{
	if (buzzerOn) {
		if (++buzzerCnt >= BuzzerTop) {
			buzzerCnt = 0;
			BUZZER_PIN ^= 1;
		}
	}
	else
		BUZZER_PIN = 0;

	csecCnt++;
}

static inline u8 ugetchar(void)			// wait until byte received
{
   while (!srx_pending)
      sleep_cpu();
   srx_pending = 0;
   return srx_cur;
}

static inline uint8_t hornPattern(void)
{
	if (1 == PATRN_PIN)
		return upTime & 1;

	switch (upTime % 22) {
	 case 0:  return 1;
	 case 1:  return 0;
	 case 2:  return 1;
	 case 3:  return 0;
	 case 4:  return 1;
	 case 5:  return 0;
	 case 6:  return 1;
	 case 7:  return 1;
	 case 8:  return 0;
	 case 9:  return 1;
	 case 10: return 1;
	 case 11: return 0;
	 case 12: return 1;
	 case 13: return 1;
	 case 14: return 0;
	 case 15: return 1;
	 case 16: return 0;
	 case 17: return 1;
	 case 18: return 0;
	 case 19: return 1;
	 case 20: return 0;
	 case 21: return 0;
	}
	return 0;
}

uint8_t relayOn;
void alarmCond(void)
{
	if (!ALARM_ON) {
		buzzerOn = 1;
		relayOn = 1;
	}
	alarmStart = upTime ?: 1;
}

uint8_t hsecCnt;
uint16_t btnPressed;
int main(void)
{
	// Might save a few bytes here with a single assignment
	BUZZER_OUT = 1;
	LED_OUT    = 1;
	ACT_OUT    = 1;
	RELAY_OUT  = 1;
	BUTTON_PU  = 1;
	SRXD_PU    = 1;
	PULSE_PU   = 1;
	PATRN_PU   = 1;
	DBG_INIT(1);

	ACT_PIN = 0;
	LED_ON(1);

	// 1. Pin change interrupt enable
	// 2. Waveform Generation is CTC (Clear timer on compare) 
	// 3. Timer connected to main clock. Prescale = 8
	// 4. Timer running at baud rate  -- with interrupt enabled
	MCUCR = BIT(ISC01);    // 1.
	GIMSK = BIT(PCIE0);    // 1.
	TCCR0A = BIT(WGM01);   // 2.
	TCCR0B = BIT(CS01);    // 2. 
	TCCR1A = 0;            // 2.
	TCCR1B = BIT(WGM12);   // 2.
	OCR0A = BUZZER_TOP;    // 4.
	OCR1AH = SERIAL_TOP>>8;// 4.
	OCR1AL = SERIAL_TOP&0xff;// 4.
	TIMSK0 = BIT(OCIE0A);  // 4.
	TIMSK1 = BIT(OCIE1A);  // 4.

	SRXD_IEN   = 1;       // Arm start bit interrupt
	buzzerOn = 1;

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
			csecCnt -= CSEC_TOP;
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
					// Test buzzer / led / horn -- Toggle Sart test armed mode
					armTestPressed = armTestPressed ? 0 : (upTime ?: 1);
				}
				else { 
					buzzerOn = 0;
				}
				hsecCnt = 0;          // Synchronize tone alternate interval
			}
			if (btnPressed < 32000)
				btnPressed++;
		}
		else {
			if (!ALARM_ON && upTime > 0 && testTimer == 0) {
				buzzerOn = 0;
				relayOn = 0;
			}
			btnPressed = 0;
		}
		if (!ALARM_ON && armTestPressed && 0 == (upTime & 0x3)) {
			// brief flash every 2 sec
			if (hsecCnt == 0)
				LED_ON(1);
			else
				LED_ON(0);
		}

		if (ALARM_ON && btnPressed == 950) {   // Mark re/arm with a beep
			buzzerOn = 1;
			armTestPressed = 0;
		}
		if (ALARM_ON && btnPressed == 1000) {
			// 10 second press cancels / re-arms alarm
			btnPressed = 0x8000; // Suspend button until released & pressed
			buzzerOn = 0;
			alarmStart = 0;
		}

		if (btnPressed == 50) {
			// Only brief presses starts test mode
			armTestPressed = 0;
		}
		if (btnPressed == 100 && !ALARM_ON) {
			buzzerOn = 1;
		}
		if (btnPressed == 200) {
			if (ALARM_ON)
				relayOn = 0; // Turn relay / horn off
			else
				relayOn = 1; // Test relay / horn
		}

		if (actTimer < 100)
			actTimer++;
		if (actTimer > 5 && aisState == 0)
			ACT_PIN = 1;                 // Activity light OFF

		if (testTimer > 0 && !ALARM_ON) {
			if (testTimer < 200)
				testTimer++;
			if (testTimer == 5 || testTimer == 55 || testTimer == 105)
				LED_ON(0);                // Ack blink OFF
			else if (testTimer == 50 || testTimer == 100)
				LED_ON(1);                // Ack blink ON 
			if (testTimer == 100) {
				hsecCnt = 0;
				buzzerOn = 1;
			}
			else if (testTimer == 150) {
				testTimer = 0;
				buzzerOn = 0;
			}
		}

		if  (relayOn && (PULSE_PIN == 1 || hornPattern()))
			RELAY_PIN = 1;
		else
			RELAY_PIN = 0;
		
		hsecCnt++;
		if (hsecCnt >= 50)
			hsecCnt = 0;
		else
			continue;

		/**
		 * Henceforth every 1/2 second
		 */

		if (0 == upTime) { // End start buzzer / LED test
			buzzerOn = 0;
			LED_ON(0);
			ACT_PIN = 1;
		}
		upTime++;

		if ((upTime - alarmStart) >= AIS_TIMEOUT) {
			// Cancel / re-arm alarm if nothing seen in 10 min.
			alarmStart = 0;
		}
		if ((upTime - armTestPressed) >= 600) {
			// Cancel listening for test messages after 5 min
			armTestPressed = 0;
		}

		// Isophase heartbeat
		if ((upTime & 0x1) == 0) {
			LED_ON(0);
		}
		else {
			if (ALARM_ON || BUTTON_ON)
				LED_ON(1);
		}

	}
}

// vim: set sw=3 ts=3 noet nu:
