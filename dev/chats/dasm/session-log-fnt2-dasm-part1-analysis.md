# Session Log: convert fnt2.s to dasm — Part 1: Analysis

## Task

Convert `fnt2.s` (Fort Apocalypse font 2 data, SynAssembler format) to DASM
assembler format. Output file: `dev/dasm/fnt2.dasm`.

The `dev/dasm/` folder did not exist and needed to be created.

---

## Source File Overview

| Property | Value |
|---|---|
| Source file | `fnt2.s` (project root) |
| Lines | 752 |
| Format | SynAssembler (5-digit line-number prefix) |
| Content | Font bitmap data — 128 characters × 8 bytes = 1024 bytes |
| Labels | 1 (`FNT2.S` at line 2) |
| Blank char ranges | 2 (CHR $00–$1F, CHR $73–$7F) |
| Active char ranges | 1 (CHR $20–$72, 83 characters) |
| `.AT` directives | None |
| Commented-out `.DA` rows | None |

---

## SynAssembler Format

Each source line has the form:

```
NNNNN<space>TEXT
```

where `NNNNN` is a 5-digit decimal line number and `TEXT` is the content:

| TEXT form | Meaning |
|---|---|
| `FNT2.S` (no leading space) | Label (starts in column 7 of the file) |
| `* comment text` | Full-line comment |
| ` .DA #%XXXXXXXX` | 1-byte data (leading space = no label on this line) |
| (blank after prefix) | Empty line |

---

## Line-by-Line Structure of fnt2.s

### Line 1 (raw): blank

No content after prefix strip. Skip in output.

### Line 2: `00010 FNT2.S`

After stripping the 6-char prefix (`NNNNN<sp>`):

```
FNT2.S
```

No leading space → this is a **label**. The `.S` suffix is a SynAssembler file
extension artifact. Mapped to `FNT2` and emitted as `FNT2:` in DASM.

### Line 3: `00020 * CHR $00-$1F BLANK`

After prefix strip:

```
* CHR $00-$1F BLANK
```

Starts with `*` → comment. The phrase `CHR $00-$1F BLANK` signals that
characters $00 through $1F (32 characters) have no bitmap data in the source.
The DASM conversion must **inject** 32 × 8 = **256 zero bytes**.

### Lines 4–750: Character data CHR $20–$72

Pattern for each character:

```
NNNNN * CHR $XX          ← comment identifying the character
NNNNN  .DA #%XXXXXXXX   ← byte 0 (MSB = topmost row)
NNNNN  .DA #%XXXXXXXX   ← byte 1
...
NNNNN  .DA #%XXXXXXXX   ← byte 7 (bottommost row)
```

Total: 83 characters ($20–$72 inclusive) × 8 bytes = **664 explicit bytes**.

Each `.DA` line has the form ` .DA #%XXXXXXXX` (1 leading space, `.DA`,
space, `#%`, 8 binary digits).

### Line 751: `07500 * CHR $73-$7F BLANK`

Comment signalling that characters $73 through $7F (13 characters) have no
bitmap data. The DASM conversion injects 13 × 8 = **104 zero bytes**.

### Line 752: blank

End of file. Skip.

---

## Byte Count Verification

| Range | Chars | Bytes |
|---|---|---|
| CHR $00–$1F (blank, injected) | 32 | 256 |
| CHR $20–$72 (explicit `.DA`) | 83 | 664 |
| CHR $73–$7F (blank, injected) | 13 | 104 |
| **Total** | **128** | **1024** |

---

## DASM Conversion Rules

### 1. Header

Emit a `PROCESSOR 6502` directive and a 14-line comment block describing the
conversion before any source content.

### 2. Line-number prefix stripping

Strip exactly 6 characters (5-digit number + 1 mandatory space) from the
start of every non-blank line.

The stripped remainder falls into one of these categories:
- Starts with non-space, non-`*`: **label**
- Starts with `*`: **comment**
- Starts with space followed by `.DA`: **data byte**
- Empty: **skip**

### 3. Label conversion

`FNT2.S` is the only label. It is mapped explicitly to `FNT2` (dropping the
`.S` extension artifact). Emitted as `FNT2:` with a colon.

General rule for unlisted labels: replace `.` with `_`, append `:`.

### 4. Comment conversion

`* TEXT` → `; TEXT`

### 5. Blank range injection

When a comment contains the substring `CHR $XX-$YY BLANK`:

- Emit the comment line (`; CHR $XX-$YY BLANK`)
- For each character code from $XX to $YY inclusive, emit:
  ```
      DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $XX
  ```
- Accumulate 8 bytes per injected line in the byte counter.

### 6. Data byte conversion

` .DA #%XXXXXXXX` → `    DC.B    %XXXXXXXX`

Accumulate 1 byte per converted line.

### 7. No other directives

`fnt2.s` contains no `.AT`, `.BS`, `.OR`, `.EQ`, `.IN`, or commented-out
`;.DA` lines. No special handling is needed for these.

---

## Design Decisions

| Decision | Rationale |
|---|---|
| Map `FNT2.S` → `FNT2` | `.S` is a SynAssembler file extension in the source comment; the meaningful label name is `FNT2`. Using `FNT2_S` would be misleading. |
| One `DC.B` line per blank character | Matches fnt1.dasm style; individual character comments aid visual debugging of the bitmap. |
| Inline Python via stdin heredoc | Avoids /tmp file persistence issue between Bash tool calls; proven pattern from fnt1.s conversion. |
| `dev/dasm/` as output folder | As specified by the task. Created with `mkdir -p`. |
| No `FNT2.S` as a DASM local label | DASM `.` prefix is reserved for local labels; the source `FNT2.S` at the top of the file is a module label, not a local reference. |

---

## Expected Output Structure

```
    PROCESSOR 6502

; ============================================================
; fnt2.dasm -- Fort Apocalypse font 2 data (DASM format)
; Converted from fnt2.s (SynAssembler format)
;
; Conversion rules applied:
;   - 5-digit line-number prefix stripped
;   - * comments converted to ; comments
;   - .DA #%BITS -> DC.B %BITS
;   - CHR $00-$1F (32 chars): 256 DC.B $00 bytes injected
;   - CHR $73-$7F (13 chars): 104 DC.B $00 bytes injected
;   - Label FNT2.S -> FNT2: (dot stripped, colon added)
; Total bytes: 128 chars x 8 bytes = 1024
; ============================================================

FNT2:
; CHR $00-$1F BLANK
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $00
    ...
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $1F
; CHR $20
    DC.B    %00000000
    DC.B    %00100100
    ...
; CHR $72
    DC.B    %00000000
    DC.B    %00000000
    ...
; CHR $73-$7F BLANK
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $73
    ...
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $7F
```

---

## File Summary (Input)

| Property | Value |
|---|---|
| Source file | `fnt2.s` |
| Total lines | 752 |
| Blank lines | 2 (first and last) |
| Label lines | 1 |
| Comment lines | 86 (1 range comment + 83 char comments + 1 end range comment + 1 file name) |
| Data lines | 664 (83 chars × 8 bytes) |
| Blank range injections | 2 (256 + 104 bytes) |
