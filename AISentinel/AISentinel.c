/**
 * (C) Copyright 2012 flemming.madsen at madsensoft.dk. See libuevent.h
 **                                                   MarkDown
 * AIS SEntinel
 * ============
 * Fires the buzzer and LED if AIT type 14 or MMSI prefix 970 is seen.
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
 * Running at Baudrate == 38400 @ 8 MHz => TOP = 208
 * Buzzer between 3840 & 1920 Hz =>
 * Toggle pin every 5 & 10 interrupts
 *
 * Timer1
 * ------
 * App timer at millisecond resolution Divide clock by 8000
 * Toggle buzzer freq every 500 ms if buzzerOn
 * Toggle LED every 500 ms if ledOn
 */

#include <stdint.h>
#include <avr/io.h>
#define F_CPU (16 * 10000UL)  // 16 MHz
#include <util/delay.h>

#define BUZZER_PIN
#define BUZZER_REG
#define LED_PIN
#define LED_REG
#define BUTTON_PIN
#define BUTTON_REG

// Maintained by timer interrupt. 
static uint32_t upTime

char AISLead[] PROGMEM = "\n!AIVDM,1,1,,\a,";

// Nonzero is active alarm. 
// Updated on every message
// Zeroed when no message seen in 10 minutes
static uint32_t alarmStart

int alarmStart()
{
	alarmStart = upTime;
}

#define AIST_MMSI_OFF 50
#define AIST_LEAD_MAX 15
static char aisState; // Counter in AISLead or state
static uint32_t mmsi;

int AISDecode(int ch)
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
			alarmStart();
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
			mmsi = (code & 0xf) << 26; // 26 bits left
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
				alarmStart();
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
uint8_t buzzerOn = 0, buzzerCnt = 0, buzzerTop = 0;
ISR(TIM0_OVF_vect)
{
	if (buzzerOn && buzzerCnt++ >= buzzerTop) {
		BUZZER_REG ^= BUZZER_PIN;
		buzzerCnt = 0;
	}
	
}

int GetCh()
{
}


int main(void)
{
	DDRB = 0xf;
   PORTB = 0xaa;
//exit(0);

	while(1) {
		_delay_ms(250);
			PORTB ^= 0x3f;
	}
}

// vim: set sw=3 ts=3 noet nu:
