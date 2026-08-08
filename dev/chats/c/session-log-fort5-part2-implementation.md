# Session Log — fort5.s → C Conversion  Part 2: Implementation
**Date:** 2026-08-08
**Branch:** `agents/asm-to-c-conversion`
**See also:** `session-log-fort5-part1-analysis.md` for assembly research and algorithm analysis

---

## 1. Design Decisions and Rationale

### 1.1 Static vs Public Function Visibility

Functions that are only reachable through a public entry point are declared `static`:

| Static function | Reachable via |
|----------------|--------------|
| `get_slave_adr()` | `s_erase`, `s_move`, `s_draw`, `s_col` |
| `fe()` | `re_fuel()` |
| `draw_base()` | `df1()` |
| `df1()` | `re_fuel()`, `f1()` |
| `f1()` | `re_fuel()` |
| `re_fuel()` | `check_fuel_base()` |
| `s_erase()` | `move_slaves()`, `s_col2()` |
| `s_move()` | `move_slaves()` |
| `s_draw()` | `move_slaves()` |
| `mult_by_40()` | `set_scanner()`, `pos_it()` |
| `do_line()` | `set_scanner()` |
| `next_part1()` | `do_checksum1()` |
| `rom_checksum_ok()` | `do_checksum1()`, `do_checksum2()` |
| `do_checksum1()` | `check_fort()` |
| `s_col()` | `move_slaves()` |
| `s_col2()` | `move_slaves()`, `s_col()` |

### 1.2 Function Definition Order

All static functions are defined before first use. The dependency chain from the bottom up:

```
get_slave_adr → (compute_map_adr is extern, always available)
fe → (empty)
draw_base → compute_map_adr
df1 → draw_base, (fuel_temp, level are globals)
f1 → df1, save_pos
re_fuel → fe, f1, df1
s_erase → get_slave_adr
s_move → get_slave_adr
s_draw → get_slave_adr
mult_by_40 → (arithmetic only)
do_line → (uses scan_adr1/2 globals)
next_part1 → inc_score, give_bonus, compute_map_adr, wait_frame, clear_sounds
rom_checksum_ok → (arithmetic only)
do_checksum1 → rom_checksum_ok, next_part1
print_slaves_left (public) → print
s_col2 → s_erase, print_slaves_left
s_col → get_slave_adr, s_col2
```

Since `print_slaves_left` is declared in `fort5.h` (included at top of file), the prototype is always visible to callers like `s_col2`, even before the definition.

### 1.3 FORT.EXP — Runtime Local Array

The `FORT.EXP` table in assembly is:
```
.DA FORT.EX1, FORT.EX2, FORT.EX3, FORT.EX4
```

In `fnt1.h`, these are declared `extern const uint8_t *const FORT_EX1;` etc. A C11 static array initializer cannot use non-constant expressions, and the address of an extern const pointer is not a compile-time constant. Therefore the table is built at runtime as a local array inside `next_part1()`:

```c
const uint8_t *fort_exp[4];
fort_exp[0] = FORT_EX1;
fort_exp[1] = FORT_EX2;
fort_exp[2] = FORT_EX3;
fort_exp[3] = FORT_EX4;
```

This avoids the static-initializer issue entirely with no semantic difference.

### 1.4 Bit-Scatter in NEXT.PART1

The assembly uses `ASL TEMP3; ROR TEMP5` to extract bits LSB-first from the loaded byte, where `ASL TEMP3` sets initial carry = 0 (TEMP3 starts at 0, shifts 0 into carry). The carry feeds into `ROR TEMP5` which right-rotates: bit 0 → carry.

In C, since initial carry = 0, this is identical to a logical right shift:

```c
uint8_t t5 = fd[temp4];
for (uint8_t bit = 0u; bit < 8u; bit++) {
    uint8_t val = (t5 & 1u) ? TILE_EXP : 0u;
    t5 >>= 1;
    uint8_t y = (uint8_t)(23u - bit * 3u);
    row_base[y] = row_base[y-1u] = row_base[y-2u] = val;
}
```

The `y = 23 - bit*3` sequence gives Y positions 23, 20, 17, 14, 11, 8, 5, 2 — counting down in groups of 3.

### 1.5 CHR_SET2 Writable Pointer Conflict

`fort3.c` defines `#define CHR_SET2 ((const uint8_t *)0x0C00u)` (read-only). `fort5.c` needs `((uint8_t *)0x0C00u)` (writable) for `WINDOW_1`. Since these are in separate translation units and `CHR_SET2` is a preprocessor macro (not a symbol), there is no ODR conflict. Each TU's macro resolves independently.

### 1.6 PRINT.SLAVES.LEFT PLAY.SCRN Logic

The assembly:
```
LDA SLAVES.LEFT
ORA #$90         ; A = slaves_left | $90
STA PLAY.SCRN+5  ; always write
CMP #$90         ; if A == $90 (slaves_left == 0 since $90|0=$90)
BEQ .1
AND #$8F         ; otherwise strip bit 4
BNE .2
.1 LDA #$8A
.2 STA PLAY.SCRN+6
```

In C:
```c
uint8_t a = (uint8_t)(slaves_left | 0x90u);
PLAY_SCRN[5] = a;
if (a == 0x90u) a = 0x8Au;
PLAY_SCRN[6] = (uint8_t)(a & 0x8Fu);
```

When `slaves_left == 0`: A = $90, PLAY_SCRN[5]=$90, PLAY_SCRN[6]=$8A.
When `slaves_left == 1..9`: A = $91..$99, PLAY_SCRN[6] = A & $8F = $81..$89.
This displays the slave count in the screen panel using Atari display codes.

### 1.7 S2.VAL Lifecycle

`s2_val` is set to $3F by `read_trig()` in fort4.c on each missile fire. In `do_sounds()`:
- S2 active while `s2_val & 0x80 == 0` (i.e., values $00–$7F, which is 0–63)
- `freq = (s2_val ^ $3F) + 16` rises from $10 (when s2_val=$3F) to $4F (when s2_val=0)
- When `freq == $4F` (s2_val=0): AUDC2=0 (silence), then `s2_val-- = $FF`
- Next frame: `$FF & $80 = $80` → skip, missile silent until next fire

The tone rises as the missile flies (s2_val counts down from 63 to 0), then silences.

### 1.8 S4 BCD Add — Cannot Use fort4.c's bcd_add

`bcd_add()` is a `static` function in fort4.c. It is not visible to fort5.c. The refuel sound handler (S4) needs BCD-add 4 to fuel1/fuel2. This is implemented inline with the same nibble-carry logic as the static helper.

The `MAX.FUEL` comparison in the assembly uses `CMP #0` on the low byte of MAX.FUEL ($2000): `CMP #0` always sets carry (A - 0 ≥ 0 always), so `SBC FUEL2, /MAX.FUEL` = `fuel2 - $20 - 0 = fuel2 - $20`. The BCS branch skips the add if fuel2 ≥ $20 (BCD value 20 = numeric 2000 = max fuel).

In C: `if (fuel2 < 0x20u) { /* BCD add */ }`.

### 1.9 Hardware Register Macros

All hardware access uses the `REG(addr)` pattern from fort1.c/fort4.c:
```c
#define REG(a)  (*(volatile uint8_t *)(uintptr_t)(a))
```

This is re-defined in fort5.c (same definition). No ODR conflict — macros are preprocessor-only.

New macros added in fort5.c beyond what was in fort4.c: AUDF1/AUDC1–AUDF4/AUDC4, HPOSP0/P2/P3, COLBK, COLPF0-3, CHBASE, WSYNC, VDSLST_LO/HI.

### 1.10 VDSLST Write Convention

VDSLST ($0200/$0201) is the deferred VBI vector. The DLI handlers write the address of the next handler to VDSLST in C using `uintptr_t` casts:

```c
VDSLST_LO = (uint8_t)((uintptr_t)line2 & 0xFFu);
VDSLST_HI = (uint8_t)((uintptr_t)line2 >> 8u);
```

On a real Atari (16-bit address space), `uintptr_t` is 16 bits and this gives the correct function address. On a modern 64-bit host (used for syntax checking), `uintptr_t` is 64 bits but the write still compiles and the semantics are defined — only the value would be wrong on real hardware, which is irrelevant for the syntax check.

### 1.11 set_scanner Row Clamp Logic

```
LDA SY
BEQ .2        ; sy == 0 → skip multiply
BMI .2        ; sy > 127 → skip multiply (invalid camera position)
CMP #17
BLT .1        ; sy < 17 → use sy directly
LDA #16       ; sy ≥ 17 → cap at 16
.1 JSR MULT.BY.40
```

In C:
```c
if (sy != 0u && !(sy & 0x80u)) {
    uint8_t row = (sy >= 17u) ? 16u : sy;
    mult_by_40(row);
}
```

When sy=0 or sy≥128: temp1 and temp2 remain 0 (set at start of function), so the scanner is positioned at the top of scanner memory with only the horizontal offset.

When sy=1..16: `mult_by_40(sy)` → row offset in temp1/temp2.
When sy=17+: `mult_by_40(16)` → capped at 16 rows from top = maximum scroll.

---

## 2. Produced Files

### `dev/src/fort5.h` (36 lines)

```c
#pragma once
#include <stdint.h>

/* fort5.h — Slave AI, fuel base, scanner, fort explosion, DLI, sounds
 *            (converted from fort5.s) */

/* ROM integrity checks */
void do_checksum2(void);
void do_checksum3(void);

/* Slave entity management */
void move_slaves(void);
void print_slaves_left(void);
int  pick_up_slave(void);  /* 1 = slave captured, 0 = none in range */

/* Fuel base logic */
void check_fuel_base(void);

/* Scanner minimap update */
void set_scanner(void);
void pos_it(void);   /* temp1=tile_x, temp2=tile_y → toggle scanner pixel */

/* Fort explosion sequence */
void check_fort(void);

/* Display-list interrupt handlers (chained: LINE1→2→3→4→LINE1) */
void line1(void);
void line2(void);
void line3(void);
void line4(void);

/* Sound subsystem (called from line4) */
void do_sounds(void);
```

### `dev/src/fort5.c` (721 lines)

Complete source listing:

```c
/* fort5.c — Slave AI, fuel base, scanner, fort explosion, DLI, sounds
 * (converted from fort5.s)
 */

#include <stdint.h>
#include "fort1.h"
#include "fort5.h"
#include "fnt1.h"   /* POS_MASK1, FORT_EX1-4 */

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

#define SCANNER_BASE  ((uint8_t *)0x39C0u)
#define CHR_SET2      ((uint8_t *)0x0C00u)
#define WINDOW_1      (CHR_SET2 + 712u)
#define PLAY_SCRN     ((uint8_t *)0x0300u)
#define S_LINE1       ((uint8_t *)0x0AE0u)
#define S_LINE2       ((uint8_t *)0x0B40u)
#define S_LINE3       ((uint8_t *)0x0BA0u)

#define TILE_EMPTY    0x48u
#define TILE_ERASE_T  0x1Fu
#define TILE_EXP      0x20u
#define TILE_MISS_L   0x71u
#define TILE_MISS_R   0x72u

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

extern void compute_map_adr(void);
extern void save_pos(void);
extern void give_bonus(void);
extern void clear_info(void);
extern void clear_sounds(void);
extern void wait_frame(uint8_t n);
extern void print(void);
extern void inc_score(uint8_t hi, uint8_t lo);

static const uint8_t slave_chr_tl[2] = { 0x4A, 0x4A };
static const uint8_t slave_chr_tr[2] = { 0x49, 0x49 };
static const uint8_t slave_chr_bl[2] = { 0x3E, 0x3D };
static const uint8_t slave_chr_br[2] = { 0x3B, 0x3C };

static const uint8_t slave_pickup_mess[] = {
    0xCD, 0xC5, 0xCE, 0x20, 0x20, 0xD4, 0xCF,
    0x20, 0x20, 0xD2, 0xC5, 0xD3, 0xC3, 0xD5, 0xC5, 0xFF
};

static const uint8_t warning_msg[] = {
    0xCC, 0xCF, 0xD7, 0x20, 0x20,
    0xCF, 0xCE, 0x20, 0x20,
    0xC6, 0xD5, 0xC5, 0xCC, 0xFF
};

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

static void get_slave_adr(uint8_t idx) {
    temp1 = slave_x[idx]; temp2 = slave_y[idx];
    compute_map_adr();
}

static void fe(void) { }

static void draw_base(void) {
    compute_map_adr();
    const uint8_t *shape = base_shape_flat + (uint16_t)temp3 * 6u;
    for (int8_t cnt = 4; cnt >= 0; cnt--) {
        uint8_t *dst = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
        for (uint8_t col = 0u; col < 6u; col++) dst[col] = *shape++;
        adr1_hi++;
    }
}

static void df1(void) {
    temp2 = 11u; temp3 = fuel_temp;
    if (level == 1u) {
        temp1 = 0x82u; draw_base();
    } else {
        temp1 = 23u; draw_base();
        temp1 = 0xECu + 2u; draw_base();
    }
    fuel_temp--;
}

static void f1(void) {
    if (chop_y >= 13u) { s4_val = 1u; }
    else { s4_val = 0u; AUDC2_HW = 0u; }
    if (chop_y >= 10u) return;
    fuel_status = STATUS_FULL;
    fuel_temp = 4u;
    df1();
    save_pos();
}

static void re_fuel(void) {
    tim4_val--;
    if (tim4_val) { fe(); return; }
    tim4_val = 1u;
    if ((int8_t)fuel_temp < 0) { f1(); return; }
    df1();
}

static void s_erase(uint8_t idx) {
    get_slave_adr(idx);
    uint8_t *bot = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
    *bot = TILE_EMPTY;
    adr1_hi--;
    uint8_t *top = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
    *top = TILE_ERASE_T;
}

static void s_move(uint8_t idx) {
    for (;;) {
        uint8_t dx = slave_dx[idx];
        if (dx & 0x80u) {
            slave_dx[idx] = (uint8_t)(((uint8_t)(dx - 1u) & 0x01u) | 0xF0u);
            slave_x[idx]--;
        } else {
            slave_dx[idx] = (uint8_t)(((uint8_t)(dx + 1u) & 0x01u) | 0x10u);
            slave_x[idx]++;
        }
        get_slave_adr(idx);
        const uint8_t *tile = (const uint8_t *)(uintptr_t)
            ((uint16_t)adr1_hi << 8 | adr1_lo);
        if (*tile == TILE_EMPTY) break;
        slave_dx[idx] ^= 0xE0u;
    }
}

static void s_draw(uint8_t idx) {
    get_slave_adr(idx);
    uint8_t dx = slave_dx[idx];
    uint8_t frame = dx & 0x03u;
    uint8_t *bot = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
    if (dx & 0x80u) {
        *bot = slave_chr_bl[frame]; adr1_hi--;
        uint8_t *top = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
        *top = slave_chr_tl[frame];
    } else {
        *bot = slave_chr_br[frame]; adr1_hi--;
        uint8_t *top = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi << 8 | adr1_lo);
        *top = slave_chr_tr[frame];
    }
}

static void mult_by_40(uint8_t val) {
    uint16_t r = (uint16_t)val * 40u;
    temp1 = (uint8_t)(r & 0xFFu);
    temp2 = (uint8_t)(r >> 8u);
}

static void do_line(void) {
    for (int8_t row = 7; row >= 0; row--) {
        uint16_t src = (uint16_t)scan_adr1_hi << 8 | scan_adr1_lo;
        uint16_t dst = (uint16_t)scan_adr2_hi << 8 | scan_adr2_lo;
        for (uint8_t col = 0u; col < 12u; col++) {
            *(uint8_t *)(uintptr_t)dst = *(const uint8_t *)(uintptr_t)src;
            src++; dst += 8u;
        }
        uint16_t sa1 = (uint16_t)((uint16_t)scan_adr1_hi<<8|scan_adr1_lo) + 40u;
        scan_adr1_lo = (uint8_t)(sa1 & 0xFFu);
        scan_adr1_hi = (uint8_t)(sa1 >> 8u);
        adr1_lo = scan_adr1_lo; adr1_hi = scan_adr1_hi;
        uint16_t sa2 = (uint16_t)((uint16_t)scan_adr2_hi<<8|scan_adr2_lo) + 1u;
        scan_adr2_lo = (uint8_t)(sa2 & 0xFFu);
        scan_adr2_hi = (uint8_t)(sa2 >> 8u);
        adr2_lo = scan_adr2_lo; adr2_hi = scan_adr2_hi;
    }
}

static void next_part1(void) {
    inc_score(0x00u, 0x50u);
    give_bonus();
    mode = STOP_MODE;
    bonus1 = 0x99u; bonus2 = 0x99u;
    land_chop_x = 0x76u; land_chop_y = 0xA0u;
    land_x = 0x6Eu; land_y = 0x11u;
    land_fx = 0x07u; land_fy = 0x96u;
    land_chop_angle = 8u;
    uint8_t *win = WINDOW_1;
    for (int8_t i = 15; i >= 0; i--) win[i] = 0u;
    const uint8_t *fort_exp[4];
    fort_exp[0] = FORT_EX1; fort_exp[1] = FORT_EX2;
    fort_exp[2] = FORT_EX3; fort_exp[3] = FORT_EX4;
    temp3 = 0u;
    while (temp3 < 4u) {
        temp1 = 121u; temp2 = 20u;
        compute_map_adr();
        adr2_lo = (uint8_t)((uintptr_t)fort_exp[temp3] & 0xFFu);
        adr2_hi = (uint8_t)((uintptr_t)fort_exp[temp3] >> 8u);
        for (temp4 = 0u; temp4 < 6u; temp4++) {
            for (temp6 = 0u; temp6 < 3u; temp6++) {
                const uint8_t *fd = (const uint8_t *)(uintptr_t)
                    ((uint16_t)adr2_hi << 8 | adr2_lo);
                uint8_t t5 = fd[temp4];
                uint8_t *row_base = (uint8_t *)(uintptr_t)
                    ((uint16_t)adr1_hi << 8 | adr1_lo);
                for (uint8_t bit = 0u; bit < 8u; bit++) {
                    uint8_t val = (t5 & 1u) ? TILE_EXP : 0u;
                    t5 >>= 1;
                    uint8_t y = (uint8_t)(23u - bit * 3u);
                    row_base[y] = val;
                    row_base[y - 1u] = val;
                    row_base[y - 2u] = val;
                }
                adr1_hi++;
            }
        }
        bak2_color = 0x10u; AUDC4_HW = 0xCFu;
        for (int8_t flash = 15; flash >= 0; flash--) {
            wait_frame(2u);
            bak2_color++;
            s3_val = 1u;
            AUDF4_HW = RANDOM_HW;
        }
        bak2_color = 0u;
        temp3++;
    }
    mode = GO_MODE;
    fort_status = STATUS_OFF;
    laser_status = STATUS_OFF;
    clear_sounds();
}

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

static void do_checksum1(void) {
    if (!rom_checksum_ok()) { for (;;) {} }
    next_part1();
}

void print_slaves_left(void) {
    adr1_lo = (uint8_t)((uintptr_t)slave_pickup_mess & 0xFFu);
    adr1_hi = (uint8_t)((uintptr_t)slave_pickup_mess >> 8u);
    temp1 = 9u; temp2 = 0u;
    print();
    uint8_t a = (uint8_t)(slaves_left | 0x90u);
    PLAY_SCRN[5] = a;
    if (a == 0x90u) a = 0x8Au;
    PLAY_SCRN[6] = (uint8_t)(a & 0x8Fu);
    tim9_val = 90u;
}

static int s_col2(uint8_t idx) {
    s_erase(idx);
    slave_status[idx] = STATUS_OFF;
    slaves_left--;
    print_slaves_left();
    return 1;
}

static int s_col(uint8_t idx) {
    get_slave_adr(idx);
    const uint8_t *bot = (const uint8_t *)(uintptr_t)
        ((uint16_t)adr1_hi << 8 | adr1_lo);
    uint8_t tile = *bot;
    if (tile==0u || tile==TILE_EXP || tile==TILE_MISS_L || tile==TILE_MISS_R)
        return s_col2(idx);
    adr1_hi--;
    const uint8_t *top = (const uint8_t *)(uintptr_t)
        ((uint16_t)adr1_hi << 8 | adr1_lo);
    tile = *top;
    if (tile==0u || tile==TILE_EXP || tile==TILE_MISS_L || tile==TILE_MISS_R)
        return s_col2(idx);
    return 0;
}

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
            s_col2(idx);
            AUDC3_HW = 0u;
            inc_score(0u, 8u);
            slaves_saved++;
        } else {
            if (!s_col(idx)) {
                s_erase(idx); s_move(idx); s_draw(idx);
            }
        }
    }
    idx++;
    if (idx >= 8u) idx = 0u;
    slave_num = idx;
    if (PLAY_SCRN[5]) {
        tim9_val--;
        if (!tim9_val) clear_info();
    }
}

int pick_up_slave(void) {
    for (int8_t i = 7; i >= 0; i--) {
        uint8_t si = (uint8_t)i;
        if (slave_status[si] == STATUS_OFF) continue;
        uint8_t dx = (uint8_t)(slave_x[si] - chop_x);
        if (dx & 0x80u) dx ^= 0xFEu;
        if (dx >= 4u) continue;
        uint8_t dy = (uint8_t)(slave_y[si] - chop_y);
        if (dy & 0x80u) dy ^= 0xFEu;
        if (dy >= 4u) continue;
        slave_status[si] = STATUS_PICKUP;
        AUDC3_HW = 0xA8u; AUDF3_HW = 32u;
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
            tim4_val = 1u;
            fuel_temp = 4u;
        }
    }
    if (fuel_status == STATUS_REFUEL) {
        AUDC2_HW = 0u; AUDF2_HW = 0x88u;
        clear_info(); return;
    }
    if (fuel2 != 0u) return;
    if (FRAME_HW & 0x08u) {
        AUDC2_HW = 0xA4u; AUDF2_HW = 0x88u;
        clear_info();
    } else {
        temp1 = 9u; temp2 = 0u;
        AUDC2_HW = 0xA4u; AUDF2_HW = 0xA4u;
        adr1_lo = (uint8_t)((uintptr_t)warning_msg & 0xFFu);
        adr1_hi = (uint8_t)((uintptr_t)warning_msg >> 8u);
        print();
    }
}

void set_scanner(void) {
    temp1 = 0u; temp2 = 0u;
    if (sy != 0u && !(sy & 0x80u)) {
        uint8_t row = (sy >= 17u) ? 16u : sy;
        mult_by_40(row);
    }
    uint32_t addr = (uint32_t)0x39C0u + (uint32_t)(sx >> 3u)
                  + (uint32_t)temp1 + ((uint32_t)temp2 << 8u);
    adr1_lo = scan_adr1_lo = (uint8_t)(addr & 0xFFu);
    adr1_hi = scan_adr1_hi = (uint8_t)((addr >> 8u) & 0xFFu);
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
    uint8_t tx = temp1;
    mult_by_40(temp2);
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

void line1(void) {
    VDSLST_LO = (uint8_t)((uintptr_t)line2 & 0xFFu);
    VDSLST_HI = (uint8_t)((uintptr_t)line2 >> 8u);
    for (uint8_t x = 0u; x < 8u; x++) {
        WSYNC_HW = 0u;
        COLBK_HW = (uint8_t)((x << 1u) | 0xE0u);
    }
    COLBK_HW = 0u;
}

void line2(void) {
    VDSLST_LO = (uint8_t)((uintptr_t)line3 & 0xFFu);
    VDSLST_HI = (uint8_t)((uintptr_t)line3 >> 8u);
    for (int8_t i = 2; i >= 0; i--)
        REG((uint16_t)0xD004u + (uint8_t)i) = rocket_x[(uint8_t)i];
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
    WSYNC_HW = 0u;
    CHBASE_HW = 0x0Cu;
    COLPF0_HW = bak_color;
    COLPF1_HW = 0x0Au;
    COLPF2_HW = 0x93u;
    COLPF3_HW = FRAME_HW;
    WSYNC_HW = 0u;
    COLBK_HW = bak2_color;
}

void line4(void) {
    VDSLST_LO = (uint8_t)((uintptr_t)line1 & 0xFFu);
    VDSLST_HI = (uint8_t)((uintptr_t)line1 >> 8u);
    for (int8_t i = 7; i >= 0; i--)
        REG((uint16_t)0xD000u + (uint8_t)i) = 0u;
    WSYNC_HW = 0u;
    COLBK_HW = 0u;
    if (mode == STOP_MODE || mode == GO_MODE) do_sounds();
}

void do_sounds(void) {
    /* S1: chopper engine */
    if (chopper_status != STATUS_OFF && !(FRAME_HW & 2u)) {
        AUDC1_HW = 0x83u;
        uint8_t freq = (s1_1_val & 0x80u) ? s1_2_val : s1_1_val;
        freq -= 4u;
        s1_1_val = freq; AUDF1_HW = freq;
    }
    /* S2: missile rising tone */
    if (!(s2_val & 0x80u)) {
        uint8_t freq = (uint8_t)((s2_val ^ 0x3Fu) + 16u);
        AUDF2_HW = freq;
        AUDC2_HW = (freq == (uint8_t)(0x3Fu + 16u)) ? 0u : 0x86u;
        s2_val--;
    }
    /* S3: explosion noise */
    if (s3_val != 0u) {
        uint8_t freq = (uint8_t)((RANDOM_HW & 3u) | s3_val);
        AUDF3_HW = (uint8_t)(freq + 0x10u);
        s3_val++;
        if (s3_val == 0x31u) s3_val = 0u;
        AUDC3_HW = (s3_val != 0u) ? 0x48u : 0u;
    }
    /* S4: refuel — BCD add 4 to fuel */
    if (s4_val != 0u) {
        uint8_t xv = (FRAME_HW & 7u) ? 0x18u : 0u;
        uint8_t yv = 0u;
        if (fuel2 < 0x20u) {
            yv = 0xA6u;
            uint8_t lo = (uint8_t)((fuel1 & 0x0Fu) + 4u);
            uint8_t c = 0u;
            if (lo >= 10u) { lo = (uint8_t)(lo - 10u); c = 1u; }
            uint8_t hi = (uint8_t)((fuel1 >> 4u) + c);
            c = 0u;
            if (hi >= 10u) { hi = (uint8_t)(hi - 10u); c = 1u; }
            fuel1 = (uint8_t)((hi << 4u) | lo);
            lo = (uint8_t)((fuel2 & 0x0Fu) + c);
            c = 0u;
            if (lo >= 10u) { lo = (uint8_t)(lo - 10u); c = 1u; }
            hi = (uint8_t)((fuel2 >> 4u) + c);
            fuel2 = (uint8_t)((hi << 4u) | lo);
        }
        AUDF2_HW = xv; AUDC2_HW = yv;
    }
    /* S5: hyper-chamber rising tone */
    if (s5_val != 0u) {
        uint8_t old = s5_val;
        s5_val++;
        uint8_t freq;
        if (old == 0x50u) { s5_val = 0u; freq = 0u; }
        else freq = old;
        AUDF2_HW = freq; AUDC2_HW = 0xA8u;
    }
    /* S6: cruise missile (every other frame) */
    if (!(FRAME_HW & 1u) && s6_val != 0u) {
        uint8_t old = s6_val;
        s6_val++;
        if (old >= 0x20u) s6_val = 0u;
        AUDF4_HW = old; AUDC4_HW = 0x07u;
    }
}
```

---

## 3. Compile Test

```
$ cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src \
     dev/src/fort2.c dev/src/fort3.c dev/src/fort4.c dev/src/fort5.c
(no output — zero warnings, zero errors)
```

All four translation units accepted. Zero diagnostics.

The `fort2.c fort3.c fort4.c` TUs were included to ensure no cross-file extern conflicts existed. All extern declarations in fort5.c for variables defined in those files resolved correctly.

`fort1.c` is intentionally excluded from this compile check (it depends on fort5 and fort4 functions that aren't yet fully wired in a link-able main), matching the same pattern used for the fort4.c syntax check.

---

## 4. Open Items After This Session

| Item | Status | Blocker |
|------|--------|---------|
| `fort6.s` → C | Pending | `extern const uint8_t laser_shapes[32]` in fort4.c is unresolved at link time |
| `fort7.s` → C | Pending | All `extern` variable declarations (scan_adr1/2, fuel_temp, tim4_val, tim9_val, temp5, temp6, and many others) need definitions |
| `fort8.s` → C | Pending | Display list initialization data |
| Full link test | Blocked | Needs fort6.c + fort7.c + fort8.c + entry point |

The `scan_adr1_lo/hi`, `scan_adr2_lo/hi`, `fuel_temp`, `tim4_val`, `tim9_val`, `temp5`, `temp6` variables introduced as `extern` in fort5.c will need to be defined (as zero-initialized globals) in the fort7.c BSS module.
