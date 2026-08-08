# Session Log: convert fort.s to dasm — Part 1: Analysis

## Task

Convert `fort.s` (Fort Apocalypse master/equates file, SynAssembler format)
to DASM assembler format. Output file: `dev/dasm/fort.dasm`.

---

## Source File Overview

| Property | Value |
|---|---|
| Source file | `fort.s` (project root) |
| Lines | 280 |
| Format | SynAssembler (5-digit line-number prefix) |
| Content | Master file: equates, constants, zero-page layout, includes, cart vector |
| Data bytes emitted | 5 (END.CART section only) |

`fort.s` is not a data or code file — it is the **master/linker file** that
defines all hardware equates, memory constants, zero-page variable layout, and
assembles the entire ROM by including the other source files.

---

## SynAssembler Format

Each source line: `NNNNN<space>TEXT`

| TEXT form | Meaning |
|---|---|
| `*` or `* text` | Comment |
| `LABEL    .DIRECTIVE operand` | Label + directive |
| `         .DIRECTIVE operand` | Directive without label |
| `LABEL` alone | Label (next line has the directive) |
| (empty after prefix) | Blank line |

---

## Directives Present in fort.s

| SynAssembler | Count | Meaning |
|---|---|---|
| `.EQ value` | ~85 | Define symbol (equate/constant) |
| `.OR addr` | 7 | Set origin (program counter) |
| `.BS n` | ~40 | Allocate n bytes of block storage |
| `.IN "H1:FILE.S"` | 10 | Include source file |
| `.DA label` | 2 | Emit 16-bit address word |
| `.DA #%bits` | 1 | Emit 8-bit immediate byte |
| `.HS hh` | 1 | Emit raw hex bytes |
| `.LI OFF` | 1 | Listing control (no assembler output) |
| `.TF "H1:FORT.OBJ"` | 1 | Set target/output filename |

---

## DASM Conversion Mapping

| SynAssembler | DASM | Notes |
|---|---|---|
| `LABEL .EQ value` | `LABEL EQU value` | No colon on EQU symbols |
| `.OR addr` | `ORG addr` | |
| `LABEL .BS n` | `LABEL: ds.b n` | Colon on address labels |
| `.IN "H1:FILE.S"` | `include "file.dasm"` | Lowercase, `.dasm` extension |
| `.DA LABEL` | `dc.w LABEL` | 16-bit address reference |
| `.DA #%BITS` | `dc.b %BITS` | 8-bit immediate |
| `.HS HH` | `dc.b $HH` | Hex pairs → `$XX` each |
| `.LI OFF` | `; .LI OFF` | No DASM equivalent, comment out |
| `.TF "..."` | `; .TF "..."` | No DASM equivalent, comment out |
| `* comment` | `; comment` | |

---

## Dot-in-Identifier Conversion

SynAssembler allows dots in label names. DASM reserves `.` for local labels.
All dots between alphanumeric characters are replaced with underscores.

Examples:
| SynAssembler | DASM |
|---|---|
| `VVBLKI.RET` | `VVBLKI_RET` |
| `VVBLKD.RET` | `VVBLKD_RET` |
| `CHECK.SUM` | `CHECK_SUM` |
| `CHR.SET1` | `CHR_SET1` |
| `CHR.SET2` | `CHR_SET2` |
| `PLAY.SCRN` | `PLAY_SCRN` |
| `POD.1` | `POD_1` |
| `POD.2` | `POD_2` |
| `S.LINE1` | `S_LINE1` |
| `MISS.CHR.RIGHT` | `MISS_CHR_RIGHT` |
| `TANK.START.X` | `TANK_START_X` |
| `TIM1.VAL` | `TIM1_VAL` |
| `S1.1.VAL` | `S1_1_VAL` |
| `TEMP.MODE` | `TEMP_MODE` |
| `ADR1.I` | `ADR1_I` |
| `S.ADR` | `S_ADR` |
| `GAME.POINTS` | `GAME_POINTS` |
| `DEMO.STATUS` | `DEMO_STATUS` |
| `POD.STATUS` | `POD_STATUS` |
| `SLAVE.STATUS` | `SLAVE_STATUS` |
| `END.CART` | `END_CART` |
| `CART.START` | `CART_START` |
| `PACKED.MAP` | `PACKED_MAP` |
| `MAX.PODS` | `MAX_PODS` |
| `MAX.LEFT` | `MAX_LEFT` |
| `EXP.WALL` | `EXP_WALL` |
| `RAM1.STUFF` | `RAM1_STUFF` |

The dot-replacement is applied everywhere in the line where an identifier dot
appears: label positions, EQU values, ORG arguments, ds.b operands, dc.w
operands, and include paths (except string literals handled separately).

Multi-dot identifiers like `S1.1.VAL` → `S1_1_VAL` require two passes because
`re.sub` does not overlap matches. The script loops until no further changes.

---

## Tricky Cases

### Trailing annotations on .EQ lines

The "CHANGE THESE CONSTANTS" section embeds size and access-mode annotations
as unquoted trailing text on `.EQ` lines:

```
PLAYER       .EQ $0       $800  R
PLAY.SCRN    .EQ $300     $300  R
CHR.SET1     .EQ $800     $400  R
```

In SynAssembler, only the first token after `.EQ` is the operand value;
everything else is ignored. In DASM, trailing tokens would cause an error.
The script takes only the first token as the EQU value and moves the rest to
a `; ` comment:

```
PLAYER               EQU $0  ; $800  R
PLAY_SCRN            EQU $300  ; $300  R
CHR_SET1             EQU $800  ; $400  R
```

Similarly for `.BS` timer lines with trailing descriptions:
```
TIM1.VAL .BS 1   LASER 1
TIM2.VAL .BS 1   LASER 2
```
→
```
TIM1_VAL:    ds.b 1  ; LASER 1
TIM2_VAL:    ds.b 1  ; LASER 2
```

### Labels on their own line before .BS

Two labels (`TANK.START.X`, `TANK.START.Y`) appear on their own lines with the
`.BS` directive on the next line:

```
TANK.START.X
         .BS MAX.TANKS
TANK.START.Y
         .BS MAX.TANKS
```

The script detects a label with no directive following it on the same line and
emits just the label with colon. The subsequent `.BS` line (which starts with a
space) is handled as a no-label `ds.b` line:

```
TANK_START_X:
    ds.b MAX_TANKS
TANK_START_Y:
    ds.b MAX_TANKS
```

### .IN filename conversion

SynAssembler device-qualified include paths use `"H1:FILE.S"` notation.
The DASM include form uses plain filenames. Conversion:
- Strip device prefix (`H1:`)
- Lowercase the filename
- Replace `.s` extension with `.dasm`

Examples:
```
.IN "H1:FORT7.S"  →  include "fort7.dasm"
.IN "H1:FNT1.S"   →  include "fnt1.dasm"
.IN "H1:FORT1.S"  →  include "fort1.dasm"
```

String literals are handled before the general dot-replacement pass so that
`FORT7.S` in a string is not corrupted by the `7.S` → `7_S` substitution.

### END.CART vector data

```
END.CART
         .BS $BFFA-*
         .DA CART.START
         .HS 00
         .DA #%10000100
         .DA CART.START
```

Conversions applied:
- `END.CART` → `END_CART:` (address label with colon)
- `.BS $BFFA-*` → `ds.b $BFFA-*` (DASM supports `*` as current PC)
- `.DA CART.START` → `dc.w CART_START` (16-bit address word)
- `.HS 00` → `dc.b $00` (hex byte pair)
- `.DA #%10000100` → `dc.b %10000100` (8-bit binary immediate)

`CART.START` is defined in fort1.s (not in fort.s itself); it resolves at
assemble time via the `include "fort1.dasm"` directive.

---

## Section Structure of fort.s

| Lines | Content | Conversion |
|---|---|---|
| 2–14 | Banner comment box | `; ` comments |
| 15 | `.LI OFF` | `; .LI OFF` |
| 16–111 | Hardware register equates (OS shadows, GTIA, ANTIC, POKEY) | `EQU` |
| 112–124 | Game constants (MIS, PL0–3, RIGHT/LEFT/DOWN/UP, CHECK.SUM) | `EQU` |
| 125–202 | Memory layout constants (with trailing size annotations) | `EQU` + `; ` comments |
| 203–218 | Zero-page allocation group 1 (ADR1–SSIZEM at $15) | `ORG $15` + `ds.b` |
| 219–229 | Zero-page allocation group 2 (sound/score at $43) | `ORG $43` + `ds.b` |
| 230–246 | Pod and slave arrays (POD.1, POD.2, SLAVES) | `EQU` + `ORG` + `ds.b` |
| 247–248 | FORT7.S include at $50 | `ORG $50` + `include` |
| 249–269 | ROM include files (FNT1, FNT2, FORT1–FORT8) | `ORG PROGRAM` + `include` |
| 270–277 | END.CART cartridge vector | `END_CART:` + `ds.b`/`dc.w`/`dc.b` |
| 278–280 | EOF comment | `; EOF` |

---

## File Summary (Input)

| Property | Value |
|---|---|
| Source file | `fort.s` |
| Total lines | 280 |
| Comment lines | ~30 |
| `.EQ` equates | ~85 |
| `.OR` origins | 7 |
| `.BS` allocations | ~40 |
| `.IN` includes | 10 |
| Data directives | 5 (`.DA` ×4, `.HS` ×1) |
| Special directives | 2 (`.LI`, `.TF`) |
