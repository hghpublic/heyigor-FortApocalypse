/* fort7.c — Global game variable storage (converted from fort7.s)
 *
 * Fort7.s is a pure BSS file: every symbol is a .BS N block storage
 * allocation, guaranteed zero-initialised at program start.  In C,
 * global variables at file scope have exactly the same guarantee.
 *
 * Status (.EQ) constants and MAX_TANKS/MAX_PODS are in fort1.h.
 * All variables are declared in fort7.h.
 */

#include "fort7.h"
#include "fort1.h"
#include <stdint.h>

/* From fort1.h:
 * - MAX_TANKS: constant used in array declarations for cruise-missile and tank
 * arrays
 * - MAX_PODS:  constant referenced in comments
 */

/* -------------------------------------------------------------------
 * Scanner display-list address pointers (SCAN.ADR1 / SCAN.ADR2)
 * Each is a 16-bit Atari address split into lo/hi bytes.
 * ------------------------------------------------------------------- */
uint8_t scan_adr1_lo, scan_adr1_hi;
uint8_t scan_adr2_lo, scan_adr2_hi;

/* -------------------------------------------------------------------
 * Scrolling position
 * ------------------------------------------------------------------- */
uint8_t sx;   /* SX   — tile column of scroll origin */
uint8_t sx_f; /* SX.F — sub-pixel fraction of sx     */
uint8_t sy;   /* SY   — tile row of scroll origin    */
uint8_t sy_f; /* SY.F — sub-pixel fraction of sy     */

/* -------------------------------------------------------------------
 * Input state
 * ------------------------------------------------------------------- */
uint8_t consol_flag; /* CONSOL.FLAG */
uint8_t trig_flag;   /* TRIG.FLAG   */

/* -------------------------------------------------------------------
 * Game progress
 * ------------------------------------------------------------------- */
uint8_t level;
uint8_t mode;

/* -------------------------------------------------------------------
 * Landing-site snapshot
 * ------------------------------------------------------------------- */
uint8_t land_x, land_y;           /* LAND.X / LAND.Y             */
uint8_t land_fx, land_fy;         /* LAND.FX / LAND.FY           */
uint8_t land_chop_x, land_chop_y; /* LAND.CHOP.X / LAND.CHOP.Y  */
uint8_t land_chop_angle;          /* LAND.CHOP.ANGLE             */

/* -------------------------------------------------------------------
 * Chopper sprite state
 * ------------------------------------------------------------------- */
uint8_t chopper_status;
uint8_t chopper_x, chopper_y; /* CHOPPER.X / CHOPPER.Y  */
uint8_t ochopper_y;           /* OCHOPPER.Y — previous Y */
uint8_t chopper_angle;
uint8_t chopper_col;      /* CHOPPER.COL — collision byte */
uint8_t chop_x, chop_y;   /* CHOP.X / CHOP.Y (tile coords) */
uint8_t chop_ox, chop_oy; /* CHOP.OX / CHOP.OY (old tile)  */

/* -------------------------------------------------------------------
 * Robot sprite / AI state
 * ------------------------------------------------------------------- */
uint8_t robot_status;
uint8_t r_status; /* R.STATUS — AI phase (FLY/OFF/CRASH) */
uint8_t robot_x, robot_y;
uint8_t orobot_y; /* OROBOT.Y — previous sprite Y */
uint8_t robot_angle;
uint8_t robot_spd; /* ROBOT.SPD — frame-rate AND mask */
uint8_t robot_col;
uint8_t r_fx, r_fy; /* R.FX / R.FY — sub-tile fractions */
uint8_t r_x, r_y;   /* R.X  / R.Y  — map tile position  */

/* -------------------------------------------------------------------
 * Rocket arrays — 3 simultaneous rockets
 * ------------------------------------------------------------------- */
uint8_t rocket_status[3];
uint8_t rocket_x[3];
uint8_t rocket_y[3];
uint8_t rocket_temp[3];  /* ROCKET.TEMP  — saved map tile at impact */
uint8_t rocket_tempx[3]; /* ROCKET.TEMPX — saved map X at impact    */
uint8_t rocket_tempy[3]; /* ROCKET.TEMPY — saved map Y at impact    */
uint8_t rocket_tim[3];   /* ROCKET.TIM   — flight timer             */
uint8_t orocket_y[3];    /* OROCKET.Y    — previous sprite Y        */

/* -------------------------------------------------------------------
 * Elevator
 * ------------------------------------------------------------------- */
uint8_t elevator_num;
uint8_t elevator_dx;
uint8_t elevator_tim;
uint8_t elevator_spd;

/* -------------------------------------------------------------------
 * Score / hi-score / bonus (3-digit BCD, one digit per byte)
 * ------------------------------------------------------------------- */
uint8_t score1, score2, score3;
uint8_t hi1, hi2, hi3;
uint8_t bonus1, bonus2;

/* -------------------------------------------------------------------
 * Fuel
 * ------------------------------------------------------------------- */
uint8_t fuel_status;
uint8_t fuel_temp; /* FUEL.TEMP — countdown to next fuel-base state */
uint8_t fuel1, fuel2;

/* -------------------------------------------------------------------
 * Display colours (applied to ATARI hardware registers by VBlank)
 * ------------------------------------------------------------------- */
uint8_t bak_color;  /* BAK.COLOR  */
uint8_t bak2_color; /* BAK2.COLOR */

/* -------------------------------------------------------------------
 * Cruise-missile arrays (MAX_TANKS = 6 entries)
 * ------------------------------------------------------------------- */
uint8_t cm_status[MAX_TANKS];
uint8_t cm_x[MAX_TANKS];
uint8_t cm_y[MAX_TANKS];
uint8_t cm_time[MAX_TANKS];
uint8_t cm_temp[MAX_TANKS];

/* -------------------------------------------------------------------
 * Tank arrays
 * TANK.TEMP is MAX_TANKS * 3 = 18 bytes (three fields packed per tank)
 * ------------------------------------------------------------------- */
uint8_t tank_status[MAX_TANKS];
uint8_t tank_x[MAX_TANKS];
uint8_t tank_y[MAX_TANKS];
uint8_t tank_dx[MAX_TANKS];
uint8_t tank_temp[18]; /* TANK.TEMP: MAX_TANKS*3 = 18 */

/* -------------------------------------------------------------------
 * Pod / slave counts
 * ------------------------------------------------------------------- */
uint8_t pod_num;
uint8_t pod_com;
uint8_t slave_num;
uint8_t slaves_left;
uint8_t slaves_saved;

/* -------------------------------------------------------------------
 * Fort / laser
 * ------------------------------------------------------------------- */
uint8_t fort_status;
uint8_t laser_status;
uint8_t laser_spd;

/* -------------------------------------------------------------------
 * Speed parameters
 * *_spd   = current per-frame speed mask (modified during play)
 * *_speed = per-level starting value (restored at level init)
 * ------------------------------------------------------------------- */
uint8_t tank_spd, tank_speed;
uint8_t missile_spd, missile_speed;

/* -------------------------------------------------------------------
 * Skill parameters
 * *_skill = raw option setting; *_skl = derived per-level value
 * ------------------------------------------------------------------- */
uint8_t grav_skill, grav_skl;
uint8_t pilot_skill, pilot_skl;

/* -------------------------------------------------------------------
 * Lives / options
 * ------------------------------------------------------------------- */
uint8_t chops;      /* total choppers remaining in the game */
uint8_t chop_left;  /* CHOP.LEFT — choppers left for current player */
uint8_t opt_num;    /* OPT.NUM — selected option row (0-2) */
uint8_t start_pods; /* START.PODS — number of pods to place at level start */
