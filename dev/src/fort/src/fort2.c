/* fort2.c — options, pods, cruise missiles, tanks, screen/print utilities
 * Converted from fort2.s (Atari 6502 SynAssembler)
 */

#include "fort2.h"
#include "fort1.h"
#include <stdint.h>
#include <string.h>


/* -----------------------------------------------------------------------
 * FORT1.H items used in this file:
 *   Variables: adr1_lo, adr1_hi, temp1, temp2, temp3, temp4,
 *              mode, consol_flag, tim5_val, tim6_val, tim7_val, temp_mode,
 *              opt_num, grav_skill, pilot_skill, chops,
 *              chop_x, chop_y, chopper_x, chopper_y, chopper_angle,
 * chopper_col, chopper_status, robot_status, r_status, sx, sy, sx_f, sy_f,
 *              bak2_color, s1_1_val, s1_2_val, s2_val, s3_val, s4_val, s5_val,
 * s6_val, pod_num, pod_com, pod_status[], pod_x[], pod_y[], pod_dx[],
 *              pod_temp1[], pod_temp2[], tank_status[], tank_x[], tank_y[],
 * tank_dx[], tank_temp[], tank_start_x[], tank_start_y[], tank_spd, tank_speed,
 *              cm_status[], cm_x[], cm_y[], cm_time[], cm_temp[],
 *              missile_spd, missile_speed, rocket_status[], rocket_x[],
 * rocket_temp[], rocket_tempx[], rocket_tempy[], bonus1, bonus2, chop_left,
 * demo_status Functions: compute_map_adr(), pos_it(), inc_score(), main_loop(),
 *              dsp_lst1[], dsp_lst2[], hit_list[], clear_sounds()
 * ----------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 * FORT.H items also declared locally in this file:
 *   Variables:  adr1_lo, adr1_hi, adr2_lo, adr2_hi,
 *               temp1, temp2, temp3, temp4, temp_mode,
 *               tim5_val, tim6_val, tim7_val,
 *               tank_start_x[], tank_start_y[],
 *               s1_1_val, s1_2_val, s2_val, s3_val, s4_val, s5_val, s6_val,
 *               pod_status[], pod_x[], pod_y[], pod_dx[], pod_temp1[],
 * pod_temp2[], demo_status Constants:  PLAY_SCRN, CHR_SET1, CHR_SET2, CHR_EXP,
 * CHR_EXP_WALL, CHR_MISS_LEFT, CHR_MISS_RIGHT HW aliases: REG();
 * FRAME_HW=FRAME, CONSOL_HW=CONSOL, TRIG0_HW=TRIG0, KBCODE_HW=KBCODE,
 * SKSTAT_HW=SKSTAT, RANDOM_HW=RANDOM, AUDC1_HW..AUDC4_HW=AUDC1..AUDC4,
 * SDLST_LO/HI=SDLST_L/H
 * ----------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 * Hardware register access (same addresses as fort1.c — redeclared here
 * for the separate compilation unit)
 * ----------------------------------------------------------------------- */
#define REG(a) (*(volatile uint8_t*)(uintptr_t)(a))
#define CONSOL_HW REG(0xD01F) /* console buttons (active low) */
#define TRIG0_HW REG(0xD010)  /* joystick trigger (0=pressed) */
#define KBCODE_HW REG(0xD209) /* keyboard scan code */
#define SKSTAT_HW REG(0xD20F) /* keyboard status: bit2=1 means no key */
#define RANDOM_HW REG(0xD20A)
#define AUDC1_HW REG(0xD201)
#define AUDC2_HW REG(0xD203)
#define AUDC3_HW REG(0xD205)
#define AUDC4_HW REG(0xD207)
#define FRAME_HW REG(0x0014)

/* SDLST OS shadow (display list pointer lo/hi) */
#define SDLST_LO (*(volatile uint8_t*)0x0230u)
#define SDLST_HI (*(volatile uint8_t*)0x0231u)

static inline void set_sdlst(const uint8_t* lst)
{
    SDLST_LO = (uint8_t)((uintptr_t)lst & 0xFF);
    SDLST_HI = (uint8_t)((uintptr_t)lst >> 8);
}

/* -----------------------------------------------------------------------
 * Memory addresses
 * ----------------------------------------------------------------------- */
#define PLAY_SCRN ((uint8_t*)0x0300u)
#define CHR_SET1 ((uint8_t*)0x0800u)
#define CHR_SET2 ((uint8_t*)0x0C00u)

/* -----------------------------------------------------------------------
 * Map character constants
 * ----------------------------------------------------------------------- */
#define CHR_EXP 0x20u      /* EXP = $20 */
#define CHR_EXP_WALL 0xC7u /* EXP.WALL = $47+128 */
#define CHR_MISS_LEFT 0x71u
#define CHR_MISS_RIGHT 0x72u
#define CHR_POD_TILE 0x40u

/* Cruise missile direction status (also happens to equal STATUS_CRASH / 4 and
 * 8) */
#define CM_LEFT 4u
#define CM_RIGHT 8u

/* -----------------------------------------------------------------------
 * External variables (declared in fort.s / fort7.s)
 * ----------------------------------------------------------------------- */
extern uint8_t adr1_lo, adr1_hi;
extern uint8_t adr2_lo, adr2_hi;
extern uint8_t temp1, temp2, temp3, temp4;

extern uint8_t mode;
extern uint8_t consol_flag; /* CONSOL.FLAG */
extern uint8_t tim5_val;    /* TANK EXPLODE timer */
extern uint8_t tim6_val;    /* DEMO timer */
extern uint8_t tim7_val;    /* ROBO EXPLODE timer */
extern uint8_t temp_mode;   /* TEMP.MODE — scratch used by wait_frame */

extern uint8_t opt_num; /* which option row is selected (0-2) */
extern uint8_t grav_skill;
extern uint8_t pilot_skill;
extern uint8_t chops;

extern uint8_t chop_x, chop_y; /* CHOP.X, CHOP.Y */
extern uint8_t chopper_x, chopper_y;
extern uint8_t chopper_angle, chopper_col;
extern uint8_t chopper_status;
extern uint8_t robot_status;
extern uint8_t r_status;
extern uint8_t sx, sy, sx_f, sy_f;

extern uint8_t bak2_color; /* BAK2.COLOR (applied to HW by VBlank handler) */

/* Sound parameter registers (applied to HW by VBlank handler) */
extern uint8_t s1_1_val, s1_2_val;
extern uint8_t s2_val, s3_val, s4_val, s5_val, s6_val;

/* Pod arrays — MAX_PODS = 39 */
extern uint8_t pod_num;
extern uint8_t pod_com;
extern uint8_t pod_status[MAX_PODS];
extern uint8_t pod_x[MAX_PODS];
extern uint8_t pod_y[MAX_PODS];
extern uint8_t pod_dx[MAX_PODS];
extern uint8_t pod_temp1[MAX_PODS];
extern uint8_t pod_temp2[MAX_PODS];

/* Tank arrays — MAX_TANKS = 6 */
extern uint8_t tank_status[MAX_TANKS];
extern uint8_t tank_x[MAX_TANKS];
extern uint8_t tank_y[MAX_TANKS];
extern uint8_t tank_dx[MAX_TANKS];
extern uint8_t tank_temp[18];           /* TANK.TEMP: MAX_TANKS*3 = 18 bytes */
extern uint8_t tank_start_x[MAX_TANKS]; /* TANK.START.X — set per-level */
extern uint8_t tank_start_y[MAX_TANKS];
extern uint8_t tank_spd;
extern uint8_t tank_speed;

/* Cruise missile arrays — one per tank slot */
extern uint8_t cm_status[MAX_TANKS];
extern uint8_t cm_x[MAX_TANKS];
extern uint8_t cm_y[MAX_TANKS];
extern uint8_t cm_time[MAX_TANKS];
extern uint8_t cm_temp[MAX_TANKS];
extern uint8_t missile_spd;
extern uint8_t missile_speed;

/* Rocket arrays (used by screen_off) */
extern uint8_t rocket_status[3];
extern uint8_t rocket_x[3];
extern uint8_t rocket_temp[3];
extern uint8_t rocket_tempx[3];
extern uint8_t rocket_tempy[3];

/* Score */
extern uint8_t bonus1, bonus2;
extern uint8_t chop_left;

/* demo_status */
extern uint8_t demo_status;

/* -----------------------------------------------------------------------
 * External functions from other modules
 * ----------------------------------------------------------------------- */
extern void compute_map_adr(void); /* temp1=x, temp2=y → adr1 */
extern void pos_it(void);          /* position sprite: temp1=x, temp2=y */
extern void inc_score(uint8_t hi, uint8_t lo); /* INC.SCORE: hi=X, lo=Y */
extern void main_loop(void);                   /* MAIN — never returns */
extern void check_chr(void); /* CHECK.CHR (also defined here below) */

/* External display lists (defined in fort6.s) */
extern const uint8_t dsp_lst1[]; /* main game display list */
extern const uint8_t dsp_lst2[]; /* blank/off display list */

/* HIT.LIST and TANK.SHAPE (defined in fort3.s) */
extern const uint8_t hit_list[];   /* HIT.LIST: 22 bytes total */
#define HIT_LIST2_LEN 21u          /* HIT.LIST2.LEN = *-HIT.LIST-1 = 21 */
#define TANK_SHAPE (hit_list + 12) /* TANK.SHAPE label = HIT.LIST+12 */

/* -----------------------------------------------------------------------
 * Static: option screen strings (.AT /.../ = raw ASCII, terminated 0xFF)
 * ----------------------------------------------------------------------- */
static const uint8_t optt1[] = {'O', 'P', 'T', 'I', 'O', 'N', 'S', 0xFF};
static const uint8_t optt2[] = {'O', 'P', 'T', 'I', 'O', 'N', 0xFF};
static const uint8_t optt3[] = {'S', 'E', 'L', 'E', 'C', 'T', 0xFF};
static const uint8_t opt1[] = {'G', 'R', 'A', 'V', 'I', 'T', 'Y',
                               ' ', 'S', 'K', 'I', 'L', 'L', 0xFF};
static const uint8_t opt2[] = {'P', 'I', 'L', 'O', 'T', ' ',
                               'S', 'K', 'I', 'L', 'L', 0xFF};
static const uint8_t opt3[] = {'R', 'O', 'B', 'O', ' ', 'P',
                               'I', 'L', 'O', 'T', 'S', 0xFF};
/* skill level strings for gravity */
static const uint8_t opt1_1[] = {'W', 'E', 'A', 'K', ' ', ' ', ' ', ' ', 0xFF};
static const uint8_t opt1_2[] = {'N', 'O', 'R', 'M', 'A', 'L', 0xFF};
static const uint8_t opt1_3[] = {'S', 'T', 'R', 'O', 'N', 'G', 0xFF};
/* skill level strings for pilot */
static const uint8_t opt2_1[] = {'N', 'O', 'V', 'I', 'C', 'E', 0xFF};
static const uint8_t opt2_2[] = {'P', 'R', 'O', ' ', ' ', ' ', ' ', ' ', 0xFF};
static const uint8_t opt2_3[] = {'E', 'X', 'P', 'E', 'R', 'T', 0xFF};
/* robo pilots (chops) strings */
static const uint8_t opt3_1[] = {'S', 'E', 'V', 'E', 'N', ' ', ' ', ' ', 0xFF};
static const uint8_t opt3_2[] = {'N', 'I', 'N', 'E', ' ', ' ', ' ', ' ', 0xFF};
static const uint8_t opt3_3[] = {'E', 'L', 'E', 'V', 'E', 'N', 0xFF};

/* OPT.1, OPT.2, OPT.3: indexed by skill (0,1,2) */
static const uint8_t* const opt_1[3] = {opt1_1, opt1_2, opt1_3};
static const uint8_t* const opt_2[3] = {opt2_1, opt2_2, opt2_3};
static const uint8_t* const opt_3[3] = {opt3_1, opt3_2, opt3_3};

/* OPT.TAB: indexed by opt_num to get the category label string */
static const uint8_t* const opt_tab[3] = {opt1, opt2, opt3};

/* -----------------------------------------------------------------------
 * POD.CHR: 8-byte animation frame table, read as pairs at (POD.COM >> 3)
 * .HS 4000 5B5C 5D5E 005F
 * ----------------------------------------------------------------------- */
static const uint8_t pod_chr[8] = {0x40, 0x00, 0x5B, 0x5C,
                                   0x5D, 0x5E, 0x00, 0x5F};

/* -----------------------------------------------------------------------
 * Hyperspace exit tables (H.XF, H.YF, H.X, H.Y, H.CX, H.CY — 4 entries)
 * ----------------------------------------------------------------------- */
static const uint8_t h_xf[4] = {0xDD, 0x76, 0x10, 0x4B};
static const uint8_t h_yf[4] = {0x7A, 0x7B, 0xB8, 0xB8};
static const uint8_t h_x[4] = {0x22, 0xBC, 0x55, 0x87};
static const uint8_t h_y[4] = {0x0F, 0x0F, 0x18, 0x18};
static const uint8_t h_cx[4] = {0x73, 0x78, 0x76, 0x75};
static const uint8_t h_cy[4] = {0x8C, 0x89, 0xAF, 0xAF};

/* -----------------------------------------------------------------------
 * Helper: print string at (col, row)
 * print() C calling convention: adr1=string, temp1=col, temp2=row
 * ----------------------------------------------------------------------- */
static inline void print_str(uint8_t col, uint8_t row, const uint8_t* str)
{
    temp1 = col;
    temp2 = row;
    adr1_lo = (uint8_t)((uintptr_t)str & 0xFF);
    adr1_hi = (uint8_t)((uintptr_t)str >> 8);
    print();
}

/* -----------------------------------------------------------------------
 * Helper: reconstruct uint8_t* from adr1_lo/hi
 * ----------------------------------------------------------------------- */
static inline uint8_t* adr1_ptr(void)
{
    return (uint8_t*)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
}

/* -----------------------------------------------------------------------
 * CCL — compute screen address from temp1(col) / temp2(row) into adr1
 * Mirrors assembly: saves/restores temp1 and temp2 around MULT.BY.40
 * ----------------------------------------------------------------------- */
void ccl(void)
{
    uint16_t a = (uint16_t)(uintptr_t)PLAY_SCRN + (uint16_t)temp2 * 40u + temp1;
    adr1_lo = (uint8_t)(a & 0xFF);
    adr1_hi = (uint8_t)(a >> 8);
}

/* -----------------------------------------------------------------------
 * PRINT — print string to screen
 * C calling convention (matches fort1.c's print_str wrapper):
 *   adr1 = string pointer (0xFF-terminated)
 *   temp1 = col, temp2 = row
 *
 * Each non-space character writes two screen bytes: b and b+32 (Atari
 * mode-4 tile pairs; the right half of a tile is char+32).
 * A 0x00 byte (space) writes only one byte (0x00).
 * ----------------------------------------------------------------------- */
void print(void)
{
    const uint8_t* src =
        (const uint8_t*)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
    uint16_t scr_off = (uint16_t)temp2 * 40u + temp1;
    uint8_t* dst = PLAY_SCRN + scr_off;

    uint8_t si = 0, di = 0;
    for (;;)
    {
        uint8_t b = src[si++];
        if (b == 0xFF)
        {
            break;
        }
        if (b != 0x00)
        {
            dst[di++] = b;
            b = (uint8_t)(b + 32u);
        }
        dst[di++] = b;
    }
}

/* -----------------------------------------------------------------------
 * PRINT.OPTS — static helper that renders the full options screen
 * Called only from check_options()
 * ----------------------------------------------------------------------- */
static void print_opts(void)
{
    /* Category labels at col=0, rows 7, 9, 11 */
    print_str(0, 7, opt1);
    print_str(0, 9, opt2);
    print_str(0, 11, opt3);

    /* Highlight selected category (OPT.NUM) in reverse-video at its row.
     * The assembly prints the same string with ORA #$80 on each byte.
     * row = 7 + opt_num*2; col = 0
     */
    {
        uint8_t row = (uint8_t)(7u + (uint8_t)(opt_num * 2u));
        temp1 = 0;
        temp2 = row;
        ccl(); /* adr1 = PLAY_SCRN + row*40 + 0 */
        uint8_t* dst = adr1_ptr();
        const uint8_t* src = opt_tab[opt_num];
        uint8_t di = 0;
        for (uint8_t si = 0;; si++)
        {
            uint8_t b = src[si];
            if (b == 0xFF)
            {
                break;
            }
            if (b != 0x00)
            {
                dst[di++] = (uint8_t)(b | 0x80u); /* reverse video */
                b = (uint8_t)((b | 0x80u) + 32u);
            }
            dst[di++] = b;
        }
    }

    /* Current skill values at col=28, rows 7, 9, 11 */
    print_str(28, 7, opt_1[grav_skill]);
    print_str(28, 9, opt_2[pilot_skill]);
    print_str(28, 11, opt_3[chops]);
}

/* -----------------------------------------------------------------------
 * CHECK.OPTIONS — process OPTION/SELECT button presses on the option screen
 * Reads CONSOL directly; called both from read_user() and from within
 * option-screen processing.
 * ----------------------------------------------------------------------- */
void check_options(void)
{
    uint8_t con = CONSOL_HW;

    if (con == 3)
    { /* OPTION button: advance opt_num */
        uint8_t n = (uint8_t)(opt_num + 1u);
        if (n >= 3)
        {
            n = 0;
        }
        opt_num = n;
    }

    if (con == 5)
    { /* SELECT button: increment current setting */
        switch (opt_num)
        {
        case 0: /* gravity skill */
        {
            uint8_t v = (uint8_t)(grav_skill + 1u);
            if (v >= 3)
            {
                v = 0;
            }
            grav_skill = v;
        }
        break;
        case 1: /* pilot skill */
        {
            uint8_t v = (uint8_t)(pilot_skill + 1u);
            if (v >= 3)
            {
                v = 0;
            }
            pilot_skill = v;
        }
        break;
        case 2: /* chops (robo pilots) */
        {
            uint8_t v = (uint8_t)(chops + 1u);
            if (v >= 3)
            {
                v = 0;
            }
            chops = v;
        }
        break;
        }
    }

    /* Print header + option labels + current values */
    print_str(13, 1, optt1); /* "OPTIONS" */
    print_str(0, 3, optt2);  /* "OPTION" */
    print_str(28, 3, optt3); /* "SELECT" */
    print_opts();
}

/* -----------------------------------------------------------------------
 * READ.USER — edge-detect CONSOL button changes; handle keyboard pause
 * ----------------------------------------------------------------------- */
void read_user(void)
{
    uint8_t con = CONSOL_HW;
    if (con == consol_flag)
    {
        goto keyboard_check; /* no change */
    }
    consol_flag = con;
    tim6_val = 0;

    if (con == 6)
    { /* START button pressed */
        mode = START_MODE;
        demo_status = 1;
        return;
    }

    if (mode == OPTION_MODE)
    {
        check_options();
        return;
    }

    if (con == 3 || con == 5)
    { /* OPTION or SELECT button */
        mode = OPTION_MODE;
        demo_status = 1;
        screen_off();
        opt_num = 0;
        check_options();
        return;
    }

keyboard_check:
    if (SKSTAT_HW & 0x04)
    {
        return; /* bit2=1: no key available */
    }
    if (KBCODE_HW != 0x21)
    {
        return; /* not SPACE ($21) */
    }

    /* Space pressed: enter pause */
    uint8_t saved_mode = mode;
    mode = PAUSE_MODE;
    clear_sounds();

    /* Wait for space key to physically release */
    while (!(SKSTAT_HW & 0x04))
    {
    }

    /* Wait for space, trigger, or console button to unpause */
    for (;;)
    {
        if (!(SKSTAT_HW & 0x04))
        { /* key available */
            if (KBCODE_HW == 0x21)
            {
                break; /* space: unpause */
            }
        }
        else
        {
            if (CONSOL_HW != 7)
            {
                break; /* any console button */
            }
            if (!TRIG0_HW)
            {
                break; /* trigger (0=pressed) */
            }
            continue;
        }
    }

    /* Wait for key release before resuming */
    while (!(SKSTAT_HW & 0x04))
    {
    }
    mode = saved_mode;
}

/* =======================================================================
 * PODS
 * ======================================================================= */

static inline uint8_t* pod_map_ptr(uint8_t xi)
{
    temp1 = pod_x[xi];
    temp2 = pod_y[xi];
    compute_map_adr();
    return adr1_ptr();
}

/* P.COL — check if the two map bytes at the pod's position are lethal.
 * Returns non-zero (carry set equivalent) if collision.
 * Also sets pod to OFF and awards 80 points ($50:$00) if hit.
 */
static int p_col(uint8_t xi)
{
    uint8_t* mp = pod_map_ptr(xi);
    uint8_t b0 = mp[0], b1 = mp[1];
    if (b0 == CHR_MISS_LEFT || b0 == CHR_MISS_RIGHT || b0 == CHR_EXP ||
        b1 == CHR_MISS_LEFT || b1 == CHR_MISS_RIGHT || b1 == CHR_EXP)
    {
        /* Erase pod, mark as OFF, award score */
        mp[0] = pod_temp1[xi];
        mp[1] = pod_temp2[xi];
        pod_status[xi] = STATUS_OFF;
        inc_score(0x50, 0x00);
        return 1;
    }
    return 0;
}

/* P.ERASE — restore saved map bytes at the pod's position */
static void p_erase(uint8_t xi)
{
    uint8_t* mp = pod_map_ptr(xi);
    mp[0] = pod_temp1[xi];
    mp[1] = pod_temp2[xi];
}

/* P.DRAW — save current map bytes, then draw pod tile pair */
static void p_draw(uint8_t xi)
{
    uint8_t* mp = pod_map_ptr(xi);
    pod_temp1[xi] = mp[0];
    pod_temp2[xi] = mp[1];
    uint8_t frame = pod_com >> 3; /* 0,2,4,6 — selects animation pair */
    mp[0] = pod_chr[frame];
    mp[1] = pod_chr[frame + 1u];
}

/* P.MOVE — advance pod animation counter and X position */
static void p_move(uint8_t xi)
{
    for (;;)
    {
        if ((int8_t)pod_dx[xi] < 0)
        {
            /* moving left */
            uint8_t c = (uint8_t)((pod_com - 0x10u) & 0x3Fu);
            pod_com = c;
            if ((c & 0xF0u) == 0x30u)
            {
                pod_x[xi]--;
            }
        }
        else
        {
            /* moving right */
            uint8_t c = (uint8_t)((pod_com + 0x10u) & 0x3Fu);
            pod_com = c;
            if ((c & 0xF0u) == 0x00u)
            {
                pod_x[xi]++;
            }
        }

        /* Check new position for obstacles and bounds */
        temp1 = pod_x[xi];
        temp2 = pod_y[xi];
        compute_map_adr();
        uint8_t* mp = adr1_ptr();
        if (mp[0] == 0 && mp[1] == 0)
        {
            uint8_t px = pod_x[xi];
            if (px >= 50 && px < (256u - 50u))
            {
                /* valid: accept new position */
                pod_status[xi] = pod_com;
                return;
            }
        }
        /* Collision or out of bounds: flip direction and retry */
        pod_dx[xi] = (uint8_t)(pod_dx[xi] ^ 0xFEu);
    }
}

/* P.BEGIN — place a newly spawned pod at a random empty location */
static void p_begin(uint8_t xi)
{
    for (;;)
    {
        uint8_t rx;
        do
        {
            rx = RANDOM_HW;
        } while (rx < 50 || rx >= (256u - 50u));
        pod_x[xi] = rx;
        uint8_t ry;
        do
        {
            ry = RANDOM_HW;
        } while (ry >= 40);
        pod_y[xi] = ry;

        uint8_t* mp = pod_map_ptr(xi);
        if (mp[0] == 0 && mp[1] == 0)
        {
            break; /* empty: use this position */
        }
    }
    pod_status[xi] = STATUS_ON;
    pod_com = STATUS_ON;
    p_draw(xi);
    pod_dx[xi] = 0x01;
}

/* MP1 — process one pod for one animation step.
 * Uses global pod_num as index; advances pod_num at end.
 */
static void mp1(void)
{
    uint8_t xi = pod_num;
    uint8_t st = pod_status[xi] & 0x0Fu;

    if (st != STATUS_OFF)
    {
        if (st == STATUS_BEGIN)
        {
            p_begin(xi);
        }
        else
        {
            if (!p_col(xi))
            {
                p_erase(xi);
                p_move(xi);
                p_draw(xi);
            }
        }
    }

    /* Advance pod_num with wraparound at MAX_PODS */
    uint8_t next = (uint8_t)(xi + 1u);
    if (next >= MAX_PODS)
    {
        next = 0;
    }
    pod_num = next;
}

/* MOVE.PODS — called once per game frame; runs POD_SPEED iterations */
void move_pods(void)
{
    for (uint8_t i = 15; i != 0; i--)
    {
        mp1();
    }
}

/* =======================================================================
 * CRUISE MISSILES
 * ======================================================================= */

static inline uint8_t* cm_map_ptr(uint8_t xi)
{
    temp1 = cm_x[xi];
    temp2 = cm_y[xi];
    compute_map_adr();
    return adr1_ptr();
}

/* M.BEGIN — launch a new cruise missile from tank xi toward the chopper */
static void m_begin(uint8_t xi)
{
    cm_x[xi] = (uint8_t)(tank_x[xi] + 1u);
    cm_y[xi] = (uint8_t)(tank_y[xi] - 2u);
    /* Determine direction: if chopper is to the right, go RIGHT */
    uint8_t dir = CM_RIGHT;
    if ((int8_t)(chop_x - tank_x[xi]) < 0)
    {
        dir = CM_LEFT;
    }
    cm_status[xi] = dir;
    cm_temp[xi] = 0;
    cm_time[xi] = 20;
    s6_val = 1;
}

/* M.COL2 — missile hit something with pixels (carry equivalent: collision) */
static int m_col2(uint8_t xi)
{
    /* Erase missile, mark OFF, award 16 ($10) points, signal hit sound */
    uint8_t* mp = cm_map_ptr(xi);
    uint8_t saved = cm_temp[xi];
    /* Only restore if the saved byte is a wall char ($C7), normal tile
     * (< $5B, != $40), or non-special (>= $60, < $E0)                  */
    if (!(saved >= 0xE0u || saved == CHR_POD_TILE ||
          (saved >= 0x5Bu && saved < 0x60u)))
    {
        mp[0] = saved;
    }
    s3_val = 1;
    cm_status[xi] = STATUS_OFF;
    cm_time[xi] = (uint8_t)-1;
    inc_score(0x10, 0x00);
    return 1; /* carry set */
}

/* M.COL — check if missile position contains an explosion tile */
static int m_col(uint8_t xi)
{
    uint8_t* mp = cm_map_ptr(xi);
    if (mp[0] == CHR_EXP)
    {
        return m_col2(xi);
    }
    return 0;
}

/* M.ERASE — restore map byte at missile position (with type-filtering) */
static void m_erase(uint8_t xi)
{
    uint8_t* mp = cm_map_ptr(xi);
    uint8_t b = cm_temp[xi];
    /* Restore only "ordinary" map tiles — not pod/tank/reverse-video chars */
    if (b == CHR_EXP_WALL || (b < 0x5Bu && b != CHR_POD_TILE) ||
        (b >= 0x60u && b < 0xE0u))
    {
        mp[0] = b;
    }
}

/* M.MOVE — advance missile position one step, homing toward chopper */
static void m_move(uint8_t xi)
{
    /* Horizontal movement */
    if (cm_status[xi] == CM_LEFT)
    {
        cm_x[xi]--;
    }
    else
    {
        cm_x[xi]++;
    }

    /* Vertical homing logic */
    if ((int8_t)cm_time[xi] < 0)
    {
        /* Time expired: fall straight down */
        cm_y[xi]++;
        goto check_pos;
    }

    {
        /* Compute horizontal error to chopper */
        int8_t dx = (int8_t)(chop_x - cm_x[xi]);
        /* If missile heading left and chopper is to left (dx<0): go down */
        /* If missile heading right and chopper is to right (dx>0): go down */
        int going_down;
        if (cm_status[xi] == CM_LEFT)
        {
            going_down = (dx < 0);
        }
        else
        {
            going_down = (dx >= 0);
        }

        if (!going_down)
        {
            /* Check X bounds before tracking vertically */
            if (cm_x[xi] >= 0xD8u || cm_x[xi] < 0x2Du)
            {
                cm_y[xi]++;
                goto check_pos;
            }
            /* Home vertically: move toward chopper */
            int8_t dy = (int8_t)((uint8_t)(chop_y + 1u) - cm_y[xi]);
            if (dy == 0)
            {
                goto check_pos;
            }
            if (dy < 0)
            {
                cm_y[xi]--;
            }
            else
            {
                cm_y[xi]++;
            }
        }
        else
        {
            cm_y[xi]++;
        }
    }

check_pos: {
    /* Check if new position is blocked by another missile */
    uint8_t* mp = cm_map_ptr(xi);
    if (mp[0] == CHR_MISS_LEFT || mp[0] == CHR_MISS_RIGHT)
    {
        cm_y[xi]++; /* can't occupy — go one lower */
    }
}

    /* Decrement time counter (don't underflow past -1) */
    if ((int8_t)cm_time[xi] >= 0)
    {
        cm_time[xi]--;
    }
}

/* check_chr_byte — test if glyph has any non-zero rows in CHR.SET2 */
static int check_chr_byte(uint8_t chr_byte)
{
    /* Strip high bit, multiply by 8 to get offset into CHR.SET2 */
    uint16_t off = (uint16_t)((chr_byte & 0x7Fu) * 8u);
    const uint8_t* glyph = CHR_SET2 + off;
    for (int y = 7; y >= 0; y--)
    {
        if (glyph[y])
        {
            return 1;
        }
    }
    return 0;
}

/* Revised m_draw using check_chr_byte inline */
static int m_draw_and_check(uint8_t xi)
{
    uint8_t* mp = cm_map_ptr(xi);
    uint8_t saved = mp[0];
    cm_temp[xi] = saved;
    mp[0] = (cm_status[xi] == CM_LEFT) ? CHR_MISS_LEFT : CHR_MISS_RIGHT;
    if (check_chr_byte(saved))
    {
        return m_col2(xi);
    }
    return 0;
}

/* MM1 — process all missiles for one step, then check for new launches */
static void mm1(void)
{
    /* Process each active missile */
    for (int xi = MAX_TANKS - 1; xi >= 0; xi--)
    {
        uint8_t st = cm_status[xi];
        if (st == STATUS_OFF)
        {
            /* skip: no missile */
        }
        else if (st == STATUS_BEGIN)
        {
            m_begin((uint8_t)xi);
        }
        else
        {
            if (!m_col((uint8_t)xi))
            {
                m_erase((uint8_t)xi);
                m_move((uint8_t)xi);
                m_draw_and_check((uint8_t)xi);
            }
        }

        /* Check if this tank should fire a new missile at the chopper */
        if (tank_status[xi] == STATUS_ON)
        {
            int8_t dy = (int8_t)((int)tank_y[xi] - chop_y);
            if (dy >= 0 && dy < 14)
            {
                if (cm_status[xi] == STATUS_OFF)
                {
                    int8_t dx = (int8_t)(chop_x - tank_x[xi] - 2);
                    if (dx < 0)
                    {
                        dx = (int8_t)-dx;
                    }
                    if ((uint8_t)dx < 9u)
                    {
                        cm_status[xi] = STATUS_BEGIN;
                    }
                }
            }
        }
    }

    /* If no missiles are active, silence the missile sound */
    for (int xi = MAX_TANKS - 1; xi >= 0; xi--)
    {
        if (cm_status[xi] != STATUS_OFF)
        {
            return;
        }
    }
    AUDC4_HW = 0;
    s6_val = 0;
}

/* MOVE.CRUISE.MISSILES */
void move_cruise_missiles(void)
{
    missile_spd--;
    if (missile_spd != 0)
    {
        return;
    }
    missile_spd = missile_speed;
    mm1();
}

/* =======================================================================
 * HYPER CHAMBER
 * ======================================================================= */

void check_hyper_chamber(void)
{
    if (mode != HYPERSPACE_MODE)
    {
        return;
    }

    mode = STOP_MODE;
    bak2_color = 0x0F; /* flash white */
    wait_frame(2);
    bak2_color = 0;

    uint8_t idx = RANDOM_HW & 3u;
    sx_f = h_xf[idx];
    sy_f = h_yf[idx];
    sx = h_x[idx];
    sy = h_y[idx];
    chopper_x = h_cx[idx];
    chopper_y = h_cy[idx];
    chopper_angle = 8;
    chopper_col = 0;
    mode = GO_MODE;
}

/* =======================================================================
 * CHECK.CHR — public assembly-compatible version (stub)
 * Provided so fort3.s callers (CHECK.CHR.I etc.) can work when converted.
 * Internal code uses check_chr_byte() directly.
 * ----------------------------------------------------------------------- */
void check_chr(void) {}

/* =======================================================================
 * TANKS
 * ======================================================================= */

/* POS.TANK — position tank sprite at (tank_x, tank_y - 1) via pos_it */
static void pos_tank(uint8_t xi)
{
    temp1 = tank_x[xi];
    temp2 = (uint8_t)(tank_y[xi] - 1u);
    pos_it();
    temp2 = tank_y[xi]; /* restore TEMP2 = tank_y as per assembly */
}

/* CHECK.TANK.COL — scan HIT.LIST[HIT_LIST2_LEN..0] for the byte at (adr1+y).
 * Returns 1 (carry set) = collision, 0 = no collision.
 * On collision: flips tank_dx[ti].
 * ti = tank index (saved in temp1 by caller).
 */
static int check_tank_col(uint8_t ti, uint8_t y)
{
    uint8_t* mp = adr1_ptr();
    uint8_t b = mp[y];
    for (int xi2 = (int)HIT_LIST2_LEN; xi2 >= 0; xi2--)
    {
        if (b == hit_list[xi2])
        {
            tank_dx[ti] = (uint8_t)(tank_dx[ti] ^ 0xFEu);
            return 1;
        }
    }
    return 0;
}

void move_tanks(void)
{
    /* MT1: rate-limit tank movement */
    tank_spd--;
    if (tank_spd != 0)
    {
        goto mt2;
    }
    tank_spd = tank_speed;

    for (int xi = MAX_TANKS - 1; xi >= 0; xi--)
    {
        uint8_t st = tank_status[xi];
        if (st == STATUS_OFF)
        {
            goto next_tank;
        }

        if (st == STATUS_BEGIN)
        {
            /* Initialise tank at its start position */
            tank_status[xi] = STATUS_ON;
            tank_x[xi] = tank_start_x[xi];
            tank_y[xi] = tank_start_y[xi];
            tank_dx[xi] = ((int8_t)RANDOM_HW >= 0) ? 1 : (uint8_t)-1;
            pos_tank((uint8_t)xi);
            goto draw_tank;
        }

        if (st == STATUS_CRASH)
        {
            goto next_tank;
        }

        /* --- Restore old position ---------------------------------------- */
        temp1 = tank_x[xi];
        temp2 = tank_y[xi];
        compute_map_adr();
        {
            uint8_t* mp = adr1_ptr();
            uint8_t tt = (uint8_t)((uint8_t)xi * 3u);
            uint8_t collision = 0;
            for (uint8_t y = 0; y < 3u; y++)
            {
                uint8_t b = mp[y];
                if (b == CHR_EXP || b == CHR_MISS_LEFT || b == CHR_MISS_RIGHT)
                {
                    collision = 1;
                    break;
                }
                mp[y] = tank_temp[tt + y];
            }
            if (collision)
            {
                tank_status[xi] = STATUS_CRASH;
                mp[0] = CHR_EXP;
                mp[1] = CHR_EXP;
                mp[2] = CHR_EXP;
                tim5_val = 10;
                goto next_tank;
            }
            /* Clear row above, col+1 */
            mp[-256 + 1] = 0;
        }

        /* --- Move X ------------------------------------------------------- */
    draw_tank:
        pos_tank((uint8_t)xi);
        tank_x[xi] = (uint8_t)(tank_x[xi] + tank_dx[xi]);

        /* --- Save new position -------------------------------------------- */
        pos_tank((uint8_t)xi);
        temp1 = tank_x[xi];
        temp2 = tank_y[xi];
        compute_map_adr();
        {
            uint8_t* mp = adr1_ptr();
            uint8_t tt = (uint8_t)((uint8_t)xi * 3u);
            for (uint8_t y = 0; y < 3u; y++)
            {
                tank_temp[tt + y] = mp[y];
            }

            /* Check collision at new position (bytes 0 and 2) */
            if (check_tank_col((uint8_t)xi, 0))
            {
                goto draw_tank;
            }
            if (check_tank_col((uint8_t)xi, 2))
            {
                goto draw_tank;
            }

            /* Draw tank lower body: 3 chars from TANK.SHAPE */
            mp[2] = TANK_SHAPE[2];
            mp[1] = TANK_SHAPE[1];
            mp[0] = TANK_SHAPE[0];

            /* Draw direction indicator one row above, col+1 */
            uint8_t indicator =
                (uint8_t)((chop_x >= tank_x[xi]) ? 0xEFu : 0xF0u);
            mp[-256 + 1] = indicator;
        }

    next_tank:;
    }

mt2:
    /* MT2: handle exploding tank timers */
    tim5_val--;
    if (tim5_val != 0)
    {
        goto mt2_respawn;
    }
    tim5_val = 10;

    for (int xi = MAX_TANKS - 1; xi >= 0; xi--)
    {
        if (tank_status[xi] == STATUS_CRASH)
        {
            tank_status[xi] = STATUS_OFF;

            temp1 = tank_x[xi];
            temp2 = tank_y[xi];
            compute_map_adr();
            {
                uint8_t* mp = adr1_ptr();
                uint8_t tt = (uint8_t)((uint8_t)xi * 3u);
                for (uint8_t y = 0; y < 3u; y++)
                {
                    mp[y] = tank_temp[tt + y];
                }
                mp[-256 + 1] = 0;
            }
            inc_score(0x50, 0x02);
            pos_tank((uint8_t)xi);
        }
    }

mt2_respawn:
    /* Respawn: check if a crashed (OFF) tank can restart */
    for (int xi = MAX_TANKS - 1; xi >= 0; xi--)
    {
        if (tank_status[xi] != STATUS_OFF)
        {
            continue;
        }
        /* Chopper must be near tank start row */
        if (chop_y < 3)
        {
            continue;
        }
        if ((uint8_t)(chop_y - 3u) < tank_start_y[xi])
        {
            continue;
        }

        /* Check 13 consecutive map bytes near spawn point for obstacles */
        temp1 = (uint8_t)(tank_start_x[xi] - 5u);
        temp2 = tank_start_y[xi];
        compute_map_adr();
        {
            uint8_t* mp = adr1_ptr();
            uint8_t clear = 1;
            for (int y = 12; y >= 0; y--)
            {
                if (mp[y] & 0x80u)
                {
                    clear = 0;
                    break;
                }
            }
            if (clear)
            {
                tank_status[xi] = STATUS_BEGIN;
            }
        }
    }
}

/* =======================================================================
 * SCREEN ON / OFF
 * ======================================================================= */

void screen_on(void)
{
    screen_off();
    set_sdlst(dsp_lst1);
}

void screen_off(void)
{
    set_sdlst(dsp_lst2);
    chopper_status = STATUS_OFF;
    robot_status = STATUS_OFF;
    if (r_status == STATUS_CRASH)
    {
        r_status = STATUS_OFF;
    }

    /* Clear CHR.SET1[$2E0..$2FF] and CHR.SET1[$300..$3FF] */
    memset(CHR_SET1 + 0x2E0u, 0, 0x20u);
    memset(CHR_SET1 + 0x300u, 0, 0x100u);

    /* Clear all three pages of PLAY.SCRN */
    memset(PLAY_SCRN, 0, 0x100u);
    memset(PLAY_SCRN + 0x100u, 0, 0x100u);
    memset(PLAY_SCRN + 0x200u, 0, 0x100u);

    /* Zero sound shadow values */
    s1_1_val = 0;
    s2_val = 0;
    s3_val = 0;
    s4_val = 0;
    s5_val = 0;
    s6_val = 0;
    bak2_color = 0;
    s1_2_val = 20;

    tim7_val = MAX_TANKS - 1;

    /* Cancel any active cruise missiles */
    for (int xi = MAX_TANKS - 1; xi >= 0; xi--)
    {
        if (cm_status[xi] != STATUS_OFF)
        {
            cm_status[xi] = STATUS_OFF;
            m_erase((uint8_t)xi);
        }
    }

    /* Restore any exploding rockets' map positions */
    for (int ri = 2; ri >= 0; ri--)
    {
        if (rocket_status[ri] == 7)
        { /* STATUS == 7 = EXP for rockets */
            temp1 = rocket_tempx[ri];
            temp2 = rocket_tempy[ri];
            compute_map_adr();
            adr1_ptr()[0] = rocket_temp[ri];
        }
        rocket_status[ri] = 0;
        rocket_x[ri] = 0;
    }
}

/* =======================================================================
 * CLEAR.SOUNDS
 * ======================================================================= */

void clear_sounds(void)
{
    AUDC1_HW = 0;
    AUDC2_HW = 0;
    AUDC3_HW = 0;
    AUDC4_HW = 0;
}

/* =======================================================================
 * GIVE.BONUS — add bonus points and increment chop_left by 2 (BCD)
 * ----------------------------------------------------------------------- */
void give_bonus(void)
{
    inc_score(bonus1, bonus2);
    bonus1 = 0;
    bonus2 = 0;

    /* SED; CLC; ADC #2; CLD — BCD add 2 to chop_left */
    uint8_t lo = (uint8_t)(chop_left & 0x0Fu);
    uint8_t hi = (uint8_t)(chop_left & 0xF0u);
    lo = (uint8_t)(lo + 2u);
    if (lo >= 10u)
    {
        lo = (uint8_t)(lo - 10u);
        hi = (uint8_t)(hi + 0x10u);
    }
    chop_left = hi | lo;
}

/* =======================================================================
 * WAIT.FRAME — wait n frames, processing user input each frame.
 * If mode changes during the wait, reset and jump into main_loop().
 * ----------------------------------------------------------------------- */
void wait_frame(uint8_t n)
{
    uint8_t saved = mode;
    do
    {
        uint8_t f = FRAME_HW;
        while (f == FRAME_HW)
        {
        } /* spin until frame counter advances */
        read_user();
        if (mode != saved)
        {
            /* Assembly: LDX #$FF; TXS; JMP MAIN — abandon current call chain */
            main_loop(); /* never returns */
        }
    } while (--n != 0);
}

/* =======================================================================
 * CLEAR.INFO — clear the top info row of PLAY.SCRN (first 40 bytes)
 * ----------------------------------------------------------------------- */
void clear_info(void)
{
    memset(PLAY_SCRN, 0, 40u);
}
