#pragma once
#include <stdint.h>

/* =========================================================================
 * fort.h — Master hardware equates and game constants
 * Converted from fort.s (Fort Apocalypse, SynAssembler master file)
 *
 * Provides:
 *  - REG() macro for volatile hardware register access
 *  - All hardware register #defines from fort.s .EQ directives
 *  - Memory layout constants
 *  - Game constants
 *  - extern declarations for zero-page variables defined in fort.c
 *
 * Note: mode/status constants (TITLE_MODE, STATUS_OFF, etc.) are in fort1.h
 * which reflects their source in fort7.s.
 * ========================================================================= */

/* Volatile hardware register access helper */
#define REG(a) (*(volatile uint8_t*)(uintptr_t)(a))

/* -------------------------------------------------------------------------
 * OS RAM shadow registers
 * ------------------------------------------------------------------------- */
#define FRAME REG(0x0014)    /* frame counter (ticks each VBlank) */
#define ATTRACT REG(0x004D)  /* attract mode flag */
#define VDSLST_L REG(0x0200) /* display list interrupt vector lo */
#define VDSLST_H REG(0x0201) /* display list interrupt vector hi */
#define VVBLKI_L REG(0x0222) /* immediate VBlank vector lo */
#define VVBLKI_H REG(0x0223) /* immediate VBlank vector hi */
#define VVBLKD_L REG(0x0224) /* deferred VBlank vector lo */
#define VVBLKD_H REG(0x0225) /* deferred VBlank vector hi */
#define SDMCTL REG(0x022F)   /* DMA control shadow */
#define SDLST_L REG(0x0230)  /* display list pointer lo (shadow) */
#define SDLST_H REG(0x0231)  /* display list pointer hi (shadow) */
#define PRIOR REG(0x026F)    /* priority register shadow */
#define PCOLR0 REG(0x02C0)   /* player 0 colour shadow */
#define PCOLR1 REG(0x02C1)   /* player 1 colour shadow */
#define PCOLR2 REG(0x02C2)   /* player 2 colour shadow */
#define PCOLR3 REG(0x02C3)   /* player 3 colour shadow */
#define COLOR0 REG(0x02C4)   /* playfield colour 0 shadow */
#define COLOR1 REG(0x02C5)   /* playfield colour 1 shadow */
#define COLOR2 REG(0x02C6)   /* playfield colour 2 shadow */
#define COLOR3 REG(0x02C7)   /* playfield colour 3 shadow */
#define COLOR4 REG(0x02C8)   /* background colour shadow */
#define CHBAS REG(0x02F4)    /* character base page (shadow) */
#define CH2 REG(0x02F2)      /* keyboard char code 2 */
#define CH REG(0x02FC)       /* keyboard char code */
#define STICK REG(0x0278)    /* joystick direction bits */
#define CDTMV1_L REG(0x0218) /* countdown timer 1 lo */
#define CDTMV1_H REG(0x0219) /* countdown timer 1 hi */
#define CDTMV2_L REG(0x021A) /* countdown timer 2 lo */
#define CDTMV2_H REG(0x021B) /* countdown timer 2 hi */
#define CDTMA1_L REG(0x0226) /* timer 1 action vector lo */
#define CDTMA1_H REG(0x0227) /* timer 1 action vector hi */
#define CDTMA2_L REG(0x0228) /* timer 2 action vector lo */
#define CDTMA2_H REG(0x0229) /* timer 2 action vector hi */

/* -------------------------------------------------------------------------
 * GTIA hardware registers
 * ------------------------------------------------------------------------- */

/* Collision read (write-only in player's own address space on hardware) */
#define M0PF REG(0xD000) /* missile 0 / playfield collision */
#define M1PF REG(0xD001)
#define M2PF REG(0xD002)
#define M3PF REG(0xD003)
#define P0PF REG(0xD004) /* player 0 / playfield collision */
#define P1PF REG(0xD005)
#define P2PF REG(0xD006)
#define P3PF REG(0xD007)
#define M0PL REG(0xD008) /* missile 0 / player collision */
#define M1PL REG(0xD009)
#define M2PL REG(0xD00A)
#define M3PL REG(0xD00B)
#define P0PL REG(0xD00C) /* player 0 / player collision */
#define P1PL REG(0xD00D)
#define P2PL REG(0xD00E)
#define P3PL REG(0xD00F)

/* Horizontal position / size (write) — same addresses as collision (read) */
#define HPOSP0 REG(0xD000)
#define HPOSP1 REG(0xD001)
#define HPOSP2 REG(0xD002)
#define HPOSP3 REG(0xD003)
#define HPOSM0 REG(0xD004)
#define HPOSM1 REG(0xD005)
#define HPOSM2 REG(0xD006)
#define HPOSM3 REG(0xD007)
#define SIZEP0 REG(0xD008)
#define SIZEP1 REG(0xD009)
#define SIZEP2 REG(0xD00A)
#define SIZEP3 REG(0xD00B)
#define SIZEM REG(0xD00C)

/* Colour registers (write) */
#define COLPM0 REG(0xD012)
#define COLPM1 REG(0xD013)
#define COLPM2 REG(0xD014)
#define COLPM3 REG(0xD015)
#define COLPF0 REG(0xD016)
#define COLPF1 REG(0xD017)
#define COLPF2 REG(0xD018)
#define COLPF3 REG(0xD019)
#define COLBK REG(0xD01A)

#define TRIG0 REG(0xD010)  /* joystick trigger 0 (0=pressed) */
#define GRACTL REG(0xD01D) /* graphics control */
#define HITCLR REG(0xD01E) /* clear collision registers (write) */
#define CONSOL REG(0xD01F) /* console buttons */

/* -------------------------------------------------------------------------
 * ANTIC hardware registers
 * ------------------------------------------------------------------------- */
#define DMACTL REG(0xD400) /* DMA control */
#define DLIST REG(0xD402)  /* display list pointer lo */
#define HSCROL REG(0xD404) /* horizontal scroll */
#define VSCROL REG(0xD405) /* vertical scroll */
#define PMBASE REG(0xD407) /* player-missile base page */
#define CHBASE REG(0xD409) /* character base page */
#define WSYNC REG(0xD40A)  /* wait for horizontal sync (write) */
#define VCOUNT REG(0xD40B) /* vertical line counter (read) */
#define NMIEN REG(0xD40E)  /* NMI enable */

/* -------------------------------------------------------------------------
 * POKEY hardware registers
 * ------------------------------------------------------------------------- */
#define AUDF1 REG(0xD200) /* audio frequency 1 */
#define AUDC1 REG(0xD201) /* audio control 1 */
#define AUDF2 REG(0xD202)
#define AUDC2 REG(0xD203)
#define AUDF3 REG(0xD204)
#define AUDC3 REG(0xD205)
#define AUDF4 REG(0xD206)
#define AUDC4 REG(0xD207)
#define AUDCTL REG(0xD208) /* audio control */
#define KBCODE REG(0xD209) /* keyboard code */
#define RANDOM REG(0xD20A) /* random number generator */
#define SKCTL REG(0xD20F)  /* serial port / keyboard control (write) */
#define SKSTAT REG(0xD20F) /* serial port / keyboard status (read) */

/* -------------------------------------------------------------------------
 * OS ROM utility addresses (used as interrupt return destinations)
 * ------------------------------------------------------------------------- */
static constexpr uint16_t VVBLKI_RET =
    0xE45Fu; /* JMP here at end of immediate VBlank handler */
static constexpr uint16_t VVBLKD_RET =
    0xE462u; /* JMP here at end of deferred VBlank handler */

/* -------------------------------------------------------------------------
 * Player-missile data base addresses
 * ------------------------------------------------------------------------- */
#define MIS_BASE ((uint8_t*)0x0300u) /* MIS — missile data ($300) */
#define PL0_BASE ((uint8_t*)0x0400u) /* PL0 — player 0 ($400) */
#define PL1_BASE ((uint8_t*)0x0500u) /* PL1 — player 1 ($500) */
#define PL2_BASE ((uint8_t*)0x0600u) /* PL2 — player 2 ($600) */
#define PL3_BASE ((uint8_t*)0x0700u) /* PL3 — player 3 ($700) */

/* -------------------------------------------------------------------------
 * Fixed-address memory regions
 * (from fort.s .EQ "CHANGE THESE CONSTANTS" section)
 * ------------------------------------------------------------------------- */
#define PLAY_SCRN ((uint8_t*)0x0300u)    /* screen display buffer */
#define CHR_SET1 ((uint8_t*)0x0800u)     /* character set 1 (game font) */
#define CHR_SET2 ((uint8_t*)0x0C00u)     /* character set 2 (graphics) */
#define MAP_BASE ((uint8_t*)0x1103u)     /* game map ($1100+3) */
#define SLAVES_BASE ((uint8_t*)0x3904u)  /* slave data array */
#define SCANNER_BASE ((uint8_t*)0x39C0u) /* scanner minimap bitmap */
#define RAM1_STUFF ((uint8_t*)0x0C90u)   /* CHR_SET2+144: init data copy */
#define RAM2_STUFF ((uint8_t*)0x0100u)   /* panel display buffer */
#define PACKED_MAP_BASE ((const uint8_t*)0x8000u)
#define PACKED_SCAN_BASE ((const uint8_t*)(0x8000u + 0x0D34u))

/* ROM + program layout (from fort.s) */
#define PROGRAM_BASE ((const uint8_t*)(0x8000u + 0x1221u))

/* -------------------------------------------------------------------------
 * Sub-regions of CHR_SET1 (game font overlays)
 * ------------------------------------------------------------------------- */
#define S_LINE1 (CHR_SET1 + 736u) /* CHR.SET1+736  ($0AE0) */
#define S_LINE2 (CHR_SET1 + 832u) /* CHR.SET1+832  ($0B40) */
#define S_LINE3 (CHR_SET1 + 928u) /* CHR.SET1+928  ($0BA0) */
// #define S_LINE1       ((uint8_t *)0x0AE0u)   /* CHR.SET1 + 736 */
// #define S_LINE2       ((uint8_t *)0x0B40u)   /* CHR.SET1 + 832 */
// #define S_LINE3       ((uint8_t *)0x0BA0u)   /* CHR.SET1 + 928 */

/* -------------------------------------------------------------------------
 * Sub-regions of CHR_SET2 (graphics font overlays)
 * Note: These depend on CHR_SET2 runtime value, kept as macros for
 * compatibility
 * ------------------------------------------------------------------------- */
#define LASERS_1 (CHR_SET2 + 8u)  /* LASERS.1 */
#define LASERS_2 (CHR_SET2 + 40u) /* LASERS.2 */
#define LASER_3 (CHR_SET2 + 72u)  /* LASER.3 */
#define BLOCK_1 (CHR_SET2 + 80u)
#define BLOCK_2 (CHR_SET2 + 88u)
#define BLOCK_3 (CHR_SET2 + 96u)
#define BLOCK_4 (CHR_SET2 + 104u)
#define BLOCK_5 (CHR_SET2 + 112u)
#define BLOCK_6 (CHR_SET2 + 120u)
#define BLOCK_7 (CHR_SET2 + 128u)
#define BLOCK_8 (CHR_SET2 + 136u)
#define WINDOW_1 (CHR_SET2 + 712u)  /* WINDOW.1 */
#define WINDOW_2 (CHR_SET2 + 720u)  /* WINDOW.2 */
#define EXPLOSION (CHR_SET2 + 256u) /* explosion animation frames */
#define EXPLOSION2 (CHR_SET2 + 504u)
#define MISS_CHR_LEFT (CHR_SET2 + 904u)  /* missile sprite (left-facing) */
#define MISS_CHR_RIGHT (CHR_SET2 + 912u) /* missile sprite (right-facing) */

/* -------------------------------------------------------------------------
 * Map/character constants
 * ------------------------------------------------------------------------- */
static constexpr uint8_t CHR_EXP = 0x20u;  /* EXP: explosion tile */
static constexpr uint8_t CHR_EXP2 = 0x3Fu; /* EXP2 */
static constexpr uint8_t CHR_MISS_LEFT =
    0x71u; /* MISS.LEFT missile character */
static constexpr uint8_t CHR_MISS_RIGHT =
    0x72u; /* MISS.RIGHT missile character */
static constexpr uint8_t CHR_EXP_WALL = 0xC7u; /* EXP.WALL = $47+128 */

/* -------------------------------------------------------------------------
 * Joystick direction bits (STICK register)
 * ------------------------------------------------------------------------- */
static constexpr uint8_t DIR_RIGHT = 0x08u;
static constexpr uint8_t DIR_LEFT = 0x04u;
static constexpr uint8_t DIR_DOWN = 0x02u;
static constexpr uint8_t DIR_UP = 0x01u;

/* -------------------------------------------------------------------------
 * Chopper / robot movement boundary limits
 * ------------------------------------------------------------------------- */
static constexpr int MAX_LEFT = 48;
static constexpr int MAX_RIGHT = 192;
static constexpr int MAX_UP = 100;
static constexpr int MAX_DOWN = 212;
static constexpr int MIN_LEFT = 110;
static constexpr int MIN_RIGHT = 130;
static constexpr int MIN_UP = 146;
static constexpr int MIN_DOWN = 166;
static constexpr uint16_t MAX_FUEL = 0x2000u;

/* -------------------------------------------------------------------------
 * Array sizes (also in fort1.h — canonical source is here)
 * ------------------------------------------------------------------------- */
static constexpr int MAX_TANKS = 6;
static constexpr int MAX_PODS = 39;
static constexpr int POD_SPEED = 15;

/* Checksum constant */
static constexpr uint16_t CHECK_SUM = 0x264Cu;

/* -------------------------------------------------------------------------
 * Zero-page scratch registers
 * Allocated in fort.s at .OR $15
 * Defined in fort.c
 * ------------------------------------------------------------------------- */

/* 16-bit scratch pointers (split lo/hi for 6502 compatibility) */
extern uint8_t adr1_lo, adr1_hi; /* ADR1 */
extern uint8_t adr2_lo, adr2_hi; /* ADR2 */

/* 8-bit scratch temporaries */
extern uint8_t temp1, temp2, temp3, temp4, temp5, temp6;
extern uint8_t temp_mode; /* TEMP.MODE */

/* Interrupt-context copies (used inside ISRs; separate from temp1-6) */
extern uint8_t adr1_i_lo, adr1_i_hi; /* ADR1.I */
extern uint8_t adr2_i_lo, adr2_i_hi; /* ADR2.I */
extern uint8_t temp1_i, temp2_i, temp3_i, temp4_i;

/* Scanner address pointer and flags */
extern uint8_t s_adr_lo, s_adr_hi; /* S.ADR */
extern uint8_t s_temp;             /* S.TEMP */
extern uint8_t s_flg;              /* S.FLG */

/* Tank initial spawn positions */
extern uint8_t tank_start_x[MAX_TANKS]; /* TANK.START.X */
extern uint8_t tank_start_y[MAX_TANKS]; /* TANK.START.Y */

/* Timer countdown values (0 = inactive; each corresponds to one subsystem) */
extern uint8_t tim1_val; /* LASER 1 shot timer */
extern uint8_t tim2_val; /* LASER 2 shot timer */
extern uint8_t tim3_val; /* chopper explosion timer */
extern uint8_t tim4_val; /* refuel timer */
extern uint8_t tim5_val; /* tank explosion timer */
extern uint8_t tim6_val; /* demo / title timer */
extern uint8_t tim7_val; /* robot explosion timer */
extern uint8_t tim8_val; /* robot missile timer */
extern uint8_t tim9_val; /* slave message timer */
extern uint8_t ssizem;   /* SSIZEM: saved missile sprite size */

/* Sound channel registers (.OR $43) */
extern uint8_t s1_1_val; /* S1.1.VAL */
extern uint8_t s1_2_val; /* S1.2.VAL */
extern uint8_t s2_val;   /* S2.VAL */
extern uint8_t s3_val;   /* S3.VAL */
extern uint8_t s4_val;   /* S4.VAL */
extern uint8_t s5_val;   /* S5.VAL */
extern uint8_t s6_val;   /* S6.VAL — missile sound */

/* Scoring / demo counters */
extern uint8_t game_points; /* GAME.POINTS */
extern uint8_t demo_status; /* DEMO.STATUS */
extern uint8_t demo_count;  /* DEMO.COUNT */

/* -------------------------------------------------------------------------
 * Pod arrays
 * POD.STATUS / POD.DX at POD.1 = CHR_SET2+920 ($0FF8)
 * POD.X .. POD.TEMP2 at POD.2 = $3925
 * Defined in fort.c (MAX_PODS = 39 entries each)
 * ------------------------------------------------------------------------- */
extern uint8_t pod_status[MAX_PODS]; /* POD.STATUS */
extern uint8_t pod_dx[MAX_PODS];     /* POD.DX */
extern uint8_t pod_x[MAX_PODS];      /* POD.X */
extern uint8_t pod_y[MAX_PODS];      /* POD.Y */
extern uint8_t pod_temp1[MAX_PODS];  /* POD.TEMP1 */
extern uint8_t pod_temp2[MAX_PODS];  /* POD.TEMP2 */

/* -------------------------------------------------------------------------
 * Slave arrays
 * At SLAVES = $3904 (8 entries each)
 * Defined in fort.c
 * ------------------------------------------------------------------------- */
extern uint8_t slave_status[8]; /* SLAVE.STATUS */
extern uint8_t slave_x[8];      /* SLAVE.X */
extern uint8_t slave_y[8];      /* SLAVE.Y */
extern uint8_t slave_dx[8];     /* SLAVE.DX */
