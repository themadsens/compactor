/**
 * Screen defs
 */
#ifndef _Screen_h__
#define _Screen_h__

#include <avr/io.h>

enum SCREEN {SCREEN_NAV = 0, SCREEN_BATT, SCREEN_CNFG , SCREEN_NUM};
extern uint8_t curScreen;

#define CnvKt2Ms_I(N, D) ((N * 1852 / 3600) / D)
#define CnvKt2Ms_F(N, D) ((N * 1852 / 3600) % D)

extern int16_t   Nav_AWA;     // In degrees bows clockwise +-180
extern uint16_t  Nav_AWS;     // In 1/10ths of a knot
extern int16_t   Nav_ATMP;    // In 1/10ths of a degree +-
extern uint16_t  Nav_DBS;     // In 1/100ths m (cm up to 650m)
extern int16_t   Nav_WTMP;    // In 1/10ths of a degree +-
extern uint16_t  Nav_STW;     // In 1/100ths of a knot
extern int16_t   Nav_AWA;     // In degrees 0..360
extern int16_t   Nav_SUM;

#define NAV_WIND  0
#define NAV_DEPTH 1
#define NAV_HDG   2
extern uint8_t   Nav_redraw;

#define BATT_VHOUSE  0
#define BATT_VENGINE 1
#define BATT_VSOLAR  2
#define BATT_IHOUSE  3
#define BATT_ISOLAR  4
#define BATT_HOUSEAH 5
#define BATT_TEMP    6
extern uint8_t  Batt_redraw;

extern uint16_t  Batt_VHOUSE;     // In 1/100 Volt
extern uint16_t  Batt_VENGINE;    // In 1/100 Volt
extern uint16_t  Batt_VSOLAR;     // In 1/100 Volt
extern uint16_t  Batt_IHOUSE;     // In 1/10 Amps
extern uint16_t  Batt_ISOLAR;     // In 1/10 Amps
extern uint16_t  Batt_HOUSEAH;    // In 1/10 AmpHours
extern uint16_t  Batt_TEMP;       // In 1/10 C

extern uint8_t LED_ActivityMask; // Set bits in this for the UI

extern uint16_t Cnfg_AWSFAC EEPROM;    // in 1/1000
extern uint16_t Cnfg_STWFAC EEPROM;    // in 1/1000
extern uint16_t Cnfg_DBSOFF EEPROM;    // in cm
extern uint16_t Cnfg_AWAOFF EEPROM;    // in deg
extern uint16_t Cnfg_HOUSEAH EEPROM;   // in AH
extern uint16_t Cnfg_PEUKERT EEPROM;   // in 1/1000
extern uint8_t SatSNR[12];
extern uint8_t SatHDOP;    // 5..30 or so
extern uint8_t SatMODE;    // 1: No fix, 2: 2D fix, 3: 3D fix

void ScreenButtonTick(void);
void ScreenUpdate(void);

#endif