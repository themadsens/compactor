#include <avr/io.h>
#define F_CPU (16 * 10000UL)  // 16 MHz
#include <util/delay.h>

int main(void)
{
	DDRB = 0xf;
    PORTB = 0xaa;
//exit(0);

	while(1) {
		_delay_ms(250);
			PORTB ^= 0x3f;
	}
}

