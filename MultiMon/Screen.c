/**
 * (C) Copyright 2012 flemming.madsen at madsensoft.dk.
 **                                          MarkDown
 * NavState
 * ========
 * 
 * Displays the navigational state from the triducer and wind instruments
 *
 * WIND
 * ----
 * * AWA Apparent wind angle
 * * AWS Apparent wind speed
 * * TWS/TWA True wind speed and angle if SOG or STW is available
 * * ATMP Air Temperature if available
 *
 * DEPTH
 * -----
 * * DBS Depth below surface
 * * WTMP Water temperature
 *
 * SPEED
 * -----
 * * STW Speed through water
 * * HDG Magnetic heading
 *
 * The task is resumed when data is available. A bitmask says what to redraw
 *
*/
#include <string.h>
#include <avr/io.h>
#include <avrx.h>
#include "hardware.h"
#include "Screen.h"
#include "LCD.h"
#include "trigint_sin8.h"

#define LCD_ON() (BCLR(LCDPOWER_PORT, LCDPOWER_PIN), delay_ms(50), InitLcd()) 
#define LCD_OFF() BSET(LCDPOWER_PORT, LCDPOWER_PIN)

// --- GLOBALS
int16_t   Nav_AWA;     // In degrees bows clockwise +-180
uint16_t  Nav_AWS;     // In 1/10ths of a knot
int16_t   Nav_ATMP;    // In 1/10ths of a degree +-
uint16_t  Nav_DBS;     // In 1/100ths m (cm up to 650m)
int16_t   Nav_WTMP;    // In 1/10ths of a degree +-
uint16_t  Nav_STW;     // In 1/100ths of a knot
int16_t   Nav_AWA;     // In degrees 0..360
int16_t   Nav_HDG;     // In degrees 0..360
int16_t   Nav_SUM;     // Sumlog in N
uint8_t   Nav_redraw;

#define HIST_LEN 25
uint16_t DepthValues[HIST_LEN];
uint16_t SpeedValues[HIST_LEN];

static void ScreenInitNav(void)
{
   LcdDrawGBox_P(1, 4, PSTR("WIND"));
   LcdDrawGBox_P(6, 2, PSTR("SPEED"));
   LcdDrawGBox_P(9, 2, PSTR("DEPTH"));
}

static void ScreenRedrawNav(void)
{
	char buf[17];
   while (Nav_redraw) {
      if (SBIT(Nav_redraw, NAV_WIND)) {
         BCLR(Nav_redraw, NAV_WIND);
			LcdClearGBoxInt_M(1, 4);
			sprintf_P(buf, PSTR("AWS: %2d.%dm"), CnvKt2Ms_I(Nav_AWS, 10),
			                                     CnvKt2Ms_F(Nav_AWS, 10));
			LCDPutStrM(buf, LcdBoxIntX_M(10, 1), LcdBoxIntY_M(0));
			sprintf_P(buf, PSTR("AWA: %03d%c"), abs(Nav_AWA), Nav_AWA>0?'S':'P');
			LCDPutStrM(buf, LcdBoxIntX_M(20, 1), LcdBoxIntY_M(0));
			sprintf_P(buf, PSTR("ATMP:%2d.%dC"), Nav_ATMP/10, Nav_ATMP%10);
			LCDPutStrM(buf, LcdBoxIntX_M(30, 1), LcdBoxIntY_M(0));

			//TODO: Compute True wind speed/angle from SOG or STW 
			sprintf_P(buf, PSTR("TWS;A:%2d.%d;%03d%c"), 0,0,0,'-');
			LCDPutStrM(buf, LcdBoxIntX_M(20, 1), LcdBoxIntY_M(0));
			
			// Draw that fancy little windmeter
			LCDSetCircle(LcdBoxIntX_M(30, 1), LcdBoxIntY_M(120), 9, FG);
			LCDSetCircle2C(LcdBoxIntX_M(30, 1), LcdBoxIntY_M(120), 10, GREEN, RED);
			LCDSetCircle2C(LcdBoxIntX_M(30, 1), LcdBoxIntY_M(120), 11, GREEN, RED);
			// TRIGINT_MAX / 360 ~= 45.51 -- Keep it in 16bit!
			int deg = Nav_AWA + 180;
			int8_t sin = trigint_sin8(deg * 45 + (deg * 51 / 100));
			deg = (45 - deg + 360) % 360;
			int8_t cos = trigint_sin8(deg * 45 + (deg * 51 / 100));
			LCDSetLine(LcdBoxIntX_M(30, 1), LcdBoxIntY_M(120),
			           LcdBoxIntX_M(30, 1) + sin * 7 / 127,
						  LcdBoxIntY_M(120) + cos * 7 / 127, FG);
      }

      if (SBIT(Nav_redraw, NAV_DEPTH) || SBIT(Nav_redraw, NAV_HDG)) {
			if (SBIT(Nav_redraw, NAV_DEPTH)) {
				RotInsertValue(DepthValues, Nav_DBS, HIST_LEN);
				RotInsertValue(SpeedValues, Nav_STW, HIST_LEN);
				BCLR(Nav_redraw, NAV_DEPTH);
			}
         BCLR(Nav_redraw, NAV_HDG);

			LcdClearGBoxInt_M(6, 2);
			sprintf_P(buf, PSTR("STW: %2d.%02dm"), Nav_STW/100, Nav_STW%100);
			LCDPutStrM(buf, LcdBoxIntX_M(10, 6), LcdBoxIntY_M(0));
			sprintf_P(buf, PSTR("HDG: %03d"), Nav_HDG);
			LCDPutStrM(buf, LcdBoxIntX_M(20, 6), LcdBoxIntY_M(0));
			LcdDrawGraph(SpeedValues, 0, LcdBoxIntX_M(2, 6), LcdBoxIntY_M(100),
							 18, HIST_LEN);

			LcdClearGBoxInt_M(9, 2);
			sprintf_P(buf, PSTR("DBS: %2d.%02dm"), Nav_DBS/100, Nav_DBS%100);
			LCDPutStrM(buf, LcdBoxIntX_M(10, 9), LcdBoxIntY_M(0));
			sprintf_P(buf, PSTR("WTMP:%2d.%dC"), Nav_WTMP/10, Nav_WTMP%10);
			LCDPutStrM(buf, LcdBoxIntX_M(20, 9), LcdBoxIntY_M(0));
			LcdDrawGraph(DepthValues, 1, LcdBoxIntX_M(2, 9), LcdBoxIntY_M(100),
							 18, HIST_LEN);
      }
   }
}

uint16_t  Batt_VHOUSE;     // In 1/100 Volt
uint16_t  Batt_VENGINE;    // In 1/100 Volt
uint16_t  Batt_VSOLAR;     // In 1/100 Volt
uint16_t  Batt_IHOUSE;     // In 1/10 Amps
uint16_t  Batt_ISOLAR;     // In 1/10 Amps
uint16_t  Batt_HOUSEAH;    // In 1/10 AmpHours
uint16_t  Batt_TEMP;       // In 1/10 C
uint8_t Batt_redraw;

static const char S_reset[] PROGMEM = "RESET";
static void ShowBattReset(int fg, int bg)
{
	char buf[7];

	strcpy_P(buf, S_reset);
	LCDPutStr(buf, LcdBoxIntX_M(10, 8), LcdBoxIntY_M(0), MEDIUM, fg, bg);
}

static void ScreenInitBatt(void)
{
   LcdDrawGBox_P(1, 2, PSTR("VOLTAGE"));
   LcdDrawGBox_P(4, 2, PSTR("LOAD/CHG"));
   LcdDrawGBox_P(7, 2, PSTR("STATE"));
	ShowBattReset(FG, BG);
}

static void ScreenRedrawBatt(void)
{
	char buf[17];
   while (Batt_redraw) {
		Batt_redraw = 0;
		// Voltage
		//LcdClearGBoxInt_M(1, 2);
		sprintf_P(buf, PSTR("HSE: %2d.%2dV"), Batt_VHOUSE/100, Batt_VHOUSE%100);
		LCDPutStrM(buf, LcdBoxIntX_M(10, 1), LcdBoxIntY_M(0));
		LCDVoltBox(LcdBoxIntX_M(20, 1), LcdBoxIntY_M(95),
					  (Batt_VHOUSE-1150)/10); // Range 11.5 .. 14.5
		sprintf_P(buf, PSTR("ENG: %2d.%2dV"), Batt_VENGINE/100, Batt_VENGINE%100);
		LCDPutStrM(buf, LcdBoxIntX_M(20, 1), LcdBoxIntY_M(0));
		LCDVoltBox(LcdBoxIntX_M(10, 1), LcdBoxIntY_M(95),
					  (Batt_VENGINE-1150)/10); // Range 11.5 .. 14.5

		// Load
		//LcdClearGBoxInt_M(4, 2);
		sprintf_P(buf, PSTR("HSE: %2d.%dA"), Batt_IHOUSE/10, Batt_IHOUSE%10);
		LCDPutStrM(buf, LcdBoxIntX_M(10, 5), LcdBoxIntY_M(0));
		sprintf_P(buf, PSTR("SLR: %2d.%dA"), Batt_ISOLAR/10, Batt_ISOLAR%10);
		LCDPutStrM(buf, LcdBoxIntX_M(20, 5), LcdBoxIntY_M(0));

		// State
		//LcdClearGBoxInt_M(8, 1);
		sprintf_P(buf, PSTR("AH: %3d.%d"), Batt_HOUSEAH/10, Batt_HOUSEAH%10);
		LCDPutStrM(buf, LcdBoxIntX_M(10, 8), LcdBoxIntY_M(0));
		LcdClearGBoxInt_M(9, 1);
		sprintf_P(buf, PSTR("TEMP: %2d.%d"), Batt_TEMP/10, Batt_TEMP%10);
		LCDPutStrM(buf, LcdBoxIntX_M(20, 8), LcdBoxIntY_M(0));
	}
}

uint16_t Cnfg_AWSFAC EEPROM;    // in 1/1000
uint16_t Cnfg_STWFAC EEPROM;    // in 1/1000
uint16_t Cnfg_DBSOFF EEPROM;    // in cm
uint16_t Cnfg_AWAOFF EEPROM;    // in deg
uint16_t Cnfg_HOUSEAH EEPROM;   // in AH
uint16_t Cnfg_PEUKERT EEPROM;   // in 1/1000
uint8_t SatSNR[12]; // 00 ..99
uint8_t SatHDOP;    // 5..30 or so
uint8_t SatMODE;    // 1: No fix, 2: 2D fix, 3: 3D fix

static void ScreenInitCnfg(void)
{
   LcdDrawGBox_P(1, 6, PSTR("CONFIG"));
   LcdDrawGBox_P(8, 3, PSTR("GPS"));
}

static void ScreenRedrawCnfg(void)
{
	char buf[20];
	int cnfg;

	cnfg = AvrXReadEEPromWord(&Cnfg_AWSFAC);
	sprintf_P(buf, PSTR("AWSFAC: %d.%03d"), cnfg/1000, cnfg%1000);
	LCDPutStrM(buf, LcdBoxIntX_M(10, 1), LcdBoxIntY_M(0));

	cnfg = AvrXReadEEPromWord(&Cnfg_STWFAC);
	sprintf_P(buf, PSTR("STWFAC: %d.%03d"), cnfg/1000, cnfg%1000);
	LCDPutStrM(buf, LcdBoxIntX_M(20, 1), LcdBoxIntY_M(0));

	cnfg = AvrXReadEEPromWord(&Cnfg_DBSOFF);
	sprintf_P(buf, PSTR("DBSOFF: %d.%02dm"), cnfg/100, cnfg%100);
	LCDPutStrM(buf, LcdBoxIntX_M(30, 1), LcdBoxIntY_M(0));

	cnfg = AvrXReadEEPromWord(&Cnfg_DBSOFF);
	sprintf_P(buf, PSTR("AWAOFF: %03ddeg"), cnfg);
	LCDPutStrM(buf, LcdBoxIntX_M(40, 1), LcdBoxIntY_M(0));

	cnfg = AvrXReadEEPromWord(&Cnfg_HOUSEAH);
	sprintf_P(buf, PSTR("HOUSEAH: %04dAH"), cnfg);
	LCDPutStrM(buf, LcdBoxIntX_M(50, 1), LcdBoxIntY_M(0));

	cnfg = AvrXReadEEPromWord(&Cnfg_PEUKERT);
	sprintf_P(buf, PSTR("PEUKERT: %d.%03d"), cnfg/1000, cnfg%1000);
	LCDPutStrM(buf, LcdBoxIntX_M(60, 1), LcdBoxIntY_M(0));

	LcdClearGBoxInt_M(8, 2);
	sprintf_P(buf, PSTR("GPS: 0.0"));
	LCDPutStrM(buf, LcdBoxIntX_M(10, 8), LcdBoxIntY_M(0));

	sprintf_P(buf, PSTR("COMM: "));
	LCDPutStrM(buf, LcdBoxIntX_M(20, 8), LcdBoxIntY_M(0));
	for (u8 i = 0; i < 4; i++)
		LCDSetRect(LcdBoxIntX_M(20, 8), LcdBoxIntY_M(50 + i*10), 
		           LcdBoxIntX_M(12, 8), LcdBoxIntY_M(58 + i*10), 
					  0, FG);
}

uint8_t LED_ActivityMask;
static void ScreenRedrawComm(void)
{
	for (u8 i = 0; i < 4; i++) {
		LCDSetRect(LcdBoxIntX_M(19, 8), LcdBoxIntY_M(51 + i*10), 
					  LcdBoxIntX_M(13, 8), LcdBoxIntY_M(57 + i*10), 
					  1, (LED_ActivityMask&(1<<i)) ? RED : BG);
	}

	uint8_t fg = 1==SatMODE ? RED : 2==SatMODE ? YELLOW : GREEN;
	LCDPutChar(SatHDOP/10 + '0', LcdBoxIntX_M(10, 8), LcdBoxIntY_M(48), MEDIUM, fg, BG);
	LCDPutChar(SatHDOP%10 + '0', LcdBoxIntX_M(10, 8), LcdBoxIntY_M(56), MEDIUM, fg, BG);

	LCDSetRect(LcdBoxIntX_M(10, 8), LcdBoxIntY_M(80), LcdBoxIntX_M(20, 8), LcdBoxIntY_M(130), 1, BG);
	for (u8 i = 0; i < 12; i++) {
		LCDSetRect(LcdBoxIntX_M(10, 8), LcdBoxIntY_M(80 + i*4), 
					  LcdBoxIntX_M(10, 8) + SatSNR[i]/10, LcdBoxIntY_M(83 + i*4), 
					  SatSNR[i] > 20 ? 1 : 0, FG);
	}
	LED_ActivityMask = 0;
}

enum BTN {
	BTN_NOPRESS = 1,
	BTNPRESS_1UP,
	BTNPRESS_2UP,
	BTNPRESS_1DOWN,
	BTNPRESS_2DOWN,
	BTNPRESS_1LONG,
	BTNPRESS_2LONG,
};

static void     *_ScreenUpd;
static void     *_ScrnButtonTick;
static MessageQueue ScreenQueue;       // The message queue itself
struct btnState {
	int8_t   lastTickCs;
	unsigned lastState:1;
	unsigned isChanging:1;

	unsigned pressed:1;
	unsigned longPress:1;
} btnState[2];
static int8_t cSec;
static int8_t c8Sec;
static int8_t lastPress;

static int8_t btnSignal;
/**
 * Button bit reader
 */
void ScreenButtonTick(void) 
{
	cSec++;
	if (0 == (cSec&0x7)) {
		c8Sec++;
		if (curScreen == SCREEN_CNFG)
			ScreenRedrawComm();
	}

	btnSignal = 0;
	for (u8 i = 0; i < 2; i++) {
		struct btnState *s = btnState + i;
		u8 btnDown = 0 == (BUTTON2_PORT&(BV(i ? BUTTON1_PIN : BUTTON2_PIN)));
		if (btnDown != s->lastState) {
			s->isChanging = 1;
			s->lastTickCs = cSec;
			s->lastState = btnDown;
		}
		else if (s->isChanging && cSec - s->lastTickCs > 5) {
			s->isChanging = 0;
			if (s->lastState != s->pressed) {
				s->pressed = s->lastState;
				if (!s->pressed && !s->longPress)
					btnSignal = i ? BTNPRESS_2UP : BTNPRESS_1UP;
				else if (s->pressed)
					btnSignal = i ? BTNPRESS_2DOWN : BTNPRESS_1DOWN;
			}
		}
		else if (s->pressed && cSec - s->lastTickCs == 100) {
			s->longPress = 1;
			btnSignal = i ? BTNPRESS_2LONG : BTNPRESS_1LONG;
		}
	}
	if (btnSignal) {
		lastPress = c8Sec;
		AvrXSendMessage(&ScreenQueue, _ScrnButtonTick);
	}
	else if (c8Sec - lastPress == 255) {
		btnSignal = BTN_NOPRESS;
		AvrXSendMessage(&ScreenQueue, _ScrnButtonTick);
	}
}

/**
 * Request screen update
 */
void ScreenUpdate(void)
{
	AvrXSendMessage(&ScreenQueue, _ScreenUpd);
}

uint8_t curScreen;
static uint8_t lastScreen;
struct BtnState {
	unsigned locked:1;
	unsigned powerOn:1;
	unsigned editing:1;
	unsigned valEditing:1;
	unsigned fieldNo:3;
	unsigned charNo:3;
	unsigned maxDigit:3;
	unsigned numDecimals:2;
	unsigned editNumber:12;
} bs;
/**
 * Button handling
 */
static inline i8 ScreenButtonDo(void)
{
	int i, j;
	switch (btnSignal) {
	 case BTN_NOPRESS:
		// Timeout on button activity: Go to sleep or quit editing
		if (bs.editing) {
			bs.editing = 0;
			return 1;
		}
		else if (bs.powerOn && !bs.locked) {
			bs.powerOn = 0;
			bs.editing = 0;
			bs.valEditing = 0;
			LCD_OFF();
		}
		break;
	 case BTNPRESS_1UP:
		// Power-on, Browse screens, Rotate digit or Cancel
		if (bs.valEditing) {
			for (i=0, j=1; i < bs.charNo; i++)
				j *= 10;
			i = bs.editNumber % (j % 10);
			bs.editNumber /= j;
			bs.editNumber = (bs.editNumber/10*10) + (bs.editNumber%10 + 1)%10;
			bs.editNumber = bs.editNumber * j + i;
		}
		else if (bs.editing) {
			bs.editing = 0;
			return 1;
		}

		if (!bs.powerOn) {
			bs.powerOn = 1;
			LCD_ON();
		}
		else 
			curScreen = curScreen+1 % SCREEN_NUM;
		return 1;
	 case BTNPRESS_2UP:
		if (!bs.powerOn)
			return 0;
		// Select field or digit to edit, Confirm edit
		if (bs.valEditing) {
			bs.charNo = bs.charNo+1 % bs.maxDigit;
		}
		else {
			if (!bs.editing && curScreen == SCREEN_CNFG) {
				bs.editing = 1;
				bs.fieldNo = 0;
			}
			if (bs.editing) {
				bs.fieldNo = bs.fieldNo+1 % 6;
				LCDSetLine(LcdBoxIntX_M(60, 1), 2, LcdBoxIntX_M(0, 1), 2, BG);
				LCDSetLine(LcdBoxIntX_M(bs.fieldNo*10+10, 1), 2,
							  LcdBoxIntX_M(bs.fieldNo*10, 1), 2, FG);
			}
		}
		break;
	 case BTNPRESS_1DOWN:
		break;
	 case BTNPRESS_1LONG:
		if (!bs.powerOn)
			return 0;
		// Toggle lock screen or cancel edit
		if (bs.editing) {
			bs.editing = 0;
			return 1;
		}
		break;
	 case BTNPRESS_2DOWN:
		if (curScreen == SCREEN_BATT)
			LCDSetRect(30, 90, 40, 130, 0,FG);
		break;
	 case BTNPRESS_2LONG:
		// Start editing
		if (curScreen == SCREEN_BATT) {
			Batt_HOUSEAH = 0;
		}
		if (bs.editing && !bs.valEditing) {
			bs.valEditing = 1;
			switch (bs.fieldNo) {
			 case 0: bs.editNumber = Cnfg_AWSFAC ;  break;
			 case 1: bs.editNumber = Cnfg_STWFAC ;  break;
			 case 2: bs.editNumber = Cnfg_DBSOFF ;  break;
			 case 3: bs.editNumber = Cnfg_AWAOFF ;  break;
			 case 4: bs.editNumber = Cnfg_HOUSEAH;  break;
			 case 5: bs.editNumber = Cnfg_PEUKERT;  break;
			}
		}
		else if (bs.valEditing) {
			switch (bs.fieldNo) {
			 case 0: Cnfg_AWSFAC  = bs.editNumber;  break;
			 case 1: Cnfg_STWFAC  = bs.editNumber;  break;
			 case 2: Cnfg_DBSOFF  = bs.editNumber;  break;
			 case 3: Cnfg_AWAOFF  = bs.editNumber;  break;
			 case 4: Cnfg_HOUSEAH = bs.editNumber;  break;
			 case 5: Cnfg_PEUKERT = bs.editNumber;  break;
			}
		}
		break;
	}
	if (!bs.valEditing)
		return 0;
	
	// Value Editing window redraw
	LCDSetRect(50, 10, 70, 110, 1, BG);
	LCDSetRect(51, 11, 69, 109, 0, FG);
	LCDSetRect(52, 14 + bs.charNo*20, 68, 26 + bs.charNo*20, 0, FG);
	j = bs.editNumber;
	for (i = 4; i >= 0; i--) {
		LCDPutChar(j%10 + '0', 54, 16 + bs.charNo*20, MEDIUM, FG, BG);
		j /= 10;
	}

	return 0;
}

static const char S1[] PROGMEM = "NAVI";
static const char S2[] PROGMEM = "BATT";
static const char S3[] PROGMEM = "CNFG";


/** 
 * Screen task.
 *
 * Draws current screen to completion and wait for next resume
 * Any resumes occuring while drawing are lost
 */
void Task_Screen(void)
{

	// Hardware init
	BSET(BUTTON1_PU, BUTTON1_PIN);
	BSET(BUTTON2_PU, BUTTON2_PIN);

	BSET(LCDPOWER_DDR, LCDPOWER_PIN);

	LCD_ON();

	uint8_t btn = 0;
	for (;;) {
		// Light LED while drawing
		BCLR(LED_PORT, LED_DBG1);

		if (AvrXWaitMessage(&ScreenQueue) ==
			 (MessageControlBlock*)&_ScrnButtonTick) {
			btn = 1;
			if (!ScreenButtonDo())
				continue;
		}
		BSET(LED_PORT, LED_DBG1);

		if (curScreen != lastScreen) {
			lastScreen = curScreen;
			LCDSetRect(0,0,131,131, 1, BG); // Clear

			// Draw header
			LCDSetRect(120, 1, 130, 130, 0, FG);

			for (int i = 0; i < 3; i++) {
				char buf[5];
				int col = FG2;
				char y = 4 + i*40;
				strcpy_P(buf, !i ? S1 : 1==i ? S2 : S3);
				if (i == curScreen) {
					// Active tab
					col = FG;
					LCDSetLine(120, y, 120, y+40, BG);
					LCDSetLine(120, y, 130, y, BG);
					LCDSetLine(120, y+40, 130, y+40, BG);
				}
				LCDPutStr(buf, 121, 4 + i*40, MEDIUM, col, BG);
				LCDSetLine(1, 1, 1, 130, FG);
			}


			if      (curScreen == SCREEN_NAV) ScreenInitNav();
			else if (curScreen == SCREEN_BATT) ScreenInitBatt();
			else if (curScreen == SCREEN_CNFG) ScreenInitCnfg();
		}

		if      (curScreen == SCREEN_NAV) ScreenRedrawNav();
		else if (curScreen == SCREEN_BATT) ScreenRedrawBatt();
		else if (curScreen == SCREEN_CNFG && btn) ScreenRedrawCnfg();
	}
}

// vim: set sw=3 ts=3 noet nu:
