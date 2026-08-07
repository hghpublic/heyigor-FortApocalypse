#pragma once
#include <stdint.h>

#include "fort.h"

/*
 * Also declared in fort.h:
 * - Variables: none
 * - Functions: none
 * - Constants: MAX_TANKS, MAX_PODS
 */


/* -----------------------------------------------------------------------
 * fort1.h — Game logic and setup (converted from fort1.s)
 * ----------------------------------------------------------------------- */

/* --- Mode constants (from fort7.s .EQ) --------------------------------- */
static constexpr int TITLE_MODE = 1;
static constexpr int GO_MODE = 2;
static constexpr int START_MODE = 3;
static constexpr int NEW_LEVEL_MODE = 4;
static constexpr int NEW_PLAYER_MODE = 5;
static constexpr int GAME_OVER_MODE = 6;
static constexpr int STOP_MODE = 7;
static constexpr int PAUSE_MODE = 8;
static constexpr int OPTION_MODE = 9;
static constexpr int HYPERSPACE_MODE = 10;

/* --- Entity status constants (from fort7.s .EQ) ------------------------ */
static constexpr int STATUS_OFF = 1;
static constexpr int STATUS_ON = 2;
static constexpr int STATUS_FLY = 3;
static constexpr int STATUS_CRASH = 4;
static constexpr int STATUS_EXPLODE = 5;
static constexpr int STATUS_LAND = 6;
static constexpr int STATUS_BEGIN = 7;
static constexpr int STATUS_FULL = 8;
static constexpr int STATUS_EMPTY = 9;
static constexpr int STATUS_REFUEL = 10;
static constexpr int STATUS_PICKUP = 11;

/* --- Functions defined in fort1.c -------------------------------------- */
void fort1_start(void);
void title(void);
void t1_loop(void);
void t2_vblank(void);
void t3_game_start(void);
void main_loop(void);

void check_level(void);
void do_level_1(void);
void move_ramp(void);
void do_level_2(void);
void do_level_3(void);

void unpack(void);

void check_modes(void);
void m_start(void);
void m_new_player(void);
void m_new_level(void);
void inc_game_points(uint8_t n);
void m_game_over(void);
