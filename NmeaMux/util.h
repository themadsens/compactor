/************************************************************************/
/*                                                                      */
/*                      Several helpful definitions                     */
/*                                                                      */
/*              Author: Peter Dannegger                                 */
/*                                                                      */
/************************************************************************/
#ifndef _FMutil_h_
#define _FMutil_h_
#include <avrx.h> // sbi, cbi
#include <math.h> // sbi, cbi

// 			Access bits like variables:

typedef uint8_t  u8 ;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   i8 ;
typedef int16_t  i16;
typedef int32_t  i32;

struct bits {
  u8 b0:1;
  u8 b1:1;
  u8 b2:1;
  u8 b3:1;
  u8 b4:1;
  u8 b5:1;
  u8 b6:1;
  u8 b7:1;
} __attribute__((__packed__)) __attribute__((__may_alias__));

#define SBIT_(port,pin) (((volatile struct bits*)&port)->b##pin)
#define SBIT(port,pin) SBIT_(port,pin)

#define BSET(port,pin) sbi(port,pin)
#define BCLR(port,pin) cbi(port,pin)

#define BIT(b) (1 << (b))

inline i16 muldiv(i16 a, i16 b, i16 c) {
    return round((double) a * b / c);
}

#define CRIT(x) do {BeginCritical(); x; EndCritical();} while(0)

#define DEBUG(fmt, ...) fprintf_P(stderr, PSTR(fmt "\n"), ##__VA_ARGS__)

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))


extern int16_t msTick;
extern uint16_t secTick;
extern uint16_t hdayTick;
extern int16_t msAge;
void delay_ms(int x);
struct TimerControlBlock;
struct TimerControlBlock *myTimer(void);
#endif
