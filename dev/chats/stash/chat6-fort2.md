# Chat 7 — fort2.s → C: Sessions 6 and 7 (fort2.h + fort2.c)
**Branch:** agents/asm-to-c-conversion
**Dates:** 2026-08-07 / 2026-08-08
**Preceding sessions:** chat1–chat5 (fnt1.s, fnt2.s, fort1.s, fort1.c bug fixes)
**Files produced:**
- `dev/src/fort2.h` — public declarations
- `dev/src/fort2.c` — full implementation (1031 lines)

---

## Background

`fort2.s` is the second major game-logic file in the Fort Apocalypse Atari SynAssembler source. It handles:
- Console (button/keyboard) input
- Options screen
- Pod objects (animated fuel pods)
- Cruise missiles (tank-fired heat-seeking projectiles)
- Hyperspace chamber teleport
- Tank movement, collision, and respawn
- Screen on/off (display list switching + memory clearing)
- Print utility (ANTIC mode-4 tile rendering)
- Wait-frame, give-bonus, clear-info

The file is 1153 lines of 6502 SynAssembler. All analysis was done by reading the assembly directly.

---

## Session 6 — Analysis and fort2.h

### Step 1: Reading fort2.s structure

Scanned all labels to map the function layout:

```
READ.USER       — CONSOL edge-detect, keyboard pause
CHECK.OPTIONS   — option screen navigation
PRINT.OPTS      — option screen display (static helper)
MOVE.PODS       + MP1, P.BEGIN, P.COL, P.ERASE, P.DRAW, P.MOVE,
                  GET.POD.ADR, GET.POD.VAL, PUT.POD.VAL, POS.POD
MOVE.CRUISE.MISSILES + MM1, M.BEGIN, M.COL, M.COL2, M.ERASE,
                        M.MOVE, M.DRAW, GET.MISS.ADR
CHECK.HYPER.CHAMBER — teleport with 4-entry position tables
CHECK.CHR       — scan CHR.SET2 glyph bytes for non-zero rows
POS.TANK        — position tank player-missile sprite
MOVE.TANKS      + MT2 (timer/respawn), CHECK.TANK.COL
SCREEN.ON / SCREEN.OFF — display list switching + memory clear
CLEAR.SOUNDS    — silence POKEY
CCL             — compute screen address PLAY_SCRN + row*40 + col
PRINT           — tile-rendering string printer
GIVE.BONUS      — BCD add 2 to chop_left, apply bonus score
WAIT.FRAME      — synchronised per-frame spin with mode-change detection
CLEAR.INFO      — clear top screen row
```

Data tables at end of fort2.s:
```
POD.CHR:  .HS 4000 5B5C 5D5E 005F   → {0x40,0x00,0x5B,0x5C,0x5D,0x5E,0x00,0x5F}
H.XF: {0xDD,0x76,0x10,0x4B}         H.YF: {0x7A,0x7B,0xB8,0xB8}
H.X:  {0x22,0xBC,0x55,0x87}          H.Y:  {0x0F,0x0F,0x18,0x18}
H.CX: {0x73,0x78,0x76,0x75}          H.CY: {0x8C,0x89,0xAF,0xAF}

Option strings (raw ASCII, terminated 0xFF):
OPTT1 "OPTIONS"    OPTT2 "OPTION"     OPTT3 "SELECT"
OPT1 "GRAVITY SKILL"  OPT2 "PILOT SKILL"  OPT3 "ROBO PILOTS"
OPT1.1 "WEAK    "  OPT1.2 "NORMAL"   OPT1.3 "STRONG"
OPT2.1 "NOVICE"    OPT2.2 "PRO      " OPT2.3 "EXPERT"
OPT3.1 "SEVEN   "  OPT3.2 "NINE    "  OPT3.3 "ELEVEN"
```

### Step 2: Resolving cross-file references

**fort7.s variable sizes (verified by direct read):**
- `TANK.TEMP .BS 18` (MAX_TANKS * 3)
- `CM.STATUS`, `CM.X`, `CM.Y`, `CM.TIME`, `CM.TEMP .BS MAX.TANKS` each
- `CONSOL.FLAG .BS 1`, `BAK2.COLOR .BS 1`, `OPT.NUM .BS 1`
- `TANK.SPD`, `TANK.SPEED`, `MISSILE.SPD`, `MISSILE.SPEED .BS 1` each
- `ROCKET.STATUS`, `ROCKET.X`, `ROCKET.TEMP`, `ROCKET.TEMPX`, `ROCKET.TEMPY .BS 3` each
- `BONUS1`, `BONUS2 .BS 1` each

**fort3.s — HIT.LIST / TANK.SHAPE (verified by direct read of lines 10140–10230):**
```
HIT.LIST:   .HS 40 5B5C5D5E5F 3B3C3D3E494A    (12 bytes)
TANK.SHAPE: .HS 4C4D4E4F50 7172               (7 bytes)
            .AT /a /                           (.AT = ASCII with -$20 = $61 space)
HIT.LIST.LEN  .EQ *-HIT.LIST-1   = 18
HIT.LIST2.LEN .EQ *-HIT.LIST-1   = 21  (after TANK.SHAPE + $61 + $20)
```
TANK.SHAPE is a label inside HIT.LIST at byte offset 12.
In C: `#define TANK_SHAPE (hit_list + 12)` and `#define HIT_LIST2_LEN 21u`

**fort6.s — display lists:**
`dsp_lst1` = main game display list, `dsp_lst2` = blank/off list.

### Step 3: Key calling convention decision — PRINT

Assembly `PRINT`:
- Entry: X:Y → ADR2 (string pointer, X=lo, Y=hi), TEMP1=col, TEMP2=row
- Calls `CCL` internally to set ADR1 = screen address
- Then reads from ADR2 (the string)

But `fort1.c`'s existing `print_str()` wrapper (lines 374–381) sets:
```c
adr1_lo = str & 0xFF;
adr1_hi = str >> 8;
temp1 = col; temp2 = row;
print();
```
That wrapper puts the string in ADR1, not ADR2. All existing callers in fort1.c
use this convention. Decision: the C `print()` takes string from ADR1, not ADR2,
matching the fort1.c convention. All fort2.s PRINT call sites are converted to
`print_str()` wrappers that set ADR1.

CCL in C: just a straight arithmetic formula — no need to call it internally
from print() since print() can compute the address directly.

### Step 4: Key constant conflict — CM_LEFT vs STATUS_CRASH

In fort7.s, `CM.LEFT .EQ 4` (cruise missile going left) and `STATUS.CRASH .EQ 4`
(tank crashed). Same numeric value, different semantics. In C, define separately:
```c
#define CM_LEFT   4u   /* missile direction */
#define CM_RIGHT  8u
```
Use `STATUS_CRASH` (from fort1.h) only for tank status.

### Step 5: WAIT.FRAME stack-reset pattern

Assembly at end of WAIT.FRAME when mode changes:
```
LDX #$FF
TXS          ; reset stack pointer to $1FF (empty)
JMP MAIN     ; restart game loop — effectively longjmp to top of call chain
```
In C, the call chain can't be unwound with a direct JMP. Equivalent: call
`main_loop()` which is declared `__attribute__((noreturn))` (effectively — it
loops forever). The C version saves mode before the wait, checks for changes
each frame, and calls `main_loop()` if mode changed.

### Step 6: POD.COM animation encoding

`POD.COM` is a 6-bit counter (masked to `& 0x3F`) used both as `pod_status`
and as the animation frame selector.

Movement rules:
- Going left: `pod_com = (pod_com - 0x10) & 0x3F`. When `pod_com & 0xF0 == 0x30`,
  move pod_x left one step.
- Going right: `pod_com = (pod_com + 0x10) & 0x3F`. When `pod_com & 0xF0 == 0x00`,
  move pod_x right one step.

Frame selection: `frame = pod_com >> 3` gives 0, 2, 4, or 6 (always even).
`pod_chr[frame]` = left tile, `pod_chr[frame+1]` = right tile.

The eight entries:
```
Index: 0     1     2     3     4     5     6     7
Value: 0x40  0x00  0x5B  0x5C  0x5D  0x5E  0x00  0x5F
```
Index 1 and 6 are 0x00 (invisible), so the pod blinks when pod_com passes
through those phases.

### Step 7: M.ERASE restore logic (M.COL2)

Assembly M.COL2 restores the map byte that was under the missile only if it's
not one of the "special" characters that should never be overwritten back.
The filter logic (from assembly branch structure):
- If byte >= $E0: DO NOT restore (reverse-video char = was already a rendered tile)
- If byte == $40 (CHR_POD_TILE): DO NOT restore
- If byte >= $5B and < $60: DO NOT restore (pod/tank animation chars)
- Otherwise: restore

In C:
```c
if (!(saved >= 0xE0u || saved == CHR_POD_TILE ||
      (saved >= 0x5Bu && saved < 0x60u))) {
    mp[0] = saved;
}
```

### Step 8: MOVE.TANKS control flow

The assembly has a two-phase structure:
```
MOVE.TANKS:
    DEC TANK.SPD
    BNE MT2       ; rate-limit: skip movement this frame
    ; MT1: process all tanks (restore, move, draw)
    ...
MT2:
    DEC TIM5.VAL  ; explosion timer
    BNE MT2.RSP
    ; Clear crashed tanks
    ...
MT2.RSP:
    ; Respawn check
    ...
```

The C uses `goto mt2` for the rate-limit skip (forward goto to `mt2:` label),
which is valid C. The tank loop uses a for (MAX_TANKS-1 down to 0) with
`goto next_tank` and `goto draw_tank` labels to replicate the assembly's
BNE/JMP structure.

### Step 9: CHECK.TANK.COL

Scans `hit_list[0..HIT_LIST2_LEN]` (22 bytes total, including TANK.SHAPE + extra)
for the map byte at the given offset from adr1. If found: flip tank_dx (reverse
direction) and return 1 (carry set). The index counts DOWN from HIT_LIST2_LEN
to 0, matching the assembly's `DEY; BPL` loop.

### Step 10: SCREEN.OFF detailed breakdown

Assembly SCREEN.OFF:
1. Switch display list to dsp_lst2 (via SDLST)
2. Set CHOPPER.STATUS, ROBOT.STATUS = OFF
3. If R.STATUS == STATUS.CRASH: clear it
4. Clear CHR.SET1+$2E0 for $20 bytes (character font custom area)
5. Clear CHR.SET1+$300 for $100 bytes (second custom area)
6. Clear PLAY.SCRN for $300 bytes (3 pages of screen RAM)
7. Zero all sound shadow registers (S1.1, S1.2=20, S2–S6)
8. Clear BAK2.COLOR
9. TIM7.VAL = MAX.TANKS-1
10. For each cruise missile: if active, erase from map, set OFF
11. For each rocket: if exploding (status==7), restore map byte, then zero status/x

### Step 11: CREATE fort2.h

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
void ccl(void);      /* compute ADR1 = PLAY_SCRN + temp2*40 + temp1 */
void print(void);    /* adr1=str, temp1=col, temp2=row */
void give_bonus(void);
void wait_frame(uint8_t n);
void clear_info(void);
```

---

## Session 7 — Writing fort2.c (resumed from compaction)

Session 6 hit the output token limit twice before fort2.c was written. The
context was compacted and this session resumed directly with all analysis
already complete.

### Step 1: Write fort2.c (initial pass)

The file was written in one shot covering all ~20 functions. Key decisions
and assembly traces for each section:

---

#### CCL

Assembly:
```
CCL  STX TEMP1    ; save TEMP1
     STY TEMP2    ; save TEMP2
     LDA #0
     LDY TEMP2
     ... (MULT.BY.40: Y*40 using shift/add)
     ADC #PLAY.SCRN & 255
     STA ADR1
     LDA /PLAY.SCRN
     ADC #0
     STA ADR1+1
```

In C, the save/restore of temp1/temp2 is unnecessary since C doesn't use
registers. The formula is direct:
```c
uint16_t a = (uint16_t)(uintptr_t)PLAY_SCRN + (uint16_t)temp2 * 40u + temp1;
adr1_lo = (uint8_t)(a & 0xFF);
adr1_hi = (uint8_t)(a >> 8);
```

---

#### PRINT

Assembly PRINT (in its C calling convention where adr1 = string):
```
PRINT  LDX #0       ; source index into string
       LDY #0       ; dest index into screen
.1     LDA (ADR1,X) → read string byte (via indexed indirect)
       CMP #$FF     ; end marker
       BEQ .RTS
       BNE .2
.2     STA (ADR2),Y ; write to screen
       INY
       CMP #0       ; was it a space (0x00)?
       BEQ .1       ; space: write only 1 byte
       CLC
       ADC #32      ; non-space: write right-half tile (char+32)
       STA (ADR2),Y
       INY
       BNE .1
.RTS   RTS
```

The screen base (ADR2) points to PLAY_SCRN + row*40 + col (computed by CCL
before PRINT is called in the assembly). In the C convention the screen
address is computed inside print() from temp1/temp2.

```c
void print(void)
{
    const uint8_t *src = (const uint8_t *)(uintptr_t)
        ((uint16_t)adr1_hi << 8 | adr1_lo);
    uint16_t scr_off = (uint16_t)temp2 * 40u + temp1;
    uint8_t *dst = PLAY_SCRN + scr_off;

    uint8_t si = 0, di = 0;
    for (;;) {
        uint8_t b = src[si++];
        if (b == 0xFF) break;
        if (b != 0x00) {
            dst[di++] = b;
            b = (uint8_t)(b + 32u);
        }
        dst[di++] = b;
    }
}
```

Tile pair logic: non-space bytes write two screen bytes (b, b+32); 0x00
bytes write one byte (0x00 = space tile).

---

#### PRINT.OPTS — option screen rendering

Assembly structure:
1. Print OPT1, OPT2, OPT3 at rows 7, 9, 11, col 0 (category labels)
2. Print selected category (OPT.NUM) again at its row with ORA #$80 on
   each byte (reverse video highlight)
3. Print OPT1.1/2/3, OPT2.1/2/3, OPT3.1/2/3 at col 28, rows 7, 9, 11

The reverse-video print loop:
```asm
LDA (SRC),Y      ; get string byte
CMP #$FF
BEQ done
ORA #$80         ; set reverse-video bit
STA (DST),Y      ; write highlighted char
CLC
ADC #32          ; right-half tile = highlighted char + 32
STA (DST),Y+1
```

In C: `dst[di++] = b | 0x80u; dst[di++] = (b | 0x80u) + 32u`

The highlighted row is `7 + opt_num * 2` (since labels are at rows 7, 9, 11
= 7, 7+2, 7+4).

Initial bug in the draft: called `ccl()` twice — first with unset temp1/temp2,
then again with the correct values. Fix: set temp1=0 and temp2=row, then call
ccl() once.

---

#### CHECK.OPTIONS

Assembly structure:
```
CHECK.OPTIONS:
    LDA CONSOL
    CMP #3          ; OPTION button (bit2=0, bit1=0, bit0=1)
    BNE .1
    ; advance OPT.NUM with wraparound
.1  CMP #5          ; SELECT button (bit2=1, bit1=0, bit0=0)
    BNE .2
    ; increment current skill with wraparound
.2  ; print header + call PRINT.OPTS
```

CONSOL bit pattern (active low, $D01F):
- $07 = 0b111 = nothing pressed
- $06 = 0b110 = START only
- $05 = 0b101 = SELECT only
- $03 = 0b011 = OPTION only

CHECK.OPTIONS is called both from READ.USER (when a button is first detected)
and from PRINT.OPTS display refreshes. The assembly doesn't debounce at this
level — READ.USER handles edge detection.

---

#### READ.USER

Assembly structure:
```
READ.USER:
    LDA CONSOL
    CMP CONSOL.FLAG  ; same as last time?
    BEQ .key         ; yes: only check keyboard
    STA CONSOL.FLAG  ; no: store new value
    LDA #0
    STA TIM6.VAL     ; reset demo timer
    CMP #6           ; was START?
    BEQ .start
    LDA MODE
    CMP #OPTION.MODE
    BEQ .opt         ; already in options: redisplay
    LDA CONSOL.FLAG
    CMP #3           ; OPTION button?
    BEQ .newopt
    CMP #5           ; SELECT button?
    BNE .key         ; neither: fall through to keyboard check
.newopt:
    LDA #OPTION.MODE
    STA MODE
    LDA #1
    STA DEMO.STATUS
    JSR SCREEN.OFF
    LDA #0
    STA OPT.NUM
    JSR CHECK.OPTIONS
    RTS
.start:
    LDA #START.MODE
    STA MODE
    LDA #1
    STA DEMO.STATUS
    RTS
.opt:
    JSR CHECK.OPTIONS
    RTS
.key:
    LDA SKSTAT
    AND #4           ; bit2=1: no key
    BNE .rts
    LDA KBCODE
    CMP #$21         ; SPACE
    BNE .rts
    ; pause logic...
```

Pause logic sequence:
1. Save current mode, set mode = PAUSE_MODE, call CLEAR.SOUNDS
2. Spin until SKSTAT bit2 = 1 (key released physically)
3. Spin until: space key pressed, OR any console button, OR trigger pressed
4. Spin until SKSTAT bit2 = 1 again (debounce)
5. Restore saved mode

One detail: the "unpause loop" must handle the case where the user presses a
different key (not space). The assembly checks `KBCODE == $21` for space;
other keys don't exit the loop. Console buttons and trigger do exit.

---

#### MOVE.PODS / MP1 / helpers

The pod system:
- `pod_num`: circular index, advances 15 per game frame
- `pod_status`: bottom nibble = status (OFF/BEGIN/ON), upper bits = pod_com value
- `pod_com`: 6-bit animation/position counter shared across ALL pods

Assembly MP1 (called 15 times per frame):
```
MP1:
    LDX POD.NUM
    LDA POD.STATUS,X
    AND #$0F
    BEQ .skip        ; STATUS_OFF: skip
    CMP #STATUS.BEGIN
    BEQ .begin       ; STATUS_BEGIN: initialise
    JSR P.COL        ; else: check collision
    BCS .skip        ; collision: skip
    JSR P.ERASE      ; no collision: erase, move, redraw
    JSR P.MOVE
    JSR P.DRAW
.skip:
    INC POD.NUM      ; advance to next pod
    LDA POD.NUM
    CMP #MAX.PODS
    BCC .ret
    LDA #0
    STA POD.NUM
    RTS
```

P.COL collision check: reads 2 map bytes at pod position. If either is
EXP ($20), MISS.LEFT ($71), or MISS.RIGHT ($72): restore the pod's saved map
bytes, set pod OFF, award 80 BCD points ($50:$00), return carry set.

P.BEGIN: find a random empty 2-byte position (x in [50,206], y in [0,39]),
draw pod there, set pod_dx = 1 (initially moving right).

P.MOVE: advance pod_com by ±$10 (masked to $3F); conditionally advance pod_x
by ±1 based on which phase we hit. If new position is occupied or out of
bounds, flip direction and retry (loop).

P.DRAW: save 2 map bytes, then write `pod_chr[pod_com>>3]` and
`pod_chr[(pod_com>>3)+1]`.

Important: `pod_com` is a global — all pods share the same animation counter.
In the assembly, `POD.COM` is a zero-page variable that every pod reads/writes
on each step. The side effect is all pods advance their counter in sequence
during the 15-call batch, so pods drift out of phase with each other over time.

---

#### MOVE.CRUISE.MISSILES / MM1 / helpers

Rate-limited by `missile_spd` counter (counts down to 0, reloaded from
`missile_speed`).

MM1 processes all MAX_TANKS missile slots in one go (no circular indexing like
pods). Each slot is tied to a tank: tank[i] fires missile[i].

Assembly MM1 has two interleaved loops:
1. Process existing missiles (BEGIN/active/OFF)
2. Check if tank[i] should fire a new missile

Firing condition:
- `tank_status[i] == STATUS_ON` (tank alive)
- `|tank_y[i] - chop_y| < 14` (vertically close)
- `|chop_x - tank_x[i] - 2| < 9` (horizontally close)
- `cm_status[i] == STATUS_OFF` (no missile already in flight)

M.MOVE — vertical homing logic:
```
; Horizontal step first
LDA CM.STATUS,X
CMP #CM.LEFT
BNE .right
DEC CM.X,X
BNE .vert
.right INC CM.X,X

.vert:
; If time expired (CM.TIME high bit set), fall straight down
BIT CM.TIME,X    ; bit7 = sign bit (if time wrapped negative)
BMI .fall
; Compute: going in same direction as chopper relative to missile?
LDA CHOP.X
SEC SBC CM.X,X   ; A = chop_x - cm_x
BPL .pos
; chop is to LEFT of missile
CMP CM.STATUS,X  ; if CM.LEFT and chop is left: we're aligned → go down
BEQ .down
BNE .up
.pos:
; chop is to RIGHT
CMP CM.STATUS,X
BEQ .up
; We should home vertically...
```

The vertical logic: when the missile is "converging" horizontally toward the
chopper, it also homes vertically. When diverging, it falls. Bounds check
before vertical homing: missile must be in X range [$2D, $D7].

M.DRAW checks if the map character under the missile (CHR_SET2 glyph) has
any non-zero rows (check_chr_byte). If it does, the missile is hitting a
solid tile → collision → m_col2.

---

#### CHECK.HYPER.CHAMBER

Assembly:
```
CHECK.HYPER.CHAMBER:
    LDA MODE
    CMP #HYPERSPACE.MODE
    BNE .rts
    LDA #STOP.MODE
    STA MODE
    LDA #$0F
    STA BAK2.COLOR       ; flash white
    LDA #2
    JSR WAIT.FRAME
    LDA #0
    STA BAK2.COLOR
    LDA RANDOM
    AND #3               ; pick one of 4 exit positions
    TAX
    LDA H.XF,X → SX.F
    LDA H.YF,X → SY.F
    LDA H.X,X  → SX
    LDA H.Y,X  → SY
    LDA H.CX,X → CHOPPER.X
    LDA H.CY,X → CHOPPER.Y
    LDA #8 → CHOPPER.ANGLE
    LDA #0 → CHOPPER.COL
    LDA #GO.MODE → MODE
    RTS
```

`SX`, `SY`: sub-pixel position of chopper sprite
`SX.F`, `SY.F`: fractional (fine-position) for smooth scrolling

---

#### CHECK.CHR

Assembly (called by M.DRAW with char value in A):
```
CHECK.CHR:
    AND #$7F         ; strip reverse-video bit
    ASL A            ; multiply by 8 (3 shifts)
    ASL A
    ASL A
    ; Note: A is now the low byte of offset; high byte comes from page calc
    TAY              ; Y = low byte of glyph offset
    LDA /CHR.SET2    ; high byte of CHR.SET2
    ADC #...         ; add page overflow from ASL
    STA TEMP1+1      ; form pointer to glyph in TEMP1
    ...
    LDA #8
    STA TEMP4        ; 8 rows to check
.loop:
    LDA (TEMP1),Y+
    BNE .found       ; non-zero byte = has pixels
    DEC TEMP4
    BNE .loop
    CLC              ; no pixels: carry clear
    RTS
.found:
    SEC              ; has pixels: carry set
    RTS
```

In C, the carry equivalent is the return value of `check_chr_byte`:
```c
static int check_chr_byte(uint8_t chr_byte)
{
    uint16_t off = (uint16_t)((chr_byte & 0x7Fu) * 8u);
    const uint8_t *glyph = CHR_SET2 + off;
    for (int y = 7; y >= 0; y--)
        if (glyph[y]) return 1;
    return 0;
}
```

The public `check_chr()` is a stub for external compatibility (fort3.s callers
will use it when converted). Internal code calls `check_chr_byte()` directly.

---

#### MOVE.TANKS — detailed trace

Full assembly structure:
```
MOVE.TANKS:
    DEC TANK.SPD
    BNE MT2                   ; rate-limit

    LDA TANK.SPEED
    STA TANK.SPD
    LDX #MAX.TANKS-1          ; process tanks MAX-1 down to 0

.tank_loop:
    LDA TANK.STATUS,X
    BEQ .next                 ; STATUS_OFF: skip
    CMP #STATUS.BEGIN
    BEQ .begin
    CMP #STATUS.CRASH
    BEQ .next                 ; STATUS_CRASH: skip (wait for timer)

    ; --- Restore old position ---
    LDA TANK.X,X → TEMP1
    LDA TANK.Y,X → TEMP2
    JSR COMPUTE.MAP.ADR       ; ADR1 = map[TEMP2][TEMP1]
    LDY #0
.restore_loop:
    LDA (ADR1),Y
    CMP #CHR.EXP
    BEQ .crash                ; hit explosion tile
    CMP #CHR.MISS.LEFT
    BEQ .crash
    CMP #CHR.MISS.RIGHT
    BEQ .crash
    LDA TANK.TEMP + 3*X, Y    ; restore saved map byte
    STA (ADR1),Y
    INY
    CPY #3
    BNE .restore_loop
    LDA #0
    STA (ADR1-256+1),Y        ; clear direction indicator row above

    ; --- Move X ---
.draw_tank:
    JSR POS.TANK              ; update player-missile sprite position
    LDA TANK.X,X
    CLC ADC TANK.DX,X
    STA TANK.X,X

    ; --- Save new position ---
    JSR POS.TANK
    LDA TANK.X,X → TEMP1
    LDA TANK.Y,X → TEMP2
    JSR COMPUTE.MAP.ADR
    LDY #0
.save_loop:
    LDA (ADR1),Y
    STA TANK.TEMP + 3*X, Y    ; save current map byte
    INY CPY #3 BNE .save_loop

    JSR CHECK.TANK.COL (Y=0)  ; check byte 0
    BCS .draw_tank             ; collision: flip dx, retry
    JSR CHECK.TANK.COL (Y=2)  ; check byte 2
    BCS .draw_tank

    ; Draw tank: write TANK.SHAPE[2,1,0] to map[0,1,2]
    LDA TANK.SHAPE+2 → (ADR1),2
    LDA TANK.SHAPE+1 → (ADR1),1
    LDA TANK.SHAPE+0 → (ADR1),0
    ; Draw direction indicator
    LDA CHOP.X
    CMP TANK.X,X
    BCS .right
    LDA #$F0 → (ADR1-256+1)  ; left indicator
    BCC .done
.right:
    LDA #$EF → (ADR1-256+1)  ; right indicator

.next:
    DEX
    BPL .tank_loop

MT2:
    DEC TIM5.VAL
    BNE MT2.RSP
    ...clear crashed tanks...
MT2.RSP:
    ...respawn check...
```

Map addressing: `(ADR1-256+1)` in assembly = `mp[-256 + 1]` in C. This is
valid pointer arithmetic since PLAY_SCRN is contiguous and the tank body is
never on row 0 (where -256 would underflow). The map is page-based: each
row occupies exactly one 256-byte page. So row above = same column offset in
previous page = -256 from current map pointer.

---

#### GIVE.BONUS — BCD add

Assembly:
```
GIVE.BONUS:
    JSR INC.SCORE (BONUS1, BONUS2)  ; add bonus to score
    LDA #0 → BONUS1, BONUS2         ; clear bonus
    SED              ; enable BCD
    LDA CHOP.LEFT
    CLC
    ADC #2           ; BCD add 2
    CLD
    STA CHOP.LEFT
```

6502 BCD mode: `CLC; ADC #2` adds 2 in decimal. `09 + 2 = 11` in BCD, not
`0B` in hex. Example: `CHOP.LEFT = 0x09 → 0x11` (one-one, not seventeen).

C implementation (no BCD instruction available):
```c
uint8_t lo = chop_left & 0x0Fu;
uint8_t hi = chop_left & 0xF0u;
lo = (uint8_t)(lo + 2u);
if (lo >= 10u) { lo = (uint8_t)(lo - 10u); hi = (uint8_t)(hi + 0x10u); }
chop_left = hi | lo;
```

Only the units digit can need a carry here since we're adding just 2. The tens
digit could carry too theoretically (e.g., 0x99 + 2 = 0x01 in BCD with carry
out), but `chop_left` is the number of lives remaining (practical range 1–17),
so the tens digit overflow case is not handled (matching assembly behaviour where
hundreds-carry is also ignored by the 8-bit storage).

---

#### WAIT.FRAME

Assembly:
```
WAIT.FRAME:
    ; A = count on entry
    STA TEMP.MODE    ; TEMP.MODE = count (reused for count here)
    ; save current mode (not shown — it reads MODE into temp before looping)
    ...
.loop:
    LDA FRAME        ; OS frame counter ($0014)
.spin:
    CMP FRAME
    BEQ .spin        ; wait for increment
    JSR READ.USER    ; process input
    LDA MODE
    CMP SAVED.MODE   ; check if mode changed
    BEQ .same
    LDX #$FF         ; mode changed: reset stack
    TXS
    JMP MAIN         ; restart game loop
.same:
    DEC TEMP.MODE
    BNE .loop
    RTS
```

The `LDX #$FF; TXS; JMP MAIN` pattern resets the 6502 hardware stack to $1FF
and jumps directly to MAIN, abandoning all return addresses. In C there's no
equivalent instruction. The only safe option is to call `main_loop()` which
never returns (it's a `for(;;)` loop calling check_modes, move_pods, etc.).

```c
void wait_frame(uint8_t n)
{
    uint8_t saved = mode;
    do {
        uint8_t f = FRAME_HW;
        while (f == FRAME_HW) {}
        read_user();
        if (mode != saved) {
            main_loop();   /* never returns */
        }
    } while (--n != 0);
}
```

---

### Step 2: Bug fixes after initial write

After writing the complete initial draft, three bugs were identified and fixed
before compilation:

---

#### Bug 1: Duplicate check_chr definition

The initial draft had three related blocks:
1. `m_draw()` static function that called `check_chr()` — this function was
   a dead/incomplete draft superseded by `m_draw_and_check()`
2. `check_chr_byte()` static helper
3. First `check_chr()` public stub (inside the missile section)
4. Second `check_chr()` public stub (below the hyperspace section)

The compiler reported:
```
dev/src/fort2.c:762:6: error: redefinition of 'check_chr'
```

Fix: Removed the dead `m_draw()` function entirely, removed the first
duplicate `check_chr` stub (kept only the one in the final position below
hyperspace section).

---

#### Bug 2: Double-decrement of xi in move_tanks

The original loop body ended with:
```c
next_tank:
    xi--;  /* outer for loop will decrement again */
    continue;
```
This decremented xi twice per iteration (once by the explicit `xi--` and once
by the for loop's `xi--`). The note was left from a previous draft. Fix: remove
`xi--;` and `continue`, replace with just the label and a null statement:
```c
next_tank:
    ;
```

---

#### Bug 3: Double ccl() call in print_opts

Original draft:
```c
uint8_t row = (uint8_t)(7u + (uint8_t)(opt_num * 2u));
ccl();         /* first call: temp1 and temp2 not yet set correctly */
temp2 = row;
ccl();         /* second call: correct */
```
The first call computed ccl() with whatever temp1/temp2 happened to contain
from the preceding print_str() calls (col=28, row=11 from the last call to
print_str(28, 11, ...)). That result was discarded since adr1_ptr() wasn't
called until after the second ccl(), but it was still a useless computation
and showed wrong intent.

Fix:
```c
uint8_t row = (uint8_t)(7u + (uint8_t)(opt_num * 2u));
temp1 = 0;    /* col = 0 for the highlighted label */
temp2 = row;
ccl();        /* adr1 = PLAY_SCRN + row*40 + 0 */
```

---

### Step 3: Compilation verification

```
cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src dev/src/fort2.c
```
No output = no errors or warnings. Clean compile.

---

## Decisions and trade-offs

### Why check_chr() is a stub

`CHECK.CHR` in the assembly takes its input in the 6502 A register — it tests
whatever byte the caller computed most recently. There's no clean C analog for
this register-passing convention. The approach taken:
- `check_chr_byte(uint8_t)` is the real implementation, used internally
- `check_chr()` is a no-op public stub so that future conversions of fort3.s
  callers can include `check_chr.h` without linker errors; those callers will
  need to be adapted to pass the byte explicitly

### Why goto is used in move_tanks

The assembly MOVE.TANKS has 6 local labels that are branch targets, some of
which jump forward and some backward. The C equivalent would require either:
a) Goto (used)
b) Nested if/else with boolean flags
c) Breaking the function into more helper functions

Option (a) is the most direct translation and keeps the code structure parallel
to the assembly, making it easier to compare and verify. The gotos are all
within the same function and all well-defined.

### Why pod_com is global

In the assembly, `POD.COM` is shared zero-page state. All 39 pods share the
same animation counter. The 15-call batch per frame means pod 0 always gets
the current counter, pod 1 gets the counter after pod 0 advanced it, etc. This
creates a natural stagger between pods' animation phases without any explicit
per-pod phase offset. The C code preserves this exactly by keeping `pod_com`
as an external uint8_t.

### Why TANK.SHAPE is a macro not an extern

`TANK.SHAPE` in fort3.s is not a separate variable — it's a label that points
into the middle of the `HIT.LIST` array. It has no `.BS` allocation; it just
marks offset 12 into hit_list[]. Using a pointer macro
`#define TANK_SHAPE (hit_list + 12)` is the correct translation: it compiles
to the same addressing and makes the relationship explicit.

---

## Final file listing

### dev/src/fort2.h

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
void ccl(void);      /* compute ADR1 = PLAY_SCRN + temp2*40 + temp1 */
void print(void);    /* adr1=str, temp1=col, temp2=row */
void give_bonus(void);
void wait_frame(uint8_t n);
void clear_info(void);
```

### dev/src/fort2.c — complete (1031 lines)

```c
/* fort2.c — options, pods, cruise missiles, tanks, screen/print utilities
 * Converted from fort2.s (Atari 6502 SynAssembler)
 */

#include <stdint.h>
#include <string.h>
#include "fort1.h"
#include "fort2.h"

/* Hardware register access */
#define REG(a)       (*(volatile uint8_t *)(uintptr_t)(a))
#define CONSOL_HW    REG(0xD01F)   /* active low: bit0=START,bit1=SELECT,bit2=OPTION */
#define TRIG0_HW     REG(0xD010)   /* 0=joystick trigger pressed */
#define KBCODE_HW    REG(0xD209)
#define SKSTAT_HW    REG(0xD20F)   /* bit2=1: no key available */
#define RANDOM_HW    REG(0xD20A)
#define AUDC1_HW     REG(0xD201)
#define AUDC2_HW     REG(0xD203)
#define AUDC3_HW     REG(0xD205)
#define AUDC4_HW     REG(0xD207)
#define FRAME_HW     REG(0x0014)   /* OS frame counter */

#define SDLST_LO  (*(volatile uint8_t *)0x0230u)
#define SDLST_HI  (*(volatile uint8_t *)0x0231u)

#define PLAY_SCRN   ((uint8_t *)0x0300u)
#define CHR_SET1    ((uint8_t *)0x0800u)
#define CHR_SET2    ((uint8_t *)0x0C00u)

#define CHR_EXP        0x20u
#define CHR_EXP_WALL   0xC7u
#define CHR_MISS_LEFT  0x71u
#define CHR_MISS_RIGHT 0x72u
#define CHR_POD_TILE   0x40u
#define CM_LEFT   4u
#define CM_RIGHT  8u

/* [all extern variable declarations, extern functions, data tables, and
 *  full function implementations as in the file — see full listing above] */
```

The complete file is at `dev/src/fort2.c` (1031 lines).

---

## Cross-reference: functions and their assembly origins

| C function             | Assembly label       | Source      |
|------------------------|----------------------|-------------|
| `ccl()`                | `CCL`                | fort2.s     |
| `print()`              | `PRINT`              | fort2.s     |
| `print_opts()` (static)| `PRINT.OPTS`         | fort2.s     |
| `check_options()`      | `CHECK.OPTIONS`      | fort2.s     |
| `read_user()`          | `READ.USER`          | fort2.s     |
| `pod_map_ptr()` (static)| `GET.POD.ADR`       | fort2.s     |
| `p_col()` (static)     | `P.COL`              | fort2.s     |
| `p_erase()` (static)   | `P.ERASE`            | fort2.s     |
| `p_draw()` (static)    | `P.DRAW`             | fort2.s     |
| `p_move()` (static)    | `P.MOVE`             | fort2.s     |
| `p_begin()` (static)   | `P.BEGIN`            | fort2.s     |
| `mp1()` (static)       | `MP1`                | fort2.s     |
| `move_pods()`          | `MOVE.PODS`          | fort2.s     |
| `cm_map_ptr()` (static)| `GET.MISS.ADR`       | fort2.s     |
| `m_begin()` (static)   | `M.BEGIN`            | fort2.s     |
| `m_col2()` (static)    | `M.COL2`             | fort2.s     |
| `m_col()` (static)     | `M.COL`              | fort2.s     |
| `m_erase()` (static)   | `M.ERASE`            | fort2.s     |
| `m_move()` (static)    | `M.MOVE`             | fort2.s     |
| `check_chr_byte()` (static)| `CHECK.CHR` (core)| fort2.s    |
| `m_draw_and_check()` (static)| `M.DRAW`         | fort2.s     |
| `mm1()` (static)       | `MM1`                | fort2.s     |
| `move_cruise_missiles()`| `MOVE.CRUISE.MISSILES`| fort2.s  |
| `check_hyper_chamber()`| `CHECK.HYPER.CHAMBER`| fort2.s    |
| `check_chr()`          | `CHECK.CHR` (stub)   | fort2.s     |
| `pos_tank()` (static)  | `POS.TANK`           | fort2.s     |
| `check_tank_col()` (static)| `CHECK.TANK.COL` | fort2.s     |
| `move_tanks()`         | `MOVE.TANKS` + `MT2` | fort2.s     |
| `screen_on()`          | `SCREEN.ON`          | fort2.s     |
| `screen_off()`         | `SCREEN.OFF`         | fort2.s     |
| `clear_sounds()`       | `CLEAR.SOUNDS`       | fort2.s     |
| `give_bonus()`         | `GIVE.BONUS`         | fort2.s     |
| `wait_frame()`         | `WAIT.FRAME`         | fort2.s     |
| `clear_info()`         | `CLEAR.INFO`         | fort2.s     |

---

## What remains

Per fort.s `.IN` include order:
- `fort3.s` — VERTBLKD, SAVE.POS, UPDATE.CHOPPER, DO.CHECKSUM2
- `fort4.s` — HOVER, COMPUTE.MAP.ADR, DDIG, DRAW.MAP, joystick read
- `fort5.s` — MOVE.SLAVES, SET.SCANNER, CHECK.FUEL.BASE, CHECK.FORT, DO.SOUNDS, LINE1 (DLI)
- `fort6.s` — data: display lists, chopper shapes, laser shapes
- `fort7.s` — variable declarations (needs minimal C — mostly extern declarations)
- `fort8.s` — display list init data blocks Z1/Z2
