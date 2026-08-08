#pragma once
#include <stdint.h>


/* -----------------------------------------------------------------------
 * fort2.h — options, pods, missiles, tanks, screen, print utilities
 *            (converted from fort2.s)
 * ----------------------------------------------------------------------- */


void read_user(void);
void check_options(void);

void move_pods(void);
void move_cruise_missiles(void);
void check_hyper_chamber(void);
void move_tanks(void);

void screen_on(void);
void screen_off(void);
void clear_sounds(void);

void ccl(void);           /* compute ADR1 = PLAY_SCRN + temp2*40 + temp1 */
void print(void);         /* print string: adr1=str, temp1=col, temp2=row */
void give_bonus(void);
void wait_frame(uint8_t n);
void clear_info(void);
