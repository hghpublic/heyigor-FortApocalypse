#pragma once
#include <stdint.h>

/* -----------------------------------------------------------------------
 * fort3.h — VBlank handler, chopper/robot AI, rocket updates
 *            (converted from fort3.s)
 * ----------------------------------------------------------------------- */

/* VBlank deferred interrupt handler */
void vertblkd(void);

/* Game-loop functions (called from VERTBLKD during GO_MODE) */
void robot_brains(void);
void do_chopper(void);
void do_robot_chopper(void);

/* VBlank sprite-update functions */
void update_chopper(void);
void update_robot_chopper(void);
void update_rockets(void);

/* Utility */
void save_pos(void);  /* copy current chopper position to LAND.* variables */

/* HIT.LIST data (also used by fort2.c) */
extern const uint8_t hit_list[];    /* 22 bytes; indices 0-18 = HIT.LIST.LEN range */
#define HIT_LIST_LEN   18u          /* HIT.LIST.LEN  = *-HIT.LIST-1 */
#define HIT_LIST2_LEN  21u          /* HIT.LIST2.LEN = *-HIT.LIST-1 */
#define TANK_SHAPE     (hit_list + 12u)   /* TANK.SHAPE label = HIT.LIST+12 */

/* Rocket motion tables — indexed by direction status 1-5 (use [status-1]) */
extern const int8_t rocket_dx[5];
extern const int8_t rocket_dy[5];
