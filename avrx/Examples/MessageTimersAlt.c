/*
    Alternative Timer Message Example

    This example is identical to "MessageTimers.c" except that
    it uses the function AvrXCancelTimerMessage() to remove the
    timer from the time queue manager and restart it.

*/
//#define ENABLE_BIT_DEFINITIONS
//#define _SFR_ASM_COMPAT 1
#include <avrx-io.h>
#include <avrx-signal.h>
#include "avrx.h"               // AvrX System calls/data structures
#include "serialio.h"           // From AvrX...
#include "hardware.h"           // Sample code hardware defines

// Declare all my AvrX data structures

AVRX_IAR_TASK(Monitor, 0, 20, 0);       // External Task: Debug Monitor
AVRX_GCC_TASK(Monitor, 20, 0);          // External Task: Debug Monitor

TimerControlBlock SwTimer;      // Plain vanilla timer
TimerMessageBlock Timer;        // TimerMessage timer (just a timer with a
                                // message queue pointer appended)

// This is how you build up a message element that transports data

typedef struct MyMessage
{
    MessageControlBlock mcb;
    unsigned char data;
}
*pMyMessage, MyMessage;

MyMessage SwitchMsg;            // Declare the actual message
MessageQueue MyQueue;           // The message queue itself

/*
 Timer 0 Overflow Interrupt Handler

 Prototypical Interrupt handler:
 . Switch to kernel context
 . handle interrupt
 . switch back to interrupted context.
 */
#pragma optimize=z 4
AVRX_SIGINT(SIG_OVERFLOW0)
{
    IntProlog();                // Save interrupted context, switch stacks
    TCNT0 = TCNT0_INIT;
//    outp(TCNT0_INIT, TCNT0);    // Reload the timer counter
    AvrXTimerHandler();         // Process Timer queue
    Epilog();                   // Restore context of next running task
}

AVRX_IAR_TASKDEF(flasher, 4, 16, 2)
AVRX_GCC_TASKDEF(flasher, 20, 2)    // Note I added r_stack and c_stack!
{
    unsigned char led = 0;
    pMyMessage pMsg;

	LEDDDR = 0xFF;
//    outp(0xFF, LEDDDR);            // Initialize LED port to outputs

    while(1)
    {

        AvrXStartTimerMessage(&Timer, 150, &MyQueue);
        LED = ~led;
//        outp(~led, LED);
        pMsg = (pMyMessage)AvrXWaitMessage(&MyQueue);
        if (pMsg == &SwitchMsg)
        {
            led ^= ~(pMsg->data);
            AvrXAckMessage(&pMsg->mcb);
            if (AvrXCancelTimerMessage(&Timer, &MyQueue) != &Timer.u.mcb)
                AvrXHalt();     // The timer should always be found...
        }
        else if ((pTimerMessageBlock)pMsg == &Timer)
        {
#ifdef __IAR_SYSTEMS_ICC__
            led = led+led+((led & 0x80)?1:0);
#else
            asm("rol %0\n"\
                "\tbrcc .+2\n"\
                "\tinc %0\n"  ::"r" (led));
#endif
        }
        else
        {
            AvrXHalt();
        }
    }
}
AVRX_IAR_TASKDEF(switcher, 4, 16, 3)
AVRX_GCC_TASKDEF(switcher, 10, 3)
{
//    outp(0xFF, SWITCHP);            // Enable pullups on switch inputs
    SWITCHP = 0xFF;            // Enable pullups on switch inputs
    while(1)
    {
        AvrXDelay(&SwTimer, 10);    // Delay 10 milliseconds
//        if (SwitchMsg.data != inp(SWITCH)) // On change, send message
        if (SwitchMsg.data != SWITCH) // On change, send message
        {
//            SwitchMsg.data = inp(SWITCH);
            SwitchMsg.data = SWITCH;
            AvrXSendMessage(&MyQueue, &SwitchMsg.mcb);
            AvrXWaitMessageAck(&SwitchMsg.mcb);
        }
    }
}

int main(void)
{
    AvrXSetKernelStack(0);
    MCUCR = 1<<SE;
    TCNT0 = TCNT0_INIT;
    TCCR0 = TMC8_CK256;
    TIMSK = 1<<TOIE0;
/*
    outp((1<<SE) , MCUCR);      // Enable "sleep" mode (low power when idle)
    outp(TCNT0_INIT, TCNT0);    // Load overflow counter of timer0
    outp(TMC8_CK256 , TCCR0);   // Set Timer0 to CPUCLK/256
    outp((1<<TOIE0), TIMSK);    // Enable interrupt flag
*/
    AvrXRunTask(TCB(flasher));
    AvrXRunTask(TCB(switcher));
    AvrXRunTask(TCB(Monitor));

    AvrXSetSemaphore(&EEPromMutex); // Enables EEPROM access in monitor
    InitSerialIO(UBRR_INIT);    // Initialize USART baud rate generator

    Epilog();                   // Switch from AvrX Stack to first task
    return 0;
}

