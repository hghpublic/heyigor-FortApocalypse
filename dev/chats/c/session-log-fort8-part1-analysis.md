# Session Log: convert fort8.s — Part 1: Analysis

## Task
Convert `fort8.s` (DISPLAY LIST RAM BASED STUFF) to C.
Output files: `dev/src/fort8.h` and `dev/src/fort8.c`.

---

## fort8.s — Source File Overview

`fort8.s` is 67 lines. Unlike fort7.s (pure BSS), fort8.s is a pure **data** file:
no code, no BSS blocks — only `.EQ` equates and `.HS`/`.DA`/`.AT` data definitions.

It defines:
1. **Z1** — 74-byte ANTIC display list ROM template
2. **NAVA.PANEL** — 33-byte navigation bar title text
3. **Z2** — 200-byte panel display buffer ROM template
4. Several `.EQ` offset constants (DSP.MAP, MAP.LINES, SCORE.DIG, FUEL.DIG, BONUS.DIG, Z1.LEN, Z2.LEN)

---

## Key Architectural Concept: ROM Templates vs RAM Buffers

Two address equates at the top of fort8.s define the critical relationship:

```
DSP.LST1 .EQ RAM1.STUFF     ; display list lives in RAM at $0C90
PANEL    .EQ RAM2.STUFF     ; panel buffer lives in RAM at $0100
```

The data blocks Z1 and Z2 are **ROM templates** — they live in the cartridge ROM.
At startup, `CART.START` (or `unpack()` in the C port) copies them to RAM:
- Z1 → RAM1.STUFF ($0C90) = the working game display list
- Z2 → RAM2.STUFF ($0100) = the working panel display buffer

This means:
- `z1[]` and `z2[]` in C are `const` (ROM templates, never written at runtime)
- `dsp_lst1[]` is a mutable RAM buffer (working copy of z1, patched at runtime)
- fort1.c's `unpack()` copies z1 to RAM1_STUFF and z2 to RAM2_STUFF

---

## References in Existing C Files

From grepping fort1.c–fort5.c:

### fort1.c
```c
#define RAM1_STUFF     ((uint8_t *)0x0C90u)   /* CHR.SET2 + 144 */
#define RAM2_STUFF     ((uint8_t *)0x0100u)
extern const uint8_t z1[];
extern const uint8_t z2[];
extern const uint8_t z1_len;    /* .EQ *-Z1-1 */
extern const uint8_t z2_len;    /* .EQ *-Z2-1 */
/* copies z1 to RAM1_STUFF, z2 to RAM2_STUFF at startup */
```

### fort2.c
```c
extern const uint8_t dsp_lst1[];   /* main game display list */
set_sdlst(dsp_lst1);               /* passes dsp_lst1 address to SDLST shadow register */
```

### fort4.c
```c
#define RAM1_STUFF      ((uint8_t *)0x0C90u)
#define RAM2_STUFF_BASE ((uint8_t *)0x0100u)
#define DSP_MAP_PTR     (RAM1_STUFF + 20u)      /* confirmed DSP.MAP offset = 20 */
#define SCORE_DIG_PTR   (RAM2_STUFF_BASE + 19u)  /* confirmed SCORE.DIG offset = 19 */
#define BONUS_DIG_PTR   (RAM2_STUFF_BASE + 149u) /* confirmed BONUS.DIG offset = 149 */
#define FUEL_DIG_PTR    (RAM2_STUFF_BASE + 122u)  /* confirmed FUEL.DIG offset = 122 */
```

These macros in fort4.c **confirm all four critical offsets** before any manual counting.

---

## Critical Discovery: `.DA #byte,addr` Generates 3 Bytes

In SynAssembler:
- `.DA #$44,PANEL` = byte(0x44) + lo(PANEL) + hi(PANEL) = **3 bytes**
- `.DA #$74,0` = byte(0x74) + lo(0) + hi(0) = **3 bytes** (NOT 2 bytes)
- `.DA #$74,0,#$74,0` = **6 bytes** (two 3-byte LMS entries)

This means each ANTIC map scan line entry (mode $74 = HSCROL+VSCROL+LMS+mode4) is
3 bytes: opcode + screen_lo + screen_hi.

**Impact on Z1 size:**
- Initial incorrect estimate: 57 bytes (assuming `.DA #$74,0` = 2 bytes)
- Correct count: **74 bytes** (each .DA = 3 bytes)
- z1_len = 73

The session summary inherited from context had noted "Z1 byte layout (74 bytes total,
z1_len=73)" — this was confirmed by the corrected byte counting.

---

## Z1 Byte Structure (74 bytes)

### Header (offsets 0–19, 20 bytes)

| Offset | Bytes | Assembly | Description |
|--------|-------|----------|-------------|
| 0–3    | 4 | `.HS 70708070` | 4 blank-line entries |
| 4–6    | 3 | `.DA #$44,PANEL` | LMS+mode4 → PANEL ($0100) |
| 7–10   | 4 | `.HS 04040404` | 4 mode-4 rows |
| 11–13  | 3 | `.DA #$44,NAVA.PANEL` | LMS+mode4 → NAVA.PANEL |
| 14–16  | 3 | `.DA #$44+$80,PLAY.SCRN` | DLI+LMS+mode4 → PLAY.SCRN ($0300) |
| 17–19  | 3 | `.HS 502080` | mode bytes |

**DSP.MAP at offset 20** (confirmed by `fort4.c: DSP_MAP_PTR = RAM1_STUFF + 20`)

### Map Scan Lines (offsets 20–70, 51 bytes)

17 entries × 3 bytes = 51 bytes:
- Entries 1–16: 8 lines of `.DA #$74,0,#$74,0` = 8 × 6 = 48 bytes
  Each entry: `0x74, 0x00, 0x00` (HSCROL+VSCROL+LMS+mode4, addr=0; updated at runtime)
- Entry 17: `.DA #$54+$80,0` = `0xD4, 0x00, 0x00` (DLI+HSCROL+VSCROL+LMS+mode4)

### JVB (offsets 71–73, 3 bytes)

`.DA #$41,DSP.LST1` = `0x41, 0x90, 0x0C` (JVB → RAM1.STUFF = $0C90)

In the ROM template `z1[]`, this hardcodes the Atari RAM address. In `dsp_lst1[]`
(the C port working copy), `fort8_init()` patches bytes [72..73] with dsp_lst1's
actual C address.

---

## NAVA.PANEL Structure (33 bytes)

Navigation bar title, displayed via the LMS entry at dsp_lst1[11..13]:

```
.AT /           /   → 11 × 0x00  (11 spaces in ATARI screen code)
.HS AECEA1C1B6D6A1C1 → 8 bytes   (NAVA — double-wide inverse chars)
.HS B4D4B2D2AFCFAECE → 8 bytes   (TRON — double-wide inverse chars)
.AT /      /        → 6 × 0x00   (6 spaces)
```
Total: 11 + 8 + 8 + 6 = **33 bytes**

NAVA.PANEL is not referenced by any existing C file → defined `static` in fort8.c.

---

## Z2 Byte Structure (200 bytes)

### ATARI Screen Code Conversion

`.AT /text/` converts characters: ATARI_code = ASCII_code − 0x20
- space (0x20) → 0x00
- `:` (0x3A) → 0x1A
- `;` (0x3B) → 0x1B
- `<` (0x3C) → 0x1C
- `=` (0x3D) → 0x1D
- `>` (0x3E) → 0x1E
- `?` (0x3F) → 0x1F

### Offset Map

| Offset | Size | Content | Note |
|--------|------|---------|------|
| 0–6    | 7    | `.AT /       /` (7 sp) | |
| 7–16   | 10   | `.HS B3D3...A5C5` SCORE | inverse chars |
| 17–18  | 2    | `.AT /  /` (2 sp) | |
| **19** | —    | **SCORE.DIG** | digit area start |
| 19–30  | 12   | `.AT /            /` (12 sp) | |
| 31–39  | 9    | `.AT /         /` (9 sp) | |
| 40–59  | 20   | `.AT /                    /` (20 sp) | |
| 60–79  | 20   | `.AT /                    /` (20 sp) | |
| 80–81  | 2    | `.AT /  /` (2 sp) | |
| 82–89  | 8    | `.HS A6C6...ACCC` FUEL | inverse chars |
| 90–92  | 3    | `.AT /  :/` sp sp `:` | |
| 93–104 | 12   | `.HS 5C5D...6667` fuel bar | |
| 105–107| 3    | `.AT /=  /` `=` sp sp | |
| 108–117| 10   | `.HS A2C2...B3D3` BONUS | inverse chars |
| 118–121| 4    | `.AT /    /` (4 sp) | |
| **122**| —    | **FUEL.DIG** | digit area start |
| 122–129| 8    | `.AT /        /` (8 sp) | |
| 130–132| 3    | `.AT /  ;/` sp sp `;` | |
| 133–144| 12   | `.HS 6869...7273` hi-score bar | |
| 145–148| 4    | `.AT />   /` `>` 3sp | |
| **149**| —    | **BONUS.DIG** | digit area start |
| 149–156| 8    | `.AT /        /` (8 sp) | |
| 157–159| 3    | `.AT /   /` (3 sp) | |
| 160–172| 13   | `.AT /            </` 12sp `<` | |
| 173–184| 12   | `.HS 7475...7E7F` bonus bar | |
| 185–199| 15   | `.AT /?              /` `?` 14sp | |

Total: 200 bytes, z2_len = 199. ✓

Verification: 7+10+2=19 ✓ · 19+12+9+20+20+2+8+3+12+3+10+4=122 ✓ · 122+8+3+12+4=149 ✓

---

## dsp_lst1 vs z1 Design Decision

`fort6.c` established the pattern for `dsp_lst2` and `dsp_lst3`:
- Defined as `uint8_t` (mutable), not `const`
- `fort6_init()` patches the JVB self-reference bytes at runtime
- fort2.c has its own `extern const uint8_t dsp_lst2[]` declaration (different TU, const view)

The same pattern applies to `dsp_lst1`:
- `fort8.c` defines `uint8_t dsp_lst1[74]` (mutable)
- `fort8.h` declares `extern uint8_t dsp_lst1[74]`
- fort2.c keeps its own `extern const uint8_t dsp_lst1[]` declaration (not changed)
- `-fsyntax-only` does not cross-check TU type compatibility → no error

---

## Design Decisions

1. **`z1[]` and `z2[]` are `const`** — they are ROM templates, read by fort1.c's unpack()
2. **`dsp_lst1[]` is mutable** — it is the working RAM display list; fort8_init() patches it
3. **`nava_panel[]` is `static`** — not referenced outside fort8.c
4. **JVB in z1 = 0x41, 0x90, 0x0C** — correct Atari address (RAM1.STUFF=$0C90)
5. **JVB in dsp_lst1 = 0x41, 0x00, 0x00** — patched by fort8_init() to dsp_lst1's C address
6. **NAVA.PANEL in z1 = 0x44, 0x00, 0x00** — placeholder; dsp_lst1 patched to nava_panel's C address
7. **No `MAP_LINES`, `DSP_MAP`, `SCORE_DIG`, `FUEL_DIG`, `BONUS_DIG` in fort8.h** — fort4.c already handles these as local `#define` macros with RAM1_STUFF/RAM2_STUFF_BASE arithmetic
8. **`z1_len = 73`, `z2_len = 199`** — declared `const uint8_t` to match fort1.c's extern declarations
