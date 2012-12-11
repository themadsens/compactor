/**
 * Soft UART in & out
 */

#define CR 13
#define LF 10

typedef struct NmeaBuf {
	pMutex  next;
	uint8_t id;
	uint8_t sz;
	uint8_t ix;
	uint8_t off;
	char    buf[];
} __attribute__((__packed__)) NmeaBuf;

// Uart Out
void NmeaPutFifo(uint8_t f, char *s);

void Task_SoftUartOut(void);
// vim: set sw=3 ts=3 noet nu:
