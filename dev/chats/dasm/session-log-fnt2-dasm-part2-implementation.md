# Session Log: convert fnt2.s to dasm — Part 2: Implementation

## File Produced

- `dev/dasm/fnt2.dasm` (811 lines, 1024 bytes)

---

## Implementation Approach

Inline Python via stdin heredoc (proven to avoid `/tmp` persistence issues
between Bash tool calls). The script reads `fnt2.s`, applies all conversion
rules, writes `dev/dasm/fnt2.dasm`, and asserts `byte_count == 1024`.

---

## Python Conversion Script

```python
import re, sys

src = ".../fnt2.s"
dst = ".../dev/dasm/fnt2.dasm"

with open(src) as f:
    lines = f.readlines()

out = []
byte_count = 0

# --- Header ---
out += [
    "    PROCESSOR 6502\n",
    "\n",
    "; ============================================================\n",
    "; fnt2.dasm -- Fort Apocalypse font 2 data (DASM format)\n",
    "; Converted from fnt2.s (SynAssembler format)\n",
    ";\n",
    "; Conversion rules applied:\n",
    ";   - 5-digit line-number prefix stripped\n",
    ";   - * comments converted to ; comments\n",
    ";   - .DA #%BITS -> DC.B %BITS\n",
    ";   - CHR $00-$1F (32 chars): 256 DC.B $00 bytes injected\n",
    ";   - CHR $73-$7F (13 chars): 104 DC.B $00 bytes injected\n",
    ";   - Label FNT2.S -> FNT2: (dot stripped, colon added)\n",
    "; Total bytes: 128 chars x 8 bytes = 1024\n",
    "; ============================================================\n",
    "\n",
]

LABEL_MAP = {"FNT2.S": "FNT2"}

def inject_blank_chars(start_chr, end_chr):
    global byte_count
    rows = []
    for c in range(start_chr, end_chr + 1):
        rows.append(
            f"    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR ${c:02X}\n"
        )
        byte_count += 8
    return rows

for raw in lines:
    raw = raw.rstrip('\n')

    # Strip 5-digit prefix + mandatory space (6 chars total)
    m = re.match(r'^\d{5} (.*)', raw)
    if m:
        rest = m.group(1)
    elif re.match(r'^\d{5}$', raw.strip()):
        rest = ''
    else:
        rest = raw

    if not rest.strip():
        continue   # skip blank lines

    # Label: no leading space, doesn't start with *
    if rest[0] not in (' ', '*'):
        label_raw = rest.strip()
        label_name = LABEL_MAP.get(label_raw, label_raw.replace('.', '_'))
        out.append(f"{label_name}:\n")
        continue

    # Comment: starts with *
    if rest.startswith('*'):
        comment = rest[1:].strip()
        out.append(f"; {comment}\n" if comment else ";\n")
        if 'CHR $00-$1F BLANK' in comment:
            out.extend(inject_blank_chars(0x00, 0x1F))
        elif 'CHR $73-$7F BLANK' in comment:
            out.extend(inject_blank_chars(0x73, 0x7F))
        continue

    # Data: .DA #%BITS
    m2 = re.match(r'^\s+\.DA\s+#%([01]{8})\s*$', rest)
    if m2:
        out.append(f"    DC.B    %{m2.group(1)}\n")
        byte_count += 1
        continue

    # Fallback: pass through as comment
    out.append(f"; {rest.strip()}\n")

assert byte_count == 1024, f"Expected 1024 bytes, got {byte_count}"

with open(dst, 'w') as f:
    f.writelines(out)

print(f"OK: fnt2.dasm written — {byte_count} bytes, {len(out)} output lines")
```

### Script execution

```
mkdir -p dev/dasm && python3 - <<'PYEOF'
... (script body) ...
PYEOF
```

### Script output

```
OK: fnt2.dasm written — 1024 bytes, 811 output lines
```

Byte assertion passed. No exceptions.

---

## Output Verification

### Line count and file ending

```
$ wc -l dev/dasm/fnt2.dasm
811 dev/dasm/fnt2.dasm

$ tail -3 dev/dasm/fnt2.dasm
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $7D
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $7E
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $7F
```

File ends at CHR $7F as expected.

---

## Spot Checks

### Header region (lines 1–17)

```dasm
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
```

Label correctly emitted at line 17 as `FNT2:`.

### CHR $00–$1F blank injection (lines 18–50)

```dasm
; CHR $00-$1F BLANK
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $00
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $01
    ...
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $1E
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $1F
```

32 lines injected (lines 19–50), one per character, 8 bytes each = 256 bytes.

### CHR $20 transition (lines 51–59)

Source line `00030 * CHR $20` converted to:

```dasm
; CHR $20
    DC.B    %00000000
    DC.B    %00100100
    DC.B    %01101110
    DC.B    %01111110
    DC.B    %01001000
    DC.B    %00111110
    DC.B    %00101100
    DC.B    %00000000
```

8 binary bytes matching source `.DA` lines exactly.

### CHR $72 (last active character, lines 789–797)

Source:
```
07410 * CHR $72
07420  .DA #%00000000
07430  .DA #%00000000
07440  .DA #%10100000
07450  .DA #%11101010
07460  .DA #%11101010
07470  .DA #%10100000
07480  .DA #%00000000
07490  .DA #%00000000
```

Output:
```dasm
; CHR $72
    DC.B    %00000000
    DC.B    %00000000
    DC.B    %10100000
    DC.B    %11101010
    DC.B    %11101010
    DC.B    %10100000
    DC.B    %00000000
    DC.B    %00000000
```

Bits preserved exactly.

### CHR $73–$7F blank injection (lines 798–811)

```dasm
; CHR $73-$7F BLANK
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $73
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $74
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $75
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $76
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $77
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $78
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $79
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $7A
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $7B
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $7C
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $7D
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $7E
    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR $7F
```

13 lines (CHR $73–$7F), 104 bytes.

---

## Byte Accounting

| Segment | Characters | Bytes |
|---|---|---|
| CHR $00–$1F injected zeros | 32 | 256 |
| CHR $20–$72 explicit `.DA` | 83 | 664 |
| CHR $73–$7F injected zeros | 13 | 104 |
| **Total** | **128** | **1024** |

Python assertion: `assert byte_count == 1024` → **passed**.

---

## Comparison with fnt1.dasm Conversion

| Property | fnt1.dasm | fnt2.dasm |
|---|---|---|
| Source lines | ~878 | 752 |
| Output lines | 876 | 811 |
| Total bytes | 1024 | 1024 |
| Named labels | 7 (`FNT1`, `POS_MASK1`, `FORT_EX1`–4, `T_5`, `EXP_SHAPE`) | 1 (`FNT2`) |
| `.AT` directives | 1 (`T.5` → raw ASCII DC.B sequence) | None |
| Commented-out `.DA` rows activated | Yes (CHR $01, 7 rows) | None |
| Blank ranges | 1 (`$5B–$7F`, 37 chars) | 2 (`$00–$1F`, `$73–$7F`) |
| Script complexity | Higher (label map, `.AT` handler, comment-flag state) | Lower (no `.AT`, no `;.DA`) |

---

## File Summary (Output)

| Property | Value |
|---|---|
| Output file | `dev/dasm/fnt2.dasm` |
| Lines | 811 |
| Byte count | 1024 |
| Header lines | 16 (PROCESSOR + comment block) |
| Label | `FNT2:` (line 17) |
| CHR $00–$1F block | Lines 18–50 (1 comment + 32 data lines) |
| CHR $20–$72 block | Lines 51–797 (83 char comments + 664 data lines) |
| CHR $73–$7F block | Lines 798–811 (1 comment + 13 data lines) |
