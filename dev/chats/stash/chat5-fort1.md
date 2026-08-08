# Chat 5 — fort1.s → C: Bug Review Session
**Branch:** agents/asm-to-c-conversion  
**Date:** 2026-08-08  
**Continued from:** chat4.md (context limit hit mid-session)

---

## Session Context (from prior compaction summary)

The prior session converted `fort1.s` to `dev/src/fort1.c` and `dev/src/fort1.h`. The summary noted one confirmed bug and left open a note to fix it. This session resumed from that point.

Prior session completed:
- All hardware register macros (`REG(a)`, SDMCTL, PRIOR, TRIG0, CONSOL, RANDOM_HW, etc.)
- Extern declarations for all game variables and external functions
- All string data tables using `S(c)` macro for Atari screen codes (`.AT -/text/` = subtract 0x20)
- All numeric data tables (grav_tab, chop_tab, laser_tab, pod_tab, elevator_tab, etc.)
- CHR1/CHR2 RLE tables (23-byte and 4-byte)
- All 20+ functions: fort1_start, title, t1_loop, t2_vblank, t3_game_start, main_loop,
  check_level, do_level_1/2/3, move_ramp, unpack, check_modes, m_start, m_new_player,
  m_new_level, make_conture, s_begin, m_game_over, set_fonts, inc_game_points

---

## This Session

**User:** "Output token limit hit. Resume directly — no apology, no recap. Break remaining work into smaller pieces."

**User (current session):** "save a complete copy including your thoughts in a file to my local folder"

---

## What I did first — reviewing for bugs

Before writing anything new, I re-read fort1.c in full and traced the assembly carefully for every section I was uncertain about. Here is my full reasoning chain for each section I checked.

---

### Reasoning: MOVE.RAMP

Assembly:
```
MOVE.RAMP
         LDX #4
.1       LDA ADR1
         STA ADR2
         LDY ADR1+1
         DEY              ; ADR2+1 = ADR1+1 - 1 (one page lower in memory)
         STY ADR2+1
         LDY #5
.2       LDA (ADR2),Y     ; copy from one page below
         STA (ADR1),Y     ; to current page
         DEY
         BPL .2           ; Y=5,4,3,2,1,0 → 6 bytes
         DEC ADR1+1       ; move ADR1 up one page
         DEX
         BPL .1           ; X=4,3,2,1,0 → 5 iterations
         RTS
```

Trace: each iteration copies 6 bytes from the page below to the current page, then ADR1 moves up one page. This scrolls 5 rows up visually by copying each row's content from the row above it. Used for the landing ramp animation in DO.LEVEL.1.

The C conversion writes `dst -= 256` (page up) and `src = dst - 256` (one page further up). After the 5-iteration loop, ADR1_HI is decremented 5 times. The caller (do_level_1) restores ADR1 from temp3/temp4 on each outer iteration, so the modification is harmless.

**Verdict:** C code was correct.

---

### Reasoning: T2 VBlank handler — demo path

Assembly `.4` path (timer expiry = start demo):
```
.4   LDX #-1        ; X = 0xFF
     BNE .5         ; always taken (X != 0)
.5   STX DEMO.STATUS ; demo_status = 0xFF = -1
     JMP T3
```

NOTE: `.4` is reached by `CMP #$F3; BEQ .4` before the button-check code. Mode is NOT touched anywhere in this path. Mode is still TITLE_MODE when JMP T3 fires.

T3 installs VERTBLKD and calls MAIN. In MAIN:
```
LDA DEMO.STATUS
BPL .3              ; if >= 0, skip
INC DEMO.STATUS     ; -1 → 0
LDA #START.MODE
STA MODE
```

So MAIN detects demo_status=-1 on the very next frame, increments it to 0, and sets mode=START_MODE. Then CHECK.MODES fires M.START → NEW.LEVEL.MODE → game starts.

**Bug I found:** The original C code had:
```c
if (temp1_i == 0xF3) {
    demo_status = (uint8_t)-1;
    mode = START_MODE;   // WRONG: assembly never sets mode here
    return;              // WRONG: should call t3_game_start()
}
```

Two problems:
1. Setting `mode = START_MODE` in the VBlank handler, bypassing the one-frame MAIN delay. The assembly leaves mode as TITLE_MODE so MAIN can update it.
2. `return` exits the VBlank handler without calling T3, so VERTBLKD is never installed and MAIN is never entered. The game would just stay in the title VBlank handler loop indefinitely.

**Fix:**
```c
if (temp1_i == 0xF3) {
    demo_status = (uint8_t)-1;
    t3_game_start();   // installs VERTBLKD, enters MAIN (never returns)
    return;
}
```

---

### Reasoning: T2 VBlank handler — button check logic

Assembly:
```
         LDX #START.MODE     ; X = 3 (preset for the "start game" path)
         LDA TRIG0
         BEQ .3              ; TRIG0 == 0 means trigger PRESSED → jump to .3
         ; TRIG0 != 0: trigger not pressed, check console buttons
         LDA CONSOL
         CMP #6
         BEQ .3              ; CONSOL == 6: START button pressed → .3 (X still = START_MODE)
         LDX #OPTION.MODE    ; X = 9
         CMP #7
         BNE .3              ; CONSOL != 7: some other button → .3 (X = OPTION_MODE)
         ; CONSOL == 7: no buttons pressed at all
         JMP VVBLKD.RET      ; return from VBlank without starting game
.3       STX MODE
         LDX #0
         STX OPT.NUM
         INX                 ; X = 1
.5       STX DEMO.STATUS     ; demo_status = 1
         JMP T3
```

Atari hardware context:
- `TRIG0 ($D010)`: 0 = joystick trigger pressed (active low), 1 = not pressed
- `CONSOL ($D01F)` bit pattern (active low):
  - bit 0 = START: 0 means pressed
  - bit 1 = SELECT: 0 means pressed  
  - bit 2 = OPTION: 0 means pressed
  - $07 = `0b111` = nothing pressed
  - $06 = `0b110` = START pressed only
  - $05 = `0b101` = SELECT pressed
  - $03 = `0b011` = OPTION pressed

Four cases:
1. TRIG0==0 (trigger pressed): game_start with START_MODE
2. TRIG0!=0 && CONSOL==6 (START button): game_start with START_MODE
3. TRIG0!=0 && CONSOL!=6 && CONSOL!=7 (SELECT/OPTION): game_start with OPTION_MODE
4. TRIG0!=0 && CONSOL==7 (nothing pressed): return, do nothing

**Bug I found:** The original C code had this:
```c
uint8_t new_mode = START_MODE;
if (TRIG0 != 0) {            // "trigger not pressed" branch — correct
    uint8_t con = CONSOL;
    if (con == 6) {
        /* fire → start */   // comment wrong; 6 is START button, not fire trigger
    } else if (con == 7) {
        new_mode = OPTION_MODE;  // WRONG: 7 = no buttons = should return
    } else {
        return;              // WRONG: else = SELECT/OPTION = should be OPTION_MODE
    }
}
```

Cases 3 and 4 were completely swapped. The "no buttons" path was setting OPTION_MODE, and the "SELECT/OPTION button" path was returning. That means pressing SELECT or OPTION would do nothing, and pressing nothing at all would jump into the options menu.

**Fix:**
```c
uint8_t new_mode = START_MODE;
if (TRIG0 != 0) {               /* trigger NOT pressed */
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
```

---

### Reasoning: M.NEW.PLAYER — BCD decrement

Assembly:
```
SED              ; enable decimal (BCD) mode
LDA CHOP.LEFT
SEC              ; set carry (no borrow into SBC)
SBC #1           ; BCD subtract: A = A - 1
STA CHOP.LEFT
CLD              ; clear decimal mode
;        LDA CHOP.LEFT   ; (commented out — CMP uses A from SBC)
CMP #$99
BNE .1           ; not wrapped: continue
LDA #GAME.OVER.MODE
STA MODE
RTS
```

In 6502 BCD mode, SEC before SBC means "no borrow in". So `A = A - 1` in BCD arithmetic:
- $07 → $06 ✓ (units digit 7→6, no tens borrow needed)
- $10 → $09 ✓ (units digit 0→9 with borrow from tens: 10 becomes 09)
- $20 → $19 ✓ (units 0→9 with borrow from tens: 20 becomes 19)
- $00 → $99 ✓ (both digits 0→9, hundreds borrow: -1 in BCD = 99)

The last case ($00 → $99) is the wraparound. After it, CMP #$99 is true → GAME_OVER_MODE.

**Bug I found:** Original C:
```c
if ((chop_left & 0x0F) == 0)
    chop_left = (uint8_t)((chop_left - 0x10) | 0x09);
else
    chop_left--;
```

For `chop_left == 0x00`: units nibble IS 0, so takes top branch. Computes:
`0x00 - 0x10 = 0xF0` (underflow in 8-bit!), then `0xF0 | 0x09 = 0xF9`.
But the correct BCD result is $99, not $F9.

The formula `(val - 0x10) | 0x09` only works when the tens digit is non-zero (normal borrow from tens). When tens is also 0, we need to wrap to 0x99.

**Fix:**
```c
if ((chop_left & 0x0F) == 0) {
    if ((chop_left & 0xF0) == 0)
        chop_left = 0x99;                              /* BCD 00 - 1 = 99 */
    else
        chop_left = (uint8_t)((chop_left - 0x10) | 0x09);
} else {
    chop_left--;
}
```

Verification of all BCD-relevant values:
- 0x07: units=7≠0 → 0x06 ✓
- 0x09: units=9≠0 → 0x08 ✓
- 0x10: units=0, tens=1 → (0x10-0x10)|0x09 = 0x00|0x09 = 0x09 ✓
- 0x11: units=1≠0 → 0x10 ✓
- 0x17: units=7≠0 → 0x16 ✓
- 0x19: units=9≠0 → 0x18 ✓
- 0x20: units=0, tens=2 → (0x20-0x10)|0x09 = 0x10|0x09 = 0x19 ✓
- 0x00: units=0, tens=0 → 0x99 ✓ (was wrong before)

---

### Reasoning: M.GAME.OVER — chops penalty formula

Assembly:
```
LDA #2
CLC              ; carry CLEAR before SBC = add one borrow
SBC CHOPS        ; A = 2 - CHOPS - (1 - C) = 2 - CHOPS - 1 = 1 - CHOPS
EOR #-1          ; A = ~A = ~(1 - CHOPS)
JSR INC.GAME.POINTS
```

In 8-bit: `~(1 - CHOPS)`:
- CHOPS=0 (hardest, 7 lives): `~(1-0)` = `~1` = 0xFE (add -2 to game_points)
- CHOPS=1 (medium, 9 lives):  `~(1-1)` = `~0` = 0xFF (add -1)
- CHOPS=2 (easiest, 17 lives): `~(1-2)` = `~0xFF` = 0x00 (add 0)

This makes more lives = lower rank. Clever.

Alternative derivation: `~(1-CHOPS)` = `CHOPS - 2` in 8-bit arithmetic:
- CHOPS=0: 0-2 = 0xFE ✓
- CHOPS=1: 1-2 = 0xFF ✓
- CHOPS=2: 2-2 = 0x00 ✓

**Bug I found:** Original C had:
```c
inc_game_points((uint8_t)(int8_t)(2 - (int8_t)chops) ^ 0xFF);
```

This computes `(2 - chops) ^ 0xFF`. Let me check:
- CHOPS=0: (2-0) ^ 0xFF = 2 ^ 0xFF = 0xFD ≠ 0xFE (off by one!)
- CHOPS=2: (2-2) ^ 0xFF = 0 ^ 0xFF = 0xFF ≠ 0x00 (completely wrong!)

The error: `^` is XOR (bitwise NOT when XORed with 0xFF), but the formula before XOR was wrong. `(2 - chops)` should be `(1 - chops)` because `CLC; SBC` subtracts one extra (the borrow). I had used `(2 - chops)` instead of `(1 - chops)`.

**Fix:**
```c
inc_game_points((uint8_t)(chops - 2));
```

This directly gives the correct values for all three CHOPS values.

---

### Reasoning: M.NEW.LEVEL — level name selection

Assembly:
```
         LDA #2
         STA TEMP1
         LDA #8
         STA TEMP2
         LDY LEVEL
         DEY          ; Y = LEVEL - 1
         BEQ .0       ; branch if LEVEL - 1 == 0, i.e., LEVEL == 1
         ; LEVEL != 1: print LVL.1
.5       LDX #LVL.1
         LDY /LVL.1
         JSR PRINT
         JMP .1
.0       ; LEVEL == 1: print LVL.2
         LDX #LVL.2
         LDY /LVL.2
         JSR PRINT
.1       ...
```

Tracing:
- LEVEL=0: DEY → Y=$FF ≠ 0, BEQ not taken → LVL.1 ("VAULTS OF DRACONIS")
- LEVEL=1: DEY → Y=0, BEQ taken → LVL.2 ("CRYSTALLINE CAVES")
- LEVEL=2: DEY → Y=1 ≠ 0, BEQ not taken → LVL.1 ("VAULTS OF DRACONIS")

Level 0 and level 2 both use the same packed map (PACKED_MAP_BASE+0x000) and same title. Level 2 is the second time through the Vaults (deeper level). Level 1 is Crystalline Caves (different map at PACKED_MAP_BASE+0x62B).

**Bug I found:** Original C had `if (level == 0) { print lvl_2 } else { print lvl_1 }`.
- level 0 got "CRYSTALLINE CAVES" (wrong, should be "VAULTS")
- level 1 got "VAULTS OF DRACONIS" (wrong, should be "CRYSTALLINE CAVES")
- level 2 got "VAULTS OF DRACONIS" (accidentally correct)

**Fix:**
```c
if (level == 1) {
    print_str(2, 8, lvl_2);    /* CRYSTALLINE CAVES */
} else {
    print_str(2, 8, lvl_1);    /* VAULTS OF DRACONIS */
}
```

---

### Reasoning: S.BEGIN — outer rescan loop exits too early

Assembly:
```
LDX #7
.9   LDA #MAP          ; reset scan pointer to map start
     STA ADR1
     LDA /MAP
     STA ADR1+1
     LDY #0
.10  [check (ADR1),Y and Y+1 for pattern $48,$48 with $1F above]
     [if valid random: place slave at X, DEX, BMI .12]
.11  INY
     BNE .10           ; Y wrapped → move to next page
     INC ADR1+1
     LDA ADR1+1
     CMP /MAP+$2800
     BNE .10           ; keep scanning this map
     TXA
     BPL .9            ; X >= 0: more slaves to place → RESCAN from beginning
.12  ...
     LDA #NEW.PLAYER.MODE
     STA MODE
     RTS
```

The `TXA; BPL .9` after a full-map scan: if X (slave slot index) is still >= 0, we have unplaced slaves, so restart the scan from MAP start. This handles cases where the random check (10 ≤ rnd < 50) missed some valid spawn points on the first pass.

**Bug I found:** Original C:
```c
for (;;) {
    uint8_t *mp = MAP_BASE;
    for (;;) {
        for (int y = 0; y < 256; y++) {
            // scan...
            if (--slot < 0) goto done;
        }
        mp += 256;
        if (mp >= (uint8_t *)MAP_END_ADDR) break;
    }
    if (slot >= 0) break;   // BUG: this is ALWAYS true when reached
}
done:
```

The `goto done` fires when `slot < 0` (all placed). The only way to reach `if (slot >= 0) break` is when a full map scan completed WITHOUT placing all slaves (i.e., slot is still >= 0). So the condition `if (slot >= 0)` is ALWAYS true when we get there, meaning it ALWAYS breaks after exactly one scan pass.

The correct behavior is to rescan indefinitely (until `goto done` fires). The break should never happen.

**Fix:** Remove the `if (slot >= 0) break` line entirely, letting the `for(;;)` repeat until all slaves are placed via `goto done`.

```c
for (;;) {
    uint8_t *mp = MAP_BASE;
    for (;;) {
        for (int y = 0; y < 256; y++) {
            // scan...
            if (--slot < 0) goto done;
        }
        mp += 256;
        if (mp >= (uint8_t *)MAP_END_ADDR) break;
    }
    /* rescan (TXA; BPL .9): slot < 0 already handled by goto done */
}
done:
```

---

### Reasoning: SCANNER_END_ADDR

Fort.s equate: `SCANNER .EQ $39C0 $640 R`

`$640` hex = 1600 decimal. Scanner is 40 columns × 40 rows = 1600 bytes.

`SCANNER_END_ADDR = 0x39C0 + 0x0640 = 0x4000`.

**Bug:** Original had `0x39C0 + 0x1600` where I confused the hex value `0x0640` with the decimal value 1600 = `0x0640`, and instead wrote `0x1600` (which is 5632 decimal). That would make the scanner unpack routine write 5632 bytes starting at $39C0, overwriting through $4FC0 — deep into the display list area.

**Fix:** `#define SCANNER_END_ADDR (0x39C0u + 0x0640u)`

---

### Reasoning: M.GAME.OVER rank display

Assembly (after printing G.3 = "CLASS"):
```
         LDA ADR1+1    ; adr1 now points past the string PRINT just wrote
         ; [compute rank from game_points]
         LDA GAME.POINTS
         AND #3
         EOR #3        ; flip low 2 bits
         CLC
         ADC #1        ; rank = ((game_points & 3) ^ 3) + 1  →  range 1..4
         ORA #$90      ; reverse-video digit
         STA (ADR1),Y  ; write at position Y=12 of last print row
         AND #$8F      ; keep reverse-video bit, clear lower nibble upper part?
         STA (ADR1),Y+1; write at Y=13
```

Wait, the exact instruction is:
```
         LDA GAME.POINTS
         AND #3         ; low 2 bits = "class within grade" (0..3)
         EOR #3         ; invert: 0→3, 1→2, 2→1, 3→0
         CLC
         ADC #1         ; +1: range 1..4
         ORA #$90       ; set reverse-video flag ($80) and high nibble bit ($10)?
         STA ...
         AND #$8F       ; mask: keep $80 and low nibble
```

`$90 = 0b10010000`. ORA #$90 sets bits 7 and 4. AND #$8F = 0b10001111 clears bit 6 and 4 (since $8F = $80|$0F). Wait: `$8F = 0b10001111`. So AND #$8F preserves bit 7 ($80) and bits 0-3, but clears bits 4,5,6.

Actually `ORA #$90` sets: reverse-video ($80) + something else ($10). Then `AND #$8F` clears $10 (bit 4) — so the second store has only the reverse-video bit plus the low 3 bits (rank digit). The first store has reverse-video + $10 + rank digit.

This writes two bytes to the screen: a reverse-video rank digit (with an extra flag bit), and then the digit without that flag. These are Atari screen codes for the rank display in the game-over screen.

My C was:
```c
uint8_t rank_num = (uint8_t)(((game_points & 3) ^ 3) + 1);
row[12] = rank_num | 0x90;
row[13] = (row[12]) & 0x8F;
```

This is correct. `row[12] = rank_num | 0x90` = the rank_num with bits 7 and 4 set. `row[13] = row[12] & 0x8F` = same but bit 4 cleared.

**Verdict:** C code was correct.

---

### Reasoning: MAKE.CONTURE tile replacement

Assembly:
```
.2       LDA RANDOM
         AND #3
;        CMP #0
         BEQ .2     ; retry if random is 0 (want 1-3)
         CLC
         ADC #$62-1  ; r + $61 = $62,$63,$64 for r=1,2,3
         BNE .5      ; forced (result != 0)
.3       CMP #$74    ; 't'
         BNE .5
.4       LDA RANDOM
         AND #3
;        CMP #0
         BEQ .4     ; retry if random is 0
         CLC
         ADC #$65-1  ; r + $64 = $65,$66,$67 for r=1,2,3
```

For 's' ($73): replacement = (random & 3) + $61 where random != 0 → range $62..$64
For 't' ($74): replacement = (random & 3) + $64 where random != 0 → range $65..$67

My C:
```c
b = (uint8_t)(r + 0x62 - 1);  // r=1: $62, r=2: $63, r=3: $64
b = (uint8_t)(r + 0x65 - 1);  // r=1: $65, r=2: $66, r=3: $67
```

**Verdict:** C code was correct.

---

### Reasoning: MAKE.CONTURE — map row copy

Assembly:
```
         LDA #MAP           ; ADR1 = MAP ($1103)
         STA ADR1
         LDA /MAP
         STA ADR1+1
         LDA #MAP+255-40    ; ADR2 = MAP + 215 ($11DA)
         STA ADR2
         LDA /MAP+255-40    ; = $11
         STA ADR2+1
         LDX #0
.6       LDY #0
.7       LDA (ADR1),Y
         STA (ADR2),Y
         INY
         CPY #40
         BNE .7             ; copy 40 bytes (row width)
         INC ADR1+1
         INC ADR2+1
         INX
         CPX #40
         BNE .6             ; 40 rows
```

ADR1 starts at $1103. ADR2 starts at $11DA = MAP+215. Both high bytes are incremented together, so after row k: ADR1 = ($1103 + k*256), ADR2 = ($11DA + k*256). The copy is always 40 bytes at Y=0..39.

In flat C memory: `src[y]` where src=MAP_BASE covers MAP[0..39] = $1103..$112A for row 0. After `src += 256`: src=MAP_BASE+256 → covers $1203..$122A for row 1. etc.

`dst = MAP_BASE + 215` = $11DA. `dst[y]` for y=0..39 covers $11DA..$1201 (crosses page boundary, which is fine in flat memory). After `dst += 256`: $12DA..$1301. etc.

**Verdict:** C code was correct. The flat array model handles the page-crossing correctly.

---

## Files produced this session

No new files — only corrections to the files produced in chat4.md:

**`dev/src/fort1.c`** — 6 bugs fixed:
1. `SCANNER_END_ADDR`: `0x1600` → `0x0640`
2. `t2_vblank` demo path: added `t3_game_start()` call, removed wrong `mode = START_MODE`
3. `t2_vblank` button logic: CONSOL==7 and CONSOL!=6&&!=7 cases were swapped
4. `m_new_player` BCD: added `0x00→0x99` wrap case
5. `m_new_level` level name: `if (level == 0)` → `if (level == 1)`
6. `s_begin` rescan: removed always-true break that terminated scan after one pass
7. `m_game_over` chops penalty: `(2-chops)^0xFF` → `(chops-2)` (formula was off by one)

---

## Next files to convert

Per fort.s `.IN` include order after fort1.s:
- `fort2.s` — pod/missile/tank movement, hyper chamber, screen on/off, PRINT, DDIG, WAIT.FRAME
- `fort3.s` — VERTBLKD, SAVE.POS, UPDATE.CHOPPER, DO.CHECKSUM2
- `fort4.s` — HOVER, COMPUTE.MAP.ADR, DDIG, DRAW.MAP, joystick read
- `fort5.s` — MOVE.SLAVES, SET.SCANNER, CHECK.FUEL.BASE, CHECK.FORT, DO.SOUNDS, LINE1 (DLI)
- `fort6.s` — data: display lists, chopper shapes, laser shapes
- `fort7.s` — variable declarations (already read; minimal C needed)
- `fort8.s` — display list init data blocks Z1/Z2
