/* fort5.c — Slave AI, fuel base, scanner, fort explosion, DLI, sounds
 * (converted from fort5.s)
 */

#include <stdint.h>
#include "fort1.h"
#include "fort5.h"
#include "fnt1.h"   /* POS_MASK1, FORT_EX1-4 */

/* From fnt1.h: constants — POS_MASK1; data — FORT_EX1, FORT_EX2, FORT_EX3, FORT_EX4. */

/* From fort1.h: variables — adr1_lo, adr1_hi, adr2_lo, adr2_hi, temp1-6, mode, level,
 * fort_status, laser_status, fuel_status, fuel1, fuel2, fuel_temp, bonus1, bonus2,
 * chopper_status, chop_x, chop_y, sx, sy, bak_color, bak2_color, robot_x, slave_status[8],
 * slave_x[8], slave_y[8], slave_dx[8], slave_num, slaves_left, slaves_saved, rocket_x[3],
 * tim4_val, tim9_val, s1_1_val, s1_2_val, s2_val, s3_val, s4_val, s5_val, s6_val,
 * scan_adr1_lo, scan_adr1_hi, scan_adr2_lo, scan_adr2_hi, land_x, land_y, land_fx, land_fy,
 * land_chop_x, land_chop_y, land_chop_angle.
 * Functions — compute_map_adr, save_pos, give_bonus, clear_info, clear_sounds, wait_frame,
 * print, inc_score, do_sounds. */

/* -----------------------------------------------------------------------
 * FORT.H items also declared locally in this file:
 *   Variables:  adr1_lo, adr1_hi, adr2_lo, adr2_hi,
 *               temp1, temp2, temp3, temp4, temp5, temp6,
 *               tim4_val, tim9_val,
 *               s1_1_val, s1_2_val, s2_val, s3_val, s4_val, s5_val, s6_val,
 *               slave_status[], slave_x[], slave_y[], slave_dx[]
 *   Constants:  PLAY_SCRN, CHR_SET2, SCANNER_BASE,
 *               S_LINE1, S_LINE2, S_LINE3, WINDOW_1
 *   Aliases:    TILE_EXP=CHR_EXP, TILE_MISS_L=CHR_MISS_LEFT, TILE_MISS_R=CHR_MISS_RIGHT
 *   HW aliases: REG(); FRAME_HW=FRAME, RANDOM_HW=RANDOM,
 *               AUDF1_HW..AUDF4_HW=AUDF1..AUDF4, AUDC1_HW..AUDC4_HW=AUDC1..AUDC4,
 *               HPOSP0_HW=HPOSP0, HPOSP2_HW=HPOSP2, HPOSP3_HW=HPOSP3,
 *               COLBK_HW=COLBK, COLPF0_HW..COLPF3_HW=COLPF0..COLPF3,
 *               CHBASE_HW=CHBASE, WSYNC_HW=WSYNC,
 *               VDSLST_LO=VDSLST_L, VDSLST_HI=VDSLST_H
 * ----------------------------------------------------------------------- */


/* -----------------------------------------------------------------------
 * Hardware register access
 * ----------------------------------------------------------------------- */
#define REG(a)       (*(volatile uint8_t *)(uintptr_t)(a))
#define FRAME_HW     REG(0x0014)
#define RANDOM_HW    REG(0xD20A)
#define AUDF1_HW     REG(0xD200)
#define AUDC1_HW     REG(0xD201)
#define AUDF2_HW     REG(0xD202)
#define AUDC2_HW     REG(0xD203)
#define AUDF3_HW     REG(0xD204)
#define AUDC3_HW     REG(0xD205)
#define AUDF4_HW     REG(0xD206)
#define AUDC4_HW     REG(0xD207)
#define HPOSP0_HW    REG(0xD000)
#define HPOSP2_HW    REG(0xD002)
#define HPOSP3_HW    REG(0xD003)
#define COLBK_HW     REG(0xD01A)
#define COLPF0_HW    REG(0xD016)
#define COLPF1_HW    REG(0xD017)
#define COLPF2_HW    REG(0xD018)
#define COLPF3_HW    REG(0xD019)
#define CHBASE_HW    REG(0xD409)
#define WSYNC_HW     REG(0xD40A)
#define VDSLST_LO    REG(0x0200)
#define VDSLST_HI    REG(0x0201)

/* -----------------------------------------------------------------------
 * Fixed memory addresses
 * ----------------------------------------------------------------------- */
#define SCANNER_BASE  ((uint8_t *)0x39C0u)
#define CHR_SET2      ((uint8_t *)0x0C00u)
#define WINDOW_1      (CHR_SET2 + 712u)
#define PLAY_SCRN     ((uint8_t *)0x0300u)
#define S_LINE1       ((uint8_t *)0x0AE0u)   /* CHR.SET1 + 736 */
#define S_LINE2       ((uint8_t *)0x0B40u)   /* CHR.SET1 + 832 */
#define S_LINE3       ((uint8_t *)0x0BA0u)   /* CHR.SET1 + 928 */

/* Character constants */
#define TILE_EMPTY    0x48u   /* slave floor tile (free to walk on) */
#define TILE_ERASE_T  0x1Fu   /* written to top tile on slave erase */
#define TILE_EXP      0x20u   /* explosion character */
#define TILE_MISS_L   0x71u   /* missile left character */
#define TILE_MISS_R   0x72u   /* missile right character */

/* -----------------------------------------------------------------------
 * Extern variable declarations
 * ----------------------------------------------------------------------- */
extern uint8_t adr1_lo, adr1_hi;
extern uint8_t adr2_lo, adr2_hi;
extern uint8_t temp1, temp2, temp3, temp4, temp5, temp6;
extern uint8_t mode;
extern uint8_t level;
extern uint8_t fort_status;
extern uint8_t laser_status;
extern uint8_t fuel_status;
extern uint8_t fuel1, fuel2;
extern uint8_t fuel_temp;
extern uint8_t bonus1, bonus2;
extern uint8_t chopper_status;
extern uint8_t chop_x, chop_y;
extern uint8_t sx, sy;
extern uint8_t bak_color, bak2_color;
extern uint8_t robot_x;
extern uint8_t slave_status[8];
extern uint8_t slave_x[8];
extern uint8_t slave_y[8];
extern uint8_t slave_dx[8];
extern uint8_t slave_num;
extern uint8_t slaves_left;
extern uint8_t slaves_saved;
extern uint8_t rocket_x[3];
extern uint8_t tim4_val;
extern uint8_t tim9_val;
extern uint8_t s1_1_val, s1_2_val;
extern uint8_t s2_val, s3_val, s4_val, s5_val, s6_val;
extern uint8_t scan_adr1_lo, scan_adr1_hi;
extern uint8_t scan_adr2_lo, scan_adr2_hi;
extern uint8_t land_x, land_y, land_fx, land_fy;
extern uint8_t land_chop_x, land_chop_y, land_chop_angle;

/* -----------------------------------------------------------------------
 * External function declarations
 * ----------------------------------------------------------------------- */
extern void compute_map_adr(void);
extern void save_pos(void);
extern void give_bonus(void);
extern void clear_info(void);
extern void clear_sounds(void);
extern void wait_frame(uint8_t n);
extern void print(void);           /* temp1=col, temp2=row, adr1=string */
extern void inc_score(uint8_t hi, uint8_t lo);

/* -----------------------------------------------------------------------
 * Static data tables
 * ----------------------------------------------------------------------- */

/* Slave animation characters (2 frames each) */
static const uint8_t slave_chr_tl[2] = { 0x4A, 0x4A };  /* top-left  */
static const uint8_t slave_chr_tr[2] = { 0x49, 0x49 };  /* top-right */
static const uint8_t slave_chr_bl[2] = { 0x3E, 0x3D };  /* bot-left  */
static const uint8_t slave_chr_br[2] = { 0x3B, 0x3C };  /* bot-right */

/* LAND.CHR — landing-pad tile characters; same bytes as SLAVE.CHR.B.L+B.R+0x44
 * fort3.c iterates this array to decide whether a tile is a safe landing tile. */
const uint8_t land_chr[5] = { 0x3E, 0x3D, 0x3B, 0x3C, 0x44 };

/* String: "MEN  TO  RESCUE" (inverted) + 0xFF terminator */
static const uint8_t slave_pickup_mess[] = {
    0xCD, 0xC5, 0xCE,  /* MEN */
    0x20, 0x20,         /*     */
    0xD4, 0xCF,         /* TO  */
    0x20, 0x20,         /*     */
    0xD2, 0xC5, 0xD3, 0xC3, 0xD5, 0xC5,  /* RESCUE */
    0xFF
};

/* String: "LOW  ON  FUEL" (inverted) + 0xFF terminator */
static const uint8_t warning_msg[] = {
    0xCC, 0xCF, 0xD7,  /* LOW  */
    0x20, 0x20,
    0xCF, 0xCE,         /* ON   */
    0x20, 0x20,
    0xC6, 0xD5, 0xC5, 0xCC,  /* FUEL */
    0xFF
};

/* Fuel base shape: 9 rows × 6 bytes.  draw_base writes rows [temp3]..[temp3+4]. */
static const uint8_t base_shape_flat[54] = {
    0x00,0x00,0x00,0x00,0x00,0x00,  /* row 0 */
    0x00,0x00,0x00,0x00,0x00,0x00,  /* row 1 */
    0x00,0x00,0x00,0x00,0x00,0x00,  /* row 2 */
    0x00,0x00,0x00,0x00,0x00,0x00,  /* row 3 */
    0x44,0x44,0x44,0x44,0x44,0x44,  /* row 4 */
    0x55,0x58,0x58,0x58,0x58,0x56,  /* row 5 */
    0x55,0x26,0x35,0x25,0x2C,0x56,  /* row 6 */
    0x55,0x58,0x58,0x58,0x58,0x56,  /* row 7 */
    0x54,0x00,0x00,0x00,0x00,0x54,  /* row 8 */
};

/* -----------------------------------------------------------------------
 * Static helpers
 * ----------------------------------------------------------------------- */

/* Set adr1 to the map address for slave[idx] using compute_map_adr */
static void get_slave_adr(uint8_t idx) {
    temp1 = slave_x[idx];
    temp2 = slave_y[idx];
    compute_map_adr();
}

/* fe: empty return (FE label in asm = just RTS) */
static void fe(void) { }

/* Draw fuel base at (temp1, temp2); shape rows starting at temp3*6 */
static void draw_base(void) {
    compute_map_adr();
    const uint8_t *shape = base_shape_flat + (uint16_t)temp3 * 6u;
    for (int8_t cnt = 4; cnt >= 0; cnt--) {
        uint8_t *dst = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
        for (uint8_t col = 0u; col < 6u; col++) dst[col] = *shape++;
        adr1_hi++;
    }
}

/* DF1: draw base for current fuel_temp step, then decrement fuel_temp */
static void df1(void) {
    temp2 = 11u;
    temp3 = fuel_temp;
    if (level == 1u) {
        temp1 = 0x82u;
        draw_base();
    } else {
        temp1 = 23u;
        draw_base();
        temp1 = 0xECu + 2u;   /* = 238 */
        draw_base();
    }
    fuel_temp--;
}

/* F1: final refuel descent phase */
static void f1(void) {
    if (chop_y >= 13u) {
        s4_val = 1u;
    } else {
        s4_val = 0u;
        AUDC2_HW = 0u;
    }
    if (chop_y >= 10u) return;
    fuel_status = STATUS_FULL;
    fuel_temp = 4u;
    df1();
    save_pos();
}

/* RE.FUEL: per-frame refuel timer handler */
static void re_fuel(void) {
    tim4_val--;
    if (tim4_val) { fe(); return; }
    tim4_val = 1u;
    if ((int8_t)fuel_temp < 0) { f1(); return; }
    df1();
}

/* Erase slave from map: write TILE_EMPTY (bottom) and TILE_ERASE_T (top) */
static void s_erase(uint8_t idx) {
    get_slave_adr(idx);
    uint8_t *bot = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
    *bot = TILE_EMPTY;
    adr1_hi--;
    uint8_t *top = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
    *top = TILE_ERASE_T;
}

/* Move slave one step; retry if destination is not TILE_EMPTY (wall bounce) */
static void s_move(uint8_t idx) {
    for (;;) {
        uint8_t dx = slave_dx[idx];
        if (dx & 0x80u) {
            /* moving left: DEC dx, normalize, move x left */
            slave_dx[idx] = (uint8_t)(((uint8_t)(dx - 1u) & 0x01u) | 0xF0u);
            slave_x[idx]--;
        } else {
            /* moving right: INC dx, normalize, move x right */
            slave_dx[idx] = (uint8_t)(((uint8_t)(dx + 1u) & 0x01u) | 0x10u);
            slave_x[idx]++;
        }
        get_slave_adr(idx);
        const uint8_t *tile = (const uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
        if (*tile == TILE_EMPTY) break;
        /* collision: flip direction bits 7:5 and retry (naturally bounces back) */
        slave_dx[idx] ^= 0xE0u;
    }
}

/* Draw slave sprite: select chars from dx direction and frame bits */
static void s_draw(uint8_t idx) {
    get_slave_adr(idx);
    uint8_t dx = slave_dx[idx];
    uint8_t frame = dx & 0x03u;
    uint8_t *bot = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
    if (dx & 0x80u) {
        *bot = slave_chr_bl[frame];
        adr1_hi--;
        uint8_t *top = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
        *top = slave_chr_tl[frame];
    } else {
        *bot = slave_chr_br[frame];
        adr1_hi--;
        uint8_t *top = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
        *top = slave_chr_tr[frame];
    }
}

/* Multiply val by 40; result lo byte → temp1, hi byte → temp2 */
static void mult_by_40(uint8_t val) {
    uint16_t r = (uint16_t)val * 40u;
    temp1 = (uint8_t)(r & 0xFFu);
    temp2 = (uint8_t)(r >> 8u);
}

/* Copy 8 rows × 12 bytes from map (scan_adr1) to scanner char data (scan_adr2).
 * Each source byte is written at stride-8 in the destination (one scanline per
 * character cell).  Updates scan_adr1/adr1 (+40 per row) and scan_adr2/adr2 (+1).
 */
static void do_line(void) {
    for (int8_t row = 7; row >= 0; row--) {
        uint16_t src = (uint16_t)scan_adr1_hi << 8 | scan_adr1_lo;
        uint16_t dst = (uint16_t)scan_adr2_hi << 8 | scan_adr2_lo;
        for (uint8_t col = 0u; col < 12u; col++) {
            *(uint8_t *)(uintptr_t)dst = *(const uint8_t *)(uintptr_t)src;
            src++;
            dst += 8u;
        }
        uint16_t sa1 = (uint16_t)((uint16_t)scan_adr1_hi << 8 | scan_adr1_lo) + 40u;
        scan_adr1_lo = (uint8_t)(sa1 & 0xFFu);
        scan_adr1_hi = (uint8_t)(sa1 >> 8u);
        adr1_lo = scan_adr1_lo;
        adr1_hi = scan_adr1_hi;
        uint16_t sa2 = (uint16_t)((uint16_t)scan_adr2_hi << 8 | scan_adr2_lo) + 1u;
        scan_adr2_lo = (uint8_t)(sa2 & 0xFFu);
        scan_adr2_hi = (uint8_t)(sa2 >> 8u);
        adr2_lo = scan_adr2_lo;
        adr2_hi = scan_adr2_hi;
    }
}

/* Fort explosion sequence (called after checksum passes) */
static void next_part1(void) {
    inc_score(0x00u, 0x50u);
    give_bonus();
    mode = STOP_MODE;
    bonus1 = 0x99u;
    bonus2 = 0x99u;
    land_chop_x    = 0x76u;
    land_chop_y    = 0xA0u;
    land_x         = 0x6Eu;
    land_y         = 0x11u;
    land_fx        = 0x07u;
    land_fy        = 0x96u;
    land_chop_angle = 8u;

    /* Clear WINDOW.1 (16 bytes) */
    uint8_t *win = WINDOW_1;
    for (int8_t i = 15; i >= 0; i--) win[i] = 0u;

    /* Local table to avoid static-initializer-from-extern-const issue */
    const uint8_t *fort_exp[4];
    fort_exp[0] = FORT_EX1;
    fort_exp[1] = FORT_EX2;
    fort_exp[2] = FORT_EX3;
    fort_exp[3] = FORT_EX4;

    temp3 = 0u;
    while (temp3 < 4u) {
        temp1 = 121u;
        temp2 = 20u;
        compute_map_adr();
        adr2_lo = (uint8_t)((uintptr_t)fort_exp[temp3] & 0xFFu);
        adr2_hi = (uint8_t)((uintptr_t)fort_exp[temp3] >> 8u);

        /* Expand 6-byte frame × 3 rows × 8-bit-scatter-to-24-chars */
        for (temp4 = 0u; temp4 < 6u; temp4++) {
            for (temp6 = 0u; temp6 < 3u; temp6++) {
                const uint8_t *fd = (const uint8_t *)(uintptr_t)
                    ((uint16_t)adr2_hi << 8 | adr2_lo);
                /* Scatter 8 bits into 8 groups of 3 chars at Y=23..0.
                 * Initial carry = 0 (from ASL TEMP3 with TEMP3=0..3).
                 * ROR TEMP5: bit0→carry; val = carry ? EXP : 0 → 3 chars. */
                uint8_t t5 = fd[temp4];
                uint8_t *row_base = (uint8_t *)(uintptr_t)
                    ((uint16_t)adr1_hi << 8 | adr1_lo);
                for (uint8_t bit = 0u; bit < 8u; bit++) {
                    uint8_t val = (t5 & 1u) ? TILE_EXP : 0u;
                    t5 >>= 1;
                    uint8_t y = (uint8_t)(23u - bit * 3u);
                    row_base[y]       = val;
                    row_base[y - 1u]  = val;
                    row_base[y - 2u]  = val;
                }
                adr1_hi++;   /* advance to next map row */
            }
        }

        /* 16-frame flash-and-sound sequence */
        bak2_color = 0x10u;
        AUDC4_HW   = 0xCFu;
        for (int8_t flash = 15; flash >= 0; flash--) {
            wait_frame(2u);
            bak2_color++;
            s3_val = 1u;
            AUDF4_HW = RANDOM_HW;
        }
        bak2_color = 0u;
        temp3++;
    }

    mode         = GO_MODE;
    fort_status  = STATUS_OFF;
    laser_status = STATUS_OFF;
    clear_sounds();
}

/* ROM checksum $9000-$AFFF: 1 = pass, 0 = fail */
static int rom_checksum_ok(void) {
    uint8_t sum = 0u, ovf = 0u;
    for (uint8_t page = 0x90u; page != 0xB0u; page++) {
        const uint8_t *mem = (const uint8_t *)(uintptr_t)((uint16_t)page << 8u);
        for (uint16_t i = 0u; i < 256u; i++) {
            uint16_t s = (uint16_t)sum + mem[i];
            sum = (uint8_t)(s & 0xFFu);
            if (s > 0xFFu) ovf++;
        }
    }
    return (sum == 0u && ovf == 0u);
}

/* CHECK.FORT falls through to DO.CHECKSUM1, which falls through to NEXT.PART1 */
static void do_checksum1(void) {
    if (!rom_checksum_ok()) { for (;;) {} }
    next_part1();
}

/* -----------------------------------------------------------------------
 * Public: print slave count to PLAY.SCRN
 * ----------------------------------------------------------------------- */
void print_slaves_left(void) {
    adr1_lo = (uint8_t)((uintptr_t)slave_pickup_mess & 0xFFu);
    adr1_hi = (uint8_t)((uintptr_t)slave_pickup_mess >> 8u);
    temp1 = 9u;
    temp2 = 0u;
    print();
    uint8_t a = (uint8_t)(slaves_left | 0x90u);
    PLAY_SCRN[5] = a;
    if (a == 0x90u) a = 0x8Au;
    PLAY_SCRN[6] = (uint8_t)(a & 0x8Fu);
    tim9_val = 90u;
}

/* -----------------------------------------------------------------------
 * Slave collision helpers
 * ----------------------------------------------------------------------- */

/* s_col2: kill slave (erase, set OFF, dec count, print count); returns 1 */
static int s_col2(uint8_t idx) {
    s_erase(idx);
    slave_status[idx] = STATUS_OFF;
    slaves_left--;
    print_slaves_left();
    return 1;
}

/* s_col: check if slave hits a dangerous tile.
 * Returns 1 (hit/killed via s_col2) or 0 (safe to move). */
static int s_col(uint8_t idx) {
    get_slave_adr(idx);
    const uint8_t *bot = (const uint8_t *)(uintptr_t)
        ((uint16_t)adr1_hi << 8 | adr1_lo);
    uint8_t tile = *bot;
    if (tile == 0u || tile == TILE_EXP || tile == TILE_MISS_L || tile == TILE_MISS_R)
        return s_col2(idx);
    /* check row above */
    adr1_hi--;
    const uint8_t *top = (const uint8_t *)(uintptr_t)
        ((uint16_t)adr1_hi << 8 | adr1_lo);
    tile = *top;
    if (tile == 0u || tile == TILE_EXP || tile == TILE_MISS_L || tile == TILE_MISS_R)
        return s_col2(idx);
    return 0;
}

/* -----------------------------------------------------------------------
 * Public functions
 * ----------------------------------------------------------------------- */

void do_checksum2(void) {
    if (!rom_checksum_ok()) { for (;;) {} }
}

void do_checksum3(void) {
    const uint8_t *mem = (const uint8_t *)0xB980u;
    uint8_t sum = 0u;
    for (uint16_t i = 0u; i < 256u; i++) sum = (uint8_t)(sum + mem[i]);
    if (sum != 0u) { for (;;) {} }
}

void move_slaves(void) {
    uint8_t idx = slave_num;
    uint8_t st  = slave_status[idx];

    if (st != STATUS_OFF) {
        if (st == STATUS_PICKUP) {
            /* Chopper rescued this slave: score, silence, tally */
            s_col2(idx);
            AUDC3_HW = 0u;
            inc_score(0u, 8u);
            slaves_saved++;
        } else {
            /* Active slave: check collision then erase/move/redraw */
            if (!s_col(idx)) {
                s_erase(idx);
                s_move(idx);
                s_draw(idx);
            }
        }
    }

    /* Advance slave_num (mod 8) */
    idx++;
    if (idx >= 8u) idx = 0u;
    slave_num = idx;

    /* Slave info display timer */
    if (PLAY_SCRN[5]) {
        tim9_val--;
        if (!tim9_val) clear_info();
    }
}

int pick_up_slave(void) {
    for (int8_t i = 7; i >= 0; i--) {
        uint8_t si = (uint8_t)i;
        if (slave_status[si] == STATUS_OFF) continue;
        /* Check horizontal proximity: |slave_x - chop_x| < 4
         * Uses EOR #$FE on negative result (preserves exact 6502 behaviour). */
        uint8_t dx = (uint8_t)(slave_x[si] - chop_x);
        if (dx & 0x80u) dx ^= 0xFEu;
        if (dx >= 4u) continue;
        uint8_t dy = (uint8_t)(slave_y[si] - chop_y);
        if (dy & 0x80u) dy ^= 0xFEu;
        if (dy >= 4u) continue;
        slave_status[si] = STATUS_PICKUP;
        AUDC3_HW = 0xA8u;
        AUDF3_HW = 32u;
        return 1;
    }
    return 0;
}

void check_fuel_base(void) {
    if (fuel_status == STATUS_REFUEL) { re_fuel(); return; }

    if (chopper_status == STATUS_LAND && chop_y >= 9u && chop_y < 13u) {
        uint8_t cx = chop_x;
        int in_range;
        if (level == 0u)
            in_range = (cx >= 23u && cx < 244u);
        else
            in_range = (cx >= 0x82u && cx < (uint8_t)(0x82u + 6u));
        if (in_range) {
            fuel_status = STATUS_REFUEL;
            tim4_val    = 1u;
            fuel_temp   = 4u;
        }
    }

    /* Warning / status check (always runs after the landing test) */
    if (fuel_status == STATUS_REFUEL) {
        AUDC2_HW = 0u;
        AUDF2_HW = 0x88u;
        clear_info();
        return;
    }
    if (fuel2 != 0u) return;
    /* fuel2 == 0: flash warning */
    if (FRAME_HW & 0x08u) {
        AUDC2_HW = 0xA4u;
        AUDF2_HW = 0x88u;
        clear_info();
    } else {
        temp1    = 9u;
        temp2    = 0u;
        AUDC2_HW = 0xA4u;
        AUDF2_HW = 0xA4u;
        adr1_lo  = (uint8_t)((uintptr_t)warning_msg & 0xFFu);
        adr1_hi  = (uint8_t)((uintptr_t)warning_msg >> 8u);
        print();
    }
}

void set_scanner(void) {
    temp1 = 0u;
    temp2 = 0u;
    if (sy != 0u && !(sy & 0x80u)) {
        uint8_t row = (sy >= 17u) ? 16u : sy;
        mult_by_40(row);
    }
    /* Base address = SCANNER + (sx>>3) + (temp2:temp1) = $39C0 + col + row_offset */
    uint32_t addr = (uint32_t)0x39C0u + (uint32_t)(sx >> 3u)
                  + (uint32_t)temp1 + ((uint32_t)temp2 << 8u);
    adr1_lo = scan_adr1_lo = (uint8_t)(addr & 0xFFu);
    adr1_hi = scan_adr1_hi = (uint8_t)((addr >> 8u) & 0xFFu);

    /* Fill scanner display lines (scan_adr1 continues advancing across calls) */
    scan_adr2_lo = adr2_lo = (uint8_t)((uintptr_t)S_LINE1 & 0xFFu);
    scan_adr2_hi = adr2_hi = (uint8_t)((uintptr_t)S_LINE1 >> 8u);
    do_line();

    scan_adr2_lo = adr2_lo = (uint8_t)((uintptr_t)S_LINE2 & 0xFFu);
    scan_adr2_hi = adr2_hi = (uint8_t)((uintptr_t)S_LINE2 >> 8u);
    do_line();

    scan_adr2_lo = adr2_lo = (uint8_t)((uintptr_t)S_LINE3 & 0xFFu);
    scan_adr2_hi = adr2_hi = (uint8_t)((uintptr_t)S_LINE3 >> 8u);
    do_line();
}

void pos_it(void) {
    /* temp1 = tile_x, temp2 = tile_y (set by caller) */
    uint8_t tx = temp1;   /* save tile_x before mult_by_40 overwrites temp1 */
    mult_by_40(temp2);    /* temp1 = (tile_y*40)&0xFF, temp2 = (tile_y*40)>>8 */
    uint32_t addr = (uint32_t)0x39C0u + 3u + (uint32_t)(tx >> 3u)
                  + (uint32_t)temp1 + ((uint32_t)temp2 << 8u);
    adr2_lo = (uint8_t)(addr & 0xFFu);
    adr2_hi = (uint8_t)((addr >> 8u) & 0xFFu);
    uint8_t *p = (uint8_t *)(uintptr_t)((uint16_t)adr2_hi << 8 | adr2_lo);
    *p ^= POS_MASK1[tx & 7u];
}

void check_fort(void) {
    if (fort_status == STATUS_EXPLODE) do_checksum1();
}

/* -----------------------------------------------------------------------
 * Display-list interrupt handlers (chained: LINE1 → LINE2 → LINE3 → LINE4 → LINE1)
 * ----------------------------------------------------------------------- */

void line1(void) {
    VDSLST_LO = (uint8_t)((uintptr_t)line2 & 0xFFu);
    VDSLST_HI = (uint8_t)((uintptr_t)line2 >> 8u);
    /* 8-line gradient: colour = (x<<1) | $E0 */
    for (uint8_t x = 0u; x < 8u; x++) {
        WSYNC_HW = 0u;
        COLBK_HW = (uint8_t)((x << 1u) | 0xE0u);
    }
    COLBK_HW = 0u;   /* LINEC: clear background */
}

void line2(void) {
    VDSLST_LO = (uint8_t)((uintptr_t)line3 & 0xFFu);
    VDSLST_HI = (uint8_t)((uintptr_t)line3 >> 8u);
    /* Set missile horizontal positions from rocket_x[2..0] → HPOSM0-2 */
    for (int8_t i = 2; i >= 0; i--)
        REG((uint16_t)0xD004u + (uint8_t)i) = rocket_x[(uint8_t)i];
    /* 8-line gradient descending */
    for (int8_t x = 7; x >= 0; x--) {
        WSYNC_HW = 0u;
        COLBK_HW = (uint8_t)(((uint8_t)x << 1u) | 0xE0u);
    }
    COLBK_HW = 0u;
}

void line3(void) {
    VDSLST_LO = (uint8_t)((uintptr_t)line4 & 0xFFu);
    VDSLST_HI = (uint8_t)((uintptr_t)line4 >> 8u);
    HPOSP2_HW = robot_x;
    HPOSP3_HW = (uint8_t)(robot_x + 8u);
    WSYNC_HW  = 0u;
    CHBASE_HW = 0x0Cu;   /* /CHR.SET2 = hi byte of $0C00 */
    COLPF0_HW = bak_color;
    COLPF1_HW = 0x0Au;
    COLPF2_HW = 0x93u;
    COLPF3_HW = FRAME_HW;
    WSYNC_HW  = 0u;
    COLBK_HW  = bak2_color;
}

void line4(void) {
    VDSLST_LO = (uint8_t)((uintptr_t)line1 & 0xFFu);
    VDSLST_HI = (uint8_t)((uintptr_t)line1 >> 8u);
    /* Clear all 8 player/missile horizontal positions ($D000-$D007) */
    for (int8_t i = 7; i >= 0; i--)
        REG((uint16_t)0xD000u + (uint8_t)i) = 0u;
    WSYNC_HW = 0u;
    COLBK_HW = 0u;
    if (mode == STOP_MODE || mode == GO_MODE) do_sounds();
}

/* -----------------------------------------------------------------------
 * Sound subsystem — six channels, all fall-through (S1→S2→S3→S4→S5→S6→RTS)
 * ----------------------------------------------------------------------- */
void do_sounds(void) {
    /* S1: chopper engine (every other 2-frame; freq descends from s1_1/s1_2_val) */
    if (chopper_status != STATUS_OFF && !(FRAME_HW & 2u)) {
        AUDC1_HW = 0x83u;
        uint8_t freq = (s1_1_val & 0x80u) ? s1_2_val : s1_1_val;
        freq -= 4u;
        s1_1_val  = freq;
        AUDF1_HW  = freq;
    }

    /* S2: missile — rising tone from s2_val=$3F down to 0, then silence */
    if (!(s2_val & 0x80u)) {
        uint8_t freq = (uint8_t)((s2_val ^ 0x3Fu) + 16u);
        AUDF2_HW = freq;
        AUDC2_HW = (freq == (uint8_t)(0x3Fu + 16u)) ? 0u : 0x86u;
        s2_val--;
    }

    /* S3: explosion noise — random freq while s3_val active */
    if (s3_val != 0u) {
        uint8_t freq = (uint8_t)((RANDOM_HW & 3u) | s3_val);
        AUDF3_HW = (uint8_t)(freq + 0x10u);
        s3_val++;
        if (s3_val == 0x31u) s3_val = 0u;
        AUDC3_HW = (s3_val != 0u) ? 0x48u : 0u;
    }

    /* S4: refuel — alternating freq; BCD-add 4 to fuel while below max */
    if (s4_val != 0u) {
        uint8_t xv = (FRAME_HW & 7u) ? 0x18u : 0u;
        uint8_t yv = 0u;
        /* CMP #MAX.FUEL ($2000) — carry always set (fuel1 - 0 ≥ 0), so
         * effectively: if fuel2 < $20, fuel is below max → add */
        if (fuel2 < 0x20u) {
            yv = 0xA6u;
            /* BCD add 4 to fuel1/fuel2 (SED/ADC/CLD in asm) */
            uint8_t lo = (uint8_t)((fuel1 & 0x0Fu) + 4u);
            uint8_t c  = 0u;
            if (lo >= 10u) { lo = (uint8_t)(lo - 10u); c = 1u; }
            uint8_t hi = (uint8_t)((fuel1 >> 4u) + c);
            c = 0u;
            if (hi >= 10u) { hi = (uint8_t)(hi - 10u); c = 1u; }
            fuel1 = (uint8_t)((hi << 4u) | lo);
            lo = (uint8_t)((fuel2 & 0x0Fu) + c);
            c  = 0u;
            if (lo >= 10u) { lo = (uint8_t)(lo - 10u); c = 1u; }
            hi = (uint8_t)((fuel2 >> 4u) + c);
            fuel2 = (uint8_t)((hi << 4u) | lo);
        }
        AUDF2_HW = xv;
        AUDC2_HW = yv;
    }

    /* S5: hyper-chamber rising tone */
    if (s5_val != 0u) {
        uint8_t old = s5_val;
        s5_val++;
        uint8_t freq;
        if (old == 0x50u) { s5_val = 0u; freq = 0u; }
        else               freq = old;
        AUDF2_HW = freq;
        AUDC2_HW = 0xA8u;
    }

    /* S6: cruise-missile tone (every other frame) */
    if (!(FRAME_HW & 1u) && s6_val != 0u) {
        uint8_t old = s6_val;
        s6_val++;
        if (old >= 0x20u) s6_val = 0u;
        AUDF4_HW = old;
        AUDC4_HW = 0x07u;
    }
}
