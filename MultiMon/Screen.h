/**
 * Screen defs
 */
#ifndef _Screen_h__
#define _Screen_h__

#include <avr/io.h>
#include <stdarg.h>

#define NO_VAL 0xffff

enum SCREEN {
	SCREEN_NAV = 0,
	SCREEN_BATT,
	SCREEN_GPS,
	SCREEN_ANCH,
	SCREEN_CNFG,
	SCREEN_NUM
};
extern uint8_t curScreen;

#define CnvKt2Ms_I(N, D) (muldiv(N, 1852, 3600) / D)
#define CnvKt2Ms_F(N, D) (muldiv(N, 1852, 3600) % D)

extern int16_t   Nav_AWA;     // In degrees bows clockwise +-180
extern uint16_t  Nav_AWS;     // In 1/10ths of a knot
extern int16_t   Nav_ATMP;    // In 1/10ths of a degree +-
extern uint16_t  Nav_DBS;     // In 1/100ths m (cm up to 650m)
extern int16_t   Nav_WTMP;    // In 1/10ths of a degree +-
extern uint16_t  Nav_STW;     // In 1/100ths of a knot
extern int16_t   Nav_AWA;     // In degrees 0..360
extern int16_t   Nav_HDG;     // In degrees 0..360
extern uint16_t  Nav_SUM;

extern uint16_t maxWind[24];
extern uint16_t maxWindCur;

#define NAV_WIND  0
#define NAV_DEPTH 1
#define NAV_HDG   2
#define NAV_ATMP  3
extern uint8_t   Nav_redraw;

extern uint8_t  Batt_redraw;

#define GPS_SAT 0
#define GPS_POS 1
extern uint8_t   Gps_redraw;
extern uint8_t SatSNR[12];
extern uint16_t SatHDOP;    // 5..30 or so
extern uint8_t SatMODE;    // 1: No fix, 2: 2D fix, 3: 3D fix
extern char *Gps_STR[];

extern uint16_t  Batt_VHOUSE;     // In 1/100 Volt
extern uint16_t  Batt_VENGINE;    // In 1/100 Volt
extern uint16_t  Batt_VSOLAR;     // In 1/100 Volt
extern uint16_t  Batt_IHOUSE;     // In 1/10 Amps
extern uint16_t  Batt_ISOLAR;     // In 1/10 Amps
extern uint16_t  Batt_HOUSEAH;    // In 1/10 AmpHours
extern uint16_t  Batt_TEMP;       // In 1/10 C
#define BATT_VHOUSE  0
#define BATT_VENGINE 1
#define BATT_IHOUSE  2
#define BATT_ISOLAR  3
#define BATT_TEMP    4
#define BATT_HOUSEAH 5

extern uint8_t LED_ActivityMask; // Set bits in this for the UI

extern uint16_t Cnfg_AWSFAC EEPROM;    // in 1/1000
extern uint16_t Cnfg_STWFAC EEPROM;    // in 1/1000
extern uint16_t Cnfg_DBSOFF EEPROM;    // in cm
extern uint16_t Cnfg_AWAOFF EEPROM;    // in deg
extern uint16_t Cnfg_HOUSEAH EEPROM;   // in AH
extern uint16_t Cnfg_PEUKERT EEPROM;   // in 1/1000

extern void DoBuzzer(void);
extern void ScreenUpdate(void);
extern void ScreenUpdateInt(void);

extern void setAlarm_P(const char *str, ...);

#endif
