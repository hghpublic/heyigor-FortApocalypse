/* fort.c — Global variable storage
 * Converted from fort.s (Fort Apocalypse master/equates file)
 *
 * fort.s allocates three groups of storage:
 *
 *  1. Zero-page scratch area (.OR $15):
 *       ADR1/ADR2 (16-bit pointer pairs), TEMP1-6, TEMP.MODE,
 *       ADR1.I/ADR2.I/TEMP1.I-4.I (interrupt-context copies),
 *       S.ADR/S.TEMP/S.FLG (scanner helpers),
 *       TANK.START.X/Y, TIM1-9.VAL, SSIZEM
 *
 *  2. Sound + score state (.OR $43):
 *       S1.1.VAL, S1.2.VAL, S2-S6.VAL, GAME.POINTS, DEMO.STATUS, DEMO.COUNT
 *
 *  3. Fixed-address entity arrays:
 *       POD.STATUS / POD.DX  at POD.1 = $0FF8 (CHR_SET2+920)
 *       POD.X .. POD.TEMP2   at POD.2 = $3925
 *       SLAVE.STATUS .. SLAVE.DX  at SLAVES = $3904
 *
 * Variables from fort7.s (.OR $50 onwards, fort7.c) are NOT repeated here.
 * All variables are zero-initialised by the C runtime (global scope).
 */

#include "fort.h"

/* -------------------------------------------------------------------------
 * Zero-page scratch registers (.OR $15)
 * ------------------------------------------------------------------------- */

uint8_t adr1_lo, adr1_hi;   /* ADR1 — main 16-bit scratch pointer */
uint8_t adr2_lo, adr2_hi;   /* ADR2 — secondary 16-bit scratch pointer */

uint8_t temp1;               /* TEMP1 */
uint8_t temp2;               /* TEMP2 */
uint8_t temp3;               /* TEMP3 */
uint8_t temp4;               /* TEMP4 */
uint8_t temp5;               /* TEMP5 */
uint8_t temp6;               /* TEMP6 */
uint8_t temp_mode;           /* TEMP.MODE */

/* Interrupt-context copies — kept separate so ISRs don't corrupt temp1-6 */
uint8_t adr1_i_lo, adr1_i_hi;          /* ADR1.I */
uint8_t adr2_i_lo, adr2_i_hi;          /* ADR2.I */
uint8_t temp1_i, temp2_i, temp3_i, temp4_i;

uint8_t s_adr_lo, s_adr_hi;  /* S.ADR — scanner display-list address */
uint8_t s_temp;               /* S.TEMP */
uint8_t s_flg;                /* S.FLG  — scanner direction flag */

uint8_t tank_start_x[MAX_TANKS];  /* TANK.START.X — initial X per tank */
uint8_t tank_start_y[MAX_TANKS];  /* TANK.START.Y — initial Y per tank */

/* Timer countdown values (decremented by game logic; 0 = inactive) */
uint8_t tim1_val;   /* TIM1.VAL — laser 1 shot cooldown */
uint8_t tim2_val;   /* TIM2.VAL — laser 2 shot cooldown */
uint8_t tim3_val;   /* TIM3.VAL — chopper explosion duration */
uint8_t tim4_val;   /* TIM4.VAL — refuel sequence step */
uint8_t tim5_val;   /* TIM5.VAL — tank explosion duration */
uint8_t tim6_val;   /* TIM6.VAL — demo / title timeout */
uint8_t tim7_val;   /* TIM7.VAL — robot explosion duration */
uint8_t tim8_val;   /* TIM8.VAL — robot missile flight */
uint8_t tim9_val;   /* TIM9.VAL — slave rescue message display */
uint8_t ssizem;     /* SSIZEM   — saved missile sprite size byte */

/* -------------------------------------------------------------------------
 * Sound + score state (.OR $43)
 * ------------------------------------------------------------------------- */

uint8_t s1_1_val;  /* S1.1.VAL — channel 1 frequency (part 1) */
uint8_t s1_2_val;  /* S1.2.VAL — channel 1 frequency (part 2) */
uint8_t s2_val;    /* S2.VAL   — channel 2 control */
uint8_t s3_val;    /* S3.VAL   — channel 3 control */
uint8_t s4_val;    /* S4.VAL   — channel 4 control */
uint8_t s5_val;    /* S5.VAL   — channel 5 control */
uint8_t s6_val;    /* S6.VAL   — missile sound channel */

uint8_t game_points;   /* GAME.POINTS — accumulated post-game rating score */
uint8_t demo_status;   /* DEMO.STATUS — 0=none, 1=human, $FF=demo countdown */
uint8_t demo_count;    /* DEMO.COUNT  — demo frame counter */

/* -------------------------------------------------------------------------
 * Pod entity arrays
 *
 * In the original ROM, POD.STATUS and POD.DX occupy POD.1 = $0FF8
 * (CHR_SET2 + 920) and the remaining pod fields occupy POD.2 = $3925.
 * In the C port these are plain global arrays; the Atari hardware mapping
 * is handled separately by the startup code that copies CHR_SET2 data.
 * ------------------------------------------------------------------------- */

uint8_t pod_status[MAX_PODS];  /* POD.STATUS — per-pod state machine */
uint8_t pod_dx[MAX_PODS];      /* POD.DX     — movement direction + anim */
uint8_t pod_x[MAX_PODS];       /* POD.X      — map column */
uint8_t pod_y[MAX_PODS];       /* POD.Y      — map row */
uint8_t pod_temp1[MAX_PODS];   /* POD.TEMP1  — scratch (tile save) */
uint8_t pod_temp2[MAX_PODS];   /* POD.TEMP2  — scratch */

/* -------------------------------------------------------------------------
 * Slave entity arrays
 *
 * In the original ROM, these occupy SLAVES = $3904 in interleaved groups of
 * 8 bytes: SLAVE.STATUS[8], SLAVE.X[8], SLAVE.Y[8], SLAVE.DX[8].
 * ------------------------------------------------------------------------- */

uint8_t slave_status[8];  /* SLAVE.STATUS — per-slave state machine */
uint8_t slave_x[8];       /* SLAVE.X      — map column */
uint8_t slave_y[8];       /* SLAVE.Y      — map row */
uint8_t slave_dx[8];      /* SLAVE.DX     — direction + animation frame */
