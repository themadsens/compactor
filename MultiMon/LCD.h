/**
 * @File LCD.h
 * Graphic primitives on top of driver routines
 */
#ifndef _LCD_h__
#define _LCD_h__

#include "stdlib.h"
#include "epson.h"

// Normal string colors
#define LCDPutStrM(S, X, Y) LCDPutStr((S), (X), (Y), MEDIUM, FG, BG)
// Calc "box" offsets
#define LcdBoxIntX_M(o, L)  (112 - 10*(L-1) - o)
#define LcdBoxIntY_M(o)     (5 + o)
// Clear box intenal area
#define LcdClearGBoxInt_M(L, S) LCDSetRect(LcdBoxIntX_M(0, L + S), 4, \
														 LcdBoxIntX_M(0, L), 128, 1, BG)

#if 0
#  define FG  WHITE
#  define BG  BLACK
#  define BG2 GRAY3
#  define FG2 0X999
#else
#  define FG  BLACK
#  define BG  WHITE
#  define BG2 GRAY12
#  define FG2 GRAY3
#endif
#if EPSON_EN_CIRCLE == 1
  void LCDSetCircle(byte x0, byte y0, int radius, int color);
  void LCDSetCircle2C(byte x0, byte y0, int radius, int color1, int color2);
#endif

#if (EPSON_EN_LINE == 1) || (EPSON_EN_RECTANGLE == 1)
  void LCDSetLine(byte x1, byte y1, byte x2, byte y2, int color);
#endif

#if EPSON_EN_TEXT == 1
void LCDPutStr2(const char *pString, 
                const int inRam,
               const byte x, 
               const byte y, 
               const int fontSize, 
               const int fColor, 
               const int bColor);

inline void LCDPutStr(const char *pString, 
                 const byte x, 
                 const byte y, 
                 const int fontSize, 
                 const int fColor, 
                 const int bColor) {
    LCDPutStr2(pString, 1, x, y, fontSize, fColor, bColor);
}
inline void LCDPutStr_p(const char *pString, 
                 const byte x, 
                 const byte y, 
                 const int fontSize, 
                 const int fColor, 
                 const int bColor) {
    LCDPutStr2(pString, 0, x, y, fontSize, fColor, bColor);
}
#endif

void LcdDrawGBox_P(uint8_t row, uint8_t nRow, const char *pStr);
void RotInsertValue(uint16_t *arr, int val, uint8_t arrLen);
void LcdDrawGraph(uint16_t *arr, uint8_t inverted, uint8_t x, uint8_t y, uint8_t h, uint8_t w);
void LCDVoltBox(uint8_t x, uint8_t y, uint8_t val1to30);

#endif
