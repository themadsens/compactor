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
#include "Screen.h"
#include "trigint_sin8.h"

// --- GLOBALS
int16_t   Nav_AWA;     // In degrees bows clockwise +-180
uint16_t  Nav_AWS;     // In 1/10ths of a knot
int16_t   Nav_ATMP;    // In 1/10ths of a degree +-
uint16_t  Nav_DBS;     // In 1/100ths m (cm up to 650m)
int16_t   Nav_WTMP     // In 1/10ths of a degree +-
uint16_t  Nav_STW;     // In 1/100ths of a knot
int16_t   Nav_AWA;     // In degrees 0..360
uint8_t   Nav_redraw;

uint8_t DepthValues[HIST_LEN];
uint8_t SpeedValues[HIST_LEN];

static void ScreenInitNav()
{
   LcdDrawGBox_P(1, 4, PSTR("WIND"));
   LcdDrawGBox_P(6, 2, PSTR("SPEED"));
   LcdDrawGBox_P(9, 2, PSTR("DEPTH"));
}

static void ScreenRedrawNav()
{
	char buf[17];
   while (Nav_redraw) {
      if (SBIT(Nav_redraw, NAV_WIND)) {
         BCLR(Nav_redraw, NAV_WIND):
			LcdClearGBoxInt_M(1, 4);
			sprintf_P(buf, PSTR("AWS: %2d.%dm"), CnvKt2Ms_I(Nav_AWS, 10),
			                                     CnvKt2Ms_F(Nav_AWS, 10));
			LCDPutStrM(buf, LcdBoxIntX_M(30, 1), LcdBoxIntY_M(0));
			sprintf_P(buf, PSTR("AWA: %03d%c"), abs(Nav_AWA), Nav_AWA>0?'S':'P');
			LCDPutStrM(buf, LcdBoxIntX_M(20, 1), LcdBoxIntY_M(0));
			sprintf_P(buf, PSTR("ATMP:%2d.%dC"), Nav_ATMP/10, Nav_ATMP%10);
			LCDPutStrM(buf, LcdBoxIntX_M(20, 1), LcdBoxIntY_M(0));

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
				RotInsertValue(DepthValues, Nav_DBS);
				RotInsertValue(SpeedValues, Nav_STW);
				BCLR(Nav_redraw, NAV_DEPTH);
			}
         BCLR(Nav_redraw, NAV_HDG);

			LcdClearGBoxInt_M(6, 2);
			sprintf_P(buf, PSTR("STW: %2d.%02dm"), Nav_STW/100, Nav_STW%100);
			LCDPutStrM(buf, LcdBoxIntX_M(10, 6), LcdBoxIntY_M(0));
			sprintf_P(buf, PSTR("HDG: %03d"), Nav_HDG);
			LCDPutStrM(buf, LcdBoxIntX_M(0, 6), LcdBoxIntY_M(0));
			LcdDrawGraph(SpeedValues, 1, LcdBoxIntX_M(2, 6), LcdBoxIntY_M(100)
							 18, HIST_LEN);

			LcdClearGBoxInt_M(9, 2);
			sprintf_P(buf, PSTR("DBS: %2d.%02dm"), Nav_DBS/100, Nav_DBS%100);
			LCDPutStrM(buf, LcdBoxIntX_M(10, 9), LcdBoxIntY_M(0));
			sprintf_P(buf, PSTR("WTMP:%2d.%dC"), Nav_DBS/10, Nav_DBS%10);
			LCDPutStrM(buf, LcdBoxIntX_M(0, 9), LcdBoxIntY_M(0));
			LcdDrawGraph(DepthValues, 0, LcdBoxIntX_M(2, 9), LcdBoxIntY_M(100),
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

static const char *S_reset PROGMEM = "RESET";
static void ShowBattReset(int fg, int bg)
	char buf[7];
	strcpy_P(buf, S_reset);
	LCDPutStr(buf, LcdBoxIntX_M(10, 8), LcdBoxIntY_M(0), fg, bg);
}

static void ScreenInitBatt()
{
   LcdDrawGBox_P(1, 3, PSTR("VOLTAGE"));
   LcdDrawGBox_P(5, 2, PSTR("LOAD/CHG"));
   LcdDrawGBox_P(8, 2, PSTR("STATE"));
	ShowBattReset(FG, BG);
}

static void ScreenRedrawBatt()
{
	char buf[17];
   while (Batt_redraw) {
		// Voltage
      if (SBIT(Batt_redraw, BATT_VHOUSE)) {
         BCLR(Batt_redraw, BATT_VHOUSE):
			LcdClearGBoxInt_M(1, 1);
			sprintf_P(buf, PSTR("HSE: %2d.%2dV"), Batt_VHOUSE/100, Batt_VHOUSE%100);
			LCDPutStrM(buf, LcdBoxIntX_M(20, 1), LcdBoxIntY_M(0));
			LCDVoltBox(buf, LcdBoxIntX_M(20, 1), LcdBoxIntY_M(95), 30,
						  (Batt_VHOUSE-1150)/10); // Range 11.5 .. 14.5
		}
      if (SBIT(Batt_redraw, BATT_VENGINE)) {
         BCLR(Batt_redraw, BATT_VENGINE):
			LcdClearGBoxInt_M(2, 1);
			sprintf_P(buf, PSTR("ENG: %2d.%2dV"), Batt_VENGINE/100, Batt_VENGINE%100);
			LCDPutStrM(buf, LcdBoxIntX_M(10, 1), LcdBoxIntY_M(0));
			LCDVoltBox(buf, LcdBoxIntX_M(10, 1), LcdBoxIntY_M(95), 30,
						  (Batt_VENGINE-1150)/10); // Range 11.5 .. 14.5
		}
      if (SBIT(Batt_redraw, BATT_VSOLAR)) {
         BCLR(Batt_redraw, BATT_VSOLAR):
			LcdClearGBoxInt_M(3, 1);
			sprintf_P(buf, PSTR("SLR: %2d.%2dV"), Batt_VSOLAR/100, Batt_VSOLAR%100);
			LCDPutStrM(buf, LcdBoxIntX_M(10, 1), LcdBoxIntY_M(0));
		}

		// Load
      if (SBIT(Batt_redraw, BATT_IHOUSE)) {
         BCLR(Batt_redraw, BATT_IHOUSE):
			LcdClearGBoxInt_M(5, 1);
			sprintf_P(buf, PSTR("HSE: %2d.%dA"), Batt_IHOUSE/10, Batt_IHOUSE%10);
			LCDPutStrM(buf, LcdBoxIntX_M(20, 5), LcdBoxIntY_M(0));
		}
      if (SBIT(Batt_redraw, BATT_ISOLAR)) {
         BCLR(Batt_redraw, BATT_ISOLAR):
			LcdClearGBoxInt_M(6, 1);
			sprintf_P(buf, PSTR("SLR: %2d.%dA"), Batt_ISOLAR/10, Batt_ISOLAR%10);
			LCDPutStrM(buf, LcdBoxIntX_M(10, 5), LcdBoxIntY_M(0));
		}

		// State
      if (SBIT(Batt_redraw, BATT_HOUSEAH)) {
         BCLR(Batt_redraw, BATT_HOUSEAH):
			//LcdClearGBoxInt_M(8, 1);
			sprintf_P(buf, PSTR("AH: %3d.%d"), Batt_HOUSEAH/10, Batt_HOUSEAH%10);
			LCDPutStrM(buf, LcdBoxIntX_M(10, 8), LcdBoxIntY_M(0));
		}
      if (SBIT(Batt_redraw, BATT_TEMP)) {
         BCLR(Batt_redraw, BATT_TEMP):
			LcdClearGBoxInt_M(9, 1);
			sprintf_P(buf, PSTR("TEMP: %2d.%d"), Batt_TEMP/10, Batt_TEMP%10);
			LCDPutStrM(buf, LcdBoxIntX_M(0, 8), LcdBoxIntY_M(0));
		}
	}
}

static void ScreenInitCnfg()
{
   LcdDrawGBox_P(1, 6, PSTR("CONFIG"));
   LcdDrawGBox_P(8, 3, PSTR("GPS"));
}

uint8_t LED_ActivityMask;
static int8_t curTick;
static char ledTick[5];
static void ScreenRedrawCnfg()
{
}

static void ScreenButtonTick() 
{
	curTick++;
}

void     *_ScreenUpd;            // Declare the actual message
void     *_ScrnButtonTick;           // Declare the actual message
MessageQueue ScreenQueue;       // The message queue itself

/** 
 * Screen task.
 *
 * Draws current screen to completion and wait for next resume
 * Any resumes occuring while drawing are lost
 */
void Task_Screen(void)
{
	static uint8_t lastScreen;
	for (;;) {
		// Light LED while drawing
		BSET(LED_PORT, LED_DRAW);

		if (AvrXWaitMessage(&ScreenQueue) == &_ScrnButtonTick) {
			if (!ScreenButtonTick())
				continue;
		}
		BCLR(LED_PORT, LED_DRAW);

		if (curScreen != lastScreen) {
			LCDSetRect(0,0,131,131, 1, BG); // Clear

			// Draw header
			LCDSetRect(120, 1, 130, 130, FG);

			const char *S1 PROGMEM = "NAVI";
			const char *S2 PROGMEM = "BATT";
			const char *S3 PROGMEM = "CNFG";
			const char *Shead[] PROGMEM = { S1, S2, S3 };

			for (int i = 0; i < 3; i++) {
				char buf[5];
				char col = FG1;
				char y = 4 + i*40;
				strcpy_P(buf, Shead[i]);
				if (i == curScreen) {
					// Active tab
					col = FG;
					LCDSetLine(120, x, 120, x+40, BG);
					LCDSetLine(120, x, 130, x, BG);
					LCDSetLine(120, x+40, 130, x+40, BG);
				}
				LCDPutStr(buf, 121, 4 + i*40, col);
			}


			if      (curScreen == SCREEN_NAV) ScreenInitNav();
			else if (curScreen == SCREEN_BATT) ScreenInitBatt();
			else if (curScreen == SCREEN_CNFG) ScreenInitCnfg();
		}

		if      (curScreen == SCREEN_NAV) ScreenRedrawNav();
		else if (curScreen == SCREEN_BATT) ScreenRedrawBatt();
		else if (curScreen == SCREEN_CNFG) ScreenRedrawCnfg();
	}
}

// vim: set sw=3 ts=3 noet:
