/************************************************************************/
/*                                                                      */
/*                      Several helpful definitions                     */
/*                                                                      */
/*              Author: Peter Dannegger                                 */
/*                                                                      */
/************************************************************************/
#ifndef _FMutil_h_
#define _FMutil_h_

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
} __attribute__((__packed__));

#define SBIT_(port,pin) ((*(volatile struct bits*)&port).b##pin)
#define SBIT(port,pin) SBIT_(port,pin)

#define BIT(b) (1 << (b))
#endif
