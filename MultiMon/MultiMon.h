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


// Hardware definitions. Thse are all on port B
#define LCD_PORT	PORTB
#define LCD_DDR 	DDRB
#define PIN_CS    4   // ~Chip select
#define PIN_CLOCK 7   // Clock
#define PIN_DATA  5   // Data
#define PIN_RESET 6   // ~Reset
