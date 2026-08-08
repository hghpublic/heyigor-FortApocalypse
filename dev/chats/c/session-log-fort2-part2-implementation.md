# Session Log: convert fort2.s — Part 2: Implementation

## Files Produced
- `dev/src/fort2.h` (26 lines)
- `dev/src/fort2.c` (1031 lines)

---

## fort2.h

```c
#pragma once
#include <stdint.h>

void read_user(void);
void check_options(void);

void move_pods(void);
void move_cruise_missiles(void);
void check_hyper_chamber(void);
void move_tanks(void);

void screen_on(void);
void screen_off(void);
void clear_sounds(void);

void ccl(void);           /* compute ADR1 = PLAY_SCRN + temp2*40 + temp1 */
void print(void);         /* print string: adr1=str, temp1=col, temp2=row */
void give_bonus(void);
void wait_frame(uint8_t n);
void clear_info(void);
```

---

## Key Function Implementations

### CCL — Screen Address Computation
Assembly computes `ADR1 = PLAY_SCRN + temp2*40 + temp1` using MULT.BY.40 (shift/add)
after saving/restoring temp1 and temp2. In C: direct arithmetic, no save/restore needed:
```c
void ccl(void) {
    uint16_t a = (uint16_t)(uintptr_t)PLAY_SCRN
               + (uint16_t)temp2 * 40u + temp1;
    adr1_lo = (uint8_t)(a & 0xFF);
    adr1_hi = (uint8_t)(a >> 8);
}
```

### PRINT — Tile-Pair String Renderer
Each non-space character writes two bytes: the char and the char+32 (ANTIC mode-4
tile pairs). Space (0x00) writes only one byte. String terminated by 0xFF.
```c
void print(void) {
    const uint8_t *src = (const uint8_t *)(uintptr_t)
        ((uint16_t)adr1_hi << 8 | adr1_lo);
    uint8_t *dst = PLAY_SCRN + (uint16_t)temp2 * 40u + temp1;
    uint8_t si = 0, di = 0;
    for (;;) {
        uint8_t b = src[si++];
        if (b == 0xFF) break;
        if (b != 0x00) { dst[di++] = b; b = (uint8_t)(b + 32u); }
        dst[di++] = b;
    }
}
```

### PRINT.OPTS — Option Screen Rendering (static)
1. Print OPT1/OPT2/OPT3 at col=0, rows 7/9/11 (category labels)
2. Print selected category (`opt_tab[opt_num]`) at its row with `b | 0x80` = reverse video
3. Print current skill values at col=28, rows 7/9/11

Reverse-video loop (assembly: `ORA #$80` on each byte before storing):
```c
uint8_t *dst = adr1_ptr();
const uint8_t *src = opt_tab[opt_num];
uint8_t di = 0;
for (uint8_t si = 0; ; si++) {
    uint8_t b = src[si];
    if (b == 0xFF) break;
    if (b != 0x00) {
        dst[di++] = (uint8_t)(b | 0x80u);
        b = (uint8_t)((b | 0x80u) + 32u);
    }
    dst[di++] = b;
}
```
Highlighted row = `7 + opt_num * 2` (rows 7, 9, 11 for opt_num 0, 1, 2).

### READ.USER — Console Edge-Detection and Keyboard Pause
Assembly: edge-detects CONSOL register changes via `CONSOL.FLAG`. On change:
- CONSOL==6 (START): set mode=START_MODE, demo_status=1, return
- mode==OPTION_MODE: redisplay options (no state change)
- CONSOL==3 (OPTION) or CONSOL==5 (SELECT): enter option mode, clear screen, init

Keyboard pause sequence:
1. Save mode, set mode=PAUSE_MODE, call clear_sounds()
2. `while (!(SKSTAT & 0x04)) {}` — wait for space physically released
3. Loop: exit on space pressed, any console button, or trigger pressed
4. `while (!(SKSTAT & 0x04)) {}` — wait for key released again
5. Restore mode

Key detail: in step 3, if a key IS available (`!(SKSTAT & 0x04)`) but it's NOT
space, the loop does NOT exit — only space exits. Console buttons and trigger exit
unconditionally. Assembly:
```asm
.5  LDA SKSTAT
    AND #4 : BNE .38    ; no key: check console/trigger
    LDA KBCODE
    CMP #$21 : BEQ .6   ; space: exit
    JMP .5              ; other key: keep waiting
.38 LDA CONSOL
    CMP #7 : BNE .6     ; any console button: exit
    LDA TRIG0
    BNE .5              ; no trigger: keep waiting
.6  ; exit
```

In C:
```c
for (;;) {
    if (!(SKSTAT_HW & 0x04)) {
        if (KBCODE_HW == 0x21) break;
    } else {
        if (CONSOL_HW != 7) break;
        if (!TRIG0_HW) break;
        continue;
    }
}
```

### P.MOVE — Pod Animation and Movement
POD.COM is a 6-bit counter (always masked to `& 0x3F`). Left movement subtracts
$10; right adds $10. When the upper nibble reaches $30 (going left) or $00 (going
right), pod_x advances ±1. If the new position is occupied or out of bounds
[50, 206], direction is flipped and the loop retries:
```c
static void p_move(uint8_t xi) {
    for (;;) {
        if ((int8_t)pod_dx[xi] < 0) {
            uint8_t c = (uint8_t)((pod_com - 0x10u) & 0x3Fu);
            pod_com = c;
            if ((c & 0xF0u) == 0x30u) pod_x[xi]--;
        } else {
            uint8_t c = (uint8_t)((pod_com + 0x10u) & 0x3Fu);
            pod_com = c;
            if ((c & 0xF0u) == 0x00u) pod_x[xi]++;
        }
        uint8_t px = pod_x[xi];
        uint8_t *mp = pod_map_ptr(xi);
        if (mp[0] == 0 && mp[1] == 0 && px >= 50 && px < (256u - 50u)) {
            pod_status[xi] = pod_com;
            return;
        }
        pod_dx[xi] = (uint8_t)(pod_dx[xi] ^ 0xFEu);  /* flip direction */
    }
}
```

### M.MOVE — Cruise Missile Homing
Horizontal: advance cm_x ±1 based on cm_status (CM_LEFT/CM_RIGHT).
Vertical homing logic:
- If cm_time expired (`(int8_t)cm_time < 0`): fall straight down (cm_y++)
- Else: compute horizontal error `dx = chop_x - cm_x`
  - If missile heading left and dx < 0 (chopper is to the left = we're converging): fall down
  - If missile heading right and dx >= 0 (converging): fall down
  - If diverging: bounds check first (cm_x in [$2D, $D7]), then home vertically

"Home vertically": `dy = (chop_y + 1) - cm_y`; if dy==0 → no change; if dy<0 → cm_y--; if dy>0 → cm_y++.

After movement: if the new position has another missile tile, advance cm_y++ to avoid occupying same cell.

### MOVE.TANKS — Two-Phase Control Flow
```
move_tanks():
  tank_spd--
  if (tank_spd != 0) goto mt2   ← rate-limit skip

  ; Phase 1: MT1 loop (MAX_TANKS-1 downto 0)
  for xi = MAX_TANKS-1 downto 0:
    if OFF   → next_tank
    if BEGIN → init, goto draw_tank
    if CRASH → next_tank
    ; restore old position (3 bytes from tank_temp[xi*3..xi*3+2])
    ; check for explosion tiles → STATUS_CRASH, TIM5_VAL=10
    ; clear direction indicator above: mp[-256+1] = 0

draw_tank:
    pos_tank(xi)
    tank_x += tank_dx
    pos_tank(xi)
    ; save new position (3 bytes into tank_temp[xi*3..])
    ; check_tank_col(xi, 0) → if collision, goto draw_tank (retry)
    ; check_tank_col(xi, 2) → if collision, goto draw_tank
    ; write TANK_SHAPE[0..2] to map
    ; write direction indicator: mp[-256+1] = (chop_x >= tank_x) ? 0xEF : 0xF0

next_tank:

mt2:
  ; Phase 2: explosion timer
  tim5_val--
  if (tim5_val != 0) goto mt2_respawn
  ; clear crashed tanks (OFF → restore map, award 80+2 BCD points)

mt2_respawn:
  ; For each OFF tank: if chopper near start row and 13 map bytes clear → BEGIN
```

The `goto draw_tank` retry on collision is correct: CHECK.TANK.COL already
flipped tank_dx, so the next iteration of the draw_tank loop moves tank_x
in the opposite direction, which naturally reverses the position. The new
position is then re-saved and re-checked.

### CHECK.TANK.COL — Hit List Scan
Scans `hit_list[HIT_LIST2_LEN..0]` (22 entries) for the map byte at `(adr1+y)`.
If found: flip `tank_dx[ti]` and return 1. Assembly counts DOWN from HIT_LIST2_LEN
to 0 (`DEY; BPL`). C mirrors with `for (int xi2 = HIT_LIST2_LEN; xi2 >= 0; xi2--)`.

### SCREEN.OFF — Detailed Breakdown
11-step sequence:
1. Switch display list pointer to dsp_lst2 (via SDLST_LO/HI)
2. chopper_status = STATUS_OFF, robot_status = STATUS_OFF
3. If r_status == STATUS_CRASH: r_status = STATUS_OFF
4. `memset(CHR_SET1 + 0x2E0, 0, 0x20)` — clear 32-byte custom font region 1
5. `memset(CHR_SET1 + 0x300, 0, 0x100)` — clear 256-byte custom font region 2
6. `memset(PLAY_SCRN, 0, 0x100)` × 3 pages — clear 768 bytes of screen RAM
7. Zero sound shadow registers (s1_1_val=0, s1_2_val=20, s2..s6=0)
8. bak2_color = 0
9. tim7_val = MAX_TANKS - 1
10. For each active cruise missile: set OFF, call m_erase()
11. For each rocket: if status==7 (EXP), restore rocket_temp to map; then status=0, x=0

The `memset` replaces three assembly loops (loops over pages $0300–$03FF, $0400–$04FF, $0500–$05FF, plus the separate CHR.SET1 loops). Using `memset` is safe because the underlying arrays are defined as exactly these sizes in the Atari memory model.

---

## Bugs Found and Fixed

### Bug 1 — Duplicate `check_chr()` definition

**Root cause:** Initial draft had two code paths:
1. An incomplete `m_draw()` function that called `check_chr()` — this was a dead
   draft superseded by `m_draw_and_check()`, but the stub `check_chr()` it referenced
   was inserted right after the missile section
2. A second `check_chr()` stub was then added after the hyperspace section for
   the final "public stub" position

**Compiler error:**
```
dev/src/fort2.c:762:6: error: redefinition of 'check_chr'
```

**Fix:** Removed the incomplete `m_draw()` function entirely and its associated
stub. Kept only the final `check_chr()` stub (no-op body) in the single correct
location below `check_hyper_chamber()`.

---

### Bug 2 — Double-decrement of `xi` in `move_tanks`

**Root cause:** The `next_tank:` label target had an explicit `xi--;` before
the `continue` statement, while the enclosing `for` loop's update expression
also decremented `xi`. Result: xi decremented twice per normal iteration, skipping
every other tank.

**Code before fix:**
```c
for (int xi = MAX_TANKS - 1; xi >= 0; xi--) {
    ...
next_tank:
    xi--;   /* leftover from draft — double decrement */
    continue;
}
```

**Fix:** Replace the label body with a null statement:
```c
next_tank:
    ;   /* for loop's xi-- handles the decrement */
```

---

### Bug 3 — Double `ccl()` call in `print_opts`

**Root cause:** Initial draft set temp2=row only after calling ccl() once:
```c
ccl();         /* first call: temp1 and temp2 from previous print_str() */
temp2 = row;
ccl();         /* second call: correct */
```
The first call used stale temp1=28 and temp2=11 (from the preceding
`print_str(28, 11, opt3)` call), computing an incorrect ADR1 that was then
immediately overwritten by the second call. No visible bug at runtime since only
the second call's ADR1 was used, but the first call was wasted work and showed
wrong intent.

**Fix:**
```c
temp1 = 0;     /* col = 0 for the highlighted label */
temp2 = row;
ccl();         /* single correct call */
```

---

## Compile Test

### Command
```
cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src \
    dev/src/fort2.c dev/src/fort3.c dev/src/fort4.c \
    dev/src/fort5.c dev/src/fort6.c dev/src/fort7.c dev/src/fort8.c
```

### Result
```
(no output — zero diagnostics)
```

---

## Design Decisions

| Decision | Rationale |
|---|---|
| `check_chr()` is a no-op stub | CHECK.CHR takes input in A register; no C equivalent. Internal callers use `check_chr_byte(byte)`. Stub prevents linker error for future fort3.s callers |
| `goto` in `move_tanks` | Replicates assembly's BNE/JMP structure with labeled forward/backward jumps. Makes verification against assembly easier |
| `pod_com` stays global | Assembly POD.COM is shared state; all pods use the same counter. Preserves the emergent stagger effect |
| `TANK_SHAPE` is a macro | It's a label inside HIT.LIST, not a separate variable. `hit_list + 12` is the correct address |
| `wait_frame` calls `main_loop()` on mode change | Assembly does `LDX #$FF; TXS; JMP MAIN` — stack reset + jump. C equivalent: call `main_loop()` which never returns |
| `screen_off()` uses `memset` | Replaces three assembly byte loops; correct because PLAY_SCRN is a known-size contiguous array |

---

## File Summary

| File | Lines | Purpose |
|------|-------|---------|
| `dev/src/fort2.h` | 26 | Public declarations for all fort2 systems |
| `dev/src/fort2.c` | 1031 | Full implementation: input, pods, missiles, tanks, screen, print, utilities |
