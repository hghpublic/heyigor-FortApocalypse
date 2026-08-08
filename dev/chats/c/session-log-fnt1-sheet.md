# Session Log: create a bitmap with all characters from fnt1.c

## Task
Render all 128 characters from `dev/src/fnt1.c` into BMP files for visual
inspection of the full font. Output: `dev/src/fnt1_sheet_1bpp.bmp` and
`dev/src/fnt1_sheet_2bpp.bmp`.

---

## Tool Inventory

Before writing any code, the available bitmap infrastructure was surveyed:

```
dev/src/fnt1_bitmap.h/c   — fnt1-specific: fnt1_decode_char, fnt1_create_bitmap,
                             fnt1_write_bmp; NO sheet function
dev/src/font_bitmap.h/c   — generic: font_decode_char, font_create_bitmap,
                             font_create_sheet, font_write_bmp;
                             takes const uint8_t *font_data as first argument
dev/src/fnt1.h            — extern fnt1_data[128 * 8] + 7 named pointers
```

**Key finding:** `font_bitmap.h` already provides `font_create_sheet()` which renders
all n_chars characters into a grid of a given column count. `fnt1_bitmap.h` has no
equivalent. Using `font_create_sheet(fnt1_data, 128, 16, mode, &w, &h)` covers the
entire task in a single call — no custom renderer needed.

---

## Design: Two Sheets, Not One

fnt1's character ranges use **two distinct display modes** on Atari hardware:

| Range | Mode | Content |
|---|---|---|
| $00–$0F | 1BPP | Special tiles, explosion frames, POS_MASK1, FORT_EX1–4 |
| $10–$19 | 2BPP | Digits 0–9 |
| $20 | — | T.5 raw ASCII keyboard labels |
| $21–$3A | 2BPP | Punctuation and tile characters |
| $3C | 1BPP | EXP_SHAPE mask |
| $41–$5A | 2BPP | A–Z uppercase letters |
| $5B–$7F | — | Blank |

A single-mode sheet would misrepresent half the font:
- 1BPP on a 2BPP character shows 4 smeared doubled bits, not the intended glyph
- 2BPP on a 1BPP character shows 4 doubled 2-bit pairs, distorting the shape

**Decision:** produce both a 1BPP sheet (correct for specials/masks) and a 2BPP
sheet (correct for digits/letters), 128×64 each. The user can open both.

---

## Grid Layout

`font_create_sheet(fnt1_data, 128, 16, mode, &w, &h)`:
- 128 characters total, 16 columns → 8 rows
- Each cell is 8×8 pixels → output is **128×64 pixels**
- Character index → grid position: `col = chr % 16`, `row = chr / 16`

Grid rows correspond to character ranges:
```
Row 0 ($00-$0F)  — blank + specials + POS_MASK1 + FORT_EX1-4
Row 1 ($10-$1F)  — digits 0-9 + misc
Row 2 ($20-$2F)  — T.5 + 2BPP punctuation
Row 3 ($30-$3F)  — 2BPP tiles + EXP_SHAPE
Row 4 ($40-$4F)  — heart + 2BPP punctuation
Row 5 ($50-$5F)  — A-Z start ($41-$4F in this row)
Row 6 ($60-$6F)  — A-Z remainder
Row 7 ($70-$7F)  — end of A-Z + blank
```

---

## Program

Written to `/tmp/fnt1_sheet.c` (not added to the source tree — disposable tool):

```c
#include <stdio.h>
#include <stdlib.h>
#include "fnt1.h"
#include "font_bitmap.h"

int main(void)
{
    const char *out1 = "fnt1_sheet_1bpp.bmp";
    const char *out2 = "fnt1_sheet_2bpp.bmp";
    int w, h, rc;

    /* 1BPP sheet: raw bit patterns — correct for $00-$0F specials and masks */
    uint8_t *s1 = font_create_sheet(fnt1_data, 128, 16, FONT_1BPP, &w, &h);
    if (!s1) { fputs("alloc failed\n", stderr); return 1; }
    rc = font_write_bmp(out1, s1, w, h, NULL);
    printf("1BPP %dx%d → %s: %s\n", w, h, out1, rc == 0 ? "OK" : "FAILED");
    free(s1);

    /* 2BPP sheet: hardware-stretched view — correct for digits and A-Z */
    static const uint32_t pal2[4] = { 0x000000, 0x555555, 0xAAAAAA, 0xFFFFFF };
    uint8_t *s2 = font_create_sheet(fnt1_data, 128, 16, FONT_2BPP, &w, &h);
    if (!s2) { fputs("alloc failed\n", stderr); return 1; }
    rc = font_write_bmp(out2, s2, w, h, pal2);
    printf("2BPP %dx%d → %s: %s\n", w, h, out2, rc == 0 ? "OK" : "FAILED");
    free(s2);

    return 0;
}
```

**Palette choices:**
- 1BPP (`NULL` → default): `{0xFFFFFF, 0x000000, -, -}` — white background, black pixels.
  Crisp and readable for single-bit masks and explosion shapes.
- 2BPP (`pal2`): `{0x000000, 0x555555, 0xAAAAAA, 0xFFFFFF}` — 4-step greyscale ramp.
  Maps colour values 0–3 to distinct visible shades, making multi-level tile detail legible.
  Default palette has white=0/black=1 which leaves colours 2 and 3 as grey tones; for 2BPP
  the full range matters so an explicit 4-step ramp was preferred.

---

## Build and Run

```
cd dev/src
gcc -Wall -Wextra -std=c11 \
    -o /tmp/fnt1_sheet /tmp/fnt1_sheet.c fnt1.c font_bitmap.c -I. \
    && /tmp/fnt1_sheet
```

**Why `font_bitmap.c` not `fnt1_bitmap.c`:** `font_create_sheet` is defined in
`font_bitmap.c`. `fnt1_bitmap.c` provides the older `fnt1_create_bitmap` (strip renderer)
and `fnt1_write_bmp`, but no sheet function. Linking only `font_bitmap.c` is sufficient.

### Output
```
1BPP 128x64 → fnt1_sheet_1bpp.bmp: OK
2BPP 128x64 → fnt1_sheet_2bpp.bmp: OK
```

Zero warnings, zero errors.

---

## Output Files

Both written to `dev/src/` (same directory as the source, alongside the existing
`fnt2_demo.bmp`, `fnt2_sheet_*.bmp` files):

```
dev/src/fnt1_sheet_1bpp.bmp   128×64 px, 24-bit BMP, ~24 kB
dev/src/fnt1_sheet_2bpp.bmp   128×64 px, 24-bit BMP, ~24 kB
```

**Reading the sheets:** Character at index `N` is in column `N % 16`, row `N / 16`.
To find a known character: e.g. `FORT.EX3` = $0E → col 14, row 0 (top row, 15th cell).
`A` = $41 → col 1, row 4. Digit `0` = $10 → col 0, row 1.

---

## Design Decisions

| Decision | Rationale |
|---|---|
| Use `font_create_sheet` from `font_bitmap.h` | Already exists and does exactly this; no new code needed |
| Two output files (1BPP + 2BPP) | fnt1 has genuinely mixed-mode characters; one sheet can't represent both correctly |
| 16 columns × 8 rows | 128 / 16 = 8 rows, output 128×64 — compact, matches standard character-set grid convention |
| 4-step greyscale palette for 2BPP | Makes all four colour values distinct; the default black/white palette renders colours 2+3 as arbitrary grey, making 2BPP tile detail hard to read |
| Write to `dev/src/` (not `/tmp/`) | Persistent output alongside existing BMP files; user can open directly from the project folder |
| Program written to `/tmp/fnt1_sheet.c` | Disposable render tool; not worth adding to the source tree permanently |

---

## File Summary

| File | Size | Content |
|---|---|---|
| `dev/src/fnt1_sheet_1bpp.bmp` | 24 kB | 128×64, all 128 chars decoded 1BPP (white bg / black fg) |
| `dev/src/fnt1_sheet_2bpp.bmp` | 24 kB | 128×64, all 128 chars decoded 2BPP (4-step greyscale) |
