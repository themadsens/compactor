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
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/cpufunc.h>
#include "util.h"
#include <util/delay.h>
#define CPU_FREQ    F_CPU

#if defined (__AVR_ATtiny24__)

#define BUZZER_PIN SBIT(PORTA, PORTA2)
#define BUZZER_OUT SBIT(DDRA,  PA2)
#define LED_ON(x) (SBIT(PORTB, PORTB2) = x ? 0 : 1)
#define LED_OUT    SBIT(DDRB,  PB2)
#define ACT_PIN    SBIT(PORTA, PORTA7)
#define ACT_OUT    SBIT(DDRA,  PA7)
#define RELAY_PIN  SBIT(PORTA, PORTA3)
#define RELAY_OUT  SBIT(DDRA,  PA3)
#define DBG_PIN    SBIT(PORTA, PORTA5)
#define DBG_OUT    SBIT(DDRA,  PA5)
#define DBG2_PIN   SBIT(PORTA, PORTA6)
#define DBG2_OUT   SBIT(DDRA,  PA6)
#define DBG_SET(x) (DBG_PIN = x)
#define BUTTON_PIN SBIT(PINA,  PA1)
#define BUTTON_PU  SBIT(PORTA, PORTA1)
#define SRXD_PIN   SBIT(PINA,  PA0)
#define SRXD_PU    SBIT(PORTA, PORTA0)
#define SRXD_IEN   SBIT(PCMSK0,PCINT0)
#define SER_SCALE  1
#define ISR_LEAD   4 // Determined empiric @ 12 MHz FIXME

#elif defined (__AVR_ATmega16__)
#define BUZZER_PIN SBIT(PORTB, PORTB1)
#define BUZZER_OUT SBIT(DDRD,  PB1)
#define LED_ON(x) (SBIT(PORTB, PORTB0) = x ? 1 : 0)
#define LED_OUT    SBIT(DDRB,  PB0)
#define ACT_PIN    SBIT(PORTD, PORTD7)
#define ACT_OUT    SBIT(DDRD,  PD7)
#define RELAY_PIN  SBIT(PORTB, PORTB3)
#define RELAY_OUT  SBIT(DDRB,  PB3)
#define DBG_PIN    SBIT(PORTD, PORTD4)
#define DBG2_OUT   SBIT(DDRD,  PD5)
#define DBG2_PIN   SBIT(PORTD, PORTD5)
#define DBG_OUT    SBIT(DDRD,  PD4)
#define DBG_SET(x)(DBG_PIN = x)
#define BUTTON_PIN SBIT(PIND,  PD6)
#define BUTTON_PU  SBIT(PORTD, PORTD6)
#define SRXD_PIN   SBIT(PIND,  PD3)
#define SRXD_PU    SBIT(PORTD, PORTD3)
#define SRXD_IEN   SBIT(GICR,  INT1)
#define SER_SCALE  8
#define ISR_LEAD   4 // Determined empiric @ 16 MHz

#else
#error "Must be one of ATtiny24 ATtiny44 ATmega16"
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
	volatile uint32_t num = ch - (ch > 'W' ? 56 : 48);
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
			if ((num & 0xf) != 15 || armTestPressed) // Test message only on
				alarmCond();                          // recent button press
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

#if defined (__AVR_ATtiny24__)
ISR(TIM1_COMPA_vect)
#elif defined (__AVR_ATmega16__)
ISR(TIMER0_COMP_vect)
#endif
{
	register uint8_t i;

	srx_state--;
	DBG_SET(srx_state & 1);
	switch (srx_state)
	{

	 case 9:
		if (SRXD_PIN == 0)     // start bit valid
			break;
		srx_state = 0;
		break;

	 default:
		i = srx_data >> 1;     // LSB first
		DBG_SET(SRXD_PIN);
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
#if defined (__AVR_ATtiny24__)
		TCCR1B &= ~BIT(CS10);
#elif defined (__AVR_ATmega16__)
		TCCR0 &= ~BIT(CS01);
#endif
		DBG_SET(1);
	}
	else
		DBG_SET(srx_state & 1);
}

// Start bit detect ISR
#if defined (__AVR_ATtiny24__)
ISR(PCINT0_vect)
#elif defined (__AVR_ATmega16__)
ISR(INT1_vect)
#endif
{
	// Scan again in 0.5 bit time. Scan in middle of bit
#if defined (__AVR_ATtiny24__)
#define LEAD ((SERIAL_TOP / 2) + ISR_LEAD)
   TCNT1H = LEAD >> 8;
   TCNT1L = LEAD  & 0xff;
#elif defined (__AVR_ATmega16__)
   TCNT0 = (SERIAL_TOP / 2) + ISR_LEAD;
#endif
	DBG_SET(0);

   SRXD_IEN = 0;                // Disable this interrupt until char RX
	srx_state = 10;
	// Start timer
#if defined (__AVR_ATtiny24__)
	TCCR1B |= BIT(CS10);
#elif defined (__AVR_ATmega16__)
	TCCR0 |= BIT(CS01);
#endif
}

#if defined (__AVR_ATtiny24__)
ISR(TIM0_COMPA_vect)
#elif defined (__AVR_ATmega16__)
ISR(TIMER2_COMP_vect)
#endif
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
		RELAY_PIN= 1;
	}
	alarmStart = upTime ?: 1;
}

uint8_t hsecCnt = 0;
uint16_t btnPressed = 0;
int main(void)
{
	// Might save a few bytes here with a single assignment
	BUZZER_OUT = 1;
	LED_OUT    = 1;
	ACT_OUT    = 1;
	RELAY_OUT  = 1;
	BUTTON_PU  = 1;
	SRXD_PU    = 1;
	DBG_OUT    = 1;
	DBG2_OUT   = 1;

	ACT_PIN = 0;
	LED_ON(1);

	// 1. Pin change interrupt enable
	// 2. Waveform Generation is CTC (Clear timer on compare) 
	// 3. Timer connected to main clock. Prescale = 8
	// 4. Timer running at baud rate  -- with interrupt enabled
#if defined (__AVR_ATtiny24__)
	MCUCR = BIT(ISC01);    // 1.
	GIMSK = BIT(PCIE0);    // 1.
	TCCR0A = BIT(WGM01);   // 2.
	TCCR0B = BIT(CS01);    // 2. 
	TCCR1A = 0;            // 2.
	TCCR1B = 0;            // 2.
	TCCR1B = BIT(WGM12);   // 2.
	OCR0A = BUZZER_TOP;    // 4.
	OCR1AH = SERIAL_TOP>>8;// 4.
	OCR1AL = SERIAL_TOP&0xff;// 4.
	TIMSK0 = BIT(OCIE0A);  // 4.
	TIMSK1 = BIT(OCIE1A);  // 4.
#elif defined (__AVR_ATmega16__)
	MCUCR = BIT(ISC10);    // 1.
	TCCR0 = BIT(WGM01);    // 2.
	TCCR2 = BIT(WGM21)     // 2.
	      | BIT(CS21);     // 3.
	TIMSK = BIT(OCIE0);    // 4.
	TIMSK |= BIT(OCIE2);   // 4.
	OCR0 = SERIAL_TOP;     // 4.
	OCR2 = BUZZER_TOP;     // 4.
#endif

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
				RELAY_PIN = 0;
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
			alarmStart = 0;
		}
		if (!ALARM_ON && btnPressed == 1000) {
			// 10 second press cancels / re-arms alarm
			btnPressed = 0x8000; // Suspend button until released & pressed
			buzzerOn = 0;
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
				RELAY_PIN = 0; // Turn relay / horn off
			else
				RELAY_PIN = 1; // Test relay / horn
		}

		if (actTimer < 100)
			actTimer++;
		if (actTimer > 5 && aisState == 0)
			ACT_PIN = 1;                 // Activity light OFF

		if (testTimer > 0) {
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
