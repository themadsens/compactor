/**
 * Soft UART in & out
 */

#define CR 13
#define LF 10

#define SUART_MULFREQ 5
#define SOFTBAUD 4800
#define TCNT2_TOP (F_CPU/8/SUART_MULFREQ/SOFTBAUD)

#define VHF_PAUSE_SEC 6
#define VHF_PAUSE (SOFTBAUD * VHF_PAUSE_SEC)

typedef struct NmeaBuf {
	pMutex  next;
	uint8_t id;
	uint8_t sz;
	uint8_t ix;
	uint8_t off;
	char    buf[];
} __attribute__((__packed__)) NmeaBuf;

extern uint16_t vhfChanged;

// Uart Out
void NmeaPutFifo(uint8_t f, char *s);

void Task_SoftUartOut(void);
// vim: set sw=3 ts=3 noet nu:
