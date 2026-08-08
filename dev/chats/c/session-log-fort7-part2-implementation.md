# Session Log: convert fort7.s — Part 2: Implementation & Testing

## Files Produced
- `dev/src/fort7.h` (119 lines)
- `dev/src/fort7.c` (170 lines)

---

## fort7.h

```c
#pragma once
#include <stdint.h>
#include "fort1.h"     /* MAX_TANKS, STATUS_*, *_MODE constants */

/* All variables zero-initialised at program start (C global default).
 * Status and mode constants are in fort1.h; MAX_TANKS=6, MAX_PODS=39. */

/* Scanner display-list address (SCAN.ADR1/2 — each .BS 2 split lo/hi) */
extern uint8_t scan_adr1_lo, scan_adr1_hi;
extern uint8_t scan_adr2_lo, scan_adr2_hi;

/* Scrolling position — tile and sub-pixel fractions */
extern uint8_t sx, sx_f;
extern uint8_t sy, sy_f;

/* Input state */
extern uint8_t consol_flag;
extern uint8_t trig_flag;

/* Game progress */
extern uint8_t level;
extern uint8_t mode;

/* Landing-site snapshot */
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
extern uint8_t rocket_temp[3];
extern uint8_t rocket_tempx[3];
extern uint8_t rocket_tempy[3];
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

/* Tank arrays (MAX_TANKS = 6; TANK.TEMP is MAX_TANKS*3 = 18) */
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

/* Speed parameters (*_spd = current, *_speed = per-level default) */
extern uint8_t tank_spd,    tank_speed;
extern uint8_t missile_spd, missile_speed;

/* Skill parameters (*_skill = option, *_skl = derived) */
extern uint8_t grav_skill,  grav_skl;
extern uint8_t pilot_skill, pilot_skl;

/* Lives / options */
extern uint8_t chops;
extern uint8_t chop_left;
extern uint8_t opt_num;
extern uint8_t start_pods;
```

---

## fort7.c

```c
/* fort7.c — Global game variable storage (converted from fort7.s)
 *
 * Fort7.s is a pure BSS file: every symbol is a .BS N block storage
 * allocation, guaranteed zero-initialised at program start.  In C,
 * global variables at file scope have exactly the same guarantee.
 */

#include <stdint.h>
#include "fort1.h"
#include "fort7.h"

/* Scanner display-list address pointers */
uint8_t scan_adr1_lo, scan_adr1_hi;
uint8_t scan_adr2_lo, scan_adr2_hi;

/* Scrolling position */
uint8_t sx;     /* SX   — tile column of scroll origin */
uint8_t sx_f;   /* SX.F — sub-pixel fraction of sx     */
uint8_t sy;     /* SY   — tile row of scroll origin    */
uint8_t sy_f;   /* SY.F — sub-pixel fraction of sy     */

/* Input state */
uint8_t consol_flag;
uint8_t trig_flag;

/* Game progress */
uint8_t level;
uint8_t mode;

/* Landing-site snapshot */
uint8_t land_x,      land_y;
uint8_t land_fx,     land_fy;
uint8_t land_chop_x, land_chop_y;
uint8_t land_chop_angle;

/* Chopper sprite state */
uint8_t chopper_status;
uint8_t chopper_x,   chopper_y;
uint8_t ochopper_y;                /* previous sprite Y */
uint8_t chopper_angle;
uint8_t chopper_col;
uint8_t chop_x,  chop_y;
uint8_t chop_ox, chop_oy;         /* previous tile coords */

/* Robot sprite / AI state */
uint8_t robot_status;
uint8_t r_status;
uint8_t robot_x,  robot_y;
uint8_t orobot_y;
uint8_t robot_angle;
uint8_t robot_spd;
uint8_t robot_col;
uint8_t r_fx, r_fy;
uint8_t r_x,  r_y;

/* Rocket arrays */
uint8_t rocket_status[3];
uint8_t rocket_x[3];
uint8_t rocket_y[3];
uint8_t rocket_temp[3];
uint8_t rocket_tempx[3];
uint8_t rocket_tempy[3];
uint8_t rocket_tim[3];
uint8_t orocket_y[3];

/* Elevator */
uint8_t elevator_num;
uint8_t elevator_dx;
uint8_t elevator_tim;
uint8_t elevator_spd;

/* Score / hi-score / bonus */
uint8_t score1, score2, score3;
uint8_t hi1,    hi2,    hi3;
uint8_t bonus1, bonus2;

/* Fuel */
uint8_t fuel_status;
uint8_t fuel_temp;
uint8_t fuel1, fuel2;

/* Display colours */
uint8_t bak_color;
uint8_t bak2_color;

/* Cruise-missile arrays */
uint8_t cm_status[MAX_TANKS];
uint8_t cm_x[MAX_TANKS];
uint8_t cm_y[MAX_TANKS];
uint8_t cm_time[MAX_TANKS];
uint8_t cm_temp[MAX_TANKS];

/* Tank arrays */
uint8_t tank_status[MAX_TANKS];
uint8_t tank_x[MAX_TANKS];
uint8_t tank_y[MAX_TANKS];
uint8_t tank_dx[MAX_TANKS];
uint8_t tank_temp[18];           /* MAX_TANKS*3 = 18 */

/* Pod / slave counts */
uint8_t pod_num;
uint8_t pod_com;
uint8_t slave_num;
uint8_t slaves_left;
uint8_t slaves_saved;

/* Fort / laser */
uint8_t fort_status;
uint8_t laser_status;
uint8_t laser_spd;

/* Speed parameters */
uint8_t tank_spd,    tank_speed;
uint8_t missile_spd, missile_speed;

/* Skill parameters */
uint8_t grav_skill,  grav_skl;
uint8_t pilot_skill, pilot_skl;

/* Lives / options */
uint8_t chops;
uint8_t chop_left;
uint8_t opt_num;
uint8_t start_pods;
```

---

## Compile Test

### Command
```
cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src \
    dev/src/fort2.c dev/src/fort3.c dev/src/fort4.c \
    dev/src/fort5.c dev/src/fort6.c dev/src/fort7.c
```

### Result
```
(no output — zero diagnostics)
```

---

## Resolved Extern References After fort7.c

Previously unresolved symbols in fort2.c–fort5.c now resolved:

| Symbol | Referenced in | Defined in |
|---|---|---|
| `scan_adr1_lo`, `scan_adr1_hi` | fort5.c:86 | fort7.c |
| `scan_adr2_lo`, `scan_adr2_hi` | fort5.c:87 | fort7.c |
| `sx`, `sx_f`, `sy`, `sy_f` | fort1–4.c | fort7.c |
| `consol_flag` | fort2.c:64 | fort7.c |
| `trig_flag` | fort4.c:124 | fort7.c |
| `level` | fort1.c, fort3.c, fort5.c | fort7.c |
| `mode` | fort1.c–fort5.c | fort7.c |
| `land_x`, `land_y`, `land_fx`, `land_fy` | fort1.c, fort3.c, fort5.c | fort7.c |
| `land_chop_x`, `land_chop_y`, `land_chop_angle` | fort1.c, fort3.c, fort5.c | fort7.c |
| `chopper_status` | fort1.c–fort5.c | fort7.c |
| `chopper_x`, `chopper_y`, `ochopper_y` | fort1.c–fort4.c | fort7.c |
| `chopper_angle`, `chopper_col` | fort1.c–fort4.c | fort7.c |
| `chop_x`, `chop_y` | fort1.c–fort5.c | fort7.c |
| `robot_status`, `r_status` | fort2.c–fort4.c | fort7.c |
| `robot_x`, `robot_y`, `orobot_y` | fort3.c | fort7.c |
| `robot_angle`, `robot_spd`, `robot_col` | fort1.c, fort3.c | fort7.c |
| `r_fx`, `r_fy`, `r_x`, `r_y` | fort3.c, fort4.c | fort7.c |
| `rocket_status[3]`, `rocket_x[3]`, `rocket_y[3]` | fort2.c–fort4.c | fort7.c |
| `rocket_temp[3]`, `rocket_tempx[3]`, `rocket_tempy[3]` | fort2.c, fort3.c | fort7.c |
| `rocket_tim[3]`, `orocket_y[3]` | fort3.c | fort7.c |
| `elevator_num`, `elevator_dx`, `elevator_tim`, `elevator_spd` | fort1.c, fort4.c | fort7.c |
| `score1`, `score2`, `score3` | fort1.c, fort4.c | fort7.c |
| `hi1`, `hi2`, `hi3` | fort1.c | fort7.c |
| `bonus1`, `bonus2` | fort1.c–fort5.c | fort7.c |
| `fuel_status`, `fuel_temp`, `fuel1`, `fuel2` | fort1.c, fort4.c, fort5.c | fort7.c |
| `bak_color`, `bak2_color` | fort1.c–fort5.c | fort7.c |
| `cm_status[6]`..`cm_temp[6]` | fort1.c, fort2.c | fort7.c |
| `tank_status[6]`..`tank_temp[18]` | fort1.c, fort2.c | fort7.c |
| `pod_num`, `pod_com` | fort1.c, fort2.c | fort7.c |
| `slave_num`, `slaves_left`, `slaves_saved` | fort1.c, fort5.c | fort7.c |
| `fort_status`, `laser_status`, `laser_spd` | fort1.c, fort4.c, fort5.c | fort7.c |
| `tank_spd`, `tank_speed`, `missile_spd`, `missile_speed` | fort1.c, fort2.c | fort7.c |
| `grav_skill`, `grav_skl`, `pilot_skill` | fort1.c–fort3.c | fort7.c |
| `chops`, `chop_left`, `opt_num`, `start_pods` | fort1.c, fort2.c | fort7.c |

---

## Remaining Open Items (for fort8.c)

| Symbol | Referenced in |
|---|---|
| `temp1`..`temp6` | fort2.c–fort5.c |
| `adr1_lo`, `adr1_hi`, `adr2_lo`, `adr2_hi` | fort1.c–fort5.c |
| `adr1_i_lo`, `adr1_i_hi`, `adr2_i_lo`, `adr2_i_hi` | fort3.c, fort4.c |
| `temp1_i`..`temp4_i` | fort3.c, fort4.c |
| `s_adr_lo`, `s_adr_hi`, `s_flg` | fort1.c, fort4.c |
| `frame_count` | fort1.c |
| `ssizem` | fort3.c |
| `player_base[]` | fort3.c |
| `slave_status[8]`, `slave_x[8]`, `slave_y[8]`, `slave_dx[8]` | fort1.c, fort5.c |
| `pod_status[39]`, `pod_x[39]`, `pod_y[39]`, `pod_dx[39]` | fort1.c, fort2.c |
| `pod_temp1[39]`, `pod_temp2[39]` | fort2.c |
| `tank_start_x[6]`, `tank_start_y[6]` | fort1.c, fort2.c |
| `tim1_val`..`tim9_val` | fort2.c–fort5.c |
| `s1_1_val`, `s1_2_val`, `s2_val`..`s6_val` | fort2.c–fort5.c |
| `demo_status`, `demo_count` | fort2.c, fort4.c |
| `temp_mode`, `game_points` | fort2.c |

---

## File Summary

| File | Lines | Purpose |
|---|---|---|
| `dev/src/fort7.h` | 119 | Extern declarations for all game variables |
| `dev/src/fort7.c` | 170 | Zero-initialised global definitions |
