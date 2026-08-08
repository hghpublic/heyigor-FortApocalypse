# Session Log: convert fort6.s — Part 2: Implementation & Testing

## Files Produced
- `dev/src/fort6.h` (21 lines)
- `dev/src/fort6.c` (223 lines)
- `dev/src/fort5.c` — patched: added `land_chr[5]` definition
- `dev/src/fort5.h` — patched: added extern declaration for `land_chr`

---

## fort6.h

```c
#pragma once
#include <stdint.h>

/* CHOPPER.SHAPES — 18 shape pointers, indexed by chopper_angle (0-17).
 * Each entry: 36 bytes — [0..17] = player 0 left half, [18..35] = right half. */
extern const uint8_t * const chopper_shapes[18];

/* LASER.SHAPES — 4 × 8 bytes = 32 bytes of laser animation patterns */
extern const uint8_t laser_shapes[32];

/* ANTIC display lists (JMP self-reference bytes patched by fort6_init) */
extern uint8_t dsp_lst2[39];
extern uint8_t dsp_lst3[32];

/* Patch self-referential JMP addresses in display lists at runtime */
void fort6_init(void);
```

---

## fort6.c

### File Header and Includes

```c
/* fort6.c — Sprite shapes, display lists, ROM init data
 * (converted from fort6.s)
 *
 * Shape layout: each entry is 36 bytes — [0..17] = player left half (PL0/PL2),
 * [18..35] = player right half (PL1/PL3).  The assembler source has commented-out
 * "; .HS 0000" / "; .HS 00" padding bytes at the end of each second sub-block; those
 * zeros are included here to reach the 36-byte stride used by update_chopper() /
 * update_robot_chopper() in fort3.c.
 */

#include <stdint.h>
#include "fort6.h"
```

### Static Shape Arrays (18 × 36 bytes)

Each is `static const uint8_t[36]`. Only shown here in condensed form; the actual file lays them out with 7+7+4 / 7+7+4 grouping matching the assembly sub-blocks.

```c
static const uint8_t shape_cl3_1[36] = {
    0x00,0x00,0x03,0x3D,0xC1,0x03,0x0D,
    0x11,0x21,0x23,0x6E,0x79,0x67,0x3E,
    0x10,0x11,0x9E,0x60,
    0x00,0x00,0x80,0x02,0x02,0x02,0x8E,
    0xFF,0x39,0x61,0x61,0xC0,0xC0,0x60,
    0x3C,0xE0,0x00,0x00,
};
/* ... shape_cl3_2, shape_cl2_1, shape_cl2_2, shape_cl1_1, shape_cl1_2 ... */
/* ... shape_cm1_1, shape_cm1_2 ... */
/* ... shape_cr1_1, shape_cr1_2, shape_cr2_1, shape_cr2_2 ... */

/* CR3.1 — no padding needed (second sub-block is naturally 18 bytes) */
static const uint8_t shape_cr3_1[36] = {
    0x00,0x00,0x01,0x40,0x40,0x40,0x71,
    0xFF,0x9C,0x86,0x86,0x03,0x03,0x06,
    0x3C,0x07,0x00,0x00,
    0x00,0x00,0xC0,0xBC,0x83,0xC0,0xB0,
    0x88,0x84,0xC4,0x76,0x9E,0xE6,0x7C,
    0x08,0x88,0x79,0x06,
};

/* CR3.2 — BOOT.STUFF label is at byte offset 25 (PL1[7]).
 * CART.START copies 128 bytes from BOOT.STUFF to $01C0; in the C port
 * that is assembly-only startup; no separate C symbol is needed. */
static const uint8_t shape_cr3_2[36] = {
    0x60,0x1E,0x01,0x80,0x80,0x80,0xF1,
    0x7F,0x5C,0x46,0x46,0x03,0x03,0x06,
    0x3C,0x07,0x00,0x00,
    0x00,0x00,0xC0,0x80,0x80,0xC0,0xB0,
    0x88,0x84,0xC4,0x76,0x9E,0xE6,0x7C,
    0x08,0x88,0x79,0x06,
};
```

### CHOPPER.SHAPES Pointer Table

```c
/* CHOPPER.SHAPES — 9 angle pairs × (frame1, frame2)
 * Assembly order: CL3, CL2, CL1, CM1×3, CR1, CR2, CR3 */
const uint8_t * const chopper_shapes[18] = {
    shape_cl3_1, shape_cl3_2,   /* angle  0 */
    shape_cl2_1, shape_cl2_2,   /* angle  2 */
    shape_cl1_1, shape_cl1_2,   /* angle  4 */
    shape_cm1_1, shape_cm1_2,   /* angle  6 */
    shape_cm1_1, shape_cm1_2,   /* angle  8  (CM1 repeated) */
    shape_cm1_1, shape_cm1_2,   /* angle 10  (CM1 repeated) */
    shape_cr1_1, shape_cr1_2,   /* angle 12 */
    shape_cr2_1, shape_cr2_2,   /* angle 14 */
    shape_cr3_1, shape_cr3_2,   /* angle 16 */
};
```

**Note on CM1 repetition:** Angles 6, 8, and 10 all map to the same two center shapes. The pointer table has three identical pairs pointing to `shape_cm1_1` and `shape_cm1_2`. This is a direct translation of the assembly `.DA CM1.1, CM1.2` repeated three times.

### laser_shapes

```c
/* LASER.SHAPES — 4 patterns × 8 bytes
 *   [0..7]   diagonal ↘
 *   [8..15]  diagonal ↗
 *   [16..23] horizontal bar
 *   [24..31] vertical bar */
const uint8_t laser_shapes[32] = {
    0xC0,0xC0,0x30,0x30,0x0C,0x0C,0x03,0x03,
    0x03,0x03,0x0C,0x0C,0x30,0x30,0xC0,0xC0,
    0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,
    0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,
};
```

### Display Lists

```c
/* DSP.LST2 — options/panel display list (39 bytes)
 * Bytes [37:38] = JMP target address, patched by fort6_init(). */
uint8_t dsp_lst2[39] = {
    0x70,0x70,0x80,0x70,
    0x44,0x00,0x01,           /* LMS mode-4; PANEL=$0100 */
    0x04,0x04,0x04,0x04,0x70,
    0x70,
    0x80,0x50,0x20,
    0x44,0x00,0x03,           /* LMS mode-4; PLAY.SCRN=$0300 */
    0x04,0x04,0x04,0x04,0x04,0x04,0x04,
    0x04,0x04,0x04,0x04,0x04,0x04,0x04,
    0x80,0x70,0x80,
    0x41,0x00,0x00,           /* JMP dsp_lst2 — address patched by fort6_init */
};

/* DSP.LST3 — title/play display list (32 bytes)
 * Bytes [30:31] = JMP target address, patched by fort6_init(). */
uint8_t dsp_lst3[32] = {
    0x70,0x70,0x70,0x70,0x70,0x70,
    0x44,0x00,0x03,           /* LMS mode-4; PLAY.SCRN=$0300 */
    0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
    0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
    0x44,0x00,0x03,           /* LMS mode-4; PLAY.SCRN=$0300 */
    0x41,0x00,0x00,           /* JMP dsp_lst3 — address patched by fort6_init */
};
```

### fort6_init

```c
/* Patch self-referential JMP target bytes so ANTIC loops both display lists. */
void fort6_init(void)
{
    uintptr_t a2 = (uintptr_t)dsp_lst2;
    dsp_lst2[37] = (uint8_t)(a2 & 0xFFu);
    dsp_lst2[38] = (uint8_t)((a2 >> 8) & 0xFFu);

    uintptr_t a3 = (uintptr_t)dsp_lst3;
    dsp_lst3[30] = (uint8_t)(a3 & 0xFFu);
    dsp_lst3[31] = (uint8_t)((a3 >> 8) & 0xFFu);
}
```

---

## fort5.c / fort5.h Patch — land_chr

### Problem

`fort3.c` line 137 declares:
```c
extern const uint8_t land_chr[5];
```
Used at line 312:
```c
if (b == land_chr[i]) return;   /* landing tile: OK, no increment */
```

`land_chr` was not defined anywhere in the existing C files. The previous `fort5.c` conversion included `slave_chr_bl[2]` and `slave_chr_br[2]` but omitted the full `land_chr[5]` array (which in the assembly is the same address as `SLAVE.CHR.B.L` and spans the full sequence `3E 3D 3B 3C 44`).

This was a latent link error: the syntax check passed before because `fort3.c` was not in the compile line previously, but adding `fort3.c` would fail.

### Root Cause

In fort5.s, `SLAVE.CHR.B.L = LAND.CHR` — the label `LAND.CHR` starts at the same address as the bottom-left slave character. The full 5-byte sequence:
```
LAND.CHR / SLAVE.CHR.B.L:  3E 3D  (bot-left chars, frames 0 and 1)
SLAVE.CHR.B.R:              3B 3C  (bot-right chars, frames 0 and 1)
                             44     (additional landing tile char)
```

The C conversion split these into separate `slave_chr_bl[2]` and `slave_chr_br[2]` arrays. `land_chr[5]` was never added.

### Fix Applied to fort5.c

Added after `slave_chr_br` at line 111:

```c
/* LAND.CHR — landing-pad tile characters; same bytes as SLAVE.CHR.B.L+B.R+0x44
 * fort3.c iterates this array to decide whether a tile is a safe landing tile. */
const uint8_t land_chr[5] = { 0x3E, 0x3D, 0x3B, 0x3C, 0x44 };
```

Key: **not** `static` — must be externally visible.

### Fix Applied to fort5.h

Added at the end:

```c
/* Landing-pad tile characters — checked by fort3.c save_pos() */
extern const uint8_t land_chr[5];
```

---

## Compile Test

### Command
```
cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src \
    dev/src/fort2.c dev/src/fort3.c dev/src/fort4.c \
    dev/src/fort5.c dev/src/fort6.c
```

### Result
```
(no output — zero diagnostics)
```

All five translation units compile clean with no warnings or errors.

---

## Resolved Extern References After fort6.c

| Symbol | Declared in | Defined in | Status |
|---|---|---|---|
| `chopper_shapes[18]` | fort3.c:134 | fort6.c | ✓ resolved |
| `laser_shapes[32]` | fort4.c:127 | fort6.c | ✓ resolved |
| `land_chr[5]` | fort3.c:137 | fort5.c | ✓ resolved (this session) |
| `dsp_lst2[]` | fort6.h | fort6.c | ✓ available |
| `dsp_lst3[]` | fort6.h | fort6.c | ✓ available |

---

## Remaining Open Items

| Item | Source | Will be in |
|---|---|---|
| `player_base[]` | fort3.c:47 extern | fort7.c or fort8.c |
| `scan_adr1`, `scan_adr2` | fort5.c externs | fort7.c |
| `fuel_temp`, `tim4_val`, `tim9_val` | fort5.c externs | fort7.c |
| `temp5`, `temp6` | fort5.c externs | fort7.c |
| Main display list `dsp_lst1` | fort1.c | fort8.c |

Next conversion target: **fort7.s** (variable / BSS definitions) and **fort8.s** (display list init data).

---

## File Summary

| File | Lines | Purpose |
|---|---|---|
| `dev/src/fort6.h` | 21 | Public declarations for shapes, display lists, init |
| `dev/src/fort6.c` | 223 | All shape data, pointer table, laser patterns, DLs |
| `dev/src/fort5.c` | 724 | +3 lines: added `land_chr[5]` definition |
| `dev/src/fort5.h` | 38 | +3 lines: added `extern land_chr[5]` |
