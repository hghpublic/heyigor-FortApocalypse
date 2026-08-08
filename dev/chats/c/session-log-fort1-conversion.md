# fort1.s → C Conversion: Session Notes
**Date:** 2026-08-08  
**Branch:** agents/asm-to-c-conversion  
**Files produced:** `dev/src/fort1.h`, `dev/src/fort1.c`

---

## What was done

Converted `fort1.s` (Fort Apocalypse game logic and startup) to C. The source is a 1232-line Atari 400/800 6502 SynAssembler file. The C output spans ~1210 lines across two files.

---

## Bugs found and fixed this session

### 1. `SCANNER_END_ADDR` — wrong hex literal

**Code:** `dev/src/fort1.c`, `#define SCANNER_END_ADDR`

**Bug:** was `0x39C0 + 0x1600` (= 0x4FC0), but the scanner is 1600 bytes = `0x640`.  
**Fix:** `0x39C0 + 0x0640` (= 0x4000).

**How I caught it:** Fort.s says `SCANNER .EQ $39C0 $640 R`. The `$640` is 1600 decimal. I wrote `0x1600` instead of `0x0640` — both are 1600 in decimal vs. hex, a classic confusion. The wrong value made the scanner unpack routine write 5632 bytes instead of 1600, overwriting $4FC0 — well into the display list area.

---

### 2. `t2_vblank` — demo path incorrectly sets `mode`

**Code:** lines ~472-478

**Bug:**
```c
if (temp1_i == 0xF3) {
    demo_status = (uint8_t)-1;
    mode = START_MODE;   // WRONG
    return;
}
```

**Fix:**
```c
if (temp1_i == 0xF3) {
    demo_status = (uint8_t)-1;
    t3_game_start();     // jumps into MAIN; MAIN sets mode=START_MODE on next frame
    return;
}
```

**Reasoning:**  
Assembly path `.4`: `LDX #-1; BNE .5` → `.5: STX DEMO.STATUS; JMP T3`. Mode is never touched in this path. Control goes to T3 → MAIN loop. In MAIN: `if ((int8_t)demo_status < 0) { demo_status++; mode = START_MODE; }`. So the mode transition is deferred by one frame — this is intentional (one frame of latency before the demo game starts).

Setting `mode = START_MODE` in the VBlank handler is wrong because:
- it happens mid-interrupt, before the MAIN loop runs
- it bypasses the one-frame demo_status delay
- T3 is never called, so the VBlank handler is never switched back to VERTBLKD

---

### 3. `t2_vblank` — TRIG0/CONSOL logic completely inverted

**Code:** lines ~481-496

**Bug:**
```c
if (TRIG0 != 0) {            // "not pressed" branch
    uint8_t con = CONSOL;
    if (con == 6) {
        /* fire → start */   // comment wrong: 6 = START button, not fire
    } else if (con == 7) {
        new_mode = OPTION_MODE;  // WRONG: 7 = no buttons = should return
    } else {
        return;              // WRONG: else = option/select = should be OPTION_MODE
    }
}
```

**Fix:**
```c
if (TRIG0 != 0) {               /* trigger NOT pressed (TRIG0==0 means pressed) */
    uint8_t con = CONSOL;
    if (con != 6) {             /* START button not pressed */
        if (con == 7) return;   /* no console buttons at all: do nothing */
        new_mode = OPTION_MODE; /* SELECT or OPTION button pressed */
    }
    /* con == 6: START pressed → keep new_mode = START_MODE */
}
```

**Reasoning — Atari hardware:**  
- `TRIG0 ($D010)`: 0 = joystick trigger pressed, 1 = not pressed. (Active low.)
- `CONSOL ($D01F)` bits (active low): bit 0 = START, bit 1 = SELECT, bit 2 = OPTION.
  - 7 = `0b111` = no console buttons pressed
  - 6 = `0b110` = START pressed (bit 0 cleared)
  - 5 = `0b101` = SELECT pressed
  - 3 = `0b011` = OPTION pressed

**Assembly trace:**
```
LDX #START.MODE          ; preset X = 3
LDA TRIG0
BEQ .3                   ; TRIG0 == 0 (pressed) → start game with X=START_MODE
; TRIG0 != 0 (not pressed):
LDA CONSOL
CMP #6
BEQ .3                   ; CONSOL == 6 (START pressed) → start game
LDX #OPTION.MODE         ; X = 9
CMP #7
BNE .3                   ; CONSOL != 7 (some other button) → go to options
JMP VVBLKD.RET           ; CONSOL == 7 (no buttons) → return, do nothing
```

So the four cases are:
1. TRIG0==0: start with START_MODE
2. TRIG0!=0 && CONSOL==6: start with START_MODE
3. TRIG0!=0 && CONSOL!=6 && CONSOL!=7: go to OPTION_MODE (SELECT/OPTION pressed)
4. TRIG0!=0 && CONSOL==7: return (nothing pressed)

The original code had cases 3 and 4 swapped.

---

### 4. `m_new_player` — BCD decrement misses `0x00 → 0x99` wrap

**Code:** lines ~815-819

**Bug:**
```c
if ((chop_left & 0x0F) == 0)
    chop_left = (uint8_t)((chop_left - 0x10) | 0x09);
else
    chop_left--;
```

When `chop_left == 0x00`: units nibble is 0, so takes the top branch. Computes `0x00 - 0x10 = 0xF0`, then `0xF0 | 0x09 = 0xF9`. But BCD 00 - 1 = 99 = `0x99`.

**Fix:**
```c
if ((chop_left & 0x0F) == 0) {
    if ((chop_left & 0xF0) == 0)
        chop_left = 0x99;                              /* BCD 00 - 1 = 99 */
    else
        chop_left = (uint8_t)((chop_left - 0x10) | 0x09);  /* borrow from tens */
} else {
    chop_left--;
}
```

**Reasoning:**  
The assembly uses `SED; SEC; SBC #1; CLD`. In 6502 BCD mode, SBC with SEC set performs A - operand (no borrow). BCD 00 - 1 = 99, same as two's complement thinking in decimal. The Atari BCD mode handles this correctly in hardware. To replicate it in C, I need to check both nibbles.

**Verification:**
- `0x07` → units=7 ≠ 0 → `0x06` ✓
- `0x10` → units=0, tens=1 → `(0x10-0x10)|0x09 = 0x09` ✓
- `0x20` → units=0, tens=2 → `(0x20-0x10)|0x09 = 0x10|0x09 = 0x19` ✓
- `0x00` → units=0, tens=0 → `0x99` ✓

---

### 5. `m_new_level` — level-name string selection inverted

**Code:** lines ~971-975

**Bug:**
```c
if (level == 0) {
    print_str(2, 8, lvl_2);   // WRONG: prints CRYSTALLINE CAVES for level 0
} else {
    print_str(2, 8, lvl_1);   // WRONG: prints VAULTS for levels 1 and 2
}
```

**Fix:**
```c
/* LDY LEVEL; DEY; BEQ .0 — branches to LVL.2 only when LEVEL==1 */
if (level == 1) {
    print_str(2, 8, lvl_2);   /* CRYSTALLINE CAVES */
} else {
    print_str(2, 8, lvl_1);   /* VAULTS OF DRACONIS */
}
```

**Reasoning:**  
Assembly: `LDY LEVEL; DEY; BEQ .0`. DEY after LDY gives Y=LEVEL-1. BEQ branches if Y==0, i.e., if LEVEL==1. Branch target `.0` prints LVL.2. If not taken, prints LVL.1.

- LEVEL=0: Y=$FF, BEQ not taken → LVL.1 (Vaults of Draconis) ✓
- LEVEL=1: Y=0, BEQ taken → LVL.2 (Crystalline Caves) ✓
- LEVEL=2: Y=1, BEQ not taken → LVL.1 (Vaults of Draconis — same map reused) ✓

This makes sense with the map layout: levels 0 and 2 use `PACKED_MAP_BASE+0x000` (same compressed map), level 1 uses `PACKED_MAP_BASE+0x62B`.

---

### 6. `s_begin` — outer rescan loop terminates after one pass

**Code:** lines ~953-957

**Bug:**
```c
for (;;) {
    uint8_t *mp = MAP_BASE;
    for (;;) {
        for (int y = 0; y < 256; y++) {
            // ...
            if (--slot < 0) goto done;
        }
        mp += 256;
        if (mp >= (uint8_t *)MAP_END_ADDR) break;
    }
    if (slot >= 0) break;   // ALWAYS TRUE when reached → one-pass bug
}
done:
```

The `goto done` fires when `slot < 0`. The outer loop's `if (slot >= 0) break` is therefore ALWAYS true when reached (since `slot < 0` exits via goto). So after one full map scan with unplaced slaves, the loop exits instead of rescanning.

**Fix:** Remove the `if (slot >= 0) break;` line:
```c
    /* Rescan from beginning (TXA; BPL .9 in assembly): slot < 0 exits via goto done */
```

**Reasoning:**  
Assembly: after inner scan completes (all pages scanned), `TXA; BPL .9`. X = slot. If slot >= 0 (more slaves to place), BPL is taken → jump back to `.9` which resets ADR1 to MAP start and rescans. This handles sparse maps where some passes find no valid spawn points due to the random 10≤r<50 check. The rescan keeps trying until all 8 slaves are placed (or infinitely, which never happens on the actual map data).

---

## Key translation decisions from prior session (for reference)

- **`.AT -/TEXT/`**: subtracts 0x20 from each ASCII byte (Atari screen codes). Converted using `S(c)` macro: `#define S(c) ((uint8_t)((unsigned char)(c) - 0x20u))`.
- **`.AT /TEXT/`**: raw ASCII bytes, no subtraction.
- **TRIG0/CONSOL**: active-low hardware registers.
- **ADR1/ADR2**: 16-bit zero-page pointers stored as two `uint8_t` variables (lo, hi). Reconstructed as pointer: `(uint8_t*)(uintptr_t)((uint16_t)hi<<8|lo)`.
- **VVBLKD.RET ($E462)**: Atari OS VBlank return address. In C: just `return` from the handler.
- **BLT/BGE**: SynAssembler pseudo-ops for BCC/BCS. `BLT` = unsigned less-than (carry clear after CMP), `BGE` = unsigned ≥ (carry set).
- **PROT code**: self-modifying anti-piracy instructions omitted in C (comments preserved).
- **BCD arithmetic**: 6502 `SED; SEC; SBC #1; CLD` pattern reproduced manually in C (see bug #4 above).
- **`INX; BNE .label`** (from X=0): runs 256 times — modeled as `for (int i = 255; i >= 0; i--)` or `do { ... } while (++x != 0)`.
