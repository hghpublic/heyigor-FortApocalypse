/* fort3.c — VBlank interrupt driver, chopper/robot AI, rocket updates
 * Converted from fort3.s (Atari 6502 SynAssembler)
 */

#include <stdint.h>
#include "fort1.h"
#include "fort3.h"

/* -----------------------------------------------------------------------
 * From fort1.h:
 * STATUS_* constants: OFF, ON, LAND, PICKUP, FLY, CRASH, BEGIN, EMPTY, REFUEL, EXPLODE
 * Mode constants: GO_MODE, HYPERSPACE_MODE, NEW_PLAYER_MODE
 * Constant: HIT_LIST_LEN
 * External variables: mode, level, chopper_status, chopper_x, chopper_y, chopper_angle,
 *   chopper_col, robot_status, robot_x, robot_y, robot_angle, robot_col,
 *   robot_spd, r_status, r_fx, r_fy, r_x, r_y, sx, sy, sx_f, sy_f,
 *   chop_x, chop_y, ochopper_y, orobot_y, orocket_y[],
 *   rocket_status[], rocket_x[], rocket_y[], rocket_tim[], rocket_temp[],
 *   rocket_tempx[], rocket_tempy[], tim3_val, tim7_val, tim8_val,
 *   fuel_status, fort_status, grav_skl, ssizem, s2_val, s3_val, s5_val, bak2_color,
 *   land_fx, land_fy, land_x, land_y, land_chop_x, land_chop_y, land_chop_angle,
 *   chopper_shapes[18]
 * External functions: pos_chopper(), pos_robot(), compute_map_adr_i(),
 *   pick_up_slave(), inc_score(), do_numbers(), draw_map(), read_trig(),
 *   do_exp(), do_laser_1(), do_laser_2(), do_blocks(), do_elevator(), read_stick()
 * ----------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 * FORT.H items also declared locally in this file:
 *   Variables:  adr1_i_lo, adr1_i_hi, adr2_i_lo, adr2_i_hi,
 *               temp1_i, temp2_i, temp3_i, temp4_i,
 *               tim3_val, tim7_val, tim8_val, ssizem,
 *               s2_val, s3_val, s5_val
 *   Constants:  CHR_SET2
 *   PM aliases: PLAYER_MIS=MIS_BASE, PLAYER_PL0=PL0_BASE, PLAYER_PL1=PL1_BASE,
 *               PLAYER_PL2=PL2_BASE, PLAYER_PL3=PL3_BASE
 *   HW aliases: REG(); FRAME_HW=FRAME, ATTRACT_HW=ATTRACT, RANDOM_HW=RANDOM,
 *               M0PL_HW..M3PL_HW=M0PL..M3PL, P0PL_HW..P3PL_HW=P0PL..P3PL,
 *               P0PF_HW..P3PF_HW=P0PF..P3PF, HITCLR_HW=HITCLR,
 *               HPOSP0_HW=HPOSP0, HPOSP1_HW=HPOSP1, SIZEM_HW=SIZEM,
 *               PCOLR0_HW..PCOLR3_HW=PCOLR0..PCOLR3
 * ----------------------------------------------------------------------- */


/* -----------------------------------------------------------------------
 * Hardware register access
 * ----------------------------------------------------------------------- */
#define REG(a)       (*(volatile uint8_t *)(uintptr_t)(a))
#define ATTRACT_HW   REG(0x004D)  /* OS attract-mode timer (zero = off) */
#define RANDOM_HW    REG(0xD20A)
#define FRAME_HW     REG(0x0014)  /* OS frame counter */

/* Collision read registers ($D000-range, read-only) */
#define M2PL_HW      REG(0xD00A)  /* missile 2 → player */
#define M3PL_HW      REG(0xD00B)  /* missile 3 → player */
#define P0PL_HW      REG(0xD00C)  /* player 0 → player (read) */
#define P1PL_HW      REG(0xD00D)
#define P0PF_HW      REG(0xD004)  /* player 0 → playfield */
#define P1PF_HW      REG(0xD005)
#define M0PL_HW      REG(0xD008)  /* missile 0 → player */
#define M1PL_HW      REG(0xD009)
#define P2PL_HW      REG(0xD00E)
#define P2PF_HW      REG(0xD006)
#define P3PL_HW      REG(0xD00F)
#define P3PF_HW      REG(0xD007)
#define HITCLR_HW    REG(0xD01E)  /* write: clear all collision latches */

/* Sprite position and size write registers */
#define HPOSP0_HW    REG(0xD000)
#define HPOSP1_HW    REG(0xD001)
#define SIZEM_HW     REG(0xD00C)  /* write: missile size (same addr as P0PL read) */

/* Player/missile color shadows (OS page 2) */
#define PCOLR0_HW    REG(0x02C0)
#define PCOLR1_HW    REG(0x02C1)
#define PCOLR2_HW    REG(0x02C2)
#define PCOLR3_HW    REG(0x02C3)

/* -----------------------------------------------------------------------
 * PM-RAM layout  (player_base is a 0x800-byte page-aligned buffer)
 * PLAYER offsets: MIS=$300, PL0=$400, PL1=$500, PL2=$600, PL3=$700
 * ----------------------------------------------------------------------- */
// extern uint8_t player_base[];
// HACK: fixed build error
uint8_t player_base[0x800];
#define PLAYER_PL0   (player_base + 0x400u)
#define PLAYER_PL1   (player_base + 0x500u)
#define PLAYER_PL2   (player_base + 0x600u)
#define PLAYER_PL3   (player_base + 0x700u)
#define PLAYER_MIS   (player_base + 0x300u)

/* -----------------------------------------------------------------------
 * External variables (fort.s / fort7.s zero-page)
 * ----------------------------------------------------------------------- */
/* Interrupt-safe working registers */
extern uint8_t adr1_i_lo, adr1_i_hi;   /* ADR1.I .BS 2 */
extern uint8_t adr2_i_lo, adr2_i_hi;   /* ADR2.I .BS 2 */
extern uint8_t temp1_i, temp2_i, temp3_i, temp4_i; /* TEMP1.I-TEMP4.I */

/* Mode and collision results */
extern uint8_t mode;
extern uint8_t chopper_col;            /* CHOPPER.COL */
extern uint8_t robot_col;             /* ROBOT.COL */

/* Chopper state */
extern uint8_t chopper_status;
extern uint8_t chopper_x, chopper_y;
extern uint8_t chopper_angle;
extern uint8_t ochopper_y;            /* OCHOPPER.Y — old sprite Y */

/* Scroll */
extern uint8_t sx, sy;
extern uint8_t sx_f, sy_f;

/* Map tile coords of chopper */
extern uint8_t chop_x, chop_y;

/* Robot display state */
extern uint8_t robot_status;          /* sprite ON/OFF/CRASH */
extern uint8_t robot_x, robot_y;      /* sprite pixel positions */
extern uint8_t orobot_y;              /* OROBOT.Y */

/* Robot AI state */
extern uint8_t r_status;              /* game AI status: FLY/OFF/CRASH */
extern uint8_t robot_angle;           /* ROBOT.ANGLE 0-17 */
extern uint8_t robot_spd;             /* ROBOT.SPD — AND mask for frame rate */
extern uint8_t r_fx, r_fy;            /* sub-tile fractions */
extern uint8_t r_x, r_y;             /* map tile position */

/* Rocket arrays */
extern uint8_t rocket_status[3];
extern uint8_t rocket_x[3];
extern uint8_t rocket_y[3];
extern uint8_t rocket_tim[3];
extern uint8_t rocket_temp[3];        /* saved map tile at impact */
extern uint8_t rocket_tempx[3];       /* saved map X at impact */
extern uint8_t rocket_tempy[3];       /* saved map Y at impact */
extern uint8_t orocket_y[3];          /* OROCKET.Y — old sprite Y */

/* Timers */
extern uint8_t tim3_val;              /* chopper crash countdown */
extern uint8_t tim7_val;              /* robot crash/spawn timer */
extern uint8_t tim8_val;              /* robot fire rate timer */

/* Level */
extern uint8_t level;

/* Game status flags */
extern uint8_t fuel_status;
extern uint8_t fort_status;
extern uint8_t grav_skl;              /* GRAV.SKL — gravity rate AND mask */
extern uint8_t ssizem;                /* SSIZEM — shadow of SIZEM */

/* Sound parameters */
extern uint8_t s2_val, s3_val, s5_val;

/* Background color shadow */
extern uint8_t bak2_color;

/* Landing saved state */
extern uint8_t land_fx, land_fy;      /* saved scroll sub-pixel */
extern uint8_t land_x, land_y;        /* saved scroll tile */
extern uint8_t land_chop_x, land_chop_y, land_chop_angle;

/* -----------------------------------------------------------------------
 * External variables from other source files
 * ----------------------------------------------------------------------- */
/* CHOPPER.SHAPES (fort6.s): 18 shape-data pointers, one per angle 0-17 */
extern const uint8_t * const chopper_shapes[18];

/* LAND.CHR (fort5.s): 5 landing tile chars; LAND.LEN = 4 (= count-1) */
extern const uint8_t land_chr[5];
#define LAND_LEN 4

/* -----------------------------------------------------------------------
 * External functions from other modules
 * ----------------------------------------------------------------------- */
extern void pos_chopper(void);           /* fort4.c */
extern void pos_robot(void);             /* fort4.c */
extern void compute_map_adr_i(void);     /* fort4.c — (temp1_i,temp2_i)→adr1_i */
/* fort5.c — returns 1 if a slave was picked up (assembly: carry set), 0 otherwise */
extern int  pick_up_slave(void);
extern void inc_score(uint8_t hi, uint8_t lo); /* fort4.c */

/* fort4/5 game-loop helpers (called from vertblkd) */
extern void do_numbers(void);
extern void draw_map(void);
extern void read_trig(void);
extern void do_exp(void);
extern void do_laser_1(void);
extern void do_laser_2(void);
extern void do_blocks(void);
extern void do_elevator(void);
extern void read_stick(void);

/* -----------------------------------------------------------------------
 * Static helpers: pointer from adr1_i
 * ----------------------------------------------------------------------- */
static inline uint8_t *adr1_i_ptr(void)
{
    return (uint8_t *)(uintptr_t)((uint16_t)adr1_i_hi << 8 | adr1_i_lo);
}

/* -----------------------------------------------------------------------
 * Data tables
 * ----------------------------------------------------------------------- */

/* HIT.LIST (12 bytes) + TANK.SHAPE (7 bytes) + extra (3 bytes) = 22 bytes */
const uint8_t hit_list[22] = {
    /* HIT.LIST: entity tiles that are killed on rocket impact */
    0x40, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,    /* indices  0-5  */
    0x3B, 0x3C, 0x3D, 0x3E, 0x49, 0x4A,    /* indices  6-11 */
    /* TANK.SHAPE (.AT -/lmnop/ → Atari screen codes 0x4C-0x50) */
    0x4C, 0x4D, 0x4E, 0x4F, 0x50,           /* indices 12-16 */
    0x71, 0x72,                              /* MISS.LEFT, MISS.RIGHT */
    /* HIT.LIST.LEN = 18 ends here; extra data follows */
    0x61, 0x20,                              /* .AT /a / */
    0x20,                                    /* .DA #EXP — HIT.LIST2.LEN = 21 */
};

/* Robot spawn positions — 16 entries (0-7 for level>1, 8-15 for level==1) */
static const uint8_t rob_x[16] = {
    0x84, 0xC3, 0x49, 0xC3, 0x49, 0x84, 0x84, 0x84,
    0xD7, 0xD7, 0xD7, 0xD6, 0xD6, 0xD6, 0x33, 0x33,
};
static const uint8_t rob_y[16] = {
    0x00, 0x16, 0x16, 0x21, 0x21, 0x00, 0x00, 0x00,
    0x12, 0x12, 0x12, 0x06, 0x06, 0x06, 0x06, 0x06,
};

/* Rocket DX/DY per direction status 1-5 (index = status-1) */
const int8_t rocket_dx[5] = { -4, -4, 0, 4, 4 };
const int8_t rocket_dy[5] = {  2,  0, 2, 0, 2 };

/* ROCKET1.MASK: AND mask to clear rocket bits in missile plane */
static const uint8_t rocket1_mask[3] = { 0xFC, 0xF3, 0xCF };
/* ROCKET2.MASK: OR mask to set rocket body bits */
static const uint8_t rocket2_mask[3] = { 0x03, 0x0C, 0x30 };
/* ROCKET3.MASK: OR mask to set rocket size bit */
static const uint8_t rocket3_mask[3] = { 0x01, 0x04, 0x10 };

/* -----------------------------------------------------------------------
 * CHECK.CHR.I — test glyph at (temp1_i, temp2_i); returns 1 if solid
 * Reads map byte via compute_map_adr_i, strips reverse-video bit,
 * looks up 8-row glyph in CHR_SET2; any non-zero row = solid.
 * ----------------------------------------------------------------------- */
static int check_chr_i(void)
{
    compute_map_adr_i();
    uint8_t b = (uint8_t)(adr1_i_ptr()[0] & 0x7Fu);
    const uint8_t *glyph = CHR_SET2 + (uint16_t)b * 8u;
    for (int row = 7; row >= 0; row--)
        if (glyph[row]) return 1;
    return 0;
}

/* -----------------------------------------------------------------------
 * R.LEFT — try to move robot one sub-step left; check 3 positions
 * ----------------------------------------------------------------------- */
static void r_left(void)
{
    temp1_i = r_x;
    temp2_i = r_y;
    if (check_chr_i()) return;
    temp1_i--;
    if (check_chr_i()) return;
    temp1_i--;
    if (check_chr_i()) return;

    r_fx--;
    if (r_fx == 0xFFu) r_x--;

    if ((FRAME_HW & 3u) == 0u) {
        robot_angle -= 2u;
        robot_angle -= 2u;
    }
}

/* -----------------------------------------------------------------------
 * R.RIGHT — try to move robot one sub-step right; check 3 positions
 * ----------------------------------------------------------------------- */
static void r_right(void)
{
    temp1_i = r_x;
    temp2_i = r_y;
    if (check_chr_i()) return;
    temp1_i++;
    if (check_chr_i()) return;
    temp1_i++;
    if (check_chr_i()) return;

    r_fx++;
    if (r_fx == 4u) r_x++;

    if ((FRAME_HW & 3u) == 0u) {
        robot_angle += 2u;
        robot_angle += 2u;
    }
}

/* -----------------------------------------------------------------------
 * R.DOWN — try to move robot one sub-step down; check 3 positions
 * ----------------------------------------------------------------------- */
static void r_down(void)
{
    temp1_i = r_x;
    temp2_i = r_y;
    if (check_chr_i()) return;
    temp2_i++;
    if (check_chr_i()) return;
    temp2_i++;
    if (check_chr_i()) return;

    r_fy++;
    if (r_fy == 8u) r_y++;
}

/* -----------------------------------------------------------------------
 * R.UP — try to move robot one sub-step up; check 4 positions
 * (skips collision checks if R.Y < 3 — near top of map)
 * ----------------------------------------------------------------------- */
static void r_up(void)
{
    temp1_i = r_x;
    if (r_y >= 3u) {
        temp2_i = r_y;
        if (check_chr_i()) return;
        temp2_i--;
        if (check_chr_i()) return;
        temp2_i--;
        if (check_chr_i()) return;
        temp2_i--;
        if (check_chr_i()) return;
    }
    r_fy--;
    if (r_fy == 0xFFu) r_y--;
}

/* -----------------------------------------------------------------------
 * CHECK.LAND — read map byte at ADR1.I and compare to landing chars
 * Sets temp3_i++ if not a landing tile.  Sets temp4_i++ if hyper tile.
 * ----------------------------------------------------------------------- */
static void check_land(void)
{
    uint8_t b = adr1_i_ptr()[0];
    for (int i = LAND_LEN; i >= 0; i--) {
        if (b == land_chr[i]) return;   /* landing tile: OK, no increment */
        if (b == 0x48u) { temp4_i++; return; }  /* hyperspace tile */
    }
    temp3_i++;  /* non-landing tile found */
}

/* -----------------------------------------------------------------------
 * SAVE.POS — copy current chopper+scroll state to LAND.* variables
 * ----------------------------------------------------------------------- */
void save_pos(void)
{
    land_fx = sx_f;
    land_fy = sy_f;
    land_x  = sx;
    land_y  = sy;
    land_chop_x     = chopper_x;
    land_chop_y     = chopper_y;
    land_chop_angle = chopper_angle;
}

/* -----------------------------------------------------------------------
 * ROBOT.BRAINS — robot AI: spawn/countdown when OFF; rate-limited move
 * ----------------------------------------------------------------------- */
void robot_brains(void)
{
    uint8_t st = r_status;

    if (st == STATUS_OFF) {
        /* .1: spawn countdown — decrement timer; try to spawn when it hits 0 */
        if (--tim7_val != 0u) return;

        /* .0: timer reached 0 — attempt to spawn */
        PCOLR2_HW = 0x88u;
        PCOLR3_HW = 0x88u;
        robot_angle = 8u;

        uint8_t idx = RANDOM_HW & 7u;
        if (level == 1u) idx = (uint8_t)(idx + 8u);  /* level 1: second spawn set */

        r_x = rob_x[idx];
        r_y = rob_y[idx];

        /* Distance check: must be >= 34 from chopper in X or >= 8 in Y */
        uint8_t dx = (uint8_t)(r_x - chop_x);
        if ((int8_t)dx < 0) dx ^= 0xFEu;   /* EOR #-2 approximate abs */
        if (dx < 34u) {
            uint8_t dy = (uint8_t)(r_y - chop_y);
            if ((int8_t)dy < 0) dy ^= 0xFEu;
            if (dy < 8u) return;  /* too close: abort spawn, timer wraps to 255 */
        }

        /* .6: spawn robot */
        r_status  = STATUS_FLY;
        r_fx      = 0u;
        r_fy      = 0u;
        tim7_val  = 0u;
        tim8_val  = 1u;
        pos_robot();
        return;
    }

    if (st == STATUS_CRASH) return;  /* .2 */

    /* Active: rate-limit by ROBOT.SPD mask */
    if (FRAME_HW & robot_spd) return;

    /* R.START: save oscillation bit, clear it, then update sprite */
    temp4_i = robot_angle & 1u;
    robot_angle &= 0xFEu;
    pos_robot();

    /* R.F: fire rocket[2] every 5 frames if robot is visible */
    if (--tim8_val == 0u) {
        tim8_val = 5u;
        if (robot_status == STATUS_ON && rocket_status[2] == 0u) {
            /* Map even angle (0-16) to rocket direction (1-5) */
            static const uint8_t fire_dir[9] = { 1u,1u,2u,3u,3u,3u,4u,5u,5u };
            uint8_t dir_idx = (uint8_t)((robot_angle & 0x1Eu) >> 1);
            rocket_status[2] = fire_dir[dir_idx];
            rocket_x[2] = (uint8_t)((robot_x & 3u) + robot_x + 8u);
            rocket_y[2] = (uint8_t)(robot_y + 8u);
            s2_val = 0x3Fu;
        }
    }

    /* R.B: boundary check and chopper chase */
    {
        uint8_t rx = r_x;
        int at_boundary;
        uint8_t snap_x;

        if (rx == 216u) {
            snap_x = 215u; at_boundary = 1;
        } else if (rx == 48u) {
            snap_x = 49u;  at_boundary = 1;
        } else {
            snap_x = 0u;   at_boundary = 0;
        }

        if (at_boundary) {
            /* Every 4 frames: steer robot toward center */
            if ((FRAME_HW & 3u) == 0u) {
                uint8_t ang = robot_angle;
                if (ang < 4u || ang >= 14u) {
                    if (ang < 8u) { robot_angle += 2u; robot_angle += 2u; }
                    else          { robot_angle -= 2u; robot_angle -= 2u; }
                }
            }
            /* If sprite not visible: snap to just-inside boundary */
            if (robot_status == STATUS_OFF) r_x = snap_x;
            /* Skip horizontal chase; fall through to vertical chase */
        } else {
            /* Chase chopper horizontally */
            if (chop_x != r_x) {
                if (chop_x < r_x) r_left();
                else               r_right();
            }
        }

        /* Chase chopper vertically (always, regardless of boundary) */
        if (chop_y != r_y) {
            if (chop_y > r_y) r_down();
            else               r_up();
        }
        pos_robot();
    }

    /* R.END: clamp ROBOT.ANGLE to [0,17], restore oscillation bit, mask fractions */
    {
        uint8_t ang = robot_angle;
        if ((int8_t)ang < 0) ang = 0u;
        if (ang >= 18u)       ang = 16u;
        robot_angle = (uint8_t)(ang | temp4_i);
        r_fx &= 3u;
        r_fy &= 7u;
    }
}

/* -----------------------------------------------------------------------
 * DO.CHOPPER — chopper physics: gravity, collision, land/crash/hyperspace
 * ----------------------------------------------------------------------- */
void do_chopper(void)
{
    uint8_t cs = chopper_status;

    if (cs == STATUS_OFF) return;
    if (cs == STATUS_CRASH) return;

    /* Apply gravity every (GRAV.SKL+1) frames unless landing or in pickup */
    if (cs != STATUS_LAND && cs != STATUS_PICKUP) {
        if ((FRAME_HW & grav_skl) == 0u)
            chopper_y++;
    }

    /* .3: collision dispatch */
    {
        uint8_t col = chopper_col;

        /* No collision: reset status to FLY and return (.12 path) */
        if (col == 0u) { chopper_status = STATUS_FLY; return; }

        /* Laser hit (col == 4 = P1PF playfield bit): crash */
        if (col == 4u) goto crash;

        /* Hyperspace tile hit (col == 8): enter only on level 0 */
        if (col == 8u) {
            if (level != 0u) goto crash;
            chopper_col = 0u;
            mode   = HYPERSPACE_MODE;
            s3_val = 1u;
            s5_val = 1u;
            chopper_status = STATUS_FLY;
            return;
        }

        /* .6: other playfield collision — check landing tiles at CHOP.X/Y */
        temp1_i = chop_x;
        temp2_i = chop_y;
        compute_map_adr_i();
        temp3_i = 0u;
        temp4_i = 0u;
        check_land();        /* row 0 */
        adr1_i_hi++;         /* advance to next map page (next row) */
        check_land();        /* row 1 */
        adr1_i_hi++;
        check_land();        /* row 2 */
    }

    /* Missile/player hit (upper nibble set): crash regardless of tiles */
    if (chopper_col & 0xF0u) goto crash;

    /* Bit 1 = slave pickup zone: try to collect */
    if (chopper_col & 0x02u) {
        if (pick_up_slave()) {          /* returns 1 if slave picked up (carry) */
            chopper_status = STATUS_PICKUP;
            return;
        }
    }

    /* .9: all 3 checked rows were non-landing tiles → crash */
    if (temp3_i == 3u) goto crash;

    chopper_y--;    /* compensate for the gravity applied above */

    {
        uint8_t fs = fuel_status;
        /* Chopper too high for a safe landing (chop_y >= 14) AND fuel empty */
        if (chop_y >= (uint8_t)(10u + 4u)) {
            if (fs == STATUS_EMPTY) goto crash;
        }
        /* .8: forced land if refueling or hyper tile below; else save position */
        if (fs == STATUS_REFUEL) goto land;
        if (temp4_i) goto land;
        save_pos();
    }

land:
    chopper_status = STATUS_LAND;
    return;

crash:
    tim3_val       = 20u;
    s3_val         = 1u;
    chopper_status = STATUS_CRASH;
}

/* -----------------------------------------------------------------------
 * DO.ROBOT.CHOPPER — compute robot sprite pixel position from tile coords;
 * check for off-screen (→ ROBOT.STATUS=OFF) or collision (→ CRASH).
 * ----------------------------------------------------------------------- */
void do_robot_chopper(void)
{
    /* P1: off-screen or AI-inactive → ROBOT.STATUS = OFF */
    if (r_status == STATUS_OFF) {
        robot_status = STATUS_OFF;
        return;
    }

    /* Screen X range check: R.X must be in [SX, SX+48) */
    if (r_x < sx) { robot_status = STATUS_OFF; return; }
    uint8_t dx = (uint8_t)(r_x - sx);
    if (dx >= 48u) { robot_status = STATUS_OFF; return; }

    /* Screen Y range check */
    {
        uint8_t sy_clamp = ((int8_t)sy < 0) ? 0u : sy;
        if (r_y < sy_clamp) { robot_status = STATUS_OFF; return; }
        uint8_t dy = (uint8_t)(r_y - sy);
        if (dy >= 19u) { robot_status = STATUS_OFF; return; }
    }

    /* Compute pixel X: (R.X - SX)*4 + 22 + (SX.F & 3) + R.FX */
    {
        uint8_t px = (uint8_t)((dx << 2) + 22u);
        px = (uint8_t)(px + (sx_f & 3u) + r_fx);
        robot_x = px;
    }

    /* Compute pixel Y: (R.Y - max(SY,0))*8 + 83 + (~SY.F & 7) + [SY<0:+8] + R.FY */
    {
        uint8_t sy_clamp = ((int8_t)sy < 0) ? 0u : sy;
        uint8_t py = (uint8_t)(((uint8_t)(r_y - sy_clamp) << 3) + (71u + 12u));
        py = (uint8_t)(py + ((sy_f ^ 0xFFu) & 7u));
        if ((int8_t)sy < 0) py = (uint8_t)(py + 8u);
        py = (uint8_t)(py + r_fy);
        robot_y = py;
    }

    /* Collision check */
    uint8_t new_r_status = STATUS_FLY;
    if (robot_col) {
        if (r_status != STATUS_CRASH) {
            new_r_status = STATUS_CRASH;
            tim7_val = 20u;
            s3_val   = 1u;
        }
    }
    if (r_status != STATUS_CRASH)
        r_status = new_r_status;

    /* DRCE: set ROBOT.STATUS = ON */
    robot_status = STATUS_ON;
}

/* -----------------------------------------------------------------------
 * CCXY (static) — compute CHOP.X/Y from sprite coords, call pos_chopper
 * ----------------------------------------------------------------------- */
static void ccxy(void)
{
    chop_x = (uint8_t)(((uint8_t)(chopper_x - 24u) >> 2) + sx);

    uint8_t cy = (uint8_t)((uint8_t)(chopper_y - 88u) >> 3);
    temp1_i = cy;
    uint8_t sy_base = ((int8_t)sy >= 0) ? sy : 0u;
    chop_y = (uint8_t)(sy_base + temp1_i);

    pos_chopper();
}

/* -----------------------------------------------------------------------
 * UPDATE.CHOPPER — VBlank: clear old chopper sprite, draw new, handle crash
 * ----------------------------------------------------------------------- */
void update_chopper(void)
{
    uint8_t cs = chopper_status;

    /* BEGIN: first frame of new life — switch to FLY and update position */
    if (cs == STATUS_BEGIN) {
        chopper_status = STATUS_FLY;
        ccxy();
        return;
    }

    /* OFF: blank horizontal position registers */
    if (cs == STATUS_OFF) {
        HPOSP0_HW = 0u;
        HPOSP1_HW = 0u;
        return;
    }

    /* Clear old sprite rows (18 rows from last frame's Y) */
    {
        uint8_t oy = ochopper_y;
        for (int i = 0; i < 18; i++) {
            PLAYER_PL0[oy + (uint8_t)i] = 0u;
            PLAYER_PL1[oy + (uint8_t)i] = 0u;
        }
    }

    /* Set horizontal positions */
    HPOSP0_HW = chopper_x;
    HPOSP1_HW = (uint8_t)(chopper_x + 8u);

    /* Load shape pointer for current angle */
    {
        const uint8_t *shape = chopper_shapes[chopper_angle];

        /* Draw 18 rows into PL0 (left half) and PL1 (right half) */
        uint8_t y = chopper_y;
        ochopper_y = y;
        for (int i = 0; i < 18; i++) {
            PLAYER_PL0[y + (uint8_t)i] = shape[i];
            PLAYER_PL1[y + (uint8_t)i] = shape[18 + i];
        }
    }

    /* CRASH: disintegrate pixels, cycle color, handle timer */
    if (cs == STATUS_CRASH) {
        uint8_t y = chopper_y;
        for (int i = 0; i < 18; i++) {
            PLAYER_PL0[y + (uint8_t)i] &= RANDOM_HW;
            PLAYER_PL1[y + (uint8_t)i] &= RANDOM_HW;
        }
        PCOLR0_HW++;
        PCOLR1_HW++;
        bak2_color = (uint8_t)(RANDOM_HW | 0x0Fu);

        if (mode == GO_MODE) {
            /* Sink chopper every other frame */
            if ((FRAME_HW & 1u) == 0u) chopper_y++;

            /* Count down crash timer */
            if (--tim3_val == 0u) {
                /* Timer expired: clear robot, switch to new-player mode */
                if (r_status != STATUS_OFF) {
                    pos_robot();
                    r_status = STATUS_OFF;
                }
                pos_chopper();
                mode = NEW_PLAYER_MODE;
            }
        }
    }

    /* Oscillate angle bit 0 every 4 frames (animation) */
    if ((FRAME_HW & 3u) == 0u)
        chopper_angle ^= 1u;

    /* In GO_MODE: update tile-position, then call pos_chopper */
    if (mode != GO_MODE) return;
    pos_chopper();
    ccxy();
}

/* -----------------------------------------------------------------------
 * UPDATE.ROBOT.CHOPPER — VBlank: clear old robot sprite, draw new, crash
 * ----------------------------------------------------------------------- */
void update_robot_chopper(void)
{
    /* OFF: zero pixel positions (sprite invisible) */
    if (robot_status == STATUS_OFF) {
        robot_x = 0u;
        robot_y = 0u;
    }

    /* Always clear 18 rows from last frame's Y, then redraw */
    {
        uint8_t oy = orobot_y;
        for (int i = 0; i < 18; i++) {
            PLAYER_PL2[oy + (uint8_t)i] = 0u;
            PLAYER_PL3[oy + (uint8_t)i] = 0u;
        }
    }

    /* Draw new sprite using robot_angle shape */
    {
        const uint8_t *shape = chopper_shapes[robot_angle];
        uint8_t y = robot_y;
        orobot_y = y;
        for (int i = 0; i < 18; i++) {
            PLAYER_PL2[y + (uint8_t)i] = shape[i];
            PLAYER_PL3[y + (uint8_t)i] = shape[18 + i];
        }
    }

    /* CRASH: disintegrate, cycle color, handle timer */
    if (r_status == STATUS_CRASH) {
        uint8_t y = robot_y;
        for (int i = 0; i < 18; i++) {
            PLAYER_PL2[y + (uint8_t)i] &= RANDOM_HW;
            PLAYER_PL3[y + (uint8_t)i] &= RANDOM_HW;
        }
        PCOLR2_HW++;
        PCOLR3_HW++;

        if (--tim7_val == 0u) {
            /* Timer expired: robot is done crashing */
            r_status  = STATUS_OFF;
            pos_robot();
            tim7_val  = 255u;  /* long delay before robot can respawn */
        }
    }

    /* Oscillate angle bit 0 every 4 frames */
    if ((FRAME_HW & 3u) == 0u)
        robot_angle ^= 1u;
}

/* -----------------------------------------------------------------------
 * ROCKET.EXP (static) — after explosion delay, restore saved map tile
 * ----------------------------------------------------------------------- */
static void rocket_exp(void)
{
    for (int x = 2; x >= 0; x--) {
        if (rocket_status[x] != 7u) continue;     /* not exploding */
        if (--rocket_tim[x] != 0u) continue;      /* still counting down */

        /* Timer expired: restore map tile unless it was EXP or an entity */
        temp1_i = rocket_tempx[x];
        temp2_i = rocket_tempy[x];
        bak2_color = 0u;
        compute_map_adr_i();

        uint8_t saved = rocket_temp[x];
        /* Don't restore if tile was EXP ($20) — already destroyed */
        if (saved != 0x20u) {
            /* Don't restore if tile was an entity in HIT.LIST */
            int in_list = 0;
            for (int i = (int)HIT_LIST_LEN; i >= 0; i--) {
                if (saved == hit_list[i]) { in_list = 1; break; }
            }
            if (!in_list)
                adr1_i_ptr()[0] = saved;  /* restore original tile */
        }

        rocket_status[x] = 0u;  /* → OFF */
    }
}

/* -----------------------------------------------------------------------
 * UPDATE.ROCKETS / CHECK.ROCKET.COL / MOVE.ROCKETS
 * One combined pass: for each rocket x=2..0:
 *   1. Check collision with map (if active)
 *   2. Update sprite (clear old, draw new)
 *   3. Apply motion
 * Then call ROCKET.EXP.
 * ----------------------------------------------------------------------- */
void update_rockets(void)
{
    for (int x = 2; x >= 0; x--) {
        uint8_t status = rocket_status[x];

        /* ---- CHECK.ROCKET.COL ---- */
        if (status != 0u && status != 7u) {
            /* Active rocket: compute map tile at current pixel position */
            uint8_t mx = (uint8_t)(((uint8_t)(rocket_x[x] - 33u) >> 2) + sx);
            temp1_i = mx;
            rocket_tempx[x] = mx;

            /* Y: rocket_y + (sy>=0 ? sy_f&7 : 0) - 94, divided by 8, + max(sy,0) */
            uint8_t sy_fine = ((int8_t)sy >= 0) ? (uint8_t)(sy_f & 7u) : 0u;
            uint8_t my_raw = (uint8_t)((uint8_t)(rocket_y[x] + sy_fine - 94u) >> 3);
            uint8_t sy_base = ((int8_t)sy >= 0) ? sy : 0u;
            uint8_t my = (uint8_t)(sy_base + my_raw);
            temp2_i = my;
            rocket_tempy[x] = my;

            if (check_chr_i()) {
                /* Collision! */
                s3_val = 1u;
                s2_val = 0u;
                uint8_t tile = adr1_i_ptr()[0];
                rocket_temp[x] = tile;

                uint8_t new_status = 7u;  /* default: explode + restore tile */

                if (tile == 0x3Fu) {
                    /* $3F = EXP2 = fort tile: destroy fort only on level 1 */
                    if (level == 1u) {
                        rocket_temp[0] = 0u;
                        rocket_temp[1] = 0u;
                        rocket_temp[2] = 0u;
                        fort_status = STATUS_EXPLODE;
                        new_status = 0u;        /* entity: OFF, tile stays EXP */
                        goto rocket_col_done;
                    }
                    /* Wrong level: fall through to entity/hit-list check */
                }

                if (tile == 0xC7u) {
                    /* $C7 = EXP.WALL: score, place 0 as saved tile (destroys wall) */
                    bak2_color = 0x10u;
                    uint8_t tmpx = (uint8_t)x;
                    inc_score(0x20u, 0x00u);
                    rocket_temp[tmpx] = 0u;   /* saved=0: rocket_exp writes 0 (clears) */
                    new_status = 7u;
                    goto rocket_col_done;
                }

                /* Check HIT.LIST (includes $3F on wrong level via fall-through) */
                for (int i = (int)HIT_LIST_LEN; i >= 0; i--) {
                    if (tile == hit_list[i]) { new_status = 0u; break; }
                }

            rocket_col_done:
                rocket_status[x] = new_status;
                adr1_i_ptr()[0] = 0x20u;    /* place EXP character at hit location */
                rocket_tim[x] = 7u;
            }
        }

        /* ---- MOVE.ROCKETS ---- */
        status = rocket_status[x];

        /* Determine whether to cancel (clear position) */
        int clear_pos = 0;
        if (status == 0u) {
            /* OFF: clear position */
            clear_pos = 1;
        } else if (status != 7u) {
            /* Active: copy shadow SIZEM to hardware, then check bounds */
            SIZEM_HW = ssizem;
            uint8_t rx = rocket_x[x], ry = rocket_y[x];
            if (rx < 4u || rx >= 251u || ry >= (uint8_t)(212u + 18u) || ry < 100u) {
                rocket_status[x] = 0u;
                clear_pos = 1;
            }
        } else {
            /* EXP: clear position (status preserved) */
            clear_pos = 1;
        }

        if (clear_pos) {
            rocket_x[x] = 0u;
            rocket_y[x] = 0xF0u;
        }

        /* Clear old sprite pixels, draw new sprite pixels — always runs */
        {
            uint8_t *mis = PLAYER_MIS;
            uint8_t oy = orocket_y[x];
            uint8_t m1 = rocket1_mask[x];
            mis[oy]          = (uint8_t)(mis[oy]          & m1);
            mis[oy + 1u]     = (uint8_t)(mis[oy + 1u]     & m1);
            mis[oy + 4u]     = (uint8_t)(mis[oy + 4u]     & m1);
            mis[oy + 5u]     = (uint8_t)(mis[oy + 5u]     & m1);

            uint8_t ny = rocket_y[x];
            orocket_y[x] = ny;
            uint8_t m2 = rocket2_mask[x];
            mis[ny]      = (uint8_t)(mis[ny]      | m2);
            mis[ny + 1u] = (uint8_t)(mis[ny + 1u] | m2);

            ssizem = (uint8_t)(ssizem & m1);
            /* Direction 3 (straight down) uses single pixel; others use two */
            if (rocket_status[x] != 3u) {
                ssizem = (uint8_t)(ssizem | rocket3_mask[x]);
                mis[ny + 4u] = (uint8_t)(mis[ny + 4u] | m2);
                mis[ny + 5u] = (uint8_t)(mis[ny + 5u] | m2);
            }
        }

        /* Move if active (status 1-5) */
        status = rocket_status[x];
        if (status != 0u && status != 7u) {
            rocket_x[x] = (uint8_t)((int8_t)rocket_x[x] + rocket_dx[status - 1u]);
            rocket_y[x] = (uint8_t)((int8_t)rocket_y[x] + rocket_dy[status - 1u]);
        }
    }

    rocket_exp();
}

/* -----------------------------------------------------------------------
 * VERTBLKD — deferred VBlank interrupt handler
 * Reads collision registers, runs sprite updates, runs game logic.
 * Returns by jumping to VVBLKD.RET ($E462); here a plain return suffices.
 * ----------------------------------------------------------------------- */
void vertblkd(void)
{
    ATTRACT_HW = 0u;  /* prevent Atari attract mode from activating */

    /* Compute CHOPPER.COL: collisions involving players 0+1 (chopper)
     * Upper nibble = missile/player hits; lower nibble = playfield hits */
    chopper_col = (uint8_t)(
        (uint8_t)(((M2PL_HW | M3PL_HW) & 0x03u) | P0PL_HW | P1PL_HW) << 4
        | P0PF_HW | P1PF_HW);

    /* Compute ROBOT.COL: collisions involving players 2+3 (robot) */
    robot_col = (uint8_t)(
        ((M0PL_HW | M1PL_HW) & 0x0Cu)
        | P2PL_HW | P2PF_HW | P3PL_HW | P3PF_HW);

    HITCLR_HW = 0u;    /* clear all collision latches */

    do_numbers();
    draw_map();
    update_chopper();
    update_robot_chopper();
    read_trig();
    do_exp();

    if (mode == GO_MODE) {
        do_robot_chopper();
        update_rockets();
        do_laser_1();
        do_laser_2();
        do_blocks();
        do_elevator();
        do_chopper();
        robot_brains();
        read_stick();
    }
    /* Return; OS VBlank dispatch (VVBLKD.RET at $E462) resumes from here */
}
