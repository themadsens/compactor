/**
 * @File LCD.h
 * Graphic primitives on top of driver routines
 */

#include "epson.h"

// Normal string colors
#define LCDPutStrM(S, X, Y) LCDPutStr((S), (X), (Y), FG, BG)
// Calc "box" offsets
#define LcdBoxIntX_M(o, L)  (112 - 10*(L) + o)
#define LcdBoxIntY_M(o)     (3 + o)
// Clear box intenal area
#define LcdClearGBoxInt_M(S, L) LCDSetRect(LcdBoxIntX_M(0, L + S + 1) + 1, 2, \
														 LcdBoxIntX_M(0, L) - 1, 129, 1, BG)


#define FG  WHITE
#define BG  BLACK
#define BG1 GRAY1
#define FG1 GRAY12

#if EPSON_EN_CIRCLE == 1
  void LCDSetCircle(byte x0, byte y0, int radius, int color);
  void LCDSetCircle2C(byte x0, byte y0, int radius, int color1, int color2);
#endif

#if (EPSON_EN_LINE == 1) || (EPSON_EN_RECTANGLE == 1)
  void LCDSetLine(byte x1, byte y1, byte x2, byte y2, int color);
#endif

#if EPSON_EN_TEXT == 1
  void LCDPutStr(const char *pString, 
                 const byte x, 
                 const byte y, 
                 const int fontSize, 
                 const int fColor, 
                 const int bColor);
#endif
