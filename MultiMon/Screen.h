/**
 * Screen defs
 */

#include <avrx.h>

extern int16_t   Nav_AWA;     // In degrees bows clockwise +-180
extern uint16_t  Nav_AWS;     // In 1/10ths of a knot
extern int16_t   Nav_ATMP;    // In 1/10ths of a degree +-
extern uint16_t  Nav_DBS;     // In 1/100ths m (cm up to 650m)
extern int16_t   Nav_WTMP     // In 1/10ths of a degree +-
extern uint16_t  Nav_STW;     // In 1/100ths of a knot
extern int16_t   Nav_AWA;     // In degrees 0..360

#define NAV_WIND  0x01
#define NAV_DEPTH 0x02
#define NAV_HDG   0x04
extern uint8_t   Nav_redraw;

#define BATT_VHOUSE  0x01
#define BATT_VENGINE 0x02
#define BATT_VSOLAR  0x04
#define BATT_IHOUSE  0x08
#define BATT_ISOLAR  0x10
#define BATT_HOUSEAH 0x20
#define BATT_TEMP    0x40
extern uint8_t  Batt_redraw;

extern uint16_t  Batt_VHOUSE;     // In 1/100 Volt
extern uint16_t  Batt_VENGINE;    // In 1/100 Volt
extern uint16_t  Batt_VSOLAR;     // In 1/100 Volt
extern uint16_t  Batt_IHOUSE;     // In 1/10 Amps
extern uint16_t  Batt_ISOLAR;     // In 1/10 Amps
extern uint16_t  Batt_HOUSEAH;    // In 1/10 AmpHours
extern uint16_t  Batt_TEMP;       // In 1/10 C

extern uint8_t LED_ActivityMask; // Set bits in this for the UI

#define ScreenButtonTick() AvrXSendMessage(ScreenQueue, _ScrnButtonTick)
#define ScreenUpdate()     AvrXSendMessage(ScreenQueue, _ScreenUpd)
