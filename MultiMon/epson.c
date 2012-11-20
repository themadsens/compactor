// **************************************************************************************************************
// lcd.c
//
// Nokia 6610 LCD Display Driver (Epson S1D15G00 Controller)
//
// Controller used for LCD Display is a Epson S1D15G00 driver
// Note: For Olimex SAM7-EX256 boards with the G8 decal or the Sparkfun Color LCD 128x128 Nokia Knock-Off
//
//
// We will use a 132 x 132 pixel matrix - each pixel has 12 bits of color information.
//
//                               _____
//                                | |
//            ___________________|___|__ 
//           |  ======================  |
// ^ (131,0) |------------------------- |(131,131)
// |         |                          |
// |         |                          |
// |         |                          |
// |         |                          |
// |         |                          |
// |     Rows|   Nokia 6610 Display     | Note: In general, you can't view
// |         |                          | column 130 or column 131
// X         |                          |
// |         |                          |
// |         |                          |
// |         |                          |
// |         |                          |
// |         |                          |
// |         |--------------------------|
//      (0,0) Columns                    (0,131)
//
//           ------------Y-------------->
//
//
// 132 x 132 pixel matrix has two methods to specify the color info
//
// 1. 12 bits per pixel
// requires command and 1.5 data bytes to specify a single pixel
// (3 data bytes can specify 2 pixels)
//
// 2. 8 bits per pixel
// requires one command and one data byte to specify the single pixel\//
// note: pixel data byte converted by RGB table to 12-bit format above
//
// THIS IMPLEMENTATION USES THE 12 BITS PER PIXEL METHOD!
// -------------------------
//
//
//
//
// HARDWARE INTERFACE
// ------------------
//
// The Nokia 6610 display uses a SPI serial interface (9 bits)
// The SPI is bit-banged on the Arduino for lack of a 9 bit protocol.
//
// The Arduino signals that control the LCD are defined in Epson.h
// Current definitons are:
// Hardware definitions
// #define PIN_CS    0x01   // ~Chip select
// #define PIN_CLOCK 0x02   // Clock
// #define PIN_DATA  0x04   // Data
// #define PIN_RESET 0x08   // ~Reset
//
// The important thing to note is that you CANNOT read from the LCD!
//
// Functionality that was changed from the original include:
// 1) Different hardware platform
// 2) A knockoff Epson controller that does not work quite the same way
// 3) Removal of the BMP display function. (No room on an Arduino)
// 4) Added #defines to comment out unneeded functions.
// 5) Careful optimization of variables to lower memory footprint
// 6) A few bug fixes
//
//
// Author: James P Lynch August 30, 2007
// Modified from the original by Skye Sweeney February 22, 2011
// ***************************************************************************************************************
// **************************************
// Include Files
// **************************************
#include "epson.h"

#define WritePixelData(color) do {  \
            WriteSpiData(((color) >> 8) & 0x0F); \
            WriteSpiData((color) & 0xFF); \
        } while (0)


// *****************************************************************************
// WriteSpiCommand.c
//
// Writes 9-bit command to LCD display via SPI interface
//
// Inputs: command - command byte
//
//
// Author: Olimex, James P Lynch August 30, 2007
// *****************************************************************************
void WriteSpiCommand(byte command) {
  byte i;
  
  // Enable chip
  BCLR(LCD_PORT, PIN_CLOCK);
  BCLR(LCD_PORT, PIN_CS);
  
  // CLock out the zero for a command
  BCLR(LCD_PORT, PIN_DATA);
  BSET(LCD_PORT,  PIN_CLOCK);
  
  // Clock out the 8 bits MSB first  
  for (i=0; i<8; i++) {

    BCLR(LCD_PORT, PIN_CLOCK);

    // Set the data bit
    if (command & 0x80) {
      BSET(LCD_PORT, PIN_DATA);
    } else {
      BCLR(LCD_PORT, PIN_DATA);
    }
    // Toggle the clock
    BSET(LCD_PORT,  PIN_CLOCK);
    
    // Expose the next bit
    command = command << 1;
  }

  // Deslect chip  
  BSET(LCD_PORT, PIN_CS);
    
}


// *****************************************************************************
// WriteSpiData.c
//
// Writes 9-bit command to LCD display via SPI interface
//
// Inputs: data - data byte
//
//
// Author: Olimex, James P Lynch August 30, 2007
// Modified by Skye Sweeney February 2011
// *****************************************************************************
void WriteSpiData(byte data) {
  byte i;
  
  // Enable chip
  BCLR(LCD_PORT, PIN_CLOCK);
  BCLR(LCD_PORT, PIN_CS);
  
  // CLock out the one for data
  BSET(LCD_PORT, PIN_DATA);
  BSET(LCD_PORT,  PIN_CLOCK);
  
  // Clock out the 8 bits MSB first  
  for (i=0; i<8; i++) {

    BCLR(LCD_PORT, PIN_CLOCK);

    // Set the data bit
    if (data & 0x80) {
      BSET(LCD_PORT, PIN_DATA);
    } else {
      BCLR(LCD_PORT, PIN_DATA);
    }
    // Toggle the clock
    BSET(LCD_PORT,  PIN_CLOCK);
    
    // Expose the next bit
    data = data << 1;
  }

  // Deslect chip  
  BSET(LCD_PORT, PIN_CS);
  
}


// *****************************************************************************
// InitLcd.c
//
// Initializes the Epson S1D15G00 LCD Controller
//
// Inputs: none
//
// Author: James P Lynch August 30, 2007
// Modified by Skye Sweeney February 2011
// *****************************************************************************
void InitLcd(void) {

  // Set pin directions as outputs for SPI pins
  LCD_DDR |= BIT(PIN_CS) | BIT(PIN_RESET) | BIT(PIN_DATA) | BIT(PIN_CLOCK);

  // Start with PIN select HIGH  
  BSET(LCD_PORT, PIN_CS);
  BSET(LCD_PORT, PIN_CLOCK);
  
  // Hardware reset
  BCLR(LCD_PORT, PIN_RESET);
  delay_ms(20);
  BSET(LCD_PORT, PIN_RESET);
  delay_ms(20);

  // Display control
  WriteSpiCommand(DISCTL);
  WriteSpiData(0x00); // P1: 0x00 = 2 divisions, switching period=8 (default)
  WriteSpiData(0x20); // P2: 0x20 = nlines/4 - 1 = 132/4 - 1 = 32)
  WriteSpiData(0x00); // P3: 0x00 = no inversely highlighted lines
  
  // COM scan
  WriteSpiCommand(COMSCN);
  WriteSpiData(0x01); // P1: 0x01 = Scan 1->80, 160<-81
  
  // Internal oscilator ON
  WriteSpiCommand(OSCON);
  
  // Sleep out
  WriteSpiCommand(SLPOUT);
  
  // Power control
  WriteSpiCommand(PWRCTR);
  WriteSpiData(0x0f); // reference voltage regulator on, circuit voltage follower on, BOOST ON
  
  // Inverse display
  WriteSpiCommand(DISINV);
  
  // Data control
  WriteSpiCommand(DATCTL);
  // Was a 1, 0 fixes the address bug
  WriteSpiData(0x00); // P1: 0x01 = page address inverted, column address normal, address scan in column direction
  WriteSpiData(0x00); // P2: 0x00 = RGB sequence (default value)
  WriteSpiData(0x04); // P3: 0x02 = Grayscale -> 16 (selects 12-bit color, type B)
  
  // Voltage control (contrast setting)
  WriteSpiCommand(VOLCTR);
  // Was 32
  WriteSpiData(32); // P1 = 32 volume value (experiment with this value to get the best contrast)
  WriteSpiData(3); // P2 = 3 resistance ratio (only value that works)
  
  // Allow power supply to stabilize
  delay_ms(250);
  
  // Turn on the display
  WriteSpiCommand(DISON);
  
  // Create the font table based on what is selected
#if EPSON_EN_TEXT == 1
  #if EPSON_EN_TEXT_SMALL == 1
    FontTable[0] = FONT6x8;
  #else
    FontTable[0] = NULL;
  #endif
  #if EPSON_EN_TEXT_MEDIUM == 1
    FontTable[1] = FONT8x8;
  #else
    FontTable[1] = NULL;
  #endif
  #if EPSON_EN_TEXT_LARGE == 1
    FontTable[2] = FONT8x16;
  #else
    FontTable[2] = NULL;
  #endif
#endif
  
  
}




// *****************************************************************************
// LCDClearScreen.c
//
// Clears the LCD screen to single color (BLACK)
//
// Inputs: none
//
// Author: James P Lynch August 30, 2007
// *****************************************************************************
void LCDClearScreen(void) {
  int i; // loop counter
  
  // Row address set (command 0x2B)
  WriteSpiCommand(PASET);
  WriteSpiData(0);
  WriteSpiData(131);
  
  // Column address set (command 0x2A)
  WriteSpiCommand(CASET);
  WriteSpiData(0);
  WriteSpiData(131);
  
  // set the display memory to BLACK
  WriteSpiCommand(RAMWR);
  for(i = 0; i < 132 * 132; i++) {
    WritePixelData(BLACK);
  }
}


// *************************************************************************************
// LCDSetPixel.c
//
// Lights a single pixel in the specified color at the specified x and y addresses
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
// Note: see lcd.h for some sample color settings
//
// Author: James P Lynch August 30, 2007
// *************************************************************************************
void LCDSetPixel(const byte x, const byte y, const int color) {
  
  // Row address set (command 0x2B)
  WriteSpiCommand(PASET);
  WriteSpiData(x);
  WriteSpiData(x);
  
  // Column address set (command 0x2A)
  WriteSpiCommand(CASET);
  WriteSpiData(131-y);
  WriteSpiData(131-y);
  
  // Now illuminate the pixel (2nd pixel will be ignored)
  WriteSpiCommand(RAMWR);
  WritePixelData(color);
}



// *****************************************************************************************
// LCDSetRect.c
//
// Draws a rectangle in the specified color from (x1,y1) to (x2,y2)
// Rectangle can be filled with a color if desired
//
// Inputs: x = row address (0 .. 131)
// y = column address (0 .. 131)
// fill = 0=no fill, 1-fill entire rectangle
// color = 12-bit color value for lines rrrrggggbbbb
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
// Notes:
//
// The best way to fill a rectangle is to take advantage of the "wrap-around" featute
// built into the Epson S1D15G00 controller. By defining a drawing box, the memory can
// be simply filled by successive memory writes until all pixels have been illuminated.
//
// 1. Given the coordinates of two opposing corners (x0, y0) (x1, y1)
// calculate the minimums and maximums of the coordinates
//
// xmin = (x0 <= x1) ? x0 : x1;
// xmax = (x0 > x1) ? x0 : x1;
// ymin = (y0 <= y1) ? y0 : y1;
// ymax = (y0 > y1) ? y0 : y1;
//
// 2. Now set up the drawing box to be the desired rectangle
//
// WriteSpiCommand(PASET); // set the row boundaries
// WriteSpiData(xmin);
// WriteSpiData(xmax);
// WriteSpiCommand(CASET); // set the column boundaries
// WriteSpiData(ymin);
// WriteSpiData(ymax);
//
// 3. Calculate the number of pixels to be written divided by 2
//
// NumPixels = ((((xmax - xmin + 1) * (ymax - ymin + 1)) / 2) + 1)
//
// You may notice that I added one pixel to the formula.
// This covers the case where the number of pixels is odd and we
// would lose one pixel due to rounding error. In the case of
// odd pixels, the number of pixels is exact.
// in the case of even pixels, we have one more pixel than
// needed, but it cannot be displayed because it is outside
// the drawing box.
//
// We divide by 2 because two pixels are represented by three bytes.
// So we work through the rectangle two pixels at a time.
//
// 4. Now a simple memory write loop will fill the rectangle
//
// for (i = 0; i < ((((xmax - xmin + 1) * (ymax - ymin + 1)) / 2) + 1); i++) {
// WriteSpiData((color >> 4) & 0xFF);
// WriteSpiData(((color & 0xF) << 4) | ((color >> 8) & 0xF));
// WriteSpiData(color & 0xFF);
// }
//
//
// In the case of an unfilled rectangle, drawing four lines with the Bresenham line
// drawing algorithm is reasonably efficient.
//
//
// Author: James P Lynch August 30, 2007
// *****************************************************************************************
#if (EPSON_EN_RECTANGLE == 1)
void LCDSetRect(const byte x0, 
                const byte y0, 
                const byte x1, 
                const byte y1, 
                const byte fill, 
                const int color) {
  byte xmin, xmax, ymin, ymax;
  int i;
  
  // check if the rectangle is to be filled
  if (fill == FILL) {
    
    // best way to create a filled rectangle is to define a drawing box
    // and loop two pixels at a time
    // calculate the min and max for x and y directions
    xmin = (x0 <= x1) ? x0 : x1;
    xmax = (x0 > x1)  ? x0 : x1;
    ymin = (y0 <= y1) ? y0 : y1;
    ymax = (y0 > y1)  ? y0 : y1;
    
    // specify the controller drawing box according to those limits
    // Row address set (command 0x2B)
    WriteSpiCommand(PASET);
    WriteSpiData(xmin);
    WriteSpiData(xmax);
    
    // Column address set (command 0x2A)
    WriteSpiCommand(CASET);
    WriteSpiData(131-ymax);
    WriteSpiData(131-ymin);
    
    // WRITE MEMORY
    WriteSpiCommand(RAMWR);
    
    // loop on total number of pixels
    int cnt = (xmax - xmin + 1) * (ymax - ymin + 1);
    for (i = 0; i < cnt; i++) {
      WritePixelData(color);
    }
    
  // No FILL  
  } else {
    
    // best way to draw un unfilled rectangle is to draw four lines
    LCDSetLine(x0, y0, x1, y0, color);
    LCDSetLine(x0, y1, x1, y1, color);
    LCDSetLine(x0, y0, x0, y1, color);
    LCDSetLine(x1, y0, x1, y1, color);
  }
}
#endif

// *****************************************************************************
// LCDPutChar.c
//
// Draws an ASCII character at the specified (x,y) address and color
//
// Inputs: c = character to be displayed
// x = row address (0 .. 131)
// y = column address (0 .. 131)
// size = font pitch (SMALL, MEDIUM, LARGE)
// fcolor = 12-bit foreground color value rrrrggggbbbb
// bcolor = 12-bit background color value rrrrggggbbbb
//
// Returns: nothing
//
// Notes: Here's an example to display "E" at address (20,20)
//
// LCDPutChar('E', 20, 20, MEDIUM, WHITE, BLACK);
//
// (27,20) (27,27)
// | |
// | |
// ^ V V
// : _ # # # # # # # 0x7F
// : _ _ # # _ _ _ # 0x31
// : _ _ # # _ # _ _ 0x34
// x _ _ # # # # _ _ 0x3C
// : _ _ # # _ # _ _ 0x34
// : _ _ # # _ _ _ # 0x31
// : _ # # # # # # # 0x7F
// : _ _ _ _ _ _ _ _ 0x00
// ^ ^
// | |
// | |
// (20,20) (20,27)
//
// ------y----------->
//
//
// The most efficient way to display a character is to make use of the "wrap-around" feature
// of the Epson S1D16G00 LCD controller chip.
//
// Assume that we position the character at (20, 20) that's a (row, col) specification.
// With the row and column address set commands, you can specify an 8x8 box for the SMALL and MEDIUM
// characters or a 16x8 box for the LARGE characters.
//
// WriteSpiCommand(PASET); // set the row drawing limits
// WriteSpiData(20); //
// WriteSpiData(27); // limit rows to (20, 27)
//
// WriteSpiCommand(CASET); // set the column drawing limits
// WriteSpiData(20); //
// WriteSpiData(27); // limit columns to (20,27)
//
// When the algorithm completes col 27, the column address wraps back to 20
// At the same time, the row address increases by one (this is done by the controller)
//
// We walk through each row, two pixels at a time. The purpose is to create three
// data bytes representing these two pixels in the following format
//
// Data for pixel 0: RRRRGGGGBBBB
// Data for Pixel 1: RRRRGGGGBBBB
//
// WriteSpiCommand(RAMWR); // start a memory write (96 data bytes to follow)
//
// WriteSpiData(RRRRGGGG); // first pixel, red and green data
// WriteSpiData(BBBBRRRR); // first pixel, blue data; second pixel, red data
// WriteSpiData(GGGGBBBB); // second pixel, green and blue data
// :
// and so on until all pixels displayed!
// :
// WriteSpiCommand(NOP); // this will terminate the RAMWR command
//
//
// Author: James P Lynch August 30, 2007
// *****************************************************************************


#if EPSON_EN_TEXT == 1
void LCDPutChar(char c, 
                const byte x, 
                const byte y, 
                const byte size, 
                const int fColor, 
                const int bColor) {
  int i,j;
  byte nCols;
  byte nRows;
  byte nBytes;
  unsigned char PixelRow;
  unsigned char Mask;
  unsigned int color;
  unsigned char *pFont;
  unsigned char *pChar;

 
  // Get pointer to the beginning of the selected font table
  pFont = (unsigned char *)FontTable[size];
  
  // Exit if font is not defined
  if (pFont == NULL) return;
  
  // Use a '?' if character is not in font
  if ( (c < ' ') || (c > '~') ) c = '?';
  
  // Get the nColumns, nRows and nBytes
  nCols  = pgm_read_byte(pFont + 0);
  nRows  = pgm_read_byte(pFont + 1);
  nBytes = pgm_read_byte(pFont + 2);
  
  // Get pointer to the last byte of the desired character
  pChar = pFont + (nBytes * (c - 0x1F) + nBytes - 1);
 
  // Row address set (command 0x2B)
  WriteSpiCommand(PASET);
  WriteSpiData(x);                 // 10          = 10
  WriteSpiData(x + nRows - 1);     // 10 + 8 - 1  = 17
  
  // Column address set (command 0x2A)
  WriteSpiCommand(CASET);
  WriteSpiData(131-(y + nCols - 1));    // 131 - (10+8-1) = 114
  WriteSpiData(131-y);                  // 131 - 10       = 121
  
  // WRITE MEMORY
  WriteSpiCommand(RAMWR);
  
  // loop on each row, working backwards from the bottom to the top
  for (i = 0; i < nRows; i++) {
    
    // Copy pixel data from font table
    PixelRow = pgm_read_byte(pChar);

    // Decrement font data pointer
    pChar--;
    
    // Loop on each pixel in the row (left to right)
    Mask = 0x01;
    for (j = 8; j > nCols; j--)
      Mask = Mask << 1;
    
    for (j = 0; j < nCols; j++) {
      
      // If pixel bit set, use foreground color; else use the background color
      // now get the pixel color for two successive pixels
      if ((PixelRow & Mask) == 0) {
        color = bColor;
      } else {
        color = fColor;
      }
      WritePixelData(color);
      Mask = Mask << 1;
    } // Next pixel
  } // Next row

  // terminate the Write Memory command
  WriteSpiCommand(NOP);
  

}

#endif

