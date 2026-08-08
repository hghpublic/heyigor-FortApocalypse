# Session Log: convert fort4.s — Part 2: Implementation

## Files Produced
- `dev/src/fort4.h` (45 lines)
- `dev/src/fort4.c` (605 lines)

---

## fort4.h

```c
#pragma once
#include <stdint.h>

/* Digit position counter — set by caller before each group of ddig() calls.
 * Score display: set to 5 (6 digits).  Bonus/fuel: set to 3 (4 digits).
 * Single-digit displays: set to 0. */
extern uint8_t ddig_x;

void pos_chopper(void);
void pos_robot(void);
void read_stick(void);
void hover(void);
void read_trig(void);
void draw_map(void);
void compute_map_adr_i(void);   /* (temp1_i, temp2_i) → adr1_i lo/hi */
void compute_map_adr(void);     /* (temp1,   temp2)   → adr1    lo/hi */
void do_laser_1(void);
void do_laser_2(void);
void do_blocks(void);
void do_elevator(void);
void do_exp(void);
void do_numbers(void);
void ddig(uint8_t val);         /* render one BCD byte (2 digits) */
void inc_score(uint8_t hi, uint8_t lo);
```

---

## Key Function Implementations

### pos_it_i() — Scanner Minimap Pixel Toggle

```c
static void pos_it_i(void) {
    uint8_t x = temp1_i;
    uint8_t y = temp2_i;
    uint16_t offset = 3u + (uint16_t)(x >> 3) + (uint16_t)y * 40u;
    SCANNER_BASE[offset] ^= POS_MASK1[x & 7u];
}
```

Assembly computes y×40 via a multi-step shift-add:
`y*5 = y<<2 + y`, then `×8 via three ASL+ROL`, giving a 16-bit result in `TEMP3.I:A`.
In C, `(uint16_t)y * 40u` is equivalent and avoids the three-register trick entirely.

`pos_chopper()` and `pos_robot()` each call `pos_it_i()` exactly once per VBlank — this
XOR toggle erases the old pixel (first call) and draws the new one (second call across frames).

**POS.ROBOT assembly fall-through:** The assembly has `;JMP POS.IT.I` as a comment, meaning
POS.ROBOT literally falls through into POS.IT.I. In C: explicit `pos_it_i()` call at the end.

---

### read_trig() — Fire Button with Rising-Edge Detection

```c
void read_trig(void) {
    if (chopper_status == STATUS_CRASH) return;

    int do_fire = 0;

    if (!demo_status) {
        /* demo mode: fire automatically every 16 frames */
        if (!(FRAME_HW & 0x0Fu)) do_fire = 1;
    } else {
        /* real game: rising-edge detection */
        uint8_t t = TRIG0_HW;
        if (t) { trig_flag = t; return; }   /* released: store 1 */
        if (!trig_flag) return;             /* still held: no edge */
        trig_flag = 0u;                     /* edge: now pressed */

        if (mode == TITLE_MODE || mode == OPTION_MODE) {
            mode = START_MODE;
            demo_status = START_MODE;       /* value 3, not 1 — see note below */
        }
        do_fire = 1;
    }

    if (!do_fire) return;

    elevator_dx ^= 0xFEu;                  /* reverse elevator direction */

    /* Find free rocket slot (check index 1 first, then 0) */
    int8_t slot = -1;
    for (int8_t i = 1; i >= 0; i--) {
        if (!rocket_status[(uint8_t)i]) { slot = i; break; }
    }
    if (slot < 0) return;

    /* Angle → rocket direction 1-5 */
    uint8_t a = (uint8_t)((chopper_angle & 0x1Eu) >> 1u);
    uint8_t rs;
    if      (a >= 6u) rs = (a >= 8u) ? 5u : (uint8_t)(a - 2u);
    else if (a >= 4u) rs = 3u;
    else              rs = (a == 0u) ? 1u : a;

    rocket_status[(uint8_t)slot] = rs;
    rocket_x[(uint8_t)slot] = (uint8_t)((chopper_x & 3u) + chopper_x + 8u);
    rocket_y[(uint8_t)slot] = (uint8_t)(chopper_y + 8u);
    s2_val = 0x3Fu;
}
```

**`demo_status = START_MODE` (= 3, not 1):** Assembly:
```asm
LDA #START.MODE; STA MODE
; LDA #1          ← COMMENTED OUT — A still holds START.MODE = 3
STA DEMO.STATUS  ← stores 3 (not 1) into DEMO.STATUS
```
Both 1 and 3 are non-zero (real-game mode); behaviour is identical in all conditionals.
The C uses `demo_status = START_MODE` to exactly match.

---

### draw_map() — Horizontal Scroll (DO.X)

```c
if (chopper_x > MIN_RIGHT) {
    chopper_x = MIN_RIGHT;
    if (sx >= (uint8_t)(0xD8u + 1u)) sx = (uint8_t)(1u + 1u);  /* wrap */
    sx_f--;
    if ((sx_f & 3u) == 3u) sx++;   /* tile advance when sx_f wraps from 0→255 */
} else if (chopper_x < MIN_LEFT) {
    chopper_x = MIN_LEFT;
    if (sx < (uint8_t)(1u + 1u + 1u)) sx = (uint8_t)(0xD8u + 1u);
    sx_f++;
    if ((sx_f & 3u) == 0u) sx--;   /* tile advance when sx_f carries from 3→4 */
}
```

Assembly verifies tile boundary:
- Right: `LDA SX.F; AND #3; EOR #3; BNE .2; INC SX` — INC when `(sx_f & 3) XOR 3 == 0` = `sx_f & 3 == 3`
- Left: `LDA SX.F; AND #3; BNE .4; DEC SX` — DEC when `sx_f & 3 == 0`

### draw_map() — Vertical Scroll (DO.Y)

The `y_check` variable tracks the X register value in assembly (loaded once at `.80`):
```c
if (sy != 24u && chopper_y > MIN_DOWN) {
    chopper_y = MIN_DOWN;
    need_down = 1;
    y_check = MIN_DOWN;           /* X register after forced clamp */
} else {
    y_check = chopper_y;          /* X register from .80: LDX CHOPPER.Y */
    if (chopper_y > MAX_DOWN) {
        chopper_y = MAX_DOWN;
        if ((sy_f & 7u) != 0u || sy != 24u) need_down = 1;
    }
}

if (!need_down) {
    if (sy != (uint8_t)0xFFu && y_check < MIN_UP) {
        chopper_y = MIN_UP; need_up = 1;
    } else if (y_check < MAX_UP) {
        chopper_y = MAX_UP;
        if ((sy_f & 7u) != 7u || sy != (uint8_t)0xFFu) need_up = 1;
    }
}

if (need_down)      { sy_f++; if ((sy_f & 7u) == 0u) sy++; }
else if (need_up)   { sy_f--; if ((sy_f & 7u) == 7u) sy--; }
```

Sub-pixel tile boundary:
- Down: `INC SY.F; if (SY.F & 7 == 0) INC SY` — every 8 sub-pixels, new tile row
- Up: `DEC SY.F; if (SY.F & 7 == 7) DEC SY` — underflow of 3-bit field = retreat row

### Display-List Fill

```c
HSCROL_HW = sx_f & 3u;
VSCROL_HW = sy_f & 7u;
temp1_i = sx; temp2_i = sy;
compute_map_adr_i();
for (uint8_t row = 0u; row < MAP_LINES; row++) {
    dsp[row * 3u + 1u] = adr1_i_lo;
    dsp[row * 3u + 2u] = adr1_i_hi;
    adr1_i_hi++;           /* each map row = 1 page = adr_hi+1 */
}
```

Assembly uses `INX` (X=1,2,4,5,7,8,...) to step through the 3-byte display list entries,
writing only bytes at offsets 1 and 2 (lo/hi address). Byte 0 (the ANTIC command byte) was
set at boot and never changes. In C: `row*3+1`, `row*3+2`.

---

### ddig() and draw() — BCD Digit Display

```c
void ddig(uint8_t val) {
    uint8_t y = val;
    /* Hi-nibble suppression */
    if (!s_flg) {
        if (y & 0xF0u) s_flg = 1u;
        else           y |= 0xA0u;  /* mark hi as blank (0xA) */
    } else { s_flg = 1u; }
    /* Lo-nibble suppression */
    if (!s_flg) {
        if (y & 0x0Fu) s_flg = 1u;
        else           y |= 0x0Au;  /* mark lo as blank (0xA) */
    } else { s_flg = 1u; }
    draw(y >> 4u);
    draw(y & 0x0Fu);
}

static void draw(uint8_t digit) {
    if (digit == 0x0Au) {
        digit = (ddig_x == 0u) ? 0u : 0x70u;  /* ones position: show '0'; others: blank */
    }
    uint8_t hi = (uint8_t)(digit + 0x90u);
    uint8_t *p = (uint8_t *)(uintptr_t)((uint16_t)s_adr_hi << 8 | s_adr_lo);
    p[0] = hi;
    uint8_t lo = (hi == 0x90u) ? (uint8_t)(0x0Au + 0x80u) : (uint8_t)(hi & 0x8Fu);
    p[1] = lo;
    uint16_t adr = (uint16_t)((uint16_t)s_adr_hi << 8 | s_adr_lo) + 2u;
    s_adr_lo = (uint8_t)(adr & 0xFFu);
    s_adr_hi = (uint8_t)(adr >> 8);
    ddig_x--;
}
```

**Blank byte value:** Assembly `0xF0+128 = 0x170`, truncated to `0x70`.
Then `0x70 + 0x90 = 0x100` → stored as `0x00`. Both bytes = `0x00` = invisible space.

**Digit '0' second byte:** `CMP #0x90; BNE .2; LDA #0x0A+128 = 0x8A`. Special case: digit 0
writes `[0x90, 0x8A]` rather than `[0x90, 0x90 & 0x8F = 0x80]`. This gives the correct
Atari screen code for the lower half of the '0' glyph.

---

### inc_score()

```c
void inc_score(uint8_t hi, uint8_t lo) {
    if (!demo_status) return;   /* demo mode: no scoring */
    uint8_t carry = 0u;
    score1 = bcd_add(score1, lo, &carry);
    score2 = bcd_add(score2, hi, &carry);
    score3 = bcd_add(score3, 0u, &carry);
}
```

Assembly uses X=lo, Y=hi register convention. C function signature reverses the order
to `(hi, lo)` for readability, with explicit variable names for clarity.

---

## Bugs Found and Fixed

### Bug 1 — draw_map: vertical scroll `y_check` not set on FORCED path

**Root cause:** Initial draft set `y_check = chopper_y` unconditionally before the conditional
block. When the FORCED path executed (`sy != 24 && chopper_y > MIN_DOWN`), `chopper_y` was
clamped to `MIN_DOWN` and then `y_check` was set to the new (clamped) value — but the
initial assignment had already captured the pre-clamp value. This made `y_check` stale on
the FORCED path, potentially triggering a spurious up-scroll in the same frame.

**Assembly:** The X register is loaded from CHOPPER.Y AFTER the clamp (`LDX #MIN.DOWN;
STX CHOPPER.Y` — then X = MIN_DOWN used in all subsequent comparisons). The C must track
what value X holds at the `.3` label.

**Fix:** Set `y_check` INSIDE each branch immediately after the clamp:
```c
if (sy != 24u && chopper_y > MIN_DOWN) {
    chopper_y = MIN_DOWN;
    y_check = MIN_DOWN;         /* ← set to clamped value, matching X = MIN_DOWN */
    need_down = 1;
} else {
    y_check = chopper_y;        /* ← set to unclamped value */
    ...
}
```

---

### Bug 2 — do_laser_2: OFF path incorrectly cleared LASER_3

**Root cause:** Initial draft copied the OFF path from `do_laser_1` verbatim, including the
`LASER_3` clear:
```c
/* WRONG — laser_2's OFF path */
for (int i = 31; i >= 0; i--) LASERS_2[i] = 0u;
for (int i = 7;  i >= 0; i--) LASER_3[i]  = 0u;  /* ← should NOT be here */
```

**Assembly DO.LASER.2 OFF path:**
```asm
.2  LDX #32-1; LDA #0
.3  STA LASERS.2,X; DEX; BPL .3    ; clear 32 bytes of LASERS.2 only
.4  RTS                              ; NO LASER.3 clear
```

`DO.LASER.1` clears both LASERS.1 and LASER.3 when OFF. `DO.LASER.2` clears only
LASERS.2 when OFF. LASER.3 represents the "fort center" character and is owned by
DO.LASER.1.

**Fix:**
```c
void do_laser_2(void) {
    if (FRAME_HW & 7u) return;
    if (laser_status == STATUS_OFF) {
        for (int i = 31; i >= 0; i--) LASERS_2[i] = 0u;  /* LASERS.2 only */
        return;
    }
    ...
}
```

---

### Bug 3 — ddig: `s_flg` set unconditionally in `else` branch for hi nibble

**Root cause:** Initial draft:
```c
if (!s_flg) {
    if (y & 0xF0u) { s_flg = 1u; }
    else           { y |= 0xA0u; s_flg = 1u; }  /* ← wrong: should NOT set flag */
}
```
Setting `s_flg = 1u` after blanking the hi nibble would cause the lo nibble to always
be shown (never blanked), breaking leading-zero suppression for values like `score = 0x0005`.

**Assembly trace:** After `ORA #$A0; TAY`, the code does `BNE .2` — branches PAST `.1`
which sets `S.FLG = 1`. The OR result is always non-zero (`0xA0` != 0), so `.1` is skipped.

**Fix:**
```c
if (!s_flg) {
    if (y & 0xF0u) s_flg = 1u;
    else           y |= 0xA0u;   /* flag stays 0 — lo nibble may also blank */
}
```

---

## Compile Test

### Command
```
cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src \
    dev/src/fort2.c dev/src/fort3.c dev/src/fort4.c
```

### Round 1 (after initial draft)
```
dev/src/fort4.c:468: warning: implicit conversion loses integer precision
dev/src/fort4.c:524: error: conflicting types for 'draw'
```
The `draw` static function was placed after its first call site. Fixed by moving `draw()`
before `ddig()` in source order.

### Round 2 (after all fixes)
```
(no output — zero diagnostics)
```

---

## Design Decisions

| Decision | Rationale |
|---|---|
| `ddig_x` as a separate global | Assembly uses X register as digit-position counter. Existing fort1.c sets `demo_count = 0` before ddig calls (incorrect naming). `ddig_x` is the correct name; `ddig_x = 0` = ones position (last digit, always show) |
| `laser_shapes[]` as `extern` | Defined in fort6.s, not yet converted. Syntax check passes; link-time error deferred until fort6.c is written |
| `elevators[4]` with literal addresses | `static uint8_t * const elevators[4] = {(uint8_t*)0x0C70u,...}` — pre-computed `CHR_SET2 + 112/120/128/136`. Using integer-literal casts avoids strict-C non-const-init issues with pointer arithmetic |
| `CHR_SET2` as writable `uint8_t *` | fort3.c defines `CHR_SET2` as `const uint8_t *` (read-only). fort4.c writes to LASERS, BLOCKS, EXPLOSION etc., so it defines its own writable version. Different TUs, no ODR conflict |
| `bcd_add` / `bcd_sub` as static helpers | 6502 SED/CLD instructions have no C equivalent. Per-nibble carry/borrow helpers are straightforward and chain correctly |
| `do_stick` and `do_n` as `static` | Internal helpers not referenced from other TUs. Matches the assembly's use of internal labels |
| `y_check` tracking in draw_map | Mirrors the assembly X register which is loaded once and reused across the `.3`/`.81` labels even after CHOPPER.Y is clamped |
| `demo_status = START_MODE` (= 3) | Assembly `LDA #1` is commented out, so A retains `START.MODE = 3`. Exact value matches assembly; both 1 and 3 are non-zero (real-game) |

---

## File Summary

| File | Lines | Purpose |
|------|-------|---------|
| `dev/src/fort4.h` | 45 | Scanner, joystick, fire, scroll, laser/block/elevator/explosion/score declarations |
| `dev/src/fort4.c` | 605 | Full interrupt driver part II: all 22 functions + static helpers + BCD arithmetic |
