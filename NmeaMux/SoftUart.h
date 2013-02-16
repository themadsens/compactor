/**
 * Soft UART in & out
 */
#ifndef SOFTUART_H
#define SOFTUART_H 1
#include "BigFifo.h"

#define CR 13
#define LF 10

#define SUART_MULFREQ 5
#define SOFTBAUD 4800
#define TCNT2_TOP (F_CPU/8/SUART_MULFREQ/SOFTBAUD)

#define BAUDRATE     38400
#define SER_SCALE 1
#define SERIAL_TOP  (((F_CPU + BAUDRATE / 2) / BAUDRATE / SER_SCALE) - 1)

typedef struct NmeaBuf {
	pMutex  next;
	uint8_t id;
	uint8_t sz;
	uint8_t ix;
	uint8_t off;
	char    buf[];
} __attribute__((__packed__)) NmeaBuf;

pBigFifo pNmeaHiOut;
pBigFifo pNmeaLoOut;

// Uart Out
void NmeaPutFifo(uint8_t f, char *s);
int dbg_putchar(char c, FILE *stream);

void Task_SoftUartOut(void);
#endif
// vim: set sw=3 ts=3 noet nu:
