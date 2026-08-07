/* fort1.c - Fort Apocalypse game logic and startup (converted from fort1.s) */

#include <stdint.h>
#include <string.h>
#include "fort1.h"
#include "fort.h"

/*
 * Also declared in fort.h:
 * - Variables:
 *   adr1_lo, adr1_hi, adr2_lo, adr2_hi,
 *   temp1, temp2, temp3, temp4,
 *   temp1_i,
 *   s_adr_lo, s_adr_hi, s_flg,
 *   slave_status, slave_x, slave_y, slave_dx,
 *   pod_status,
 *   tank_start_x, tank_start_y,
 *   game_points, demo_status, demo_count,
 *   tim1_val, tim2_val, tim6_val
 * - Functions:
 *   none
 * - Constants:
 *   REG,
 *   DMACTL, GRACTL, SKCTL, PMBASE, AUDCTL, AUDF1, AUDC1, AUDF2, AUDC2,
 *   WSYNC, VCOUNT, NMIEN, TRIG0, CONSOL,
 *   SDMCTL, PRIOR, SDLST_L, SDLST_H, VDSLST_L, VDSLST_H, VVBLKD_L, VVBLKD_H,
 *   COLOR0, COLOR1, COLOR2, COLOR3, COLOR4, PCOLR0, PCOLR1, CHBAS, FRAME,
 *   PLAY_SCRN, CHR_SET1, CHR_SET2, MAP_BASE, SCANNER_BASE,
 *   RAM1_STUFF, RAM2_STUFF, PACKED_MAP_BASE, PACKED_SCAN_BASE,
 *   WINDOW_1, WINDOW_2,
 *   MAX_TANKS, MAX_PODS
 */


/* -----------------------------------------------------------------------
 * Atari OS and hardware addresses (from fort.s .EQ directives)
 * ----------------------------------------------------------------------- */

/* Direct hardware registers */
#define REG(a)   (*(volatile uint8_t *)(uintptr_t)(a))
#define COLPF3_HW REG(0xD019)  /* COLPF3 hardware (fort.s: COLPF3 = $D019) */
#define RANDOM_HW REG(0xD20A)

/* Helper: install 16-bit interrupt vector */
static inline void set_vector(uint16_t addr, void (*fn)(void))
{
    *(volatile uint8_t *)(uintptr_t)(addr)     = (uint8_t)((uintptr_t)fn & 0xFF);
    *(volatile uint8_t *)(uintptr_t)(addr + 1) = (uint8_t)((uintptr_t)fn >> 8);
}
#define SET_VVBLKD(fn)  set_vector(0x0224, (void(*)(void))(fn))
#define SET_VDSLST(fn)  set_vector(0x0200, (void(*)(void))(fn))
#define SET_SDLST(p)    do { SDLST_L = (uint8_t)((uintptr_t)(p) & 0xFF); \
                             SDLST_H = (uint8_t)((uintptr_t)(p) >> 8); } while(0)

/* -----------------------------------------------------------------------
 * Atari memory map (from fort.s .EQ directives)
 * ----------------------------------------------------------------------- */
#define PLAYER_BASE    ((uint8_t *)0x0000u)   /* player-missile data base */
#define PLAY_SCRN      ((uint8_t *)0x0300u)   /* screen buffer */
#define CHR_SET1       ((uint8_t *)0x0800u)   /* character set 1 */
#define CHR_SET2       ((uint8_t *)0x0C00u)   /* character set 2 */
#define MAP_BASE       ((uint8_t *)0x1103u)   /* game map  ($1100+3) */
#define MAP_END_ADDR   (0x1103u + 0x2800u)
#define SCANNER_BASE   ((uint8_t *)0x39C0u)   /* scanner bitmap */
#define SCANNER_END_ADDR (0x39C0u + 0x0640u)  /* scanner = $640 = 1600 bytes */
#define RAM1_STUFF     ((uint8_t *)0x0C90u)   /* CHR.SET2 + 144 */
#define RAM2_STUFF     ((uint8_t *)0x0100u)
#define PACKED_MAP_BASE ((const uint8_t *)0x8000u)
#define PACKED_SCAN_BASE ((const uint8_t *)(0x8000u + 0x0D34u))

/* -----------------------------------------------------------------------
 * External game variables (defined in fort.s zero-page area / fort7.s)
 * ----------------------------------------------------------------------- */

extern uint8_t mode;
extern uint8_t level;

extern uint8_t score1, score2, score3;
extern uint8_t hi1, hi2, hi3;
extern uint8_t bonus1, bonus2;
extern uint8_t fuel1, fuel2;
extern uint8_t fuel_status;
extern uint8_t fort_status;
extern uint8_t laser_status;
extern uint8_t r_status;

extern uint8_t chop_left;
extern uint8_t chop_x, chop_y;
extern uint8_t chopper_x, chopper_y;
extern uint8_t chopper_angle;
extern uint8_t chopper_col;
extern uint8_t chopper_status;
extern uint8_t land_x, land_y, land_fx, land_fy;
extern uint8_t land_chop_x, land_chop_y, land_chop_angle;
extern uint8_t sx, sy, sx_f, sy_f;

extern uint8_t slaves_left;
extern uint8_t slaves_saved;
extern uint8_t slave_status[8];
extern uint8_t slave_x[8];
extern uint8_t slave_y[8];
extern uint8_t slave_dx[8];
extern uint8_t slave_num;

extern uint8_t pod_status[MAX_PODS];
extern uint8_t pod_num;

extern uint8_t tank_status[MAX_TANKS];
extern uint8_t cm_status[MAX_TANKS];
extern uint8_t tank_start_x[MAX_TANKS];
extern uint8_t tank_start_y[MAX_TANKS];

extern uint8_t game_points;
extern uint8_t demo_status;
extern uint8_t demo_count;
extern uint8_t opt_num;

extern uint8_t pilot_skill;
extern uint8_t grav_skill;
extern uint8_t chops;
extern uint8_t grav_skl;
extern uint8_t laser_spd;
extern uint8_t start_pods;
extern uint8_t robot_spd;
extern uint8_t tank_speed;
extern uint8_t missile_speed;
extern uint8_t elevator_spd;
extern uint8_t elevator_dx;
extern uint8_t elevator_tim;
extern uint8_t tank_spd;
extern uint8_t missile_spd;

extern uint8_t bak_color;

/* Timer variables */
extern uint8_t tim1_val;
extern uint8_t tim2_val;
extern uint8_t tim6_val;

/* -----------------------------------------------------------------------
 * External functions from other fort*.s modules
 * ----------------------------------------------------------------------- */
extern void screen_off(void);
extern void screen_on(void);
extern void print(void);            /* uses temp1=col, temp2=row, adr1=string */
extern void ddig(uint8_t val);      /* display decimal digit; X=count preset */
extern void hover(void);
extern void compute_map_adr(void);  /* uses temp1=x, temp2=y → adr1 */
extern void compute_map_adr_i(void);
extern void save_pos(void);
extern void give_bonus(void);
extern void clear_info(void);
extern void clear_sounds(void);
extern void wait_frame(uint8_t n);
extern void do_checksum2(void);
extern void do_checksum3(void);
extern void print_slaves_left(void);
extern void move_pods(void);
extern void move_tanks(void);
extern void move_cruise_missiles(void);
extern void move_slaves(void);
extern void set_scanner(void);
extern void check_fuel_base(void);
extern void check_fort(void);
extern void check_hyper_chamber(void);
extern void read_user(void);
extern void vertblkd(void);         /* normal VBlank handler (VERTBLKD) */
extern void line1(void);            /* display list interrupt handler */

/* External display list data */
extern const uint8_t dsp_lst3[];

/* External font data pointers (from fnt1.s / fnt2.s) */
extern const uint8_t fnt1_data[];   /* FNT1: 128 chars × 8 bytes */
extern const uint8_t fnt2_data[];   /* FNT2: 128 chars × 8 bytes */

/* Z1/Z2: display list init data (from fort8.s) */
extern const uint8_t z1[];
extern const uint8_t z2[];
extern const uint8_t z1_len;        /* .EQ *-Z1-1 */
extern const uint8_t z2_len;        /* .EQ *-Z2-1 */

/* -----------------------------------------------------------------------
 * Atari screen-code helper
 * S(c): convert ASCII uppercase/symbol to Atari screen code (.AT - encoding)
 * ----------------------------------------------------------------------- */
#define S(c)  ((uint8_t)((unsigned char)(c) - 0x20u))

/* -----------------------------------------------------------------------
 * String constants (from fort1.s .AT directives)
 * Terminated by 0xFF (.HS FF)
 * ----------------------------------------------------------------------- */

/* T.1: "FORT  APOCALYPSE" */
static const uint8_t T_1[] = {
    S('F'),S('O'),S('R'),S('T'),
    0x20,0x20,
    S('A'),S('P'),S('O'),S('C'),S('A'),S('L'),S('Y'),S('P'),S('S'),S('E'),
    0xFF
};

/* T.2: "BY  STEVE  HALES" */
static const uint8_t T_2[] = {
    S('B'),S('Y'),
    0x20,0x20,
    S('S'),S('T'),S('E'),S('V'),S('E'),
    0x20,0x20,
    S('H'),S('A'),S('L'),S('E'),S('S'),
    0xFF
};

/* T.3: "COPYRIGHT" */
static const uint8_t T_3[] = {
    S('C'),S('O'),S('P'),S('Y'),S('R'),S('I'),S('G'),S('H'),S('T'),
    0xFF
};

/* T.4: "SYNAPSE  SOFTWARE" */
static const uint8_t T_4[] = {
    S('S'),S('Y'),S('N'),S('A'),S('P'),S('S'),S('E'),
    0x20,0x20,
    S('S'),S('O'),S('F'),S('T'),S('W'),S('A'),S('R'),S('E'),
    0xFF
};

/* NEW.PILOT: "GET  READY  PILOT" */
static const uint8_t new_pilot[] = {
    S('G'),S('E'),S('T'),
    0x20,0x20,
    S('R'),S('E'),S('A'),S('D'),S('Y'),
    0x20,0x20,
    S('P'),S('I'),S('L'),S('O'),S('T'),
    0xFF
};

/* PILOTS.LEFT: "PILOTS  LEFT" */
static const uint8_t pilots_left[] = {
    S('P'),S('I'),S('L'),S('O'),S('T'),S('S'),
    0x20,0x20,
    S('L'),S('E'),S('F'),S('T'),
    0xFF
};

/* ENTER: "ENTERING" */
static const uint8_t str_enter[] = {
    S('E'),S('N'),S('T'),S('E'),S('R'),S('I'),S('N'),S('G'),
    0xFF
};

/* LVL.1: "VAULTS  OF  DRACONIS" */
static const uint8_t lvl_1[] = {
    S('V'),S('A'),S('U'),S('L'),S('T'),S('S'),
    0x20,0x20,
    S('O'),S('F'),
    0x20,0x20,
    S('D'),S('R'),S('A'),S('C'),S('O'),S('N'),S('I'),S('S'),
    0xFF
};

/* LVL.2: "CRYSTALLINE  CAVES" */
static const uint8_t lvl_2[] = {
    S('C'),S('R'),S('Y'),S('S'),S('T'),S('A'),S('L'),S('L'),S('I'),S('N'),S('E'),
    0x20,0x20,
    S('C'),S('A'),S('V'),S('E'),S('S'),
    0xFF
};

/* G.1: "MISSION" (raw .AT, no '-') */
static const uint8_t G_1[] = { 'M','I','S','S','I','O','N', 0xFF };

/* G.A: "ABORTED" */
static const uint8_t G_A[] = { 'A','B','O','R','T','E','D', 0xFF };

/* G.C: "COMPLETED" */
static const uint8_t G_C[] = { 'C','O','M','P','L','E','T','E','D', 0xFF };

/* G.2: "YOUR  RANK  IS" */
static const uint8_t G_2[] = {
    'Y','O','U','R',0x20,0x20,'R','A','N','K',0x20,0x20,'I','S',
    0xFF
};

/* G.3: "CLASS" (.AT -) */
static const uint8_t G_3[] = { S('C'),S('L'),S('A'),S('S'),S('S'), 0xFF };

/* Rating strings (.AT -) */
static const uint8_t R_1[] = { S('S'),S('P'),S('A'),S('R'),S('R'),S('O'),S('W'), 0xFF };
static const uint8_t R_2[] = { S('C'),S('O'),S('N'),S('D'),S('O'),S('R'), 0xFF };
static const uint8_t R_3[] = { S('H'),S('A'),S('W'),S('K'), 0xFF };
static const uint8_t R_4[] = { S('E'),S('A'),S('G'),S('L'),S('E'), 0xFF };

/* HS: "HIGH  SCORE" */
static const uint8_t HS[] = {
    S('H'),S('I'),S('G'),S('H'),
    0x20,0x20,
    S('S'),S('C'),S('O'),S('R'),S('E'),
    0xFF
};

static const uint8_t *const rating_table[4] = { R_1, R_2, R_3, R_4 };

/* -----------------------------------------------------------------------
 * Data tables
 * ----------------------------------------------------------------------- */

static const uint8_t grav_tab[2]     = { 0x0F, 0x07 };
static const uint8_t robot_tab[3]    = { 3, 1, 0 };
static const uint8_t chop_tab[3]     = { 0x07, 0x09, 0x11 };
static const uint8_t laser_tab[3]    = { 4, 8, 16 };
static const uint8_t pod_tab[3]      = { 12, 25, MAX_PODS - 1 };
static const uint8_t tank_tab[1]     = { 4 };
static const uint8_t missile_tab[3]  = { 3, 2, 1 };
static const uint8_t elevator_tab[3] = { 37 + 25, 37 + 10, 37 + 0 };

/* M.TAB: game-points modifier by gravity skill (stored as signed byte) */
static const int8_t m_tab[3] = { -2, -1, 0 };

/* Level data tables */
static const uint8_t level_color[3]         = { 0x42, 0xC2, 0x42 };
static const uint8_t level_chop_start[3][2] = { {90,100}, {119,100}, {116,170} };
static const uint8_t level_start[3][2]      = { {0x02,0xFF}, {0x6D,0xFF}, {0x6E,0x18} };

static const uint8_t *const scan_info[3] = {
    PACKED_SCAN_BASE + 0,
    PACKED_SCAN_BASE + 0x1E9,
    PACKED_SCAN_BASE + 0,
};

static const uint8_t *const pack_adr[3] = {
    PACKED_MAP_BASE + 0x000,
    PACKED_MAP_BASE + 0x62B,
    PACKED_MAP_BASE + 0x000,
};

static const uint8_t tank_start_x_l1[MAX_TANKS] = { 0x53,0x63,0x90,0xA0,0x59,0xAE };
static const uint8_t tank_start_y_l1[MAX_TANKS] = { 0x12,0x12,0x12,0x12,0x26,0x26 };
static const uint8_t tank_start_x_l2[MAX_TANKS] = { 0x50,0x65,0xA0,0xB5,0x3D,0x54 };
static const uint8_t tank_start_y_l2[MAX_TANKS] = { 0x0C,0x0C,0x0C,0x0C,0x26,0x26 };

/* RLE decompressor character tables (CHR1 / CHR2) */
#define CHR1_LEN 23
static const uint8_t chr1[CHR1_LEN] = {
    /* .AT " a./01*+,-#'?st" */
    0x20,0x61,0x2E,0x2F,0x30,0x31,0x2A,0x2B,0x2C,0x2D,0x23,0x27,0x3F,0x73,0x74,
    /* .HS 41444858595AD8 */
    0x41,0x44,0x48,0x58,0x59,0x5A,0xD8,
    /* .DA #$47+128 */
    0xC7
};
#define CHR2_LEN 4
static const uint8_t chr2[CHR2_LEN] = { 0x00, 0x55, 0xAA, 0xFF };

/* -----------------------------------------------------------------------
 * PRINT helper: set position then call print()
 * temp1=col, temp2=row, adr1 must point to the string
 * ----------------------------------------------------------------------- */
static inline void print_str(uint8_t col, uint8_t row, const uint8_t *str)
{
    temp1 = col;
    temp2 = row;
    /* adr1 is a 16-bit pointer in zero page; set both bytes */
    adr1_lo = (uint8_t)((uintptr_t)str & 0xFF);
    adr1_hi = (uint8_t)((uintptr_t)str >> 8);
    print();
}

/* -----------------------------------------------------------------------
 * INC.CHR  — advance temp3 through chars $3B..$3D, wrap at $3E → $3B
 * ----------------------------------------------------------------------- */
static void inc_chr(void)
{
    temp3++;
    if (temp3 == 0x3E)
        temp3 = 0x3B;
}

/* -----------------------------------------------------------------------
 * GET.BYTE — read one byte from [adr1], advance adr1
 * ----------------------------------------------------------------------- */
static uint8_t get_byte(void)
{
    uint8_t *p = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
    uint8_t b = *p++;
    adr1_lo = (uint8_t)((uintptr_t)p & 0xFF);
    adr1_hi = (uint8_t)((uintptr_t)p >> 8);
    return b;
}

/* -----------------------------------------------------------------------
 * UNPACK — RLE decompressor
 * Source:  adr1  (read pointer)
 * Dest:    adr2  (write pointer)
 * End:     temp3:temp2  (16-bit end address, exclusive)
 * Charset: temp4 == 0 → CHR1,  temp4 != 0 → CHR2
 * ----------------------------------------------------------------------- */
void unpack(void)
{
    uint8_t *end = (uint8_t *)(uintptr_t)((uint16_t)temp3 << 8 | temp2);
    uint8_t *dst = (uint8_t *)(uintptr_t)((uint16_t)adr2_hi << 8 | adr2_lo);

    while (dst < end) {
        uint8_t b = get_byte();
        uint8_t count = 1;

        if (temp4 == 0) {
            for (uint8_t y = 0; y < CHR1_LEN; y++) {
                if (b == chr1[y]) { count = get_byte(); break; }
            }
        } else {
            for (uint8_t y = 0; y < CHR2_LEN; y++) {
                if (b == chr2[y]) { count = get_byte(); break; }
            }
        }
        for (uint8_t x = count; x != 0; x--)
            *dst++ = b;
    }
    /* write back adr2 */
    adr2_lo = (uint8_t)((uintptr_t)dst & 0xFF);
    adr2_hi = (uint8_t)((uintptr_t)dst >> 8);
}

/* -----------------------------------------------------------------------
 * INC.GAME.POINTS — add A to game_points
 * ----------------------------------------------------------------------- */
void inc_game_points(uint8_t n)
{
    game_points += n;
}

/* -----------------------------------------------------------------------
 * T.1 VBlank handler — title screen animation + demo/start detection
 * Installed as VVBLKD during the title screen.
 * ----------------------------------------------------------------------- */
void t2_vblank(void)
{
    /* Rotate COLOR0/1/2 every 4 frames */
    if ((FRAME & 3) == 0) {
        uint8_t tmp = COLOR2;
        COLOR2 = COLOR1;
        COLOR1 = COLOR0;
        COLOR0 = tmp;
    }

    /* Increment intro timer every 8 frames */
    if ((FRAME & 7) == 0)
        temp1_i++;

    /* Update title sound */
    AUDC1 = 0xAF;
    AUDC2 = 0xAF;
    AUDF1 = 0xFF - temp1_i;
    AUDF2 = (0xFF - temp1_i) - 1;

    /* Check for demo timeout ($F3 = 243 frames) */
    if (temp1_i == 0xF3) {
        demo_status = (uint8_t)-1;  /* MAIN loop will set mode=START_MODE next frame */
        t3_game_start();
        return;
    }

    /* Check for joystick trigger or console button press */
    uint8_t new_mode = START_MODE;
    if (TRIG0 != 0) {               /* trigger NOT pressed (TRIG0==0 = pressed) */
        uint8_t con = CONSOL;
        if (con != 6) {             /* START button not pressed */
            if (con == 7) return;   /* no console buttons: do nothing */
            new_mode = OPTION_MODE; /* SELECT or OPTION button */
        }
    }
    mode = new_mode;
    opt_num = 0;
    demo_status = 1;
    t3_game_start();
}

/* -----------------------------------------------------------------------
 * T1 loop — title screen raster-color animation (infinite loop)
 * Runs synchronously while waiting for input; replaced by T3 on start.
 * ----------------------------------------------------------------------- */
void t1_loop(void)
{
    for (;;) {
        uint8_t v = (uint8_t)(VCOUNT << 1);
        (void)WSYNC;         /* write triggers horizontal blank wait */
        COLPF3_HW = v;
    }
}

/* -----------------------------------------------------------------------
 * TITLE — draw and animate the title screen, then enter T1 loop
 * ----------------------------------------------------------------------- */
void title(void)
{
    /* Reset stack (not meaningful in C, but preserves the flow comment) */

    /* Title colors */
    COLOR0 = 0x43;
    COLOR1 = 0x0F;
    COLOR2 = 0x83;

    screen_off();

    /* Install display list DSP.LST3 */
    SET_SDLST(dsp_lst3);

    /* Fill first row (40 chars) with animated chars */
    temp3 = 0x3B;
    temp1_i = 0;
    for (uint8_t y = 0; y < 40; y++) {
        PLAY_SCRN[y] = temp3;
        inc_chr();
    }

    /* Fill subsequent rows using indirect ADR1 pointer, 18 rows */
    uint8_t *row_ptr = PLAY_SCRN + 39;
    temp3 = 0x3B;
    for (int x = 17; x >= 0; x--) {
        row_ptr[0] = temp3;
        inc_chr();
        row_ptr[1] = temp3;
        row_ptr += 40;
    }

    /* Install T2 as VBlank handler */
    SET_VVBLKD(t2_vblank);

    /* Print title text */
    print_str(5, 4, T_1);       /* FORT  APOCALYPSE */
    print_str(6, 6, T_2);       /* BY  STEVE  HALES */
    print_str(6, 10, T_3);      /* COPYRIGHT        */

    /* Copy T.5 (8 bytes from fnt1 CHR $20) to PLAY_SCRN+426 */
    const uint8_t *t5 = &fnt1_data[0x20 * 8];
    for (int xi = 7; xi >= 0; xi--)
        PLAY_SCRN[426 + xi] = t5[xi];

    print_str(4, 12, T_4);      /* SYNAPSE  SOFTWARE */

    /* Enter raster-color animation loop (never returns) */
    t1_loop();
}

/* -----------------------------------------------------------------------
 * T3 — initialize game colors and VBlank, then enter MAIN loop
 * ----------------------------------------------------------------------- */
void t3_game_start(void)
{
    COLOR1 = 0x0A;   /* laser block */
    COLOR2 = 0x94;   /* lasers, house */
    COLOR3 = 0x9A;   /* letters */

    /* Install normal VBlank handler */
    SET_VVBLKD(vertblkd);

    screen_off();

    /* Wait for start of frame (VCOUNT == 0) */
    while (VCOUNT != 0)
        ;

    /* Enable VBlank NMI */
    NMIEN = 0xC0;

    main_loop();
}

/* -----------------------------------------------------------------------
 * MAIN — main game loop
 * ----------------------------------------------------------------------- */
void main_loop(void)
{
    for (;;) {
        /* In GO.MODE: run game systems each frame */
        if (mode == GO_MODE) {
            move_pods();
            move_tanks();
            move_cruise_missiles();
            move_slaves();
            set_scanner();
            check_fuel_base();
            check_fort();
            check_level();
        }

        check_hyper_chamber();
        check_modes();
        read_user();

        /* Demo mode: count up to 0, then restart title */
        if ((int8_t)demo_status < 0) {
            demo_status++;          /* INC DEMO.STATUS → 0 */
            mode = START_MODE;
        }

        /* Title/Option mode: skip every other group of frames, then restart */
        if (mode == TITLE_MODE || mode == OPTION_MODE) {
            if ((FRAME & 0x04) != 0) {
                if (--tim6_val == 0) {
                    title();        /* restart title (never returns) */
                }
            }
        }
    }
}

/* -----------------------------------------------------------------------
 * CHECK.LEVEL — dispatch to per-level completion check
 * ----------------------------------------------------------------------- */
void check_level(void)
{
    if (level == 0) { do_level_1(); return; }
    if (level == 1) { do_level_2(); return; }
    do_level_3();
}

/* -----------------------------------------------------------------------
 * PSL — jump to print-slaves-left message
 * (inline: call the function and return)
 * ----------------------------------------------------------------------- */
#define PSL()  do { print_slaves_left(); return; } while(0)

/* -----------------------------------------------------------------------
 * DO.LEVEL.1 — check landing conditions for level 0 completion
 * ----------------------------------------------------------------------- */
void do_level_1(void)
{
    if (chopper_status != STATUS_LAND) return;
    if (chop_y < 35)   return;
    if (chop_x < 130)  return;
    if (chop_x >= 130 + 6 + 1) return;
    if (slaves_left)   PSL();

    level++;    /* = 1 */
    give_bonus();
    clear_info();
    clear_sounds();
    mode = STOP_MODE;

    /* Compute ramp position: col=130, row=40 */
    temp1 = 130;
    temp2 = 40;
    compute_map_adr();
    temp3 = adr1_lo;
    temp4 = adr1_hi;

    /* Animate ramp down 3 times */
    for (uint8_t i = 3; i != 0; i--) {
        move_ramp();
        for (int y = 5; y >= 0; y--) {
            wait_frame(5);
            hover();
            chopper_y++;
        }
        /* Restore adr1 from temp3/temp4 */
        adr1_lo = temp3;
        adr1_hi = temp4;
    }

    mode = NEW_LEVEL_MODE;
}

/* -----------------------------------------------------------------------
 * MOVE.RAMP — scroll 5 rows up at the current adr1/adr2 position
 * ----------------------------------------------------------------------- */
void move_ramp(void)
{
    uint8_t *dst = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
    for (int x = 4; x >= 0; x--) {
        uint8_t *src = dst - 256;   /* DEY on adr2+1 → one page up */
        for (int y = 5; y >= 0; y--)
            dst[y] = src[y];
        dst -= 256;
    }
    /* write back adr1 (decremented by 5 pages) */
    adr1_hi -= 5;
}

/* -----------------------------------------------------------------------
 * DO.LEVEL.2 — level 1 → 2 transition: fly out of fort from above
 * ----------------------------------------------------------------------- */
void do_level_2(void)
{
    if (fort_status != STATUS_OFF) return;
    if (chop_y >= 2)  return;
    if (chop_x < 130) return;
    if (chop_x >= 130 + 4 + 1) return;
    if (slaves_left) PSL();

    level++;    /* = 2 */
    give_bonus();
    mode = NEW_LEVEL_MODE;
}

/* -----------------------------------------------------------------------
 * DO.LEVEL.3 — level 2 → 3 transition: land anywhere at top
 * ----------------------------------------------------------------------- */
void do_level_3(void)
{
    if (chopper_status != STATUS_LAND) return;
    if (chop_y >= 13) return;
    if (chop_x < 0x17) return;
    if (chop_x >= 0xF4) return;

    give_bonus();
    level++;    /* = 3 */
    mode = GAME_OVER_MODE;
}

/* -----------------------------------------------------------------------
 * CHECK.MODES — dispatch from MODE to game-state entry routines
 * ----------------------------------------------------------------------- */
void check_modes(void)
{
    switch (mode) {
    case START_MODE:      m_start();      break;
    case GAME_OVER_MODE:  m_game_over();  break;
    case NEW_LEVEL_MODE:  m_new_level();  break;
    case NEW_PLAYER_MODE: m_new_player(); break;
    default:              break;
    }
}

/* -----------------------------------------------------------------------
 * M.START — initialize a new game
 * ----------------------------------------------------------------------- */
void m_start(void)
{
    screen_off();

    level       = 0;
    score1      = 0;
    score2      = 0;
    score3      = 0;
    bonus1      = 0;
    bonus2      = 0;
    tim1_val    = 0;
    fuel1       = 0;
    fuel2       = 0;
    game_points = 0;
    slaves_saved = 0;

    elevator_tim = 1;
    tank_spd     = 1;
    missile_spd  = 1;
    tim2_val     = 128;

    fort_status  = STATUS_ON;
    laser_status = STATUS_ON;
    fuel_status  = STATUS_EMPTY;
    r_status     = STATUS_OFF;

    grav_skl = grav_tab[grav_skill];

    /* Chopper lives from difficulty table; demo overrides to 2 */
    uint8_t lives = chop_tab[chops];
    if (demo_status != 0)
        lives = 2;
    /* PROT: STA MAIN — omitted */
    chop_left = lives;

    laser_spd    = laser_tab[pilot_skill];
    start_pods   = pod_tab[pilot_skill];
    robot_spd    = robot_tab[pilot_skill];
    tank_speed   = tank_tab[0];            /* only one tank-tab entry */
    missile_speed = missile_tab[pilot_skill];
    elevator_spd = elevator_tab[pilot_skill];

    /* Clear window buffers */
    for (int i = 7; i >= 0; i--) {
        WINDOW_1[i] = 0;
        WINDOW_2[i] = 0;
    }

    /* Randomise which window half is filled with $55 */
    if ((int8_t)RANDOM_HW < 0) {
        for (int i = 7; i >= 0; i--) WINDOW_2[i] = 0x55;
    } else {
        for (int i = 7; i >= 0; i--) WINDOW_1[i] = 0x55;
    }

    mode = NEW_LEVEL_MODE;
}

/* -----------------------------------------------------------------------
 * M.NEW.PLAYER — set up a new pilot (decrement lives, restore position)
 * ----------------------------------------------------------------------- */
void m_new_player(void)
{
    screen_off();

    /* Decrement CHOP.LEFT in BCD (SED; SEC; SBC #1; CLD) */
    if ((chop_left & 0x0F) == 0) {
        if ((chop_left & 0xF0) == 0)
            chop_left = 0x99;                         /* BCD 00 - 1 = 99 */
        else
            chop_left = (uint8_t)((chop_left - 0x10) | 0x09); /* borrow from tens */
    } else {
        chop_left--;
    }

    if (chop_left == 0x99) {
        mode = GAME_OVER_MODE;
        return;
    }

    PCOLR0 = 0x1F;   /* chopper sprite color */
    PCOLR1 = 0x1F;

    /* Refuel on empty */
    if (fuel_status == STATUS_EMPTY) {
        fuel_status = STATUS_FULL;
        fuel1 = 0;
        fuel2 = 1;
    }

    /* Display "GET READY PILOT" message */
    print_str(4, 8, new_pilot);
    print_str(5, 10, pilots_left);

    /* Display pilots-left digit */
    uint8_t *sadr = PLAY_SCRN + 428;
    s_adr_lo = (uint8_t)((uintptr_t)sadr & 0xFF);
    s_adr_hi = (uint8_t)((uintptr_t)sadr >> 8);
    s_flg = 0;
    demo_count = 0;
    ddig(chop_left);

    do_checksum2();
    wait_frame(75);

    /* Restore ship position from last landing point */
    sx   = land_x;
    sy   = land_y;
    sx_f = land_fx;
    sy_f = land_fy;
    chopper_x     = land_chop_x;
    chopper_y     = land_chop_y;
    chopper_angle = land_chop_angle;
    chopper_col   = 0;

    screen_on();
    chopper_status = STATUS_BEGIN;
    mode = GO_MODE;
}

/* -----------------------------------------------------------------------
 * MAKE.CONTURE — replace 's'/$73 and 't'/$74 in map with random tiles
 * ----------------------------------------------------------------------- */
static void make_conture(void)
{
    uint8_t *p  = MAP_BASE;
    uint8_t *hi = (uint8_t *)MAP_END_ADDR;

    while (p < hi) {
        uint8_t b = *p;
        if (b == 0x73) {           /* 's' */
            uint8_t r;
            do { r = RANDOM_HW & 3; } while (r == 0);
            b = (uint8_t)(r + 0x62 - 1);
        } else if (b == 0x74) {    /* 't' */
            uint8_t r;
            do { r = RANDOM_HW & 3; } while (r == 0);
            b = (uint8_t)(r + 0x65 - 1);
        }
        *p++ = b;
    }

    /* Copy first 40 rows of map to 40 rows at map+255-40 (bottom wrap) */
    uint8_t *src = MAP_BASE;
    uint8_t *dst = MAP_BASE + 255 - 40;
    for (int row = 0; row < 40; row++) {
        for (int col = 0; col < 40; col++)
            dst[col] = src[col];
        src += 256;
        dst += 256;
    }

    /* Level 2: clear a 14-wide × 3-row region at ($7E,$13) */
    if (level == 2) {
        temp1 = 0x7E;
        temp2 = 0x13;
        compute_map_adr();
        uint8_t *clr = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col <= 0x0D; col++)
                clr[col] = 0;
            clr += 256;
        }
    }
}

/* -----------------------------------------------------------------------
 * S.BEGIN — place slaves on the map for level 0/1, then go to new-player
 * ----------------------------------------------------------------------- */
static void s_begin(void)
{
    slaves_left = 8;

    for (int i = 7; i >= 0; i--)
        slave_status[i] = STATUS_OFF;

    if (level == 2) {
        /* Level 2 has no slave placement loop */
        mode = NEW_PLAYER_MODE;
        return;
    }

    /* Walk the map looking for valid slave spawn points */
    int slot = 7;
    for (;;) {
        uint8_t *mp = MAP_BASE;
        for (;;) {
            for (int y = 0; y < 256; y++) {
                uint8_t c = mp[y];
                if (c == 0x48) {   /* '^H' pattern check */
                    uint8_t right = mp[y + 1];
                    if (right != 0x48) goto next_col;
                    uint8_t above = *(mp - 256 + y);
                    if (above != 0x1F) goto next_col;
                    uint8_t rnd = RANDOM_HW;
                    if (rnd < 10 || rnd >= 50) goto next_col;

                    /* Valid spawn found */
                    slave_x[slot]  = (uint8_t)(y + 5);
                    slave_y[slot]  = (uint8_t)((uintptr_t)(mp - MAP_BASE) >> 8);
                    mp[y] = 1;
                    slave_status[slot] = STATUS_ON;
                    slave_dx[slot] = 0x10;
                    if (--slot < 0) goto done;
                }
            next_col:;
            }
            mp += 256;
            if (mp >= (uint8_t *)MAP_END_ADDR)
                break;
        }
        /* Rescan from beginning (TXA; BPL .9): slot < 0 exits via goto done */
    }
done:
    mode = NEW_PLAYER_MODE;
}

/* -----------------------------------------------------------------------
 * M.NEW.LEVEL — unpack map + scanner, place slaves, set level parameters
 * ----------------------------------------------------------------------- */
void m_new_level(void)
{
    screen_off();

    print_str(12, 6, str_enter);

    /* LDY LEVEL; DEY; BEQ .0 — level 1 uses LVL.2, all others use LVL.1 */
    if (level == 1) {
        print_str(2, 8, lvl_2);    /* CRYSTALLINE CAVES */
    } else {
        print_str(2, 8, lvl_1);    /* VAULTS OF DRACONIS */
    }

    /* Set level color and background */
    bak_color = level_color[level];
    COLOR0    = level_color[level];

    /* Set scroll start position */
    sx   = level_start[level][0];
    sy   = level_start[level][1];
    chopper_x     = level_chop_start[level][0];
    chopper_y     = level_chop_start[level][1];
    sx_f = 0;
    sy_f = (level == 2) ? 0 : 7;

    chopper_angle = 8;

    save_pos();
    do_checksum3();

    bonus1 = 0x99;
    bonus2 = 0x99;

    /* Initialize tank start positions for this level */
    for (int t = MAX_TANKS - 1; t >= 0; t--) {
        if (level == 1) {
            tank_start_x[t] = tank_start_x_l1[t];
            tank_start_y[t] = tank_start_y_l1[t];
        } else {
            tank_start_x[t] = tank_start_x_l2[t];
            tank_start_y[t] = tank_start_y_l2[t];
        }
        tank_status[t] = STATUS_BEGIN;
        cm_status[t]   = STATUS_OFF;
    }

    r_status = STATUS_OFF;

    /* Initialize pod statuses */
    for (int p = MAX_PODS - 1; p >= 0; p--)
        pod_status[p] = STATUS_OFF;
    for (int p = start_pods; p >= 0; p--)
        pod_status[p] = STATUS_BEGIN;

    pod_num   = 0;
    slave_num = 0;

    /* Unpack level map */
    adr1_lo = (uint8_t)((uintptr_t)pack_adr[level] & 0xFF);
    adr1_hi = (uint8_t)((uintptr_t)pack_adr[level] >> 8);
    adr2_lo = (uint8_t)((uintptr_t)MAP_BASE & 0xFF);
    adr2_hi = (uint8_t)((uintptr_t)MAP_BASE >> 8);
    temp2   = (uint8_t)(MAP_END_ADDR & 0xFF);
    temp3   = (uint8_t)(MAP_END_ADDR >> 8);
    temp4   = 0;
    unpack();

    make_conture();

    /* Unpack scanner data */
    adr1_lo = (uint8_t)((uintptr_t)scan_info[level] & 0xFF);
    adr1_hi = (uint8_t)((uintptr_t)scan_info[level] >> 8);
    adr2_lo = (uint8_t)((uintptr_t)SCANNER_BASE & 0xFF);
    adr2_hi = (uint8_t)((uintptr_t)SCANNER_BASE >> 8);
    temp2   = (uint8_t)(SCANNER_END_ADDR & 0xFF);
    temp3   = (uint8_t)(SCANNER_END_ADDR >> 8);
    temp4   = 1;
    unpack();

    /* Copy scanner rows with $1B column offset (scanner border copy) */
    {
        uint8_t *src = SCANNER_BASE;
        uint8_t *dst = SCANNER_BASE + 0x1B;
        for (int row = 39; row >= 0; row--) {
            for (int col = 12; col >= 0; col--)
                dst[col] = src[col];
            src += 40;
            dst += 40;
        }
    }

    s_begin();
}

/* -----------------------------------------------------------------------
 * M.GAME.OVER — tally score, display rank, return to title
 * ----------------------------------------------------------------------- */
void m_game_over(void)
{
    screen_off();

    /* Compute game points from results */
    inc_game_points((uint8_t)(slaves_saved >> 2));

    if (fort_status == STATUS_OFF)
        inc_game_points(3);

    if (level == 3)
        inc_game_points(1);

    inc_game_points(1);
    inc_game_points(score3);

    inc_game_points((uint8_t)(int8_t)m_tab[grav_skill]);
    /* CLC; A=2; SBC CHOPS; EOR #$FF  →  ~(1-chops)  =  chops-2  (mod 256) */
    inc_game_points((uint8_t)(chops - 2));
    inc_game_points((uint8_t)(int8_t)m_tab[pilot_skill]);

    /* Clamp game_points to 0..15 */
    if ((int8_t)game_points < 0) game_points = 0;
    if (game_points >= 16)       game_points = 15;

    /* Update high score if beaten */
    int new_hi = 0;
    if (score3 > hi3) new_hi = 1;
    else if (score3 == hi3) {
        if (score2 > hi2) new_hi = 1;
        else if (score2 == hi2 && score1 >= hi1) new_hi = 1;
    }
    if (new_hi) {
        hi1 = score1;
        hi2 = score2;
        hi3 = score3;
    }

    /* Display high score */
    temp1 = 2; temp2 = 0;
    s_flg = 0;
    print_str(2, 0, HS);
    uint8_t *sadr = PLAY_SCRN + 24;
    s_adr_lo = (uint8_t)((uintptr_t)sadr & 0xFF);
    s_adr_hi = (uint8_t)((uintptr_t)sadr >> 8);
    ddig(hi3);
    ddig(hi2);
    ddig(hi1);

    /* Print mission result */
    print_str(3, 5, G_1);
    temp1 = 21;
    print_str(21, 5, (level == 3) ? G_C : G_A);

    /* Print rank label */
    print_str(7, 8, G_2);

    /* Print CLASS label; after this, adr1 points to row 10 of screen */
    print_str(21, 10, G_3);

    /* Write rank numeral and letter to screen (ADR1 set by preceding PRINT) */
    {
        uint8_t *row = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
        uint8_t rank_num = (uint8_t)(((game_points & 3) ^ 3) + 1); /* 1=lowest, 4=highest */
        row[12] = rank_num | 0x90;          /* reverse-video rank digit */
        row[13] = (row[12]) & 0x8F;         /* reverse-video rank letter */
    }

    /* Print rating string */
    temp1 = 3;
    {
        uint8_t idx = (uint8_t)((game_points >> 2) & 3);
        print_str(3, 10, rating_table[idx]);
    }

    /* Return to title */
    tim6_val    = (uint8_t)-1;
    demo_status = 1;
    mode        = TITLE_MODE;
}

/* -----------------------------------------------------------------------
 * SET.FONTS — copy FNT1/FNT2 data into CHR.SET1/CHR.SET2
 * (inline startup routine from fort1.s, starting at line 00590)
 * ----------------------------------------------------------------------- */
static void set_fonts(void)
{
    for (int x = 0; x < 256; x++) {
        CHR_SET1[15 + x]        = fnt1_data[x];
        CHR_SET1[0x100 + x]     = fnt1_data[0x100 - 15 + x];
        CHR_SET1[0x200 + x]     = fnt1_data[0x200 - 15 + x];

        CHR_SET2[0x100 + 8 + x] = fnt2_data[x];
        CHR_SET2[0x200 + x]     = fnt2_data[0x100 - 8 + x];
        CHR_SET2[0x300 + x]     = fnt2_data[0x200 - 8 + x];
    }
}

/* -----------------------------------------------------------------------
 * fort1_start — program entry point (START label in fort1.s)
 * ----------------------------------------------------------------------- */
void fort1_start(void)
{
    /* Copy Z2 initialization block to RAM2.STUFF */
    {
        const uint8_t len = z2_len;
        for (int i = (int)(uint8_t)len; i > 0; i--)
            RAM2_STUFF[i] = z2[i];
    }

    /* Hardware initialization */
    SDMCTL = 0x3E;    /* %00111110 */
    PRIOR  = 0x14;
    GRACTL = 0x03;    /* %00000011 */
    SKCTL  = 0x03;    /* value from A after STA GRACTL (A was 0x03) */

    /* Set player-missile and character set base */
    PMBASE = (uint8_t)((uintptr_t)PLAYER_BASE >> 8);
    CHBAS  = (uint8_t)((uintptr_t)CHR_SET1 >> 8);

    /* Clear audio and initial state */
    AUDCTL = 0;
    COLOR4 = 0;
    tim6_val    = 0;
    pilot_skill = 0;
    grav_skill  = 0;
    elevator_dx = 1;
    chops       = 2;

    /* Install display list interrupt handler */
    SET_VDSLST(line1);

    /* Initialize game state */
    m_start();
    mode = TITLE_MODE;

    /* Copy fonts into character sets */
    set_fonts();

    /* Copy Z1 initialization block to RAM1.STUFF */
    {
        const uint8_t len = z1_len;
        for (int i = (int)(uint8_t)len; i >= 0; i--)
            RAM1_STUFF[i] = z1[i];
    }

    /* Enable VBlank and DLI NMIs, clear interrupt disable */
    NMIEN = 0x40;
    /* CLI — in C: interrupts are assumed enabled */

    /* Enter title screen (never returns) */
    title();
}
