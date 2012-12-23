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
#include <ctype.h>
#include "hardware.h"
#include "Screen.h"
#include "LCD.h"
#include "util.h"
#include "trigint_sin8.h"

#define LCD_ON() (BSET(LCDPOWER_PORT, LCDPOWER_PIN), delay_ms(50), InitLcd()) 
#define LCD_OFF() BCLR(LCDPOWER_PORT, LCDPOWER_PIN)



static void showAlarm(const char *s, int sound);

// --- GLOBALS
int16_t   Nav_AWA;     // In degrees bows clockwise +-180
uint16_t  Nav_AWS;     // In 1/10ths of a knot
int16_t   Nav_ATMP;    // In 1/10ths of a degree +-
uint16_t  Nav_DBS;     // In 1/100ths m (cm up to 650m)
int16_t   Nav_WTMP;    // In 1/10ths of a degree +-
uint16_t  Nav_STW;     // In 1/100ths of a knot
int16_t   Nav_AWA;     // In degrees 0..360
int16_t   Nav_HDG;     // In degrees 0..360
uint16_t  Nav_SUM;     // Sumlog in N
uint8_t   Nav_redraw;

#define HIST_LEN 25
uint16_t DepthValues[HIST_LEN];
uint16_t SpeedValues[HIST_LEN];
static char buf[20];

static uint8_t firstTime;
#define LCDPutStrChkM(S, V, X, Y) LCDPutStrChk((S), (V), (X), (Y), MEDIUM, FG, BG)
static void LCDPutStrChk(char *str, int16_t val, byte x, byte y_, int sz, int fg, int bg)
{
	register char *s;
	register byte y = y_;

	if (NO_VAL == val) {
		for (s = str; *s; s++)
			if (isdigit(*s))
				*s = '-';
	}
	s = str;
	if (!firstTime) {
		for (; *s && ':' != *s; s++)
			y += 8;
		if (0 == *s) {
			s = str;
			y = y_;
		}
		else {
			y += 8;
			s += 1;
			if (' ' == s[1]) {
				y += 8;
				s += 1;
			}
		}
	}
	LCDPutStr(s, x, y, sz, fg, bg);
}

static int8_t windTick;
static uint8_t showMaxwind;
uint16_t maxWind[24];
uint16_t maxWindCur;
static void ScreenInitNav(void)
{
   LcdDrawGBox_P(1, 3, PSTR("WIND"));
   LcdDrawGBox_P(5, 3, PSTR("SPEED"));
   LcdDrawGBox_P(9, 2, PSTR("DEPTH"));
	windTick = msTick - 500;
	showMaxwind = 0;
	Nav_redraw = 0xff;
}

static void ScreenRedrawNav(void)
{
	if (abs(msTick - windTick) >= 500) {
		windTick = msTick;
		if (SBIT(Nav_redraw, NAV_WIND)) {
			BCLR(Nav_redraw, NAV_WIND);


			sprintf_P(buf, PSTR("AWS: %2d.%dm/s"), CnvKt2Ms_I(Nav_AWS, 10) % 100,
															 CnvKt2Ms_F(Nav_AWS, 10));
			LCDPutStrChkM(buf, Nav_AWS, LcdBoxIntX_M(10, 1), LcdBoxIntY_M(0));
			if (showMaxwind) {
				sprintf_P(buf, PSTR("MAX: %2d.%d"), CnvKt2Ms_I(maxWindCur, 10) % 100,
																 CnvKt2Ms_F(maxWindCur, 10));
			}
			else
				sprintf_P(buf, PSTR("AWA:  %03d"), Nav_AWA/10%1000);
			LCDPutStrChkM(buf, Nav_AWA, LcdBoxIntX_M(20, 1), LcdBoxIntY_M(0));

			// Draw that fancy little windmeter
			LCDSetRect(LcdBoxIntX_M(30, 1), LcdBoxIntY_M(98), LcdBoxIntX_M(6, 1), LcdBoxIntY_M(122), 1, BG);
			LCDSetCircle(LcdBoxIntX_M(18, 1), LcdBoxIntY_M(110), 12, FG);
			LCDSetCircle2C(LcdBoxIntX_M(18, 1), LcdBoxIntY_M(110), 12, RED, GREEN);
			LCDSetCircle2C(LcdBoxIntX_M(18, 1), LcdBoxIntY_M(110), 13, RED, GREEN);
			// TRIGINT_MAX / 360 ~= 45.51 -- Keep it in 16bit!
			register int deg = Nav_AWA/10;
			register int8_t sin = trigint_sin8(deg * 45 + (deg * 51 / 100));
			deg = (90 - deg + 360) % 360;
			register int8_t cos = trigint_sin8(deg * 45 + (deg * 51 / 100));
			LCDSetLine(LcdBoxIntX_M(18, 1), LcdBoxIntY_M(110),
						  LcdBoxIntX_M(18, 1) + (cos * 10 / 127),
						  LcdBoxIntY_M(110) + (sin * 10 / 127), FG);
		}
	}
	if (SBIT(Nav_redraw, NAV_ATMP)) {
		BCLR(Nav_redraw, NAV_ATMP);
		sprintf_P(buf, PSTR("TMP: %2d.%dC"), Nav_ATMP/10%100, abs(Nav_ATMP%10));
		LCDPutStrChkM(buf, Nav_ATMP, LcdBoxIntX_M(30, 1), LcdBoxIntY_M(0));
	}

	if (SBIT(Nav_redraw, NAV_HDG)) {
		//RotInsertValue(SpeedValues, Nav_STW, HIST_LEN);
		BCLR(Nav_redraw, NAV_HDG);

		sprintf_P(buf, PSTR("STW: %2d.%02dK"), Nav_STW/100%100, Nav_STW%100);
		LCDPutStrChkM(buf, Nav_STW, LcdBoxIntX_M(10, 5), LcdBoxIntY_M(0));
		sprintf_P(buf, PSTR("HDG:   %03d"), Nav_HDG/10%1000);
		LCDPutStrChkM(buf, Nav_HDG, LcdBoxIntX_M(20, 5), LcdBoxIntY_M(0));
		sprintf_P(buf, PSTR("LOG: %5dM"), Nav_SUM);
		LCDPutStrChkM(buf, Nav_SUM, LcdBoxIntX_M(30, 5), LcdBoxIntY_M(0));
		//LcdDrawGraph(SpeedValues, 0, LcdBoxIntX_M(22, 5), LcdBoxIntY_M(94),
		//				 16, HIST_LEN);
	}

	if (SBIT(Nav_redraw, NAV_DEPTH)) {
		//RotInsertValue(DepthValues, Nav_DBS, HIST_LEN);
		BCLR(Nav_redraw, NAV_DEPTH);

		sprintf_P(buf, PSTR("DBS: %2d.%dm"), Nav_DBS/10%100, Nav_DBS%10);
		LCDPutStrChkM(buf, Nav_DBS, LcdBoxIntX_M(10, 9), LcdBoxIntY_M(0));
		sprintf_P(buf, PSTR("TMP: %2d.%dC"), Nav_WTMP/10%100, abs(Nav_WTMP%10));
		LCDPutStrChkM(buf, Nav_WTMP, LcdBoxIntX_M(20, 9), LcdBoxIntY_M(0));
		//LcdDrawGraph(DepthValues, 1, LcdBoxIntX_M(20, 9), LcdBoxIntY_M(94),
		//				 16, HIST_LEN);
	}
}

uint16_t  Batt_VHOUSE;     // In 1/10 Volt
uint16_t  Batt_VENGINE;    // In 1/10 Volt
uint16_t  Batt_IHOUSE;     // In 1/10 Amps
uint16_t  Batt_ISOLAR;     // In 1/10 Amps
uint16_t  Batt_HOUSEAH;    // In 1/10 AmpHours
uint16_t  Batt_TEMP;       // In 1/10 C
uint8_t Batt_redraw;

static const char S_reset[] PROGMEM = "RESET";
static void ShowBattReset(int fg, int bg)
{

	strcpy_P(buf, S_reset);
	LCDPutStr(buf, LcdBoxIntX_M(10, 8), LcdBoxIntY_M(0), MEDIUM, fg, bg);
}

static void ScreenInitBatt(void)
{
   LcdDrawGBox_P(2, 2, PSTR("VOLTAGE"));
   LcdDrawGBox_P(5, 2, PSTR("LOAD/CHG"));
   LcdDrawGBox_P(8, 2, PSTR("STATE"));
	ShowBattReset(FG, BG);
	Batt_redraw = 0xff;
}

static void ScreenRedrawBatt(void)
{
	if (SBIT(Batt_redraw, BATT_VHOUSE)) {
		BCLR(Batt_redraw, BATT_VHOUSE);
		// Voltage
		//LcdClearGBoxInt_M(1, 2);
		sprintf_P(buf, PSTR("HSE: %2d.%dV"), Batt_VHOUSE/10, Batt_VHOUSE%10);
		LCDPutStrChkM(buf, Batt_VHOUSE, LcdBoxIntX_M(10, 2), LcdBoxIntY_M(0));
		LCDVoltBox(LcdBoxIntX_M(10, 2), LcdBoxIntY_M(90),
					  Batt_VHOUSE-115); // Range 11.5 .. 14.5
	}
	if (SBIT(Batt_redraw, BATT_VENGINE)) {
		BCLR(Batt_redraw, BATT_VENGINE);
		sprintf_P(buf, PSTR("ENG: %2d.%dV"), Batt_VENGINE/10, Batt_VENGINE%10);
		LCDPutStrChkM(buf, Batt_VENGINE, LcdBoxIntX_M(20, 2), LcdBoxIntY_M(0));
		LCDVoltBox(LcdBoxIntX_M(20, 2), LcdBoxIntY_M(90),
					  Batt_VENGINE-115); // Range 11.5 .. 14.5
	}

	if (SBIT(Batt_redraw, BATT_IHOUSE)) {
		BCLR(Batt_redraw, BATT_IHOUSE);
		// Load
		//LcdClearGBoxInt_M(4, 2);
		sprintf_P(buf, PSTR("HOUSE: %3d.%dA"), Batt_IHOUSE/10, Batt_IHOUSE%10);
		LCDPutStrChkM(buf, Batt_IHOUSE, LcdBoxIntX_M(10, 5), LcdBoxIntY_M(0));
	}
	if (SBIT(Batt_redraw, BATT_ISOLAR)) {
		BCLR(Batt_redraw, BATT_ISOLAR);
		sprintf_P(buf, PSTR("SOLAR: %3d.%dA"), Batt_ISOLAR/10, Batt_ISOLAR%10);
		LCDPutStrChkM(buf, Batt_ISOLAR, LcdBoxIntX_M(20, 5), LcdBoxIntY_M(0));
	}

	if (SBIT(Batt_redraw, BATT_HOUSEAH)) {
		BCLR(Batt_redraw, BATT_HOUSEAH);
		// State
		//LcdClearGBoxInt_M(8, 1);
		sprintf_P(buf, PSTR("USED: %3d.%dAh"), Batt_HOUSEAH/10, Batt_HOUSEAH%10);
		LCDPutStrChkM(buf, Batt_HOUSEAH, LcdBoxIntX_M(10, 8), LcdBoxIntY_M(0));
	}
	if (SBIT(Batt_redraw, BATT_TEMP)) {
		BCLR(Batt_redraw, BATT_TEMP);
		sprintf_P(buf, PSTR("TEMP:  %2d.%dC"), Batt_TEMP/10, Batt_TEMP%10);
		LCDPutStrChkM(buf, Batt_TEMP, LcdBoxIntX_M(20, 8), LcdBoxIntY_M(0));
	}
}

uint16_t Cnfg_AWSFAC  EEPROM =  990;   // in 1/1000
uint16_t Cnfg_STWFAC  EEPROM = 1050;   // in 1/1000
uint16_t Cnfg_DBSOFF  EEPROM =   80;   // in cm
uint16_t Cnfg_AWAOFF  EEPROM =    0;   // in deg
uint16_t Cnfg_HOUSEAH EEPROM =  120;   // in AH
uint16_t Cnfg_PEUKERT EEPROM = 1070;   // in 1/1000

static void ScreenInitCnfg(void)
{
   LcdDrawGBox_P( 1, 2, PSTR("WIND"));
   LcdDrawGBox_P( 4, 2, PSTR("WATER"));
   LcdDrawGBox_P( 7, 2, PSTR("BATT"));
   LcdDrawGBox_P(10, 1, PSTR("UPTIME"));
}

static void ScreenRedrawCnfg(void)
{
	int cnfg;

	cnfg = AvrXReadEEPromWord(&Cnfg_AWSFAC);
	sprintf_P(buf, PSTR("AWSFAC: %d.%03d"), cnfg/1000, cnfg%1000);
	LCDPutStrM(buf, LcdBoxIntX_M(10, 1), LcdBoxIntY_M(0));
	cnfg = AvrXReadEEPromWord(&Cnfg_DBSOFF);
	sprintf_P(buf, PSTR("AWAOFF: %03ddeg"), cnfg);
	LCDPutStrM(buf, LcdBoxIntX_M(20, 1), LcdBoxIntY_M(0));

	cnfg = AvrXReadEEPromWord(&Cnfg_DBSOFF);
	sprintf_P(buf, PSTR("DBSOFF: %d.%02dm"), cnfg/100, cnfg%100);
	LCDPutStrM(buf, LcdBoxIntX_M(10, 4), LcdBoxIntY_M(0));
	cnfg = AvrXReadEEPromWord(&Cnfg_STWFAC);
	sprintf_P(buf, PSTR("STWFAC: %d.%03d"), cnfg/1000, cnfg%1000);
	LCDPutStrM(buf, LcdBoxIntX_M(20, 4), LcdBoxIntY_M(0));

	cnfg = AvrXReadEEPromWord(&Cnfg_HOUSEAH);
	sprintf_P(buf, PSTR("HOUSEAH: %4dAH"), cnfg);
	LCDPutStrM(buf, LcdBoxIntX_M(10, 7), LcdBoxIntY_M(0));
	cnfg = AvrXReadEEPromWord(&Cnfg_PEUKERT);
	sprintf_P(buf, PSTR("PEUKERT: %d.%03d"), cnfg/1000, cnfg%1000);
	LCDPutStrM(buf, LcdBoxIntX_M(20, 7), LcdBoxIntY_M(0));

}

uint8_t SatSNR[12]; // 00 ..99
uint16_t SatHDOP;    // 5..30 or so
uint8_t SatMODE;    // 1: No fix, 2: 2D fix, 3: 3D fix

static char Gps_UTC[8];
static char Gps_LAT[10];
static char Gps_LON[12];
char *Gps_STR[5] = { Gps_UTC, Gps_LAT, Gps_LON, NULL/*SOG*/, NULL/*COG*/ };
uint8_t   Gps_redraw;
static void ScreenInitGps(void)
{
   LcdDrawGBox_P(2, 1, PSTR("GPS"));
   LcdDrawGBox_P(4, 5, PSTR("POS"));
   LcdDrawGBox_P(10, 1, PSTR("COMM"));
	Gps_redraw = 0xff;
}

static void ScreenRedrawGps(void)
{
	if (SBIT(Gps_redraw, GPS_SAT)) {
		BCLR(Gps_redraw, GPS_SAT);

		if (firstTime) {
			sprintf_P(buf, PSTR("SAT: "));
			LCDPutStrM(buf, LcdBoxIntX_M(10, 2), LcdBoxIntY_M(0));
		}

		uint8_t fg = 3==SatMODE ? GREEN : 2==SatMODE ? YELLOW : RED;
		buf[0] = SatHDOP/10 + '0';
		buf[1] = '.';
		buf[2] = SatHDOP%10 + '0';
		buf[3] = '\0';
		LCDPutStrChk(buf, SatHDOP, LcdBoxIntX_M(10, 2), LcdBoxIntY_M(40), MEDIUM, fg, BG);

		LCDSetRect(LcdBoxIntX_M(10, 2), LcdBoxIntY_M(70), LcdBoxIntX_M(0, 2), LcdBoxIntY_M(120), 1, BG);
		for (register u8 i = 0; i < 12; i++) {
			LCDSetRect(LcdBoxIntX_M(10, 2), LcdBoxIntY_M(70 + i*4), 
						  LcdBoxIntX_M(10, 2) + MIN(SatSNR[i]/6, 10), LcdBoxIntY_M(72 + i*4), 1, FG);
		}
	}
	if (SBIT(Gps_redraw, GPS_POS)) {
		BCLR(Gps_redraw, GPS_POS);

		sprintf_P(buf, PSTR("UTC: %.2s:%.2s:%.2s"), Gps_UTC, Gps_UTC+2, Gps_UTC+4);
		LCDPutStrChkM(buf, SatHDOP, LcdBoxIntX_M(10, 4), LcdBoxIntY_M(0));
		sprintf_P(buf, PSTR("LAT:  %s"), Gps_LAT);
		LCDPutStrChkM(buf, SatHDOP, LcdBoxIntX_M(20, 4), LcdBoxIntY_M(0));
		sprintf_P(buf, PSTR("LON: %s"), Gps_LON);
		LCDPutStrChkM(buf, SatHDOP, LcdBoxIntX_M(30, 4), LcdBoxIntY_M(0));
		sprintf_P(buf, PSTR("COG:  %03d"), (int16_t) Gps_STR[4]/10);
		LCDPutStrChkM(buf, SatHDOP, LcdBoxIntX_M(40, 4), LcdBoxIntY_M(0));
		sprintf_P(buf, PSTR("SOG: %2d.%dK"), (int16_t) Gps_STR[3]/10, (int16_t) Gps_STR[3]%10);
		LCDPutStrChkM(buf, SatHDOP, LcdBoxIntX_M(50, 4), LcdBoxIntY_M(0));
	}

	if (firstTime) {
		sprintf_P(buf, PSTR("COMM: "));
		LCDPutStrM(buf, LcdBoxIntX_M(10, 10), LcdBoxIntY_M(0));
		for (register u8 i = 0; i < 5; i++)
			LCDSetRect(LcdBoxIntX_M(10, 10), LcdBoxIntY_M(50 + i*10), 
						  LcdBoxIntX_M(2, 10), LcdBoxIntY_M(58 + i*10), 
						  0, FG);
	}
}

uint8_t LED_ActivityMask;
static void ScreenRedrawFast(void)
{
	if (SCREEN_CNFG == curScreen) {
		sprintf_P(buf, PSTR("%03d %02d:%02d:%02d"), hdayTick/2,
					 secTick/3600 + (hdayTick&1)*12,
					 secTick%3600/60,
					 secTick%60);
		LCDPutStrM(buf, LcdBoxIntX_M(10, 10), LcdBoxIntY_M(15));
	}
	else if (SCREEN_GPS == curScreen) {
		for (register u8 i = 0; i < 5; i++) {
			LCDSetRect(LcdBoxIntX_M(9, 10), LcdBoxIntY_M(51 + i*10), 
						  LcdBoxIntX_M(3, 10), LcdBoxIntY_M(57 + i*10), 
						  1, (LED_ActivityMask&(1<<i)) ? RED : BG);
		}
		LED_ActivityMask = 0;
	}
}

enum BTN {
	BTN_NOPRESS = 1,
	BTNPRESS_BOTH,
	BTNPRESS_1UP,
	BTNPRESS_2UP,
	BTNPRESS_1DOWN,
	BTNPRESS_2DOWN,
	BTNPRESS_1LONG,
	BTNPRESS_2LONG,
};

static void     *_ScreenUpd;
static void     *_ScreenUpdFast;
static void     *_ScrnButtonTick;
static MessageQueue ScreenQueue;       // The message queue itself
struct btnState {
	int16_t  lastTickCs;
	unsigned lastState:1;
	unsigned isChanging:1;

	unsigned pressed:1;
	unsigned longPress:1;
} btnState[2];
static int16_t lastPress;
static int16_t lastFast;

static int8_t btnSignal;
/**
 * Button bit reader
 */
void Task_ScreenButton(void) 
{
	lastPress = msTick;
	lastFast = msTick;
	for (;;) {
		delay_ms(5); 
		if (abs(msTick - lastFast) > 80) {
			lastFast = msTick;
			if (curScreen >= SCREEN_GPS)
				AvrXSendMessage(&ScreenQueue, (pMessageControlBlock)&_ScreenUpdFast);
		}

		register uint8_t _btnSignal = 0;
		for (register u8 i = 0; i < 2; i++) {
			struct btnState *s = btnState + i;
			u8 btnDown = 0 == (BUTTON2_PORT&(BV(i ? BUTTON2_PIN : BUTTON1_PIN)));
			if (btnDown != s->lastState) {
				s->isChanging = 1;
				s->lastTickCs = msTick;
				s->lastState = btnDown;
			}
			else if (s->isChanging && abs(msTick - s->lastTickCs) > 10) {
				s->isChanging = 0;
				if (s->lastState != s->pressed) {
					s->pressed = s->lastState;
					if (!s->pressed) {
						if (!s->longPress && btnSignal != BTNPRESS_BOTH)
							_btnSignal = i ? BTNPRESS_2UP : BTNPRESS_1UP;
						s->longPress = 0;
					}
					else if (s->pressed) {
						if (btnState[~i&1].pressed)
							_btnSignal = BTNPRESS_BOTH;
						else
							_btnSignal = i ? BTNPRESS_2DOWN : BTNPRESS_1DOWN;
					}
				}
			}
			else if (s->pressed) {
				if (abs(msTick - s->lastTickCs) >= 2000 && !s->longPress) {
					s->longPress = 1;
					_btnSignal = i ? BTNPRESS_2LONG : BTNPRESS_1LONG;
				}
			}
		}
		if (_btnSignal) {
			lastPress = msTick;
			btnSignal = _btnSignal;
			AvrXSendMessage(&ScreenQueue, (pMessageControlBlock)&_ScrnButtonTick);
		}
		else if (abs(msTick - lastPress) >= 25000 && BTN_NOPRESS != btnSignal) { // 20 Sec timeout
			btnSignal = BTN_NOPRESS;
			AvrXSendMessage(&ScreenQueue, (pMessageControlBlock)&_ScrnButtonTick);
		}
	}
}

/**
 * Request screen update
 */
void ScreenUpdate(void)
{
	AvrXSendMessage(&ScreenQueue, (pMessageControlBlock)&_ScreenUpd);
}
void ScreenUpdateInt(void)
{
	AvrXIntSendMessage(&ScreenQueue, (pMessageControlBlock)&_ScreenUpd);
}

static inline void WriteEEPromWord(uint16_t *addr, uint16_t val)
{
	AvrXWriteEEProm((uint8_t *)addr, val&0xff);
	AvrXWriteEEProm((uint8_t *)addr+1, (val>>8)&0xff);
}

uint8_t curScreen;
static uint8_t lastScreen;
struct BtnState {
	unsigned char locked     ;//:1;
	unsigned char powerOff   ;//:1;
	unsigned char editing    ;//:1;
	unsigned char valEditing ;//:1;
	unsigned char fieldNo    ;//:3;
	unsigned char charNo     ;//:3;
	unsigned char maxDigit   ;//:3;
	unsigned char numDecimals;//:2;
	unsigned short editNumber ;//:12;
} bs;
/**
 * Button handling
 */
static inline i8 ScreenButtonDo(void)
{
	register int i, j;
	register uint8_t row ;
	DEBUG("---BTN: %d", btnSignal);
	switch (btnSignal) {
	 case BTN_NOPRESS:
		// Timeout on button activity: Go to sleep or quit editing
		if (bs.editing) {
			bs.editing = 0;
			bs.valEditing = 0;
			return 1;
		}
		else if (!bs.powerOff && !bs.locked) {
			bs.powerOff = 1;
			bs.editing = 0;
			bs.valEditing = 0;
			LCD_OFF();
		}
		break;
	 case BTNPRESS_1UP:
		// Power-on, Browse screens, Rotate digit or Cancel
		if (bs.powerOff) {
			bs.powerOff = 0;
			LCD_ON();
			lastScreen = 99;
		}
		else if (bs.valEditing) {
			for (i=0, j=1; i < bs.charNo; i++)
				j *= 10;
			i = bs.editNumber % (j % 10);
			bs.editNumber /= j;
			bs.editNumber = (bs.editNumber/10*10) + (bs.editNumber%10 + 1)%10;
			bs.editNumber = bs.editNumber * j + i;
		}
		else if (bs.editing)
			bs.editing = 0;
		else 
			curScreen = (curScreen+1) % SCREEN_NUM;

		return 1;
	 case BTNPRESS_BOTH:
		curScreen = (curScreen ? curScreen : SCREEN_NUM) - 1;
		return 1;
	 case BTNPRESS_2UP:
		if (bs.powerOff)
			return 0;
		// Select field or digit to edit.
		if (SCREEN_BATT == curScreen) { // Redraw number
			LCDPutStr_p(PSTR("USED: "), LcdBoxIntX_M(10, 8), LcdBoxIntY_M(0), MEDIUM, FG, BG);
		}
		else if (SCREEN_NAV == curScreen) {
			showMaxwind ^= 1;
			firstTime = 1; // Update lead text next time
			BSET(Nav_redraw, NAV_WIND);
			return 1;
		}
		else if (bs.valEditing) {
			bs.charNo = bs.charNo+1 % bs.maxDigit;
		}
		else {
			if (!bs.editing && curScreen == SCREEN_CNFG) {
				bs.editing = 1;
				bs.fieldNo = 5;
			}
			if (bs.editing) {
				row = bs.fieldNo/2*3 + bs.fieldNo%2 + 1;
				LCDSetRect(LcdBoxIntX_M(row*10+10, 1), 4,
							  LcdBoxIntX_M(row*10,    1), 129, 0, BG);

				bs.fieldNo = (bs.fieldNo+1) % 6;

				row = bs.fieldNo/2*3 + bs.fieldNo%2 + 1;
				LCDSetRect(LcdBoxIntX_M(row*10+10, 1), 4,
							  LcdBoxIntX_M(row*10,    1), 129, 0, FG);
			}
		}
		break;
	 case BTNPRESS_1DOWN:
		break;
	 case BTNPRESS_1LONG:
		if (bs.powerOff)
			return 0;
		// Toggle lock screen or cancel edit
		if (bs.editing)
			bs.editing = 0;
		else {
			bs.locked ^= 1;
			lastScreen = 99;
		}
		return 1;
	 case BTNPRESS_2DOWN:
		if (curScreen == SCREEN_BATT)
			LCDPutStr_p(PSTR("RESET>"), LcdBoxIntX_M(10, 8), LcdBoxIntY_M(0), MEDIUM, FG, BG);
		else if (curScreen == SCREEN_NAV && showMaxwind)
			LCDPutStr_p(PSTR("RESET"), LcdBoxIntX_M(20, 1), LcdBoxIntY_M(0), MEDIUM, FG, BG);
		break;
	 case BTNPRESS_2LONG:
		// Start editing or Confirm editing
		if (curScreen == SCREEN_BATT) { // Confirm reset
			Batt_HOUSEAH = 0;
			BSET(Batt_redraw, BATT_HOUSEAH);
			firstTime = 1;
			return 1;
		}
		else if (curScreen == SCREEN_NAV && showMaxwind) {
			maxWindCur = 0;
			memset(maxWind, 0, sizeof(maxWind));
			BSET(Nav_redraw, NAV_WIND);
			firstTime = 1;
			return 1;
		}
		if (bs.editing && !bs.valEditing) {
			bs.valEditing = 1;
			switch (bs.fieldNo) {
			 case 0: bs.editNumber = AvrXReadEEPromWord(&Cnfg_AWSFAC) ;  break;
			 case 1: bs.editNumber = AvrXReadEEPromWord(&Cnfg_AWAOFF) ;  break;
			 case 2: bs.editNumber = AvrXReadEEPromWord(&Cnfg_STWFAC) ;  break;
			 case 3: bs.editNumber = AvrXReadEEPromWord(&Cnfg_DBSOFF) ;  break;
			 case 4: bs.editNumber = AvrXReadEEPromWord(&Cnfg_HOUSEAH);  break;
			 case 5: bs.editNumber = AvrXReadEEPromWord(&Cnfg_PEUKERT);  break;
			}
		}
		else if (bs.valEditing) {
			switch (bs.fieldNo) {
			 case 0: WriteEEPromWord(&Cnfg_AWSFAC,  bs.editNumber);  break;
			 case 1: WriteEEPromWord(&Cnfg_AWAOFF,  bs.editNumber);  break;
			 case 2: WriteEEPromWord(&Cnfg_STWFAC,  bs.editNumber);  break;
			 case 3: WriteEEPromWord(&Cnfg_DBSOFF,  bs.editNumber);  break;
			 case 4: WriteEEPromWord(&Cnfg_HOUSEAH, bs.editNumber);  break;
			 case 5: WriteEEPromWord(&Cnfg_PEUKERT, bs.editNumber);  break;
			}
		}
		break;
	}
	if (!bs.valEditing)
		return 0;
	
	// Value Editing window redraw
	row = bs.fieldNo/2*3 + bs.fieldNo%2 + 1;
	LCDSetRect(LcdBoxIntX_M(row*10+12, 1), 52, LcdBoxIntX_M(row*10, 1), 128, 0, FG);
	LCDSetRect(LcdBoxIntX_M(row*10+11, 1), 53, LcdBoxIntX_M(row*10+1, 1), 127, 1, BG);
	LCDSetRect(LcdBoxIntX_M(row*10+11, 1), 53+bs.charNo*14, LcdBoxIntX_M(row*10+1, 1), 65+bs.charNo*14, 0, FG);
	LCDSetRect(52, 14 + bs.charNo*20, 68, 26 + bs.charNo*20, 0, FG);
	j = bs.editNumber;
	for (i = 4; i >= 0; i--) {
		LCDPutChar(j%10 + '0', LcdBoxIntX_M(row*10+10, 1), 54+i*14, MEDIUM, FG, BG);
		j /= 10;
	}

	return 0;
}

static void showAlarm(const char *s, int sound)
{
	if (bs.powerOff) {
		LCD_ON();
		bs.powerOff = 0;
	}

	//LCDSetRect();
}

void DoBuzzer(void)
{
}

static const char S1[] PROGMEM = "NAV";
static const char S2[] PROGMEM = "BAT";
static const char S3[] PROGMEM = "GPS";
static const char S4[] PROGMEM = "CFG";


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
	lastScreen = 99;
	curScreen = SCREEN_NAV;

	uint8_t btn = 0;
	for (;;) {
		void *t = AvrXWaitMessage(&ScreenQueue);
		if (t == &_ScrnButtonTick) {
			btn = 1;
			if (!ScreenButtonDo())
				continue;
		}
		else if (t == &_ScreenUpdFast) {
			ScreenRedrawFast();
			continue;
		}

		if (curScreen != lastScreen) {
			lastScreen = curScreen;
			firstTime = 1;
			LCDSetRect(0,0,131,131, 1, BG); // Clear

			// Draw header
			LCDSetRect(121, 2, 131, 131, 0, FG);
			LCDSetLine(131, 4, 131, 129, BG);

			for (int i = 0; i < 4; i++) {
				int col = FG2;
				char y = 9 + i*30;
				if (i == curScreen) {
					// Active tab
					col = FG;
					LCDSetLine(121, y-5, 121, y+29, BG);
					LCDSetLine(121, y-5, 131, y-1, FG);
					LCDSetLine(131, y-1, 131, y+25, FG);
					LCDSetLine(131, y+25, 121, y+29, FG);
				}
				strcpy_P(buf, !i ? S1 : 1==i ? S2 : 2==i ? S3 : S4);
				LCDPutStr(buf, 122, y, MEDIUM, col, BG);
			}

			if (bs.locked)
				LCDSetRect(126, 126, 129, 129, 1, RED); 

			if      (curScreen == SCREEN_NAV) ScreenInitNav();
			else if (curScreen == SCREEN_BATT) ScreenInitBatt();
			else if (curScreen == SCREEN_GPS) ScreenInitGps();
			else if (curScreen == SCREEN_CNFG) ScreenInitCnfg();
		}

		if      (curScreen == SCREEN_NAV) ScreenRedrawNav();
		else if (curScreen == SCREEN_BATT) ScreenRedrawBatt();
		else if (curScreen == SCREEN_GPS) ScreenRedrawGps();
		else if (curScreen == SCREEN_CNFG && btn) ScreenRedrawCnfg();
		firstTime = 0;
	}
}

// vim: set sw=3 ts=3 noet nu:
