#pragma once
#include <stdint.h>

/* -----------------------------------------------------------------------
 * fort4.h — Main interrupt driver part II (converted from fort4.s)
 * ----------------------------------------------------------------------- */

/* Digit position counter — set by caller before each group of ddig() calls.
 * Score display: set to 5 (6 digits).  Bonus/fuel: set to 3 (4 digits).
 * Single-digit displays: set to 0. */
extern uint8_t ddig_x;

/* Scanner minimap pixel toggle */
void pos_chopper(void);
void pos_robot(void);

/* Joystick handling */
void read_stick(void);
void hover(void);

/* Fire button */
void read_trig(void);

/* Scrolling display-list map fill */
void draw_map(void);
void compute_map_adr_i(void); /* (temp1_i, temp2_i) → adr1_i lo/hi */
void compute_map_adr(void);   /* (temp1,   temp2)   → adr1    lo/hi */

/* Laser animation */
void do_laser_1(void);
void do_laser_2(void);

/* Block / elevator animation */
void do_blocks(void);
void do_elevator(void);

/* Explosion / missile colour flash */
void do_exp(void);

/* Score / bonus / fuel panel display */
void do_numbers(void);
void ddig(uint8_t val); /* render one BCD byte (2 digits) */

/* BCD score addition */
void inc_score(uint8_t hi, uint8_t lo);
