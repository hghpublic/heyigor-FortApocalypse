#pragma once
#include <stdint.h>
#include "fort1.h"     /* MAX_TANKS, STATUS_*, *_MODE constants */

/* From fort1.h used in this file:
 *   - MAX_TANKS: array size for cm_status, cm_x, cm_y, cm_time, cm_temp,
 *                tank_status, tank_x, tank_y, tank_dx
 */

/* -----------------------------------------------------------------------
 * fort7.h — Global game variables (converted from fort7.s BSS section)
 *
 * All variables are zero-initialised at program start (C global default).
 * Status and mode constants are in fort1.h; MAX_TANKS=6, MAX_PODS=39.
 * ----------------------------------------------------------------------- */

/* Scanner display-list address (SCAN.ADR1/2 — each .BS 2 split lo/hi) */
extern uint8_t scan_adr1_lo, scan_adr1_hi;
extern uint8_t scan_adr2_lo, scan_adr2_hi;

/* Scrolling position — tile and sub-pixel fractions */
extern uint8_t sx, sx_f;
extern uint8_t sy, sy_f;

/* Input state */
extern uint8_t consol_flag;  /* CONSOL.FLAG */
extern uint8_t trig_flag;    /* TRIG.FLAG   */

/* Game progress */
extern uint8_t level;
extern uint8_t mode;

/* Landing-site snapshot (saved when chopper touches down) */
extern uint8_t land_x,      land_y;
extern uint8_t land_fx,     land_fy;
extern uint8_t land_chop_x, land_chop_y, land_chop_angle;

/* Chopper sprite state */
extern uint8_t chopper_status;
extern uint8_t chopper_x,  chopper_y,  ochopper_y;
extern uint8_t chopper_angle;
extern uint8_t chopper_col;
extern uint8_t chop_x,  chop_y;
extern uint8_t chop_ox, chop_oy;

/* Robot sprite / AI state */
extern uint8_t robot_status;
extern uint8_t r_status;
extern uint8_t robot_x,  robot_y,  orobot_y;
extern uint8_t robot_angle;
extern uint8_t robot_spd;
extern uint8_t robot_col;
extern uint8_t r_fx, r_fy;
extern uint8_t r_x,  r_y;

/* Rocket arrays (3 rockets) */
extern uint8_t rocket_status[3];
extern uint8_t rocket_x[3];
extern uint8_t rocket_y[3];
extern uint8_t rocket_temp[3];   /* saved map tile at impact */
extern uint8_t rocket_tempx[3];  /* saved map X at impact    */
extern uint8_t rocket_tempy[3];  /* saved map Y at impact    */
extern uint8_t rocket_tim[3];
extern uint8_t orocket_y[3];

/* Elevator */
extern uint8_t elevator_num;
extern uint8_t elevator_dx;
extern uint8_t elevator_tim;
extern uint8_t elevator_spd;

/* Score / hi-score / bonus */
extern uint8_t score1, score2, score3;
extern uint8_t hi1, hi2, hi3;
extern uint8_t bonus1, bonus2;

/* Fuel */
extern uint8_t fuel_status;
extern uint8_t fuel_temp;
extern uint8_t fuel1, fuel2;

/* Display colours */
extern uint8_t bak_color;
extern uint8_t bak2_color;

/* Cruise-missile arrays (MAX_TANKS = 6 entries) */
extern uint8_t cm_status[MAX_TANKS];
extern uint8_t cm_x[MAX_TANKS];
extern uint8_t cm_y[MAX_TANKS];
extern uint8_t cm_time[MAX_TANKS];
extern uint8_t cm_temp[MAX_TANKS];

/* Tank arrays (MAX_TANKS = 6 entries; TANK.TEMP is MAX_TANKS*3 = 18) */
extern uint8_t tank_status[MAX_TANKS];
extern uint8_t tank_x[MAX_TANKS];
extern uint8_t tank_y[MAX_TANKS];
extern uint8_t tank_dx[MAX_TANKS];
extern uint8_t tank_temp[18];

/* Pod / slave counts */
extern uint8_t pod_num;
extern uint8_t pod_com;
extern uint8_t slave_num;
extern uint8_t slaves_left;
extern uint8_t slaves_saved;

/* Fort / laser */
extern uint8_t fort_status;
extern uint8_t laser_status;
extern uint8_t laser_spd;

/* Speed parameters (base values set per level; *_spd = current, *_speed = per-level default) */
extern uint8_t tank_spd,     tank_speed;
extern uint8_t missile_spd,  missile_speed;

/* Skill parameters */
extern uint8_t grav_skill,   grav_skl;
extern uint8_t pilot_skill,  pilot_skl;

/* Lives / options */
extern uint8_t chops;
extern uint8_t chop_left;
extern uint8_t opt_num;
extern uint8_t start_pods;
