// ********************************************************************
// lcd.c
//
// Non controller specific routines
//
// ********************************************************************
// **************************************
// Include Files
// **************************************
#include <string.h>
#include "LCD.h"


// *********************************************************************
// LCDSetLine.c
//
// Draws a line in the specified color from (x0,y0) to (x1,y1)
//
// Inputs: x = row address (0 .. 131)
// y = column address (0 .. 131)
// color = 12-bit color value rrrrggggbbbb
// rrrr = 1111 full red
// :
// 0000 red is off
//
// gggg = 1111 full green
// :
// 0000 green is off
//
// bbbb = 1111 full blue
// :
// 0000 blue is off
//
// Returns: nothing
//
// Note: good write-up on this algorithm in Wikipedia (search for Bresenham's line algorithm)
// see lcd.h for some sample color settings
//
// Authors: Dr. Leonard McMillan, Associate Professor UNC
// Jack Bresenham IBM, Winthrop University (Father of this algorithm, 1962)
//
// Note: taken verbatim from Professor McMillan's presentation:
// http://www.cs.unc.edu/~mcmillan/comp136/Lecture6/Lines.html
//
// ********************************************************************
#if (EPSON_EN_LINE == 1) || (EPSON_EN_RECTANGLE == 1)
void LCDSetLine(byte x0, byte y0, byte x1, byte y1, int color) {
  byte dy;
  byte dx;
  int fraction;
  byte stepx, stepy;
  
  if (y0 > y1) {
    dy = y0 - y1;
    stepy = -1;
  } else {
    dy = y1 - y0;
    stepy = 1;
  }
  if (x0 > x1) {
    dx = x0 - x1;
    stepx = -1;
  } else {
    dx = x1 - x0;
    stepx = 1;
  }
  
  
  dy <<= 1; // dy is now 2*dy
  dx <<= 1; // dx is now 2*dx
  
  LCDSetPixel(x0, y0, color);
  
  if (dx > dy) {
    fraction = dy - (dx >> 1); // same as 2*dy - dx
    while (x0 != x1) {
      if (fraction >= 0) {
        y0 += stepy;
        fraction -= dx; // same as fraction -= 2*dx
      }
      x0 += stepx;
      fraction += dy; // same as fraction -= 2*dy
      LCDSetPixel(x0, y0, color);
    }
    
  } else {
    fraction = dx - (dy >> 1);
    while (y0 != y1) {
              if (fraction >= 0) {
        x0 += stepx;
        fraction -= dy;
      }
      y0 += stepy;
      fraction += dx;
      LCDSetPixel(x0, y0, color);
    }
  }
}
#endif


// ********************************************************************
// LCDSetCircle.c
//
// Draws a line in the specified color at center (x0,y0) with radius
//
// Inputs: x0 = row address (0 .. 131)
// y0 = column address (0 .. 131)
// radius = radius in pixels
// color = 12-bit color value rrrrggggbbbb
//
// Returns: nothing
//
// Author: Jack Bresenham IBM, Winthrop University (Father of this algorithm, 1962)
//
// Note: taken verbatim Wikipedia article on Bresenham's line algorithm
// http://www.wikipedia.org
//
// ********************************************************************
#if EPSON_EN_CIRCLE == 1
void LCDSetCircle(byte x0, byte y0, int radius, int color) {
  int f = 1 - radius;
  int ddF_x = 0;
  int ddF_y = -2 * radius;
  int x = 0;
  int y = radius;
  
  LCDSetPixel(x0, y0 + radius, color);
  LCDSetPixel(x0, y0 - radius, color);
  LCDSetPixel(x0 + radius, y0, color);
  LCDSetPixel(x0 - radius, y0, color);
  
  while(x < y) {
    if(f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x + 1;
    LCDSetPixel(x0 + x, y0 + y, color);
    LCDSetPixel(x0 - x, y0 + y, color);
    LCDSetPixel(x0 + x, y0 - y, color);
    LCDSetPixel(x0 - x, y0 - y, color);
    LCDSetPixel(x0 + y, y0 + x, color);
    LCDSetPixel(x0 - y, y0 + x, color);
    LCDSetPixel(x0 + y, y0 - x, color);
    LCDSetPixel(x0 - y, y0 - x, color);
  }
  
}

// Custom for red,green circle top
void LCDSetCircle2C(byte x0, byte y0, int radius, int color1, int color2) {
  int f = 1 - radius;
  int ddF_x = 0;
  int ddF_y = -2 * radius;
  int x = 0;
  int y = radius;
  
  LCDSetPixel(x0, y0 - radius, color1);
  LCDSetPixel(x0 + radius, y0, color2);
  
  while(x < y) {
    if(f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x + 1;
    LCDSetPixel(x0 + x, y0 + y, color2);
    LCDSetPixel(x0 + x, y0 - y, color1);
    LCDSetPixel(x0 + y, y0 + x, color2);
    LCDSetPixel(x0 + y, y0 - x, color1);
  }
}
#endif


// ****************************************************************************
// LCDPutStr.c
//
// Draws a null-terminates character string at the specified (x,y) address, size and color
//
// Inputs: pString = pointer to character string to be displayed
// x = row address (0 .. 131)
// y = column address (0 .. 131)
// Size = font pitch (SMALL, MEDIUM, LARGE)
// fColor = 12-bit foreground color value rrrrggggbbbb
// bColor = 12-bit background color value rrrrggggbbbb
//
//
// Returns: nothing
//
// Notes: Here's an example to display "Hello World!" at address (20,20)
//
// LCDPutChar("Hello World!", 20, 20, LARGE, WHITE, BLACK);
//
//
// Author: James P Lynch August 30, 2007
// ***************************************************************************
void LCDPutStr2(const char *pString, 
                const int inRam,
               const byte x, 
               const byte y, 
               const int fontSize, 
               const int fColor, 
               const int bColor) 
{
  byte nCols;
  byte yp;
  unsigned char *pFont;

 
  // Get pointer to the beginning of the selected font table
  pFont = (unsigned char *)FontTable[fontSize];
  
  // Exit if font is not defined
  if (pFont == NULL) return;
  
  // Get the nCols
  nCols = pgm_read_byte(pFont + 0);
  
  yp = y;
  
  // Loop until null-terminator is seen
  int ch;
  while ((ch = inRam ? *pString : pgm_read_byte(pString)) != 0x00) {
    
    // draw the character

    LCDPutChar(ch, x, yp, fontSize, fColor, bColor);
    pString++;
    
    // Advance the y position
    yp += nCols;
    
    // Bail out if y exceeds 131
    if (yp > 131) break;
  }
}

static int LCDStrLen(const char *pString, const int inRam, const int fontSize)
{
   byte nCols;
   unsigned char *pFont;
	int ret = 0;

   pFont = (unsigned char *)FontTable[fontSize];
   if (pFont == NULL) return 0;
   nCols = pgm_read_byte(pFont + 0);
   int ch;
   while ((ch = (inRam ? *pString : pgm_read_byte(pString++))) != 0x00) {
  	   ret += nCols;
   }
	return ret;
}


void LcdDrawGBox_P(uint8_t row, uint8_t nRow, const char *pStr)
{
   row--;
	int strLen = LCDStrLen(pStr, 0, SMALL);
	// Topline w. text
   LCDSetLine(117-row*10, 4, 117-row*10, 126-strLen-4, FG2);
	LCDPutStr_p(pStr, 112-row*10, 126-strLen, SMALL, FG2, BG);
   LCDSetLine(117-row*10, 128, 117-row*10, 129, FG2);
	// Left line
   LCDSetLine(116-row*10, 3, 108-(row+nRow)*10, 3, FG2);
	// Right line
   LCDSetLine(116-row*10, 130, 108-(row+nRow)*10, 130, FG2);
	// Bottom line
   LCDSetLine(107-(row+nRow)*10, 4, 107-(row+nRow)*10, 129, FG2);
}

void RotInsertValue(uint16_t *arr, int val, uint8_t arrLen)
{
	memmove(arr+1, arr, (arrLen-1)*sizeof(int));
	arr[arrLen-1] = val;
}

void LcdDrawGraph(uint16_t *arr, uint8_t inverted, uint8_t x, uint8_t y, uint8_t h, uint8_t w)
{
	int max = 0, min = 0x7fff, dif, i;

	for (i = 0; i < w; i++) {
		if (arr[i] < min)
			min = arr[i];
		if (arr[i] > max)
			max = arr[i];
	}
   LCDSetLine(x, y, x, y+w+4, FG2);
	h--;
	x++;
	x++;
	dif = max - min;
   LCDSetLine(x, y, x+dif*h/max, y, FG2);
	y++;
   LCDSetLine(x, y, x+dif*h/max, y, FG2);
	y++;
   LCDSetLine(x, y, x+h, y, FG);
	for (i = 0; i < w; i++) {
		y++;
		LCDSetLine(x+1, y, x+(max-arr[i])*(h-1)/dif, y, FG2);
	}
	y++;
   LCDSetLine(x, y, x+h, y, FG);
} 

void LCDVoltBox(uint8_t x, uint8_t y, uint8_t val1to30)
{
   LCDSetRect(x, y   , x+7, y+32, 1, BG);
   LCDSetRect(x, y   , x+7, y+32, 0, FG);
   LCDSetRect(x+4, y+1 , x+6, y+7,  1, RED);
   LCDSetRect(x+4, y+8 , x+6, y+10, 1, YELLOW);
   LCDSetRect(x+4, y+11, x+6, y+20, 1, ORANGE);
   LCDSetRect(x+4, y+21, x+6, y+31, 1, GREEN);
   LCDSetRect(x+1, y+1,x+3, y+1+val1to30, 1, FG);
}

// vim: set sw=3 ts=3 noet:
