//*******************************************************
//					Nokia Shield
//*******************************************************
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "util.h"
#include "epson.h"


static int uart_putchar(char c, FILE *stream);
uint8_t uart_getchar(void);
void ioinit(void);
void delay_us(int x);
static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);

void println(const char *s)
{
    printf("%s\n", s);
}


//********************************************************************
//
// Main loop
//
//********************************************************************
void loop(int b) {
  
  byte i, j, ch;

  const int TempColor[11] = { WHITE, 
                              BLACK, 
                              RED, 
                              GREEN, 
                              BLUE, 
                              CYAN, 
                              MAGENTA,
                              YELLOW, 
                              BROWN, 
                              ORANGE, 
                              PINK };

  const char *TempChar[11] = { "White", 
                               "Black", 
                               "Red", 
                               "Green", 
                               "E/Blue", 
                               "Cyan",
                               "Magenta", 
                               "Yellow", 
                               "N/Brown", 
                               "Orange", 
                               "Pink" };

  const char *Help[11] = {
      "Init + 0/1: Off/On", 
      "Foreground+ColName",
      "Background+ColName",
      "Clear", 
      "Names",
      "Pixels",
      "Strings",
      "Rectangles",
      "Lines",
      "Arc",
      "TextColors",
  };

  static int bgCol = BLACK;
  static int fgCol = WHITE;
  
    switch(tolower(b)) {
      
      case 'f':
      case 'b':
        {
           ch = uart_getchar();
           for (i = 0; i < 11; i++) {
               if (toupper(ch) == TempChar[i][0]) {
                   printf("%s is ", TempChar[i]);
                   if (tolower(b) == 'f') {
                       printf("FG\n");
                       fgCol = TempColor[i];
                   }
                   else {
                       printf("BG\n");
                       bgCol = TempColor[i];
                   }
               }
           }
        }
        break;

      case 'i':
        InitLcd();
        println("Initialize");
        LCDClearScreen();
        bgCol = BLACK;
        fgCol = WHITE;
        break;    

      case 'n':
        LCDPutStr("NAMES:", 120, 5, MEDIUM, fgCol, bgCol);
        LCDSetLine(117, 2, 117, 128, fgCol);
        for (j = 0; j < 11; j++) {
            LCDPutStr("Colour:", 105-j*10, 5, MEDIUM, TempColor[j], bgCol);
            LCDPutStr(TempChar[j], 105-j*10, 64, SMALL, fgCol, bgCol);
        }
        break;

      case 'h':
        LCDPutStr("HELP: (h)", 120, 5, MEDIUM, fgCol, bgCol);
        LCDSetLine(117, 2, 117, 128, fgCol);
        for (j = 0; j < 11; j++) {
            LCDPutChar(Help[j][0], 105-j*10, 2, MEDIUM, fgCol, bgCol);
            LCDPutChar(':', 110-j*10-5, 10, MEDIUM, fgCol, bgCol);
            LCDPutStr(Help[j], 105-j*10, 20, SMALL, fgCol, bgCol);
        }
        break;
      
      case 'c':
        //LCDClearScreen();
        LCDSetRect(0, 0, 131, 131, FILL, bgCol);
        println("Clear");
        break;    
     
      case '0':
        WriteSpiCommand(DISOFF);
        println("Display off");
        break;
     
      case '1':
        WriteSpiCommand(DISON);
        println("Display on");
        break;
     
     
      case 'p':
        
        // set a few pixels
        LCDSetPixel(30, 120, fgCol);
        LCDSetPixel(34, 120, fgCol);
        LCDSetPixel(38, 120, fgCol);
        LCDSetPixel(40, 120, fgCol);
        break;
        
      case 's':  
        
#if EPSON_EN_TEXT == 1

        // draw some characters
        LCDPutChar('1', 2, 2, MEDIUM, fgCol, bgCol);
        LCDPutChar('2', 122, 2, MEDIUM, fgCol, bgCol);
        LCDPutChar('3', 2, 122, MEDIUM, fgCol, bgCol);
        LCDPutChar('4', 122, 122, MEDIUM, fgCol, bgCol);
        
        // draw a string
        LCDPutStr("Hello World", 106,5, LARGE, fgCol, bgCol);
        LCDPutStr("Hello World", 88, 5, LARGE, bgCol, fgCol);
        LCDPutStr("Hello World", 60, 5, MEDIUM, fgCol, bgCol);
        LCDPutStr("Hello World", 40, 5, MEDIUM, bgCol, fgCol);
        LCDPutStr("Hello World", 20, 5, MEDIUM, fgCol, bgCol);
        LCDPutStr("Hello World", 70, 5, SMALL, fgCol, bgCol);
        LCDPutStr("Hello World", 50, 5, SMALL, bgCol, fgCol);
        LCDPutStr("Hello World", 30, 5, SMALL, fgCol, bgCol);
#endif
        break;
        
      case 'r':

#if EPSON_EN_RECTANGLE == 1     
        // draw a filled box
        LCDSetRect(120, 60, 80, 80, FILL, fgCol);
        
        // draw a empty box
        LCDSetRect(120, 85, 80, 105, NOFILL, fgCol);
#endif        
        break;
        
      case 'l':  
        
        // draw some lines
#if EPSON_EN_LINE  == 1
        LCDSetLine(120, 10, 120, 50, fgCol);
        LCDSetLine(120, 50, 80, 50, fgCol);
        LCDSetLine(80, 50, 80, 10, fgCol);
        LCDSetLine(80, 10, 120, 10, fgCol);
        LCDSetLine(120, 85, 80, 105, fgCol);
        LCDSetLine(80, 85, 120, 105, fgCol);
#endif        
        break;
        
      case 'a':  
        // draw an arc
#if EPSON_EN_CIRCLE == 1        
        LCDSetCircle(65, 100, 10, fgCol);
#endif
        break;
        
      case 't':
      
        // ***************************************************************
        // * color test - show boxes of different colors
        // ***************************************************************
        for (j = 0; j < 11; j++) {
          
          // draw a filled box
#if EPSON_EN_RECTANGLE == 1     
          LCDSetRect(10+10*j, 10, 20+10*j, 120, FILL, TempColor[j]);
#endif          
          
          // label the color
#if EPSON_EN_TEXT == 1
        for (i = 0; i < 11; i++) {
          LCDPutChar(TempChar[i][0], 11+10*j, 12+10*i, SMALL,  TempColor[i], TempColor[j]);
        }
          //LCDPutStr(TempChar[j], 25, 40, MEDIUM, YELLOW, BLACK);
#endif          
          
          // wait a bit
          //delay_ms(1000*1);
        }
        
        break;
      
      default:
        println("Bad byte");
        break;  
      
    } // end switch  
    printf("  %c\n", b);


}  


int main(void) 
{

	ioinit();
  // Initialize the LCD display  
  InitLcd();
        println("HELLO!");
  LCDClearScreen();
  LCDSetRect(120, 10, 25, 120, FILL, RED);
  delay_ms(500);

  LCDClearScreen();
  loop('t');
  delay_ms(500);
  
  LCDClearScreen();
  loop('n');
  delay_ms(500);
  
  LCDClearScreen();
  loop('h');

  for (;;)
  {
   int ch = uart_getchar();
   loop(ch);
  }
}

void ioinit(void)
{
    // USART Baud rate: 115200 (With 16 MHz Clock)
    UBRRH = (16 >> 8) & 0x7F;	//Make sure highest bit(URSEL) is 0 indicating we are writing to UBRRH
    UBRRL = 16 & 0xff;
    UCSRA = (1<<U2X);				//Double the UART Speed
    UCSRB = (1<<RXEN)|(1<<TXEN);		//Enable Rx and Tx in UART
    UCSRC = (1<<UCSZ0)|(1<<UCSZ1)|(1<<URSEL);	//8-Bit Characters
    stdout = &mystdout; //Required for printf init
    cli();
	
    // Init timer 1
    //Set Prescaler to 8. (Timer Frequency set to 16Mhz)
    //Used for delay routines
    TCCR0 = (1<<CS00); 	//Divde clock by 1 for 16 Mhz Timer 2 Frequency	
}



static int uart_putchar(char c, FILE *stream)
{
  if (c == '\n')
    uart_putchar('\r', stream);
  
  loop_until_bit_is_set(UCSRA, UDRE);
  UDR = c;
  return 0;
}

uint8_t uart_getchar(void)
{
    while( !(UCSRA & (1<<RXC)) );
	return(UDR);
}

void delay_ms(int x)
{
    for (; x > 0 ; x--)
        delay_us(1000);
}

//General short delays
void delay_us(int x)
{    
    SBIT(PORTB, 0) = 1;
    TIFR = (1<<TOV0); //Clear any interrupt flags on Timer2
    TCNT0= 240; //Setting counter to 239 will make it 16 ticks until TOV is set. .0625uS per
                //click means 1 uS until TOV is set
    while(x>0){
		while( (TIFR & (1<<TOV0)) == 0);
		TIFR = (1<<TOV0); //Clear any interrupt flags on Timer2
		TCNT0=240;
		x--;
	}
    SBIT(PORTB, 0) = 0;
} 

// vim: set sw=4 nu:
