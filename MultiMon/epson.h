// *****************************************************************************
// lcd.h
//
// include file for Epson S1D15G00 LCD Controller
//
//
// Author: James P Lynch August 30, 2007
// *****************************************************************************

#ifndef _Epson_h__
#define _Epson_h__

#include <stdio.h> // defines NULL
#include <avr/io.h>
#include <avr/pgmspace.h>
#include "util.h"

typedef unsigned char byte;

// Hardware definitions. Thse are all on port B
#define LCD_PORT	PORTB
#define LCD_DDR 	DDRB
#define PIN_CS    4   // ~Chip select
#define PIN_CLOCK 7   // Clock
#define PIN_DATA  5   // Data
#define PIN_RESET 6   // ~Reset


// Enable subsystems
#define EPSON_EN_LINE        1
#define EPSON_EN_CIRCLE      1
#define EPSON_EN_RECTANGLE   1
#define EPSON_EN_TEXT        1
#define EPSON_EN_TEXT_SMALL  1
#define EPSON_EN_TEXT_MEDIUM 1
#define EPSON_EN_TEXT_LARGE  1

// Command definitions
#define DISON   0xAF // Display on
#define DISOFF  0xAE // Display off
#define DISNOR  0xA6 // Normal display
#define DISINV  0xA7 // Inverse display
#define COMSCN  0xBB // Common scan direction
#define DISCTL  0xCA // Display control
#define SLPIN   0x95 // Sleep in
#define SLPOUT  0x94 // Sleep out
#define PASET   0x75 // Page address set
#define CASET   0x15 // Column address set
#define DATCTL  0xBC // Data scan direction, etc.
#define RGBSET8 0xCE // 256-color position set
#define RAMWR   0x5C // Writing to memory
#define RAMRD   0x5D // Reading from memory
#define PTLIN   0xA8 // Partial display in
#define PTLOUT  0xA9 // Partial display out
#define RMWIN   0xE0 // Read and modify write
#define RMWOUT  0xEE // End
#define ASCSET  0xAA // Area scroll set
#define SCSTART 0xAB // Scroll start set
#define OSCON   0xD1 // Internal oscillation on
#define OSCOFF  0xD2 // Internal oscillation off
#define PWRCTR  0x20 // Power control
#define VOLCTR  0x81 // Electronic volume control
#define VOLUP   0xD6 // Increment electronic control by 1
#define VOLDOWN 0xD7 // Decrement electronic control by 1
#define TMPGRD  0x82 // Temperature gradient set
#define EPCTIN  0xCD // Control EEPROM
#define EPCOUT  0xCC // Cancel EEPROM control
#define EPMWR   0xFC // Write into EEPROM
#define EPMRD   0xFD // Read from EEPROM
#define EPSRRD1 0x7C // Read register 1
#define EPSRRD2 0x7D // Read register 2
#define NOP     0x25 // NOP instruction


// Enums for fill
#define NOFILL  0
#define FILL    1

// Enums for text sizes
#define SMALL   0
#define MEDIUM  1
#define LARGE   2



// 12-bit color definitions
#define WHITE   0xFFF
#define BLACK   0x000
#define RED     0xF00
#define GREEN   0x0F0
#define BLUE    0x00F
#define CYAN    0x0FF
#define MAGENTA 0xF0F
#define YELLOW  0xFF0
#define BROWN   0xB22
#define ORANGE  0xFA0
#define PINK    0xF6A




void InitLcd(void);
void WriteSpiCommand(byte command);
void WriteSpiData(byte data);
void LCDClearScreen(void);
void LCDSetPixel(const byte x, const byte y, const int color);

#if (EPSON_EN_LINE == 1) || (EPSON_EN_RECTANGLE == 1)
  void LCDSetLine(byte x1, byte y1, byte x2, byte y2, int color);
#endif

#if EPSON_EN_RECTANGLE == 1
  void LCDSetRect(byte x0, byte y0, byte x1, byte y1, byte fill, int color);
#endif

#if EPSON_EN_CIRCLE == 1
  void LCDSetCircle(byte x0, byte y0, int radius, int color);
#endif

#if EPSON_EN_TEXT == 1
  void LCDPutChar(const char c, 
                  const byte x, 
                  const byte y, 
                  const byte size, 
                  const int fColor, 
                  const int bColor);
  void LCDPutStr(const char *pString, 
                 const byte x, 
                 const byte y, 
                 const int fontSize, 
                 const int fColor, 
                 const int bColor);
               


  extern const byte *FontTable[3];
  
  #if EPSON_EN_TEXT_SMALL == 1
    extern const unsigned char FONT6x8[];
  #endif
  #if EPSON_EN_TEXT_MEDIUM == 1
    extern const unsigned char FONT8x8[];
  #endif
  #if EPSON_EN_TEXT_LARGE == 1
    extern const unsigned char FONT8x16[];
  #endif
  
#endif

#endif

