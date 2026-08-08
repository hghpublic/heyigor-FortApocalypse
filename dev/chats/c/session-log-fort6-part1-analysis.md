# Session Log: convert fort6.s — Part 1: Analysis

## Task
Convert `fort6.s` (DATA, SHAPES, DISPLAY LISTS) to C.
Output files: `dev/src/fort6.h` and `dev/src/fort6.c`.

---

## fort6.s — Source File Overview

`fort6.s` is 263 lines. It is a pure DATA file — no game logic. It contains:

1. **DSP.LST2** (lines 8–18): ANTIC display list 2 — options/panel screen, 39 bytes
2. **DSP.LST3** (lines 20–26): ANTIC display list 3 — title/play screen, 32 bytes
3. **CART.START** (lines 28–68): 6502 ROM cartridge startup routine
4. **CHOPPER.SHAPES** (lines 70–79): 18-entry pointer table (9 angles × 2 frames)
5. **Sprite data blocks** (lines 81–201): CL1.1/2, CL2.1/2, CL3.1/2, CM1.1/2, CR1.1/2, CR2.1/2, CR3.1/2 — 18 arrays, each nominally 36 bytes
6. **BOOT.STUFF label** (line 199): embedded within CR3.2 sprite data
7. **INIT.OS** (lines 208–217): OS interrupt-vector setup
8. **Signature strings** (lines 219–220): "f3DSdsIaApPLa;" and "Steve Hales"
9. **Cartridge vectors** (lines 222–223): `.DA INIT.OS-1; .DA START-1`
10. **LASER.SHAPES** (lines 225–260): 32 bytes of laser animation patterns

---

## Reviewing Consumer Code in fort3.c

`fort3.c` (already converted) declares and uses `chopper_shapes` as:

```c
/* Line 134 */
extern const uint8_t * const chopper_shapes[18];
```

The usage (lines 647, 718):

```c
const uint8_t *shape = chopper_shapes[chopper_angle];
for (int i = 0; i < 18; i++) {
    PLAYER_PL0[y + (uint8_t)i] = shape[i];
    PLAYER_PL1[y + (uint8_t)i] = shape[18 + i];
}
```

**Critical deduction:** Each shape pointed to by `chopper_shapes[n]` must be **at least 36 bytes**:
- `shape[0..17]` → player 0 (left half)
- `shape[18..35]` → player 1 (right half)

This is confirmed by the assembly structure of each shape data block: two sub-blocks of 18 bytes each, separated by a blank line.

---

## Reviewing Consumer Code in fort4.c

`fort4.c` declares:

```c
/* Line 127 */
extern const uint8_t laser_shapes[32];
```

This resolves directly to `LASER.SHAPES` in fort6.s — 32 bytes of laser animation patterns.

---

## Sprite Shape Byte Layout Analysis

Each shape label (e.g. `CL3.1`) in the assembly has two sub-blocks:

```
CL3.1:
    .HS 0000033DC1030D   ; 7 bytes: first sub-block
    .HS 1121236E79673E   ; 7 bytes
    .HS 10119E60         ; 4 bytes — total first sub-block: 18 bytes

    *                    ; separator (blank line / comment)

    .HS 0000800202028E   ; 7 bytes: second sub-block
    .HS FF396161C0C060   ; 7 bytes
    .HS 3CE0             ; 2 bytes
    ;.HS 0000            ; COMMENTED OUT — 2 zero padding bytes
```

### The Commented-Out Padding Problem

Many shapes end with a commented-out `;.HS 0000` (or `;.HS 00`) line. Without those bytes:
- First sub-block: 18 bytes ✓
- Second sub-block: 16 bytes (or 17 for CR2.1/2) → shape total = 34 (or 35) bytes

But `fort3.c` accesses `shape[18+i]` for `i = 0..17` — requiring exactly 36 bytes.

**Resolution:** The commented-out padding bytes ARE conceptually part of each shape. They are zero (transparent pixels), so removing them from the binary didn't affect the sprite appearance. However, the C conversion must include them to avoid out-of-bounds access when `i = 16` or `17` in the rendering loop.

**Shapes with `;.HS 0000` padding (adds 2 zero bytes to second sub-block):**
CL3.1, CL3.2, CL2.1, CL2.2, CL1.1, CL1.2, CM1.1, CM1.2, CR1.1, CR1.2

**Shapes with `;.HS 00` padding (adds 1 zero byte to second sub-block):**
CR2.1, CR2.2

**Shapes with no padding (second sub-block already 18 bytes):**
CR3.1, CR3.2

---

## Complete Byte Data for All 18 Shapes

Each shape = 36 bytes. Notation: first 18 bytes = PL0 (left half), next 18 bytes = PL1 (right half). Padding zeros shown explicitly.

### CL3.1 (angle 0, frame 0)
```
PL0: 00 00 03 3D C1 03 0D 11 21 23 6E 79 67 3E 10 11 9E 60
PL1: 00 00 80 02 02 02 8E FF 39 61 61 C0 C0 60 3C E0 00 00
```

### CL3.2 (angle 0, frame 1)
```
PL0: 00 00 03 01 01 03 0D 11 21 23 6E 79 67 3E 10 11 9E 60
PL1: 06 78 80 01 01 01 8F FE 3A 62 62 C0 C0 60 3C E0 00 00
```

### CL2.1 (angle 2, frame 0)
```
PL0: 00 00 07 F9 01 07 09 11 21 37 7C 73 3F 18 10 A7 78 00
PL1: 00 00 C0 02 02 02 9E FF 19 71 61 C0 C0 60 3C C0 00 00
```

### CL2.2 (angle 2, frame 1)
```
PL0: 00 00 07 01 01 07 09 11 21 37 7C 73 3F 18 10 A7 78 00
PL1: 00 3E C0 01 01 01 9F FE 1A 72 62 C0 C0 60 3C C0 00 00
```

### CL1.1 (angle 4, frame 0)
```
PL0: 00 00 FF 01 01 0F 11 21 31 7F 70 3F 1F 10 A0 7F 00 00
PL1: 00 00 C0 00 02 02 82 FE 0F 79 61 C1 C0 40 20 FC 00 00
```

### CL1.2 (angle 4, frame 1)
```
PL0: 00 00 07 01 01 0F 11 21 31 7F 70 3F 1F 10 A0 7F 00 00
PL1: 00 00 FE 00 01 01 81 FF 0E 7A 62 C2 C0 40 20 FC 00 00
```

### CM1.1 (angle 6/8/10, frame 0)
```
PL0: 00 00 07 00 00 01 03 06 04 08 0D 07 03 04 08 1C 00 00
PL1: 00 00 FF 80 80 C0 E0 30 10 08 D8 F0 E0 10 08 1C 00 00
```

### CM1.2 (angle 6/8/10, frame 1)
```
PL0: 00 00 7F 00 00 01 03 06 04 08 0D 07 03 04 08 1C 00 00
PL1: 00 00 E0 80 80 C0 E0 30 10 08 D8 F0 E0 10 08 1C 00 00
```

### CR1.1 (angle 12, frame 0)
```
PL0: 00 00 03 00 40 40 41 7F F0 9E 86 83 03 02 04 3F 00 00
PL1: 00 00 FF 80 80 F0 88 84 8C FE 0E FC F8 08 05 FE 00 00
```

### CR1.2 (angle 12, frame 1)
```
PL0: 00 00 7F 00 80 80 81 FF 70 5E 46 43 03 02 04 3F 00 00
PL1: 00 00 E0 80 80 F0 88 84 8C FE 0E FC F8 08 05 FE 00 00
```

### CR2.1 (angle 14, frame 0)
```
PL0: 00 00 03 40 40 40 79 FF 98 8E 86 03 03 06 3C 03 00 00
PL1: 00 00 E0 9F 80 E0 90 88 84 EC 3E CE FC 18 08 E5 1E 00
```

### CR2.2 (angle 14, frame 1)
```
PL0: 00 7C 03 80 80 80 F9 7F 58 4E 46 03 03 06 3C 03 00 00
PL1: 00 00 E0 80 80 E0 90 88 84 EC 3E CE FC 18 08 E5 1E 00
```

### CR3.1 (angle 16, frame 0)
```
PL0: 00 00 01 40 40 40 71 FF 9C 86 86 03 03 06 3C 07 00 00
PL1: 00 00 C0 BC 83 C0 B0 88 84 C4 76 9E E6 7C 08 88 79 06
```

### CR3.2 (angle 16, frame 1) — BOOT.STUFF at PL1 byte offset 7
```
PL0: 60 1E 01 80 80 80 F1 7F 5C 46 46 03 03 06 3C 07 00 00
PL1: 00 00 C0 80 80 C0 B0 88 | 84 C4 76 9E E6 7C 08 88 79 06
                               ^-- BOOT.STUFF label (assembly byte offset 25 in shape)
```

---

## CHOPPER.SHAPES Pointer Table Analysis

Assembly (fort6.s lines 70–79):
```
CHOPPER.SHAPES:
    .DA CL3.1, CL3.2   ; table index 0,1  — angle 0
    .DA CL2.1, CL2.2   ; table index 2,3  — angle 2
    .DA CL1.1, CL1.2   ; table index 4,5  — angle 4
    .DA CM1.1, CM1.2   ; table index 6,7  — angle 6
    .DA CM1.1, CM1.2   ; table index 8,9  — angle 8  (same CM1 shapes repeated)
    .DA CM1.1, CM1.2   ; table index 10,11 — angle 10 (same CM1 shapes repeated)
    .DA CR1.1, CR1.2   ; table index 12,13 — angle 12
    .DA CR2.1, CR2.2   ; table index 14,15 — angle 14
    .DA CR3.1, CR3.2   ; table index 16,17 — angle 16
```

Total: 18 pointers. In C, fort3.c indexes as `chopper_shapes[chopper_angle]` directly — `chopper_angle` runs 0–17 and is used as a flat index. CM1.1 and CM1.2 appear three times in the table (indices 6/7, 8/9, 10/11).

---

## LASER.SHAPES Data

32 bytes, 4 patterns × 8 bytes each:

```
Pattern 0 (diagonal ↘): C0 C0 30 30 0C 0C 03 03
Pattern 1 (diagonal ↗): 03 03 0C 0C 30 30 C0 C0
Pattern 2 (horizontal): 00 00 00 FF FF 00 00 00
Pattern 3 (vertical):   30 30 30 30 30 30 30 30
```

---

## Display List Layout

### DSP.LST2 (39 bytes) — options/panel screen

Embedded Atari addresses:
- PANEL = `$0100` → lo=`$00`, hi=`$01`
- PLAY.SCRN = `$0300` → lo=`$00`, hi=`$03`
- Self-referential JMP (`$41`) at bytes 36–38: address must be patched at runtime

```
70 70 80 70          ; 4× ANTIC blank/nop
44 00 01             ; LMS mode-4, addr=PANEL ($0100)
04 04 04 04 70       ; mode-4 rows + blank
70
80 50 20
44 00 03             ; LMS mode-4, addr=PLAY.SCRN ($0300)
04 04 04 04 04 04 04 ; 7× mode-4 rows
04 04 04 04 04 04 04 ; 7× mode-4 rows
80 70 80
41 ?? ??             ; JMP dsp_lst2 (patched at runtime)
```

### DSP.LST3 (32 bytes) — title/play screen

```
70 70 70 70 70 70    ; 6× ANTIC blank
44 00 03             ; LMS mode-4, addr=PLAY.SCRN ($0300)
04 04 04 04 04 04 04 04 04   ; 9× mode-4 rows
04 04 04 04 04 04 04 04      ; 8× mode-4 rows
44 00 03             ; LMS mode-4, addr=PLAY.SCRN ($0300)
41 ?? ??             ; JMP dsp_lst3 (patched at runtime)
```

---

## BOOT.STUFF

`BOOT.STUFF` is a label at assembly byte offset 25 within `CR3.2` (PL1 byte 7 = the 8th byte of the second sub-block). `CART.START` copies 128 bytes from `BOOT.STUFF` to `$01C0`.

In the C port, this is just part of `shape_cr3_2`'s data. No separate symbol needed — `CART.START` is assembly-only startup code; the C port doesn't need to replicate the copy loop.

---

## INIT.OS

Assembly:
```
INIT.OS:
    LDX #3
    .LOOP:
    LDA $E462,X   ; OS ROM vectors $E460+2..+5
    STA $0223,X   ; write to $0222+1..$0225+1
    DEX
    BPL .LOOP
    RTS
```

This copies the deferred VBlank vector ($E462/$E463) and another vector from ROM to $0223–$0225. In the C port, `fort1.c` sets VBlank vectors directly via `SET_VVBLKD(fn)` macros; INIT.OS functionality is not needed as a standalone C function.

---

## Design Decisions

1. **All 18 shape arrays as `static const uint8_t[36]`** — private to fort6.c, accessed only via the pointer table.
2. **`chopper_shapes` as `const uint8_t * const [18]`** — matches exact type in fort3.c's extern declaration.
3. **`laser_shapes` as `const uint8_t[32]`** — matches fort4.c's extern declaration.
4. **Display lists as non-const `uint8_t[]`** — require runtime patching of JMP self-reference bytes.
5. **`fort6_init()`** — patches `dsp_lst2[37:38]` and `dsp_lst3[30:31]` with the actual host address of each array.
6. **BOOT.STUFF**: not exported; just embedded in `shape_cr3_2` data.
7. **INIT.OS**: not converted; fort1.c handles vector setup directly.
8. **CART.START**: not converted; pure assembly startup, not portable.
