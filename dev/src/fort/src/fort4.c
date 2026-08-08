/* fort4.c — Main interrupt driver part II (converted from fort4.s)
 * MAIN INTERRUPT DRIVER PART II:
 *   position things, read stick/trig, laser/block/elevator/explosion
 *   animation, score display, BCD scoring.
 */

#include "fort4.h"
#include "fnt1.h" /* POS_MASK1, EXP_SHAPE */
#include "fort1.h"
#include <stdint.h>

/* From fnt1.h — Variables & Functions Used:
 *
 * Extern const arrays:
 *   POS_MASK1[] (used for scanner minimap pixel toggling)
 *   EXP_SHAPE[] (used for explosion animation)
 */

/* From fort1.h — Variables & Functions Used:
 *
 * Extern variables (zero-page pointers):
 *   adr1_lo, adr1_hi, adr1_i_lo, adr1_i_hi, temp1, temp2, temp1_i, temp2_i
 *
 * Extern variables (display/game state):
 *   s_adr_lo, s_adr_hi, s_flg, mode, score1, score2, score3, bonus1, bonus2,
 *   fuel1, fuel2, fuel_status, laser_status, laser_spd, tim1_val, tim2_val,
 *   chopper_status, chopper_x, chopper_y, chopper_angle, chop_x, chop_y,
 *   sx, sy, sx_f, sy_f, r_x, r_y, robot_status, rocket_status[], rocket_x[],
 *   rocket_y[], demo_status, demo_count, elevator_spd, elevator_dx,
 *   elevator_tim, elevator_num, s2_val, s1_2_val, trig_flag
 *
 * Extern const array:
 *   laser_shapes[] (32-byte laser animation data)
 *
 * Functions from fort1.h:
 *   compute_map_adr_i(), compute_map_adr(), ddig()
 */

/* -----------------------------------------------------------------------
 * FORT.H items also declared locally in this file:
 *   Variables:  adr1_lo, adr1_hi, adr1_i_lo, adr1_i_hi,
 *               temp1, temp2, temp1_i, temp2_i,
 *               s_adr_lo, s_adr_hi, s_flg,
 *               tim1_val, tim2_val, s1_2_val, s2_val,
 *               demo_status, demo_count
 *   Constants:  CHR_SET2, SCANNER_BASE, RAM1_STUFF,
 *               LASERS_1, LASERS_2, LASER_3, BLOCK_1..BLOCK_5,
 *               MISS_CHR_LEFT, MISS_CHR_RIGHT,
 *               MIN_LEFT, MIN_RIGHT, MIN_UP, MIN_DOWN, MAX_UP, MAX_DOWN
 *   Aliases:    RAM2_STUFF_BASE=RAM2_STUFF,
 *               EXPLOSION_PTR=EXPLOSION, EXPLOSION2_PTR=EXPLOSION2,
 *               JOY_RIGHT=DIR_RIGHT, JOY_LEFT=DIR_LEFT,
 *               JOY_DOWN=DIR_DOWN, JOY_UP=DIR_UP
 *   HW aliases: REG(); FRAME_HW=FRAME, RANDOM_HW=RANDOM, TRIG0_HW=TRIG0,
 *               HSCROL_HW=HSCROL, VSCROL_HW=VSCROL, STICK_HW=STICK
 * ----------------------------------------------------------------------- */


/* -----------------------------------------------------------------------
 * Hardware register access
 * ----------------------------------------------------------------------- */
#define REG(a) (*(volatile uint8_t*)(uintptr_t)(a))
#define FRAME_HW REG(0x0014)  /* OS frame counter */
#define RANDOM_HW REG(0xD20A) /* hardware LFSR */
#define TRIG0_HW REG(0xD010)  /* fire button (0 = pressed) */
#define HSCROL_HW REG(0xD404) /* horizontal fine scroll */
#define VSCROL_HW REG(0xD405) /* vertical fine scroll */
#define STICK_HW REG(0x0278)  /* OS joystick shadow */

/* -----------------------------------------------------------------------
 * Fixed memory addresses
 * ----------------------------------------------------------------------- */
#define CHR_SET2 ((uint8_t*)0x0C00u)
#define SCANNER_BASE ((uint8_t*)0x39C0u)
#define RAM1_STUFF ((uint8_t*)0x0C90u) /* CHR_SET2 + 144 */
#define RAM2_STUFF_BASE ((uint8_t*)0x0100u)

/* CHR_SET2 named areas */
#define LASERS_1 (CHR_SET2 + 8u)
#define LASERS_2 (CHR_SET2 + 40u)
#define LASER_3 (CHR_SET2 + 72u)
#define BLOCK_1 (CHR_SET2 + 80u)
#define BLOCK_2 (CHR_SET2 + 88u)
#define BLOCK_3 (CHR_SET2 + 96u)
#define BLOCK_4 (CHR_SET2 + 104u)
#define BLOCK_5 (CHR_SET2 + 112u)
#define EXPLOSION_PTR (CHR_SET2 + 256u)
#define EXPLOSION2_PTR (CHR_SET2 + 504u)
#define MISS_CHR_LEFT (CHR_SET2 + 904u)
#define MISS_CHR_RIGHT (CHR_SET2 + 912u)

/* Display list map entries: DSP.MAP = *-Z1+RAM1.STUFF, offset 20 */
#define DSP_MAP_PTR (RAM1_STUFF + 20u)

/* Panel digit areas in RAM2.STUFF (Z2 offsets): */
#define SCORE_DIG_PTR (RAM2_STUFF_BASE + 19u)
#define BONUS_DIG_PTR (RAM2_STUFF_BASE + 149u)
#define FUEL_DIG_PTR (RAM2_STUFF_BASE + 122u)

/* Scroll limits */
#define MIN_RIGHT 130u
#define MIN_LEFT 110u
#define MIN_DOWN 166u
#define MAX_DOWN 212u
#define MIN_UP 146u
#define MAX_UP 100u
#define MAP_LINES 17u

/* Joystick bit masks (active-low: 0 = pressed) */
#define JOY_RIGHT 0x08u
#define JOY_LEFT 0x04u
#define JOY_DOWN 0x02u
#define JOY_UP 0x01u

/* -----------------------------------------------------------------------
 * Extern variable declarations
 * ----------------------------------------------------------------------- */
/* Zero-page pointers */
extern uint8_t adr1_lo, adr1_hi;
extern uint8_t adr1_i_lo, adr1_i_hi;
extern uint8_t temp1, temp2;
extern uint8_t temp1_i, temp2_i;

/* Display write pointer + flags */
extern uint8_t s_adr_lo, s_adr_hi;
extern uint8_t s_flg;

/* Game state */
extern uint8_t mode;
extern uint8_t score1, score2, score3;
extern uint8_t bonus1, bonus2;
extern uint8_t fuel1, fuel2;
extern uint8_t fuel_status;
extern uint8_t laser_status;
extern uint8_t laser_spd;
extern uint8_t tim1_val, tim2_val;

/* Chopper sprite/map position */
extern uint8_t chopper_status;
extern uint8_t chopper_x, chopper_y;
extern uint8_t chopper_angle;
extern uint8_t chop_x, chop_y;

/* Camera scroll */
extern uint8_t sx, sy, sx_f, sy_f;

/* Robot tile position */
extern uint8_t r_x, r_y;
extern uint8_t robot_status;

/* Rockets */
extern uint8_t rocket_status[3];
extern uint8_t rocket_x[3];
extern uint8_t rocket_y[3];

/* Demo / scores */
extern uint8_t demo_status;
extern uint8_t demo_count;

/* Elevator */
extern uint8_t elevator_spd;
extern uint8_t elevator_dx;
extern uint8_t elevator_tim;
extern uint8_t elevator_num;

/* Sound values */
extern uint8_t s2_val;
extern uint8_t s1_2_val;

/* Trigger edge flag */
extern uint8_t trig_flag;

/* Laser shape data (defined in fort6.s / future fort6.c) */
extern const uint8_t laser_shapes[32];

/* -----------------------------------------------------------------------
 * Module globals
 * ----------------------------------------------------------------------- */
uint8_t ddig_x; /* digit position counter for DRAW (0 = ones/last digit) */

/* -----------------------------------------------------------------------
 * Static data
 * ----------------------------------------------------------------------- */
static const uint8_t demo_stick[108] = {
    0x0B, 0x0B, 0x0B, 0x0B, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x0A, 0x0A,
    0x0A, 0x0A, 0x0B, 0x09, 0x09, 0x0B, 0x0A, 0x0A, 0x0B, 0x09, 0x0B, 0x0A,
    0x0B, 0x09, 0x0B, 0x0A, 0x0A, 0x0A, 0x0A, 0x0B, 0x0B, 0x09, 0x09, 0x0B,
    0x0B, 0x0A, 0x09, 0x0D, 0x09, 0x0B, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
    0x0B, 0x09, 0x09, 0x09, 0x0A, 0x0E, 0x06, 0x07, 0x07, 0x07, 0x05, 0x05,
    0x05, 0x05, 0x07, 0x06, 0x06, 0x07, 0x05, 0x06, 0x06, 0x07, 0x05, 0x05,
    0x07, 0x06, 0x05, 0x07, 0x07, 0x07, 0x06, 0x06, 0x06, 0x06, 0x06, 0x07,
    0x07, 0x06, 0x05, 0x05, 0x07, 0x06, 0x06, 0x07, 0x05, 0x05, 0x07, 0x06,
    0x06, 0x06, 0x07, 0x05, 0x05, 0x07, 0x06, 0x06, 0x07, 0x06, 0x09, 0x09,
};

/* Elevator target block pointers (BLOCK.5-8 in CHR_SET2) */
static uint8_t* const elevators[4] = {
    (uint8_t*)0x0C70u, /* BLOCK.5 = CHR_SET2 + 112 */
    (uint8_t*)0x0C78u, /* BLOCK.6 = CHR_SET2 + 120 */
    (uint8_t*)0x0C80u, /* BLOCK.7 = CHR_SET2 + 128 */
    (uint8_t*)0x0C88u, /* BLOCK.8 = CHR_SET2 + 136 */
};

/* -----------------------------------------------------------------------
 * Static helpers
 * ----------------------------------------------------------------------- */

/* Toggle scanner minimap pixel for (temp1_i, temp2_i) tile */
static void pos_it_i(void)
{
    uint8_t x = temp1_i;
    uint8_t y = temp2_i;
    /* scanner row = y*40; column byte = x/8; base+3 offset */
    uint16_t offset = 3u + (uint16_t)(x >> 3) + (uint16_t)y * 40u;
    SCANNER_BASE[offset] ^= ((int8_t*)POS_MASK1)[x & 7u];
}

/* Render one nibble digit to (s_adr) and advance s_adr by 2; decrement ddig_x.
 * digit: 0-9 = real digit, 0xA = leading-zero blank.
 * DRAW maps: digit → [hi_byte, lo_byte] at s_adr[0..1].
 *   blank (0xA, non-last): 0x70+0x90=0x00, second = 0x00
 *   '0' digit: 0x90 first, 0x8A second
 *   digit n: 0x90+n first, (0x90+n)&0x8F second
 */
static void draw(uint8_t digit)
{
    if (digit == 0x0Au)
    {
        digit = (ddig_x == 0u) ? 0u : 0x70u; /* 0xF0+128 truncated = 0x70 */
    }
    uint8_t hi = (uint8_t)(digit + 0x90u); /* +0x10+128 = +0x90 */
    uint8_t* p = (uint8_t*)(uintptr_t)((uint16_t)s_adr_hi << 8 | s_adr_lo);
    p[0] = hi;
    uint8_t lo =
        (hi == 0x90u) ? (uint8_t)(0x0Au + 0x80u) : (uint8_t)(hi & 0x8Fu);
    p[1] = lo;
    uint16_t adr = (uint16_t)((uint16_t)s_adr_hi << 8 | s_adr_lo) + 2u;
    s_adr_lo = (uint8_t)(adr & 0xFFu);
    s_adr_hi = (uint8_t)(adr >> 8);
    ddig_x--;
}

/* BCD add: a + b + *carry → result, *carry updated */
static uint8_t bcd_add(uint8_t a, uint8_t b, uint8_t* carry)
{
    uint8_t lo = (uint8_t)((a & 0x0Fu) + (b & 0x0Fu) + *carry);
    uint8_t c = 0u;
    if (lo >= 10u)
    {
        lo = (uint8_t)(lo - 10u);
        c = 1u;
    }
    uint8_t hi = (uint8_t)((a >> 4) + (b >> 4) + c);
    c = 0u;
    if (hi >= 10u)
    {
        hi = (uint8_t)(hi - 10u);
        c = 1u;
    }
    *carry = c;
    return (uint8_t)((hi << 4) | lo);
}

/* BCD subtract: a - b - *borrow → result, *borrow updated */
static uint8_t bcd_sub(uint8_t a, uint8_t b, uint8_t* borrow)
{
    uint8_t lo_a = a & 0x0Fu, lo_b = (uint8_t)((b & 0x0Fu) + *borrow);
    uint8_t brw = 0u;
    uint8_t lo;
    if (lo_a < lo_b)
    {
        lo = (uint8_t)(lo_a + 10u - lo_b);
        brw = 1u;
    }
    else
    {
        lo = (uint8_t)(lo_a - lo_b);
    }
    uint8_t hi_a = a >> 4, hi_b = (uint8_t)((b >> 4) + brw);
    brw = 0u;
    uint8_t hi;
    if (hi_a < hi_b)
    {
        hi = (uint8_t)(hi_a + 10u - hi_b);
        brw = 1u;
    }
    else
    {
        hi = (uint8_t)(hi_a - hi_b);
    }
    *borrow = brw;
    return (uint8_t)((hi << 4) | lo);
}

/* HOVER: every 8 frames nudge angle toward neutral range [4,14) by ±2 */
void hover(void)
{
    if (FRAME_HW & 7u)
    {
        return;
    }
    uint8_t a = chopper_angle;
    if (a >= 4u && a < 14u)
    {
        return;
    }
    if (a < 8u)
    {
        chopper_angle += 2u;
        chopper_angle += 2u;
    }
    else
    {
        chopper_angle -= 2u;
        chopper_angle -= 2u;
    }
}

/* Internal joystick + demo handler */
static void do_stick(void)
{
    uint8_t saved_bit0 = chopper_angle & 1u;
    chopper_angle &= 0xFEu;

    uint8_t frame = FRAME_HW;

    if (!demo_status)
    {
        uint8_t dx = demo_count;
        STICK_HW = demo_stick[dx];
        if (!(frame & 0x0Fu))
        {
            dx++;
            if (dx >= 108u)
            {
                dx = 0u;
            }
            demo_count = dx;
        }
    }

    uint8_t stick = STICK_HW;

    if (stick == 0x0Fu)
    {
        hover();
        s1_2_val = 20u;
    }

    if (fuel_status == STATUS_EMPTY)
    {
        s1_2_val = 60u;
    }

    /* RIGHT */
    if (!(stick & JOY_RIGHT))
    {
        s1_2_val = 17u;
        if (chopper_angle >= 14u || !(frame & 1u))
        {
            chopper_x++;
        }
        if (!(frame & 3u))
        {
            chopper_angle += 2u;
            chopper_angle += 2u;
        }
    }

    /* LEFT */
    if (!(stick & JOY_LEFT))
    {
        s1_2_val = 17u;
        if (chopper_angle < 4u || !(frame & 1u))
        {
            chopper_x--;
        }
        if (!(frame & 3u))
        {
            chopper_angle -= 2u;
            chopper_angle -= 2u;
        }
    }

    /* UP — only when fuel not empty */
    if (fuel_status != STATUS_EMPTY && !(stick & JOY_UP))
    {
        s1_2_val = 13u;
        chopper_y--;
        hover();
    }

    /* DOWN */
    if (!(stick & JOY_DOWN))
    {
        s1_2_val = 26u;
        if (chopper_status != STATUS_LAND && chopper_status != STATUS_PICKUP)
        {
            chopper_y++;
            hover();
        }
    }

    /* Clamp angle: negative wraps → 0; ≥ 18 → 16 */
    if (chopper_angle & 0x80u)
    {
        chopper_angle = 0u;
    }
    if (chopper_angle >= 18u)
    {
        chopper_angle = 16u;
    }
    chopper_angle = (uint8_t)(chopper_angle | saved_bit0);
}

/* Internal do_numbers worker */
static void do_n(void)
{
    /* ---- SCORE ---- */
    uint8_t* sadr;
    sadr = SCORE_DIG_PTR;
    s_adr_lo = (uint8_t)((uintptr_t)sadr & 0xFFu);
    s_adr_hi = (uint8_t)((uintptr_t)sadr >> 8);
    s_flg = 0u;
    ddig_x = 5u;
    ddig(score3);
    ddig(score2);
    ddig(score1);

    /* Decrement bonus every 8 frames in GO_MODE while non-zero */
    if (mode == GO_MODE && (bonus1 | bonus2) && !(FRAME_HW & 7u))
    {
        uint8_t borrow = 0u;
        bonus1 = bcd_sub(bonus1, 1u, &borrow);
        bonus2 = bcd_sub(bonus2, 0u, &borrow);
    }

    /* ---- BONUS ---- */
    sadr = BONUS_DIG_PTR;
    s_adr_lo = (uint8_t)((uintptr_t)sadr & 0xFFu);
    s_adr_hi = (uint8_t)((uintptr_t)sadr >> 8);
    s_flg = 0u;
    ddig_x = 3u;
    ddig(bonus2);
    ddig(bonus1);

    /* Decrement fuel every 16 frames in GO_MODE while FULL and non-zero */
    if (mode == GO_MODE && fuel_status == STATUS_FULL)
    {
        if (!(fuel1 | fuel2))
        {
            fuel_status = STATUS_EMPTY;
        }
        else if (!(FRAME_HW & 15u))
        {
            uint8_t borrow = 0u;
            fuel1 = bcd_sub(fuel1, 1u, &borrow);
            fuel2 = bcd_sub(fuel2, 0u, &borrow);
        }
    }

    /* ---- FUEL ---- */
    sadr = FUEL_DIG_PTR;
    s_adr_lo = (uint8_t)((uintptr_t)sadr & 0xFFu);
    s_adr_hi = (uint8_t)((uintptr_t)sadr >> 8);
    s_flg = 0u;
    ddig_x = 3u;
    ddig(fuel2);
    ddig(fuel1);
}

/* -----------------------------------------------------------------------
 * Public functions
 * ----------------------------------------------------------------------- */

void pos_chopper(void)
{
    temp1_i = chop_x;
    temp2_i = chop_y;
    pos_it_i();
}

void pos_robot(void)
{
    temp1_i = r_x;
    temp2_i = r_y;
    pos_it_i();
}

void read_stick(void)
{
    if (chopper_status == STATUS_OFF || chopper_status == STATUS_CRASH)
    {
        return;
    }
    do_stick();
}

void read_trig(void)
{
    if (chopper_status == STATUS_CRASH)
    {
        return;
    }

    int do_fire = 0;

    if (!demo_status)
    {
        /* demo mode: fire automatically every 16 frames */
        if (!(FRAME_HW & 0x0Fu))
        {
            do_fire = 1;
        }
    }
    else
    {
        /* real game: rising-edge detection on fire button */
        uint8_t t = TRIG0_HW;
        if (t)
        {
            trig_flag = t;
            return;
        }
        /* trigger is pressed (t == 0) */
        if (!trig_flag)
        {
            return;
        }
        trig_flag = 0u;

        if (mode == TITLE_MODE || mode == OPTION_MODE)
        {
            mode = START_MODE;
            demo_status = START_MODE; /* STA DEMO.STATUS with A=START.MODE */
        }
        do_fire = 1;
    }

    if (!do_fire)
    {
        return;
    }

    elevator_dx ^= 0xFEu; /* EOR #-2: flip bits 7:1 */

    /* Find free rocket slot (check slot 1 first, then 0) */
    int8_t slot = -1;
    for (int8_t i = 1; i >= 0; i--)
    {
        if (!rocket_status[(uint8_t)i])
        {
            slot = i;
            break;
        }
    }
    if (slot < 0)
    {
        return;
    }

    /* Map chopper angle to rocket direction 1-5 */
    uint8_t a = (uint8_t)((chopper_angle & 0x1Eu) >> 1u);
    uint8_t rs;
    if (a >= 6u)
    {
        rs = (a >= 8u) ? 5u : (uint8_t)(a - 2u);
    }
    else if (a >= 4u)
    {
        rs = 3u;
    }
    else
    {
        rs = (a == 0u) ? 1u : a;
    }

    rocket_status[(uint8_t)slot] = rs;
    rocket_x[(uint8_t)slot] = (uint8_t)((chopper_x & 3u) + chopper_x + 8u);
    rocket_y[(uint8_t)slot] = (uint8_t)(chopper_y + 8u);
    s2_val = 0x3Fu;
}

void draw_map(void)
{
    /* ---- DO.X: horizontal scroll ---- */
    if (chopper_x > MIN_RIGHT)
    {
        chopper_x = MIN_RIGHT;
        if (sx >= (uint8_t)(0xD8u + 1u))
        {
            sx = (uint8_t)(1u + 1u);
        }
        sx_f--;
        if ((sx_f & 3u) == 3u)
        {
            sx++;
        }
    }
    else if (chopper_x < MIN_LEFT)
    {
        chopper_x = MIN_LEFT;
        if (sx < (uint8_t)(1u + 1u + 1u))
        {
            sx = (uint8_t)(0xD8u + 1u);
        }
        sx_f++;
        if ((sx_f & 3u) == 0u)
        {
            sx--;
        }
    }

    /* ---- DO.Y: vertical scroll ---- */
    int need_down = 0, need_up = 0;
    uint8_t y_check; /* CHOPPER.Y value used at .3/.81 (X register in asm) */

    if (sy != 24u && chopper_y > MIN_DOWN)
    {
        /* past MIN.DOWN → force camera scroll down */
        chopper_y = MIN_DOWN;
        need_down = 1;
        y_check = MIN_DOWN;
    }
    else
    {
        /* .80: check hard lower pixel limit */
        y_check = chopper_y;
        if (chopper_y > MAX_DOWN)
        {
            chopper_y = MAX_DOWN;
            if ((sy_f & 7u) != 0u || sy != 24u)
            {
                need_down = 1;
            }
        }
    }

    if (!need_down)
    {
        /* .3/.81: check upward scroll triggers */
        if (sy != (uint8_t)0xFFu && y_check < MIN_UP)
        {
            chopper_y = MIN_UP;
            need_up = 1;
        }
        else if (y_check < MAX_UP)
        {
            chopper_y = MAX_UP;
            if ((sy_f & 7u) != 7u || sy != (uint8_t)0xFFu)
            {
                need_up = 1;
            }
        }
    }

    if (need_down)
    {
        sy_f++;
        if ((sy_f & 7u) == 0u)
        {
            sy++;
        }
    }
    else if (need_up)
    {
        sy_f--;
        if ((sy_f & 7u) == 7u)
        {
            sy--;
        }
    }

    /* .4: write fine-scroll registers */
    HSCROL_HW = sx_f & 3u;
    VSCROL_HW = sy_f & 7u;

    /* Update display-list map row addresses */
    temp1_i = sx;
    temp2_i = sy;
    compute_map_adr_i();

    uint8_t* dsp = DSP_MAP_PTR;
    for (uint8_t row = 0u; row < MAP_LINES; row++)
    {
        uint8_t base = (uint8_t)(row * 3u);
        dsp[base + 1u] = adr1_i_lo;
        dsp[base + 2u] = adr1_i_hi;
        adr1_i_hi++;
    }
}

void compute_map_adr_i(void)
{
    /* MAP - 5 = 0x1103 - 5 = 0x10FE;  lo=0xFE, hi=0x10 */
    uint16_t s = (uint16_t)(0xFEu + temp1_i);
    adr1_i_lo = (uint8_t)(s & 0xFFu);
    adr1_i_hi = (uint8_t)((uint8_t)(0x10u + (uint8_t)(s >> 8)) + temp2_i);
}

void compute_map_adr(void)
{
    uint16_t s = (uint16_t)(0xFEu + temp1);
    adr1_lo = (uint8_t)(s & 0xFFu);
    adr1_hi = (uint8_t)((uint8_t)(0x10u + (uint8_t)(s >> 8)) + temp2);
}

void do_laser_1(void)
{
    if (FRAME_HW & 7u)
    {
        return;
    }
    if (laser_status == STATUS_OFF)
    {
        /* Clear LASERS_1 (32 bytes) and LASER_3 (8 bytes) */
        for (int i = 31; i >= 0; i--)
        {
            LASERS_1[i] = 0u;
        }
        for (int i = 7; i >= 0; i--)
        {
            LASER_3[i] = 0u;
        }
        return;
    }
    tim1_val = (uint8_t)(tim1_val + laser_spd);
    if (tim1_val == 0u)
    {
        /* Overflow: copy full laser_shapes (32 bytes) → LASERS_1,
         * and bytes 24-31 → LASER_3                              */
        for (uint8_t i = 0u; i < 32u; i++)
        {
            LASERS_1[i] = laser_shapes[i];
        }
        for (uint8_t i = 0u; i < 8u; i++)
        {
            LASER_3[i] = laser_shapes[24u + i];
        }
    }
}

void do_laser_2(void)
{
    if (FRAME_HW & 7u)
    {
        return;
    }
    if (laser_status == STATUS_OFF)
    {
        for (int i = 31; i >= 0; i--)
        {
            LASERS_2[i] = 0u;
        }
        return;
    }
    tim2_val = (uint8_t)(tim2_val + laser_spd);
    if (tim2_val == 0u)
    {
        for (uint8_t i = 0u; i < 32u; i++)
        {
            LASERS_2[i] = laser_shapes[i];
        }
        for (uint8_t i = 0u; i < 8u; i++)
        {
            LASER_3[i] = laser_shapes[16u + i];
        }
    }
}

void do_blocks(void)
{
    if (FRAME_HW & 0x7Fu)
    {
        return;
    }
    /* Clear BLOCK.1 (32 bytes covers BLOCK.1-4) */
    for (int i = 31; i >= 0; i--)
    {
        BLOCK_1[i] = 0u;
    }
    /* Randomly fill each block's 8 bytes */
    if (!(RANDOM_HW & 0x80u))
    {
        for (int i = 7; i >= 0; i--)
        {
            BLOCK_1[i] = 0x55u;
        }
    }
    if (!(RANDOM_HW & 0x80u))
    {
        for (int i = 7; i >= 0; i--)
        {
            BLOCK_2[i] = 0x55u;
        }
    }
    if (!(RANDOM_HW & 0x80u))
    {
        for (int i = 7; i >= 0; i--)
        {
            BLOCK_3[i] = 0x55u;
        }
    }
    if (!(RANDOM_HW & 0x80u))
    {
        for (int i = 7; i >= 0; i--)
        {
            BLOCK_4[i] = 0x55u;
        }
    }
}

void do_elevator(void)
{
    elevator_tim--;
    if (elevator_tim)
    {
        return;
    }

    elevator_tim = elevator_spd;

    /* Clear BLOCK.5 (32 bytes covers BLOCK.5-8) */
    for (int i = 31; i >= 0; i--)
    {
        BLOCK_5[i] = 0u;
    }

    /* Advance elevator position (mod 4) */
    elevator_num = (uint8_t)((elevator_num + elevator_dx) & 3u);

    /* Fill 8 bytes of target block with 0x55 */
    uint8_t* target = elevators[elevator_num];
    for (int i = 7; i >= 0; i--)
    {
        target[i] = 0x55u;
    }
}

void do_exp(void)
{
    /* Animate explosion shapes using EXP_SHAPE masked with random */
    for (int i = 7; i >= 0; i--)
    {
        uint8_t v = (uint8_t)(((int8_t*)EXP_SHAPE)[i] & RANDOM_HW);
        EXPLOSION_PTR[i] = v;
        EXPLOSION2_PTR[i] = v;
    }
    /* Flash missile character colours */
    for (uint8_t i = 3u; i < 5u; i++)
    {
        MISS_CHR_LEFT[i] = (uint8_t)((RANDOM_HW & 0x0Fu) | 0xA0u);
    }
    for (uint8_t i = 3u; i < 5u; i++)
    {
        MISS_CHR_RIGHT[i] = (uint8_t)((RANDOM_HW & 0xE0u) | 0x0Au);
    }
}

void do_numbers(void)
{
    if (mode == NEW_PLAYER_MODE || mode == GAME_OVER_MODE)
    {
        return;
    }
    do_n();
}

void ddig(uint8_t val)
{
    uint8_t y = val;

    /* Hi-nibble leading-zero suppression */
    if (!s_flg)
    {
        if (y & 0xF0u)
        {
            s_flg = 1u;
        }
        else
        {
            y |= 0xA0u; /* mark hi nibble as blank (0xA) */
        }
    }
    else
    {
        s_flg = 1u;
    }

    /* Lo-nibble leading-zero suppression (using the updated y) */
    if (!s_flg)
    {
        if (y & 0x0Fu)
        {
            s_flg = 1u;
        }
        else
        {
            y |= 0x0Au; /* mark lo nibble as blank (0xA) */
        }
    }
    else
    {
        s_flg = 1u;
    }

    draw(y >> 4u);   /* hi nibble */
    draw(y & 0x0Fu); /* lo nibble (fall-through in asm) */
}

void inc_score(uint8_t hi, uint8_t lo)
{
    if (!demo_status)
    {
        return; /* demo mode: no scoring */
    }
    uint8_t carry = 0u;
    score1 = bcd_add(score1, lo, &carry);
    score2 = bcd_add(score2, hi, &carry);
    score3 = bcd_add(score3, 0u, &carry);
}
