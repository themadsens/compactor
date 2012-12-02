/**
 * Soft UART in & out
 */

#define CR 13
#define LF 10

extern uint8_t CurUartOut;


void SoftUartOutKick(void);
void SoftUartFifo(uint8_t spec);
void NmeaPutFifo(uint8_t f, char *s);

void Task_SoftUartIn(void);
void Task_SoftUartOut(void);

// vim: set sw=3 ts=3 noet nu:
