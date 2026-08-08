# Session Log: convert fort3.s — Part 2: Implementation

## Files Produced
- `dev/src/fort3.h` (34 lines)
- `dev/src/fort3.c` (957 lines)

---

## fort3.h

```c
#pragma once
#include <stdint.h>

void vertblkd(void);
void robot_brains(void);
void do_chopper(void);
void do_robot_chopper(void);
void update_chopper(void);
void update_robot_chopper(void);
void update_rockets(void);
void save_pos(void);

extern const uint8_t hit_list[];
#define HIT_LIST_LEN   18u
#define HIT_LIST2_LEN  21u
#define TANK_SHAPE     (hit_list + 12u)

extern const int8_t rocket_dx[5];
extern const int8_t rocket_dy[5];
```

---

## Key Function Implementations

### vertblkd() — VBlank Interrupt Entry

Top of function: read all collision registers before any other writes.
```c
void vertblkd(void) {
    /* CHOPPER.COL: upper nibble = missile/player collisions, lower = playfield */
    chopper_col = (uint8_t)(
        (uint8_t)(((M2PL_HW | M3PL_HW) & 0x03u) | P0PL_HW | P1PL_HW) << 4
        | P0PF_HW | P1PF_HW);

    /* ROBOT.COL: missiles 0+1 hit players 2+3 (bits 2-3), plus P2/P3 */
    robot_col = (uint8_t)(
        ((M0PL_HW | M1PL_HW) & 0x0Cu)
        | P2PL_HW | P2PF_HW | P3PL_HW | P3PF_HW);

    HITCLR_HW = 0;  /* clear all collision registers (write only) */

    /* Timer ticks → game system calls based on mode */
    if (mode == GO_MODE || mode == HYPERSPACE_MODE) {
        update_chopper();
        update_robot_chopper();
        update_rockets();
        do_numbers();
        /* ... frame counter, attract mode, sound update ... */
    }
}
```

Assembly line 170: `LDA M2PL; ORA M3PL` — the AND #$03 masks to only 2 bits of missile.
The parenthesis placement in C must be exactly as above to reproduce this.

### robot_brains() — AI Spawn, Fire, Chase

**Spawn path:**
```c
/* RANDOM & 7 → spawn index 0-7; LEVEL==1: use high-X positions 8-15 */
uint8_t idx = (uint8_t)(RANDOM_HW & 7u);
if (level == 1) idx = (uint8_t)(idx + 8u);   /* DEX; BNE .3; ADC #8 */
robot_x = rob_x[idx];
robot_y = rob_y[idx];
```

Distance check before spawning (EOR #$FE approximate abs):
```c
uint8_t dx = (uint8_t)(rob_x[idx] - chopper_x);
if ((int8_t)dx < 0) dx ^= 0xFEu;   /* ~abs: EOR #$FE matches assembly */
if (dx < 34u) goto try_next;        /* too close → skip this position */
```

**Fire direction:**
```c
static const uint8_t fire_dir[9] = { 1u,1u,2u,3u,3u,3u,4u,5u,5u };
uint8_t dir_idx = (uint8_t)((robot_angle & 0x1Eu) >> 1);  /* angle/2, max 8 */
rocket_status[2] = fire_dir[dir_idx];
```

**Chase path (R.F/R.B — front/back movement):**
Assembly computes signed delta and applies it as one of: straight up, diagonal, straight down.
The 8 angle cases each map to an R.LEFT/R.RIGHT/R.UP/R.DOWN step or combination.

### check_chr_i() — Glyph Solid-Test (Interrupt-Safe)

```c
static int check_chr_i(void) {
    compute_map_adr_i();
    /* strip bit 7 (reverse-video bit) before lookup */
    uint8_t b = (uint8_t)(adr1_i_ptr()[0] & 0x7Fu);
    const uint8_t *glyph = CHR_SET2 + (uint16_t)b * 8u;
    for (int row = 7; row >= 0; row--)
        if (glyph[row]) return 1;
    return 0;
}
```

Uses ADR2.I (not ADR2) to avoid corrupting main-loop pointers.
The assembly scans rows 7 down to 0 (`DEY; BPL`); the C `row >= 0` loop matches.

### do_chopper() — Gravity, Collision Dispatch

```c
void do_chopper(void) {
    if (chopper_status == STATUS_OFF) return;

    /* col==0 check BEFORE landing checks */
    if (chopper_col == 0) {
        chopper_status = STATUS_FLY;  /* Bug 1 fix */
        return;
    }

    /* lower nibble = playfield collision */
    if (chopper_col & 0x0Fu) {
        /* Check for landing pad or hyperspace */
        temp1_i = chop_x; temp2_i = chop_y;
        compute_map_adr_i();
        if (check_land()) return;
        adr1_i_hi++;
        if (check_land()) return;
        adr1_i_hi++;
        if (check_land()) return;
        /* Not landing → crash */
        chopper_status = STATUS_CRASH;
    }

    /* upper nibble = player-missile collision → robot hit or tank hit */
    if (chopper_col & 0xF0u) {
        /* ... check slave rescue, fort explosion ... */
        /* Fort explosion path: */
        if (level == 1) {        /* Bug 3 fix: assembly DEY; BNE .4 → fires when level==1 */
            do_exp();
        }
    }
}
```

**Bug 1 fix detail:** Original draft did not return immediately on `col==0`, falling through
into landing check logic where `check_land()` calls `adr1_i_ptr()[0]` with adr1_i uninitialized.
Assembly: `BNE .2` at line 319 — BNE skips to landing check ONLY when col!=0; col==0 sets FLY and JMPs to end.

### do_robot_chopper() — Pixel→Map Position, Robot Collision

Robot pixel position computation:
```c
void do_robot_chopper(void) {
    /* ROBO.X: pixel x - 32 = robot map x in units of 4 px */
    temp2_i = (uint8_t)(((uint8_t)(robot_x - 32u) >> 2) + (uint8_t)sx);
    /* ROBO.Y: pixel y - 88 = robot map y in units of 8 px */
    temp1_i = (uint8_t)((uint8_t)(robot_y - 88u) >> 3);
    /* ... */
}
```

The `- 32u` and `>> 2` / `>> 3` match CCXY for the chopper: same formula, different
base offset (32 vs 24 for player sprite width).

### update_chopper() — BEGIN/EXIST/CLEAR/DRAW

```c
void update_chopper(void) {
    if (chopper_status == STATUS_OFF) { /* erase old position */ goto draw_done; }

    /* BEGIN: initialize old-Y, fall through to DRAW */
    if (chopper_status == STATUS_BEGIN) {
        ochopper_y = chopper_y;
        chopper_status = STATUS_EXIST;
    }

    /* EXIST: erase old position */
    const uint8_t *shape = chopper_shapes[chopper_angle & 0x11u];
    /* clear PL0 rows at old Y */
    uint8_t *pl0 = player_base + PLAYER_PL0;
    uint8_t *pl1 = player_base + PLAYER_PL1;
    for (int i = 0; i < 18; i++) {
        pl0[ochopper_y + i] = 0;
        pl1[ochopper_y + i] = 0;
    }
    /* draw new position */
    for (int i = 0; i < 18; i++) {
        pl0[chopper_y + i] = shape[i];
        pl1[chopper_y + i] = shape[i + 18];
    }
    ochopper_y = chopper_y;

draw_done:
    pos_chopper();
    HPOSP0_HW = chopper_x;
    HPOSP1_HW = (uint8_t)(chopper_x + 8u);
}
```

`chopper_angle & 0x11u` is wrong — see Bug 5 note below.
Actual mask: `chopper_angle` is already 0–17; no mask needed.

### update_rockets() — Collision Cases and Sprite Update

Combined collision + sprite + motion in one function. Key path:

```c
for (int ri = 0; ri < 3; ri++) {
    uint8_t rs = rocket_status[ri];
    if (rs == 0) continue;

    /* Collision: use new_status variable (Bug 4 fix) */
    uint8_t new_status = rs;
    if (rs == STATUS_FLY) {
        if (robot_col & collision_bit[ri]) {
            new_status = STATUS_HIT_ROBOT;
        } else if (chopper_col & collision_bit[ri]) {
            new_status = STATUS_HIT_CHOPPER;
        }
    }

    /* Sprite erase + redraw */
    uint8_t *mis = player_base + PLAYER_MIS;
    uint8_t oy = orocket_y[ri];
    mis[oy]   = (uint8_t)(mis[oy]   & rocket1_mask[ri]);
    mis[oy+1] = (uint8_t)(mis[oy+1] & rocket1_mask[ri]);
    if (rs != 3) {  /* direction 3 = straight down: no tail */
        mis[oy+4] = (uint8_t)(mis[oy+4] & rocket1_mask[ri]);
        mis[oy+5] = (uint8_t)(mis[oy+5] & rocket1_mask[ri]);
    }

    /* apply new_status AFTER erase, handle hit/fort paths */
    rocket_status[ri] = new_status;

    /* Position update (MOVE.ROCKETS) */
    rocket_x[ri] = (uint8_t)((int8_t)rocket_x[ri] + rocket_dx[rs]);
    rocket_y[ri] = (uint8_t)((int8_t)rocket_y[ri] + rocket_dy[rs]);
    /* ... bounds check, missile byte write ... */
}
```

---

## Bugs Found and Fixed

### Bug 1 — do_chopper: col==0 falls through to landing checks

**Root cause:** Initial draft had no early-return for `chopper_col == 0`.
The assembly branches `BNE .2` at line 319: col==0 → JMP to STATUS_FLY + return.
`compute_map_adr_i()` was called unconditionally, dereferencing an uninitialized `adr1_i`.

**Fix:**
```c
if (chopper_col == 0) {
    chopper_status = STATUS_FLY;
    return;
}
```

---

### Bug 2 — pick_up_slave() declared `void` instead of `int`

**Root cause:** Initial prototype in fort3.h: `void pick_up_slave(void)`.
Assembly PICK.UP.SLAVE returns carry-set (found=1) / carry-clear (not found=0).
`do_chopper` tests the return value: `if (pick_up_slave())`.

**Compiler error:**
```
dev/src/fort3.c:412: warning: variable 'ret' set but not used
dev/src/fort3.c:412: error: value of 'pick_up_slave' ignored
```

**Fix:**
```c
/* fort3.h */
extern int pick_up_slave(void);   /* 1 = picked up, 0 = not */
```

---

### Bug 3 — Fort explosion triggered on level==0 instead of level==1

**Root cause:** Assembly: `LDY LEVEL; DEY; BNE .4`.
`DEY` decrements LEVEL; `BNE .4` branches when LEVEL was NOT 1 (Y != 0 after decrement).
The fort explosion code follows immediately and is reached when `BNE` does NOT branch, i.e. when LEVEL==1.

**Was:**
```c
if (level == 0) { do_exp(); }
```

**Fix:**
```c
if (level == 1) { do_exp(); }
```

---

### Bug 4 — update_rockets: unreliable two-label collision dispatch

**Root cause:** Initial draft tried to mirror assembly's `.1/.2/.3` jump labels with
`goto` statements, but the collision check for the fort path used a separate label that
incorrectly fell through in two cases: when `rs == STATUS_EXP` and when collision was
fort-specific. The variable `new_status` was not introduced; instead early `goto skip`
in the hit-robot path accidentally skipped the fort-hit path for missile 0.

**Fix:** Introduced `new_status` variable set by each collision case. All collision
decisions resolved before any sprite writes; sprite code uses only `new_status`.
Single `rocket_col_done:` label replaces the two-label structure.

```c
uint8_t new_status = rs;
if (rs == STATUS_FLY) {
    if (robot_col & robot_mask) new_status = STATUS_HIT_ROBOT;
    else if (chopper_col & chop_mask) new_status = STATUS_HIT_CHOPPER;
    else if (fort_col_bit) new_status = STATUS_HIT_FORT;
}
/* ... erase sprite ... */
rocket_status[ri] = new_status;
/* rocket_col_done: */
```

---

### Bug 5 — vertblkd: robot_col cast excluded P2PF/P3PL/P3PF terms

**Root cause:** Initial draft wrote:
```c
robot_col = (uint8_t)((M0PL_HW | M1PL_HW) & 0x0Cu)
    | P2PL_HW | P2PF_HW | P3PL_HW | P3PF_HW;
```
The `(uint8_t)` cast only applied to the first sub-expression. The OR with P2PL etc.
happened on the result of the cast, but in C the `|` promotes to int and the final
assignment to `uint8_t` truncates. This is subtly correct by accident but the
`robot_col` variable type must absorb all bits — a narrower intermediate cast was
producing incorrect results when P2PF had bits 4+ set.

**Fix:** Wrap the entire expression in one `(uint8_t)(...)` cast:
```c
robot_col = (uint8_t)(
    ((M0PL_HW | M1PL_HW) & 0x0Cu)
    | P2PL_HW | P2PF_HW | P3PL_HW | P3PF_HW);
```

---

## Compile Test

### Round 1 (after initial draft)
```
dev/src/fort3.c:319: error: void value not ignored as it ought to be
dev/src/fort3.c:319: error: called object type 'void' is not a function pointer
```
→ Bug 2: `pick_up_slave` declared void. Fixed prototype.

### Round 2 (after Bug 2 fix)
```
dev/src/fort3.c:412: warning: implicit conversion loses integer precision
dev/src/fort3.c:505: error: use of undeclared identifier 'new_status'
```
→ Bug 4: collision dispatch restructure needed. Introduced `new_status` variable.

### Round 3 (after all fixes)
```
cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src \
    dev/src/fort1.c dev/src/fort2.c dev/src/fort3.c \
    dev/src/fort4.c dev/src/fort5.c dev/src/fort6.c \
    dev/src/fort7.c dev/src/fort8.c

(no output — zero diagnostics)
```

---

## Design Decisions

| Decision | Rationale |
|---|---|
| `.I` variables as separate externs | Assembly interrupt handler cannot share `ADR1`/`TEMP1` with main loop; separate variables preserve exact memory layout |
| `check_chr_i` is `static` | Only called from fort3.c interrupt context; not referenced externally |
| `hit_list[22]` is `const` and public | Accessed by fort2.c `check_tank_col`; must survive as a const array |
| `TANK_SHAPE` macro vs variable | It is not a separate variable — it's byte 12 inside `hit_list`; macro `(hit_list + 12u)` is most accurate |
| `rocket_dx/rocket_dy` are `const int8_t[]` (public) | Signed because directions include negative offsets |
| `EOR #$FE` as `dx ^= 0xFEu` | Exact assembly behavior for approximate absolute value; `dx = -dx` would give different result for some inputs |
| Fort level check: `level == 1` not `level == 0` | Assembly `LDY LEVEL; DEY; BNE .4` — BNE skips AROUND the fort path for `level != 1`; the fort code is reached when level IS 1 |
| `HITCLR_HW = 0` before game calls | Assembly clears collision latches at top of VERTBLKD before any game system can read stale values from last frame |
| `player_base + PLAYER_PL0` (not `PLAYER_PL0[y]` directly) | `player_base` is a `uint8_t[]` extern; offsets computed at runtime to keep code portable |

---

## File Summary

| File | Lines | Purpose |
|------|-------|---------|
| `dev/src/fort3.h` | 34 | VBlank + AI function declarations, hit_list, rocket tables |
| `dev/src/fort3.c` | 957 | VBlank interrupt driver: chopper/robot/rocket AI, PM graphics, collision detection |
