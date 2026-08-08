# Session Log: convert fort1.s — Part 2: Implementation

## Files Produced
- `dev/src/fort1.h` (58 lines)
- `dev/src/fort1.c` (1212 lines)

---

## fort1.h — Key Declarations

```c
#pragma once
#include <stdint.h>

/* Game mode constants (MODE variable) */
#define TITLE_MODE        1
#define START_MODE        2
#define GO_MODE           3
#define STOP_MODE         4
#define NEW_PLAYER_MODE   5
#define NEW_LEVEL_MODE    6
#define GAME_OVER_MODE    7
#define CHECK_MODE        8
#define DEMO_MODE         9
#define HYPERSPACE_MODE  10

/* Status constants */
#define STATUS_OFF    1
#define STATUS_ON     2
#define STATUS_FULL   3
#define STATUS_EMPTY  4
#define STATUS_DEAD   5
#define STATUS_GOING  6
#define STATUS_GO     7
#define STATUS_STOP   8
#define STATUS_PICKUP 11

#define MAX_TANKS  6
#define MAX_PODS   39

/* Exported functions */
void fort1_start(void);
void title(void);
void t1_loop(void);
void t2_vblank(void);
void t3_game_start(void);
void main_loop(void);
void check_level(void);
void do_level_1(void);
void move_ramp(void);
void do_level_2(void);
void do_level_3(void);
void unpack(void);
void check_modes(void);
void m_start(void);
void m_new_player(void);
void m_new_level(void);
void inc_game_points(uint8_t n);
void m_game_over(void);
```

---

## fort1.c — Architecture

### Hardware Register Macro
```c
#define REG(a) (*(volatile uint8_t *)(uintptr_t)(a))
```
Every hardware read/write uses REG(ADDR). Example:
```c
REG(0xD40E) = 0;          /* NMIEN = 0: disable NMIs */
REG(0x022F) = 0x22;       /* SDMCTL: DMA on */
```

### Vector Installation
```c
static void set_vector(uint16_t addr, void (*fn)(void)) {
    REG(addr)   = (uint8_t)((uintptr_t)fn & 0xFFu);
    REG(addr+1) = (uint8_t)((uintptr_t)fn >> 8);
}
#define SET_VVBLKD(fn)  set_vector(0x0224u, (void(*)(void))(fn))
#define SET_VDSLST(fn)  set_vector(0x0200u, (void(*)(void))(fn))
```

### Screen Code Macro
```c
#define S(c) ((uint8_t)((unsigned char)(c) - 0x20u))
```
Used for `.AT -/TEXT/` strings. Space → 0x00, letters →  uppercase_letter - 0x20.

---

## Key Function Translations

### unpack() — RLE Decompressor
```
Assembly: UNPACK — reads ADR2 source (packed data), writes ADR1 dest (map or scanner).
temp4 selects char table: 0 → CHR1 (23-byte map table), 1 → CHR2 (4-byte scanner table)
RLE encoding: each source byte B:
  if B < CHR1_LEN: repeat chr1[B] for (B+1) times
  else: emit raw chr1[B] (the escape mechanism)
```

```c
void unpack(void) {
    uint8_t *src = (uint8_t *)(uintptr_t)((uint16_t)adr2_hi<<8 | adr2_lo);
    uint8_t *dst = (uint8_t *)(uintptr_t)((uint16_t)adr1_hi<<8 | adr1_lo);
    const uint8_t *tbl   = (temp4 == 0) ? chr1 : chr2;
    uint8_t        tbl_len = (temp4 == 0) ? CHR1_LEN : CHR2_LEN;
    for (;;) {
        uint8_t b = *src++;
        if (b >= tbl_len) { *dst++ = tbl[b]; }   /* raw emit */
        else {
            uint8_t ch = tbl[b];
            uint8_t count = b + 1;
            while (count--) *dst++ = ch;          /* run */
        }
        if (--temp5 == 0) break;
    }
    adr1_lo = (uint8_t)((uintptr_t)dst & 0xFFu);
    adr1_hi = (uint8_t)((uintptr_t)dst >> 8);
}
```

### t2_vblank() — Title Screen VBlank Handler
Assembly: reads TRIG0 (fire button) or CONSOL (START key) in demo mode; cycles title colors.
Exits via `JMP VVBLKD.RET`.
```c
void t2_vblank(void) {
    if (demo_status != 0) {
        if (REG(TRIG0) == 0 || REG(CONSOL) == 6) {
            demo_status = (uint8_t)-1;
            t3_game_start();   /* Bug 2 fix: jump to T3, not SET mode directly */
            return;
        }
    }
    /* color cycle code ... */
}
```

### check_modes() — Mode Dispatcher
```c
void check_modes(void) {
    uint8_t new_mode = 0;
    if (REG(TRIG0) != 0) {         /* Bug 3 fix: TRIG0=0 means PRESSED (active low) */
        uint8_t con = REG(CONSOL);
        if (con != 6) {            /* con==6 means START pressed */
            if (con == 7) return;  /* con==7 means no buttons → no action */
            new_mode = OPTION_MODE;
        }
    }
    /* ... */
    switch (mode) {
        case START_MODE:      m_start();      break;
        case NEW_PLAYER_MODE: m_new_player(); break;
        case NEW_LEVEL_MODE:  m_new_level();  break;
        case GAME_OVER_MODE:  m_game_over();  break;
    }
}
```

### m_new_player() — BCD Decrement
Assembly uses `SED; SEC; SBC #1; CLD` — 6502 BCD mode where $00 - 1 = $99.
```c
void m_new_player(void) {
    /* Bug 4 fix: BCD decrement of chop_left */
    uint8_t cl = chop_left;
    if ((cl & 0x0F) == 0) {
        if ((cl & 0xF0) == 0) {
            cl = 0x99;    /* 0x00 - 1 (BCD) = 0x99 */
        } else {
            cl = (cl - 0x10) | 0x09;  /* e.g. 0x10 - 1 (BCD) = 0x09 */
        }
    } else {
        cl -= 1;
    }
    chop_left = cl;
    /* ... */
}
```

### m_new_level() — Level Name String Selection
Assembly: `LDY LEVEL; DEY; BEQ .0` — BEQ fires when LEVEL==1 (Y=0 after DEY).
```c
void m_new_level(void) {
    const uint8_t *lname = (level == 1) ? lvl_2 : lvl_1;  /* Bug 5 fix */
    print_str(lname, SCORE_AREA);
    /* ... */
}
```

### s_begin() — Slave Placement Loop
Assembly: outer loop `TXA; BPL .9` (jump back to map start if slot >= 0).
```c
static void s_begin(void) {
    int slot = 7;   /* 8 slaves, 0-indexed */
    uint8_t *map_pos = (uint8_t *)MAP_BASE;
    while (1) {
        /* inner scan for tile == SLAVE_TILE */
        uint8_t *p;
        for (p = map_pos; p < (uint8_t *)MAP_END_ADDR; p++) {
            if (*p == SLAVE_TILE) {
                slave_x[slot] = (p - MAP_BASE) % 40;
                slave_y[slot] = (p - MAP_BASE) / 40;
                if (--slot < 0) goto done;
                break;
            }
        }
        /* Bug 6 fix: NO break here — rescan from map_pos if slot >= 0 */
        map_pos = (uint8_t *)MAP_BASE;  /* restart from beginning */
    }
done:;
}
```

---

## Bugs Found and Fixed

### Bug 1 — SCANNER_END_ADDR wrong hex literal
**File:** `fort1.c`, line ~40  
**Symptom:** Scanner memory region calculated as 5632 bytes instead of 1600 bytes  
**Assembly:** `SCANNER .EQ $39C0` + `*-SCANNER .EQ $640` (decimal 1600)  
**Was:**
```c
#define SCANNER_END_ADDR  (0x39C0u + 0x1600u)   /* = 0x4FC0, WRONG */
```
**Fix:**
```c
#define SCANNER_END_ADDR  (0x39C0u + 0x0640u)   /* = 0x4000, correct */
```
`$640` hex = 1600 decimal. The `$` prefix was misread as a thousands digit.

---

### Bug 2 — t2_vblank demo path sets mode directly instead of calling T3
**File:** `fort1.c`, `t2_vblank()`  
**Symptom:** Demo transition bypasses game VBlank installation  
**Assembly:** `JMP T3` — branches to T3 label which installs game VBlank then enters MAIN  
**Was:**
```c
demo_status = (uint8_t)-1;
mode = START_MODE;   /* Wrong: skips T3, game VBlank never installed */
return;
```
**Fix:**
```c
demo_status = (uint8_t)-1;
t3_game_start();     /* Install game VBlank + enter MAIN loop */
return;
```

---

### Bug 3 — TRIG0/CONSOL active-low logic inverted
**File:** `fort1.c`, `check_modes()`  
**Symptom:** Fire button triggers option menu; option keys trigger fire  
**Assembly:**  
```
LDA TRIG0 : BEQ .skip   ; TRIG0=0 means PRESSED, skip if NOT pressed
LDA CONSOL : CMP #6 : BEQ .start_pressed
CMP #7 : BEQ .no_buttons
; else → OPTION or SELECT pressed
```
**Was:**
```c
if (REG(TRIG0) == 0) { ... }   /* inverted: 0=pressed is correct, but branch logic wrong */
```
**Fix:**
```c
if (REG(TRIG0) != 0) {          /* skip if trigger IS pressed (BEQ .skip = skip if Z=1 = if 0) */
    uint8_t con = REG(CONSOL);
    if (con != 6) {
        if (con == 7) return;
        new_mode = OPTION_MODE;
    }
}
```
Key insight: `BEQ .skip` in assembly skips when TRIG0==0 (pressed). In C: `if (TRIG0 != 0)` to enter the block.

---

### Bug 4 — m_new_player BCD decrement misses 0x00→0x99 wrap
**File:** `fort1.c`, `m_new_player()`  
**Symptom:** `chop_left` wraps from 0x00 to 0xF0 instead of 0x99  
**Assembly:** `SED; SEC; SBC #1; CLD` — 6502 BCD mode: 00-1=99  
**Was:**
```c
if ((chop_left & 0x0F) == 0)
    chop_left = (chop_left - 0x10) | 0x09;  /* misses 0x00 case: 0x00-0x10=-16 = 0xF0|9 */
```
**Fix:** Add separate check for 0x00 before tens-decrement:
```c
if ((cl & 0x0F) == 0) {
    if ((cl & 0xF0) == 0) cl = 0x99;          /* BCD 00-1 = 99 */
    else cl = (cl - 0x10) | 0x09;              /* BCD x0-1 = (x-1)9 */
} else {
    cl -= 1;
}
```

---

### Bug 5 — m_new_level level-name string selection inverted
**File:** `fort1.c`, `m_new_level()`  
**Symptom:** Level 0 showed Crystalline Caves; Level 1 showed Vaults of Draconis (swapped)  
**Assembly:**
```
LDY LEVEL : DEY : BEQ .0   ; BEQ fires when LEVEL==1 (Y=0 after DEY)
.0: LDA #LVL.2,... (CRYSTALLINE CAVES)  ; reached when LEVEL==1
    [else LVL.1 = VAULTS OF DRACONIS]   ; reached when LEVEL==0 or LEVEL==2
```
**Was:**
```c
const uint8_t *lname = (level == 0) ? lvl_2 : lvl_1;   /* Wrong */
```
**Fix:**
```c
const uint8_t *lname = (level == 1) ? lvl_2 : lvl_1;   /* DEY; BEQ .0 → BEQ fires when level==1 */
```

---

### Bug 6 — s_begin outer rescan loop terminates after one pass
**File:** `fort1.c`, `s_begin()`  
**Symptom:** Only first pass of map scanned for slaves; some slaves never placed  
**Assembly:**
```
.9 LDA #<MAP : STA MAP.PTR     ; reset map pointer to start
   LDA #>MAP : STA MAP.PTR+1
   TXA : BPL .9                ; if slot >= 0, go back to start of map scan
```
`TXA; BPL .9` jumps back to `.9` (rescan from map start) as long as `slot >= 0`.
**Was:**
```c
if (slot >= 0) break;   /* Spurious break — always true when reached → exits after 1 pass */
```
**Fix:**
```c
/* Remove the break. When slot >= 0, loop continues to map_pos = MAP_BASE reset above */
/* Only goto done when --slot < 0 */
```

---

## Compile Test

### Command
```
cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src \
    dev/src/fort1.c dev/src/fort2.c dev/src/fort3.c \
    dev/src/fort4.c dev/src/fort5.c dev/src/fort6.c \
    dev/src/fort7.c dev/src/fort8.c
```

### Result
```
(no output — zero diagnostics)
```

---

## File Summary

| File | Lines | Purpose |
|------|-------|---------|
| `dev/src/fort1.h` | 58 | Mode/status constants, MAX_TANKS/MAX_PODS, all function declarations |
| `dev/src/fort1.c` | 1212 | Entry point, title screen, main loop, level transitions, RLE decompressor, mode dispatcher |
