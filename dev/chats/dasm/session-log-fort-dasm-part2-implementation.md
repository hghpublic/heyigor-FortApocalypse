# Session Log: convert fort.s to dasm — Part 2: Implementation

## File Produced

- `dev/dasm/fort.dasm` (300 lines)

---

## Implementation Approach

Inline Python via stdin heredoc (avoids `/tmp` persistence issues between
Bash tool calls). The script processes fort.s line by line, applying all
directive conversions and identifier dot-to-underscore substitutions.

---

## Python Script Structure

### fix_dots() helper

```python
def fix_dots(s):
    prev = None
    while prev != s:
        prev = s
        s = re.sub(r'([A-Za-z0-9])\.([A-Za-z0-9])', r'\1_\2', s)
    return s
```

Replaces dots between alphanumeric chars with underscores. Loops until stable
because multi-dot identifiers like `S1.1.VAL` require two passes:
- Pass 1: `S1.1.VAL` → `S1_1.VAL` (matches `1.1`)
- Pass 2: `S1_1.VAL` → `S1_1_VAL` (matches `1.V`)

### in_filename() helper

```python
def in_filename(raw):
    raw = re.sub(r'^[^:]+:', '', raw)   # strip "H1:" device prefix
    return raw.lower().replace('.s', '.dasm')
```

Converts `H1:FORT7.S` → `fort7.dasm`.

### Per-line processing

For each source line:
1. Strip 6-char prefix (`NNNNN<space>`) via `re.match(r'^\d{5} (.*)', raw)`
2. Skip blank lines (`out.append('\n')`)
3. Convert `*` comment lines to `; text`
4. Extract label if non-space start (not `*`)
5. Dispatch on directive:

| Directive | Action |
|---|---|
| `.LI` / `.TF` | Emit as `; text` comment |
| `.IN "..."` | Call `in_filename()`, emit `include "..."` |
| `.EQ value [trailing]` | Emit `LABEL EQU value  ; trailing` |
| `.OR addr` | Emit `LABEL:` (if label), then `    ORG addr` |
| `.BS n [trailing]` | Emit `LABEL:    ds.b n  ; trailing` |
| `.DA #%bits` | Emit `    dc.b %bits` |
| `.DA #$hh` | Emit `    dc.b $hh` |
| `.DA label` | Emit `    dc.w label` (16-bit word) |
| `.HS hexstr` | Emit `    dc.b $XX,$YY,...` |
| label alone | Emit `LABEL:` |
| fallback | Emit `    fix_dots(rest)` |

Key distinction: EQU symbols get **no colon** (`FRAME EQU $14`); address labels
used with `ds.b`, `dc.w`, or standalone get a **colon** (`ADR1:    ds.b 2`).

---

## Full Script

```python
import re, sys

src = ".../fort.s"
dst = ".../dev/dasm/fort.dasm"

with open(src) as f:
    lines = f.readlines()

out = []

out += [
    "    PROCESSOR 6502\n",
    "\n",
    "; ============================================================\n",
    "; fort.dasm -- Fort Apocalypse master equates and layout\n",
    "; Converted from fort.s (SynAssembler format)\n",
    ";\n",
    "; Conversion rules applied:\n",
    ";   - 5-digit line-number prefix stripped\n",
    ";   - * comments -> ; comments\n",
    ";   - .EQ  -> EQU\n",
    ";   - .OR  -> ORG\n",
    ";   - .BS  -> ds.b\n",
    ";   - .IN  -> include (filenames lowercased, .dasm extension)\n",
    ";   - .DA LABEL -> dc.w LABEL  (16-bit address)\n",
    ";   - .DA #%BITS -> dc.b %BITS (8-bit immediate)\n",
    ";   - .HS HH -> dc.b $HH\n",
    ";   - .LI, .TF -> commented out (no DASM equivalent)\n",
    ";   - Dots in identifiers replaced with underscores\n",
    ";   - Address labels get colon; EQU symbols do not\n",
    "; ============================================================\n",
    "\n",
]

def fix_dots(s):
    prev = None
    while prev != s:
        prev = s
        s = re.sub(r'([A-Za-z0-9])\.([A-Za-z0-9])', r'\1_\2', s)
    return s

def in_filename(raw):
    raw = re.sub(r'^[^:]+:', '', raw)
    return raw.lower().replace('.s', '.dasm')

for raw in lines:
    raw = raw.rstrip('\n')
    m = re.match(r'^\d{5} (.*)', raw)
    if m:
        rest = m.group(1)
    elif re.match(r'^\d{5}\s*$', raw):
        rest = ''
    else:
        rest = raw

    if not rest.strip():
        out.append('\n')
        continue

    if rest.startswith('*'):
        comment = rest[1:].strip()
        out.append(f'; {comment}\n' if comment else ';\n')
        continue

    label_dasm = None
    if rest[0] != ' ':
        m = re.match(r'^([A-Z][A-Z0-9.]*)\s*(.*)', rest)
        if m:
            label_dasm = fix_dots(m.group(1))
            rest = m.group(2)
        else:
            label_dasm = fix_dots(rest.strip())
            rest = ''

    rest = rest.lstrip()

    if label_dasm is not None and not rest:
        out.append(f'{label_dasm}:\n')
        continue

    if re.match(r'^\.LI\b', rest):
        out.append(f'; {rest.strip()}\n')
        continue
    if re.match(r'^\.TF\b', rest):
        out.append(f'; {rest.strip()}\n')
        continue

    m = re.match(r'^\.IN\s+"([^"]+)"', rest)
    if m:
        out.append(f'    include "{in_filename(m.group(1))}"\n')
        continue

    m = re.match(r'^\.EQ\s+(\S+)(.*)', rest)
    if m:
        value = fix_dots(m.group(1))
        trailing = m.group(2).strip()
        lbl = label_dasm if label_dasm else ''
        comment = f'  ; {trailing}' if trailing else ''
        out.append(f'{lbl:<20} EQU {value}{comment}\n')
        continue

    m = re.match(r'^\.OR\s+(\S+)', rest)
    if m:
        addr = fix_dots(m.group(1))
        if label_dasm:
            out.append(f'{label_dasm}:\n')
        out.append(f'    ORG {addr}\n')
        continue

    m = re.match(r'^\.BS\s+(\S+)(.*)', rest)
    if m:
        operand = fix_dots(m.group(1))
        trailing = m.group(2).strip()
        comment = f'  ; {trailing}' if trailing else ''
        if label_dasm:
            out.append(f'{label_dasm}:    ds.b {operand}{comment}\n')
        else:
            out.append(f'    ds.b {operand}{comment}\n')
        continue

    m = re.match(r'^\.DA\s+#%([01]{8})', rest)
    if m:
        if label_dasm: out.append(f'{label_dasm}:\n')
        out.append(f'    dc.b %{m.group(1)}\n')
        continue

    m = re.match(r'^\.DA\s+#\$([0-9A-Fa-f]+)', rest)
    if m:
        if label_dasm: out.append(f'{label_dasm}:\n')
        out.append(f'    dc.b ${m.group(1)}\n')
        continue

    m = re.match(r'^\.DA\s+(\S+)', rest)
    if m:
        addr = fix_dots(m.group(1))
        if label_dasm: out.append(f'{label_dasm}:\n')
        out.append(f'    dc.w {addr}\n')
        continue

    m = re.match(r'^\.HS\s+([0-9A-Fa-f]+)', rest)
    if m:
        hexstr = m.group(1)
        bl = [f'${hexstr[i:i+2]}' for i in range(0, len(hexstr), 2)]
        if label_dasm: out.append(f'{label_dasm}:\n')
        out.append(f'    dc.b {",".join(bl)}\n')
        continue

    line_out = fix_dots(rest)
    if label_dasm:
        out.append(f'{label_dasm}: {line_out}\n')
    else:
        out.append(f'    {line_out}\n')

with open(dst, 'w') as f:
    f.writelines(out)

print(f"fort.dasm written: {len(out)} output lines")
```

### Script output

```
fort.dasm written: 300 output lines
```

---

## Verification

### Remaining dots check

```sh
grep '\.' dev/dasm/fort.dasm \
  | grep -v '^;' | grep -v 'include' \
  | grep -v 'PROCESSOR' | grep -v 'dc\.' | grep -v 'ds\.'
```

**Output: (empty)** — no unconverted dotted identifiers in non-comment,
non-directive lines.

---

## Spot Checks

### Hardware equates (sample)

Source:
```
00200 FRAME    .EQ $14
01090 VVBLKI.RET .EQ $E45F
01100 VVBLKD.RET .EQ $E462
```

Output (lines 42, 131–132):
```dasm
FRAME                EQU $14
VVBLKI_RET           EQU $E45F
VVBLKD_RET           EQU $E462
```

Dot-containing labels converted; no colon on EQU symbols. ✓

### Memory constants with trailing annotations

Source:
```
01290 PLAYER       .EQ $0       $800  R
01300 PLAY.SCRN    .EQ $300     $300  R
01580 WINDOW.1       .EQ CHR.SET2+712
```

Output (lines 151–152, 180):
```dasm
PLAYER               EQU $0  ; $800  R
PLAY_SCRN            EQU $300  ; $300  R
WINDOW_1             EQU CHR_SET2+712
```

Trailing annotation → comment; label and value dots converted. ✓

### Zero-page .BS allocations

Source:
```
01840          .OR $15
01850 ADR1     .BS 2
02040 TANK.START.X
02050          .BS MAX.TANKS
02080 TIM1.VAL .BS 1   LASER 1
```

Output (lines 206–230):
```dasm
    ORG $15
ADR1:    ds.b 2
TANK_START_X:
    ds.b MAX_TANKS
TIM1_VAL:    ds.b 1  ; LASER 1
```

Label-on-own-line pattern handled; trailing text preserved as comment. ✓

### Include conversion

Source:
```
02480          .IN "H1:FORT7.S"
02581          .IN "H1:FNT1.S"
02590          .IN "H1:FORT1.S"
```

Output (lines 270, 281, 284):
```dasm
    include "fort7.dasm"
    include "fnt1.dasm"
    include "fort1.dasm"
```

Device prefix stripped, lowercase, `.dasm` extension. ✓

### .TF directive

Source:
```
02510          .TF "H1:FORT.OBJ"
```

Output (line 273):
```dasm
; .TF "H1:FORT.OBJ"
```

No DASM equivalent — commented out; string preserved as-is. ✓

### END_CART vector

Source:
```
02690 END.CART
02700          .BS $BFFA-*
02710          .DA CART.START
02720          .HS 00
02730          .DA #%10000100
02740          .DA CART.START
```

Output (lines 292–297):
```dasm
END_CART:
    ds.b $BFFA-*
    dc.w CART_START
    dc.b $00
    dc.b %10000100
    dc.w CART_START
```

`END.CART` → `END_CART:` (address label with colon);
`.BS $BFFA-*` preserved with `*` as current PC;
two `.DA CART.START` → `dc.w CART_START` (16-bit words);
`.HS 00` → `dc.b $00`;
`.DA #%10000100` → `dc.b %10000100`. ✓

---

## Design Decisions

| Decision | Rationale |
|---|---|
| EQU labels have no colon | DASM convention for EQU symbols; colons optional but omitted for clarity |
| Address labels (`.BS`, standalone, `END.CART`) get colon | Required or strongly conventional for address labels in DASM |
| Trailing size annotations on `.EQ` kept as comments | Documentation value; stripping silently would lose architectural notes |
| `.IN` → `include` with `.dasm` extension | The included files will be converted to DASM format; reflects expected final filenames |
| `.TF` commented out | DASM has no target-file directive; keeping it commented preserves intent |
| `.LI OFF` commented out | DASM has no listing-control directive |
| `fix_dots` applied to label, EQU value, and ORG/BS operands separately | Avoids accidentally modifying string literals (`.IN` filenames handled before `fix_dots`) |
| `dc.w` for `.DA LABEL` (no `#`) | In SynAssembler, `.DA LABEL` emits a 16-bit address; `dc.w` is the DASM equivalent |

---

## File Summary (Output)

| Property | Value |
|---|---|
| Output file | `dev/dasm/fort.dasm` |
| Lines | 300 |
| Header block | Lines 1–21 (`PROCESSOR 6502` + 19-line comment) |
| Banner comments | Lines 22–36 |
| Hardware equates | Lines 42–132 (~90 `EQU` lines) |
| Game constants | Lines 136–202 (~65 `EQU` lines) |
| Zero-page layout | Lines 206–268 (`ORG` + `ds.b` blocks) |
| Include directives | Lines 270–290 (10 `include` lines) |
| END_CART vector | Lines 292–297 (6 data lines) |
| EOF comment | Lines 298–300 |
