# Session Log: convert fnt1.s to dasm — Part 1: Analysis

## Task

Convert `fnt1.s` (Fort Apocalypse character set 1) from SynAssembler format to DASM
format. Output file: `fnt1.dasm` in the project root alongside `fnt1.s`.

The file is a pure data file — 128 characters × 8 bytes = 1024 bytes total, no 6502
instructions. The conversion is format-only: strip line number prefixes, translate
directives, add colons to labels, rename dot-labels that conflict with DASM syntax.

---

## SynAssembler Format (fnt1.s)

### Line structure

Every line begins with a 5-digit line number followed by one space, then content:

```
00010 * FILE FNT1.S
00020 FNT1
00030 * CHR $00 BLANK
...
00070  .DA #%11110000
```

- `*` in column 0 (after stripping the 5-digit prefix) = full comment line
- Leading space + `.DA #%XXXXXXXX` = active byte definition
- Leading `;.DA #%XXXXXXXX` = commented-out byte (inactive data)
- Bare word on its own line (no leading space, no `*`) = label

### Directives

| SynAssembler | Meaning |
|---|---|
| `.DA #%XXXXXXXX` | Define one byte from binary literal |
| `.AT -/TEXT/` | Store raw ASCII bytes of TEXT (the `-` flag bypasses screen-code conversion) |

---

## DASM Format Target

| Element | DASM |
|---|---|
| Comment | `; text` |
| Data byte | `DC.B %XXXXXXXX` or `DC.B $XX` |
| Label | `LABEL:` (colon required) |
| Local labels | Begin with `.` — so imported dot-labels must be renamed |
| Assembler hint | `PROCESSOR 6502` at top |

---

## Character Range Breakdown

Read fnt1.s in full across multiple passes. Findings:

| Range | Count | Bytes | Notes |
|---|---|---|---|
| $00 | 1 | 8 | Blank — comment only in source, no `.DA` bytes |
| $01 | 1 | 8 | Partial — 7 `;.DA #%00000000` commented out + 1 active `.DA #%11110000` |
| $02–$0A | 9 | 72 | Active `.DA` bytes, various 1BPP special tiles |
| $0B | 1 | 8 | `POS.MASK1` — scanner XOR bitmask |
| $0C | 1 | 8 | `FORT.EX1` — explosion frame 1 |
| $0D | 1 | 8 | `FORT.EX2` — explosion frame 2 |
| $0E | 1 | 8 | `FORT.EX3` — explosion frame 3 |
| $0F | 1 | 8 | `FORT.EX4` — explosion frame 4 |
| $10–$19 | 10 | 80 | Digits 0–9 (2BPP) |
| $1A–$1F | 6 | 48 | Misc symbols |
| $20 | 1 | 8 | `T.5` — keyboard labels via `.AT -/1!9)8(2"/` |
| $21–$3B | 27 | 216 | Punctuation and tile characters (2BPP) |
| $3C | 1 | 8 | `EXP.SHAPE` — explosion shape mask |
| $3D–$5A | 30 | 240 | Misc tiles, A–Z letters (2BPP) |
| $5B–$7F | 37 | 296 | Blank — comment only in source, no `.DA` bytes |

**Total:** 128 × 8 = **1024 bytes**

---

## Special Cases

### CHR $00 — Implicit Blank

In fnt1.s there is only a comment line:

```
00030 * CHR $00 BLANK
```

No `.DA` directives follow. The SynAssembler assembles nothing — the 8 bytes are zero
by default because the character set data continues immediately at CHR $01.

In DASM this implicit zeroing does not exist. Eight explicit `DC.B $00` bytes must be
injected after the comment to produce the correct binary output.

### CHR $01 — Partially Commented-Out

The source has 7 inactive rows followed by 1 active row:

```
00040 ;.DA #%00000000
00050 ;.DA #%00000000
00060 ;.DA #%00000000
00070 ;.DA #%00000000
00080 ;.DA #%00000000
00090 ;.DA #%00000000
00100 ;.DA #%00000000
00110  .DA #%11110000
```

In SynAssembler a leading `;` makes the line inactive — the 7 zeros are not assembled,
leaving only the `0xF0` byte. But `fnt1.c` (already converted from fnt1.s in a prior
session) has `{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF0}` for CHR $01 — 8 bytes total.

**Resolution:** The 7 commented-out zeros must be emitted as active `DC.B %00000000`
bytes in the DASM output so the assembled binary matches the known-correct `fnt1.c`.
This is an intentional uncomment, not an error. A comment is added to each such line:
`; was commented-out in source`.

### CHR $5B–$7F — Trailing Blank Block

The source ends at CHR $5A and then has one comment line:

```
00817 * CHR $5B-$7F BLANK
```

No `.DA` data follows for any of the 37 blank characters. The same implicit-zero issue
as CHR $00 applies, but at much larger scale: 37 × 8 = **296 bytes** must be injected.
Each is written as a separate `DC.B $00,...,$00` row with a `; CHR $XX` comment for
readability.

---

## Named Labels

Seven labels are embedded in the data. In SynAssembler a label is a bare word on its
own line with no leading space and no colon:

```
00098 POS.MASK1
00108 FORT.EX1
```

In DASM labels require a trailing colon. Additionally, DASM reserves `.` as the local
label prefix — a label beginning with `.` is scoped to the surrounding global label.
The dots in these labels are therefore illegal as global identifiers and must be
replaced with underscores.

| SynAssembler | DASM | CHR | Role |
|---|---|---|---|
| `FNT1` | `FNT1:` | — | Array start |
| `POS.MASK1` | `POS_MASK1:` | $0B | Scanner XOR bitmask |
| `FORT.EX1` | `FORT_EX1:` | $0C | Explosion frame 1 |
| `FORT.EX2` | `FORT_EX2:` | $0D | Explosion frame 2 |
| `FORT.EX3` | `FORT_EX3:` | $0E | Explosion frame 3 |
| `FORT.EX4` | `FORT_EX4:` | $0F | Explosion frame 4 |
| `T.5` | `T_5:` | $20 | Keyboard labels |
| `EXP.SHAPE` | `EXP_SHAPE:` | $3C | Explosion shape mask |

---

## `.AT -/1!9)8(2"/` — Raw ASCII Expansion

At CHR $20 (the `T_5` label) the source uses an `.AT` directive instead of `.DA` bytes:

```
00291 T.5
00292  .AT -/1!9)8(2"/
```

SynAssembler `.AT` stores Atari internal screen codes (ASCII − 0x20 for printable chars).
The `-` flag applies an additional − 0x20, which cancels out: the net result is that raw
ASCII values of the characters in the string are stored verbatim.

Expansion:

| Char | ASCII |
|---|---|
| `1` | $31 |
| `!` | $21 |
| `9` | $39 |
| `)` | $29 |
| `8` | $38 |
| `(` | $28 |
| `2` | $32 |
| `"` | $22 |

These are the key labels for the Atari numeric keys: `{1,!}`, `{9,)}`, `{8,(}`, `{2,"}`.

In DASM this becomes:

```
    DC.B    $31,$21,$39,$29,$38,$28,$32,$22  ; .AT -/1!9)8(2"/ (raw ASCII)
```

---

## Conversion Rule Summary

| Source pattern | DASM output |
|---|---|
| `00010 ` prefix | Strip (remove first 6 chars) |
| `* comment text` | `; comment text` |
| ` .DA #%XXXXXXXX` | `    DC.B    %XXXXXXXX` |
| `;.DA #%XXXXXXXX` (CHR $01 only) | `    DC.B    %XXXXXXXX  ; was commented-out in source` |
| `;.DA #%XXXXXXXX` (elsewhere) | `; DC.B    %XXXXXXXX` (kept as comment) |
| `.AT -/TEXT/` | `    DC.B    $XX,$XX,...` (raw ASCII) |
| `LABELNAME` (bare, col 0) | `DASM_LABEL:` (colon, dots→underscores) |
| `* CHR $00 BLANK` | Emit comment + inject 8 × `DC.B $00` |
| `* CHR $5B-$7F BLANK` | Emit comment + inject 37 × 8 × `DC.B $00` |

---

## File Summary (Input)

| Property | Value |
|---|---|
| Source file | `fnt1.s` |
| Format | SynAssembler (Atari 400/800) |
| Lines | 818 |
| Characters | 128 |
| Bytes assembled | 1024 |
| Named labels | 8 (FNT1 + 7 data labels) |
| Blank ranges | CHR $00 (implicit), CHR $5B–$7F (implicit) |
| Special directives | `.AT -/1!9)8(2"/` at CHR $20 |
