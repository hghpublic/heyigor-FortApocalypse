import re, sys

src = "xxx/fort.s"
dst = "xxx/dev/dasm/fort.dasm"

with open(src) as f:
    lines = f.readlines()

out = []

# Header
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
    """Replace dots between identifier characters with underscores (multi-pass)."""
    prev = None
    while prev != s:
        prev = s
        s = re.sub(r'([A-Za-z0-9])\.([A-Za-z0-9])', r'\1_\2', s)
    return s

def in_filename(raw):
    """Convert "H1:FILE.S" to "file.dasm"."""
    raw = re.sub(r'^[^:]+:', '', raw)  # strip device prefix
    return raw.lower().replace('.s', '.dasm')

for raw in lines:
    raw = raw.rstrip('\n')

    # Strip 5-digit prefix + mandatory space (6 chars)
    m = re.match(r'^\d{5} (.*)', raw)
    if m:
        rest = m.group(1)
    elif re.match(r'^\d{5}\s*$', raw):
        rest = ''
    else:
        rest = raw

    # Blank line
    if not rest.strip():
        out.append('\n')
        continue

    # Comment line
    if rest.startswith('*'):
        comment = rest[1:].strip()
        out.append(f'; {comment}\n' if comment else ';\n')
        continue

    # Extract label if line does not start with a space
    label = None
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

    # Label alone on a line (e.g. TANK.START.X before .BS on next line)
    if label_dasm is not None and not rest:
        out.append(f'{label_dasm}:\n')
        continue

    # .LI directive (listing control) — no DASM equivalent
    if re.match(r'^\.LI\b', rest):
        out.append(f'; {rest.strip()}\n')
        continue

    # .TF directive (target file) — no DASM equivalent
    if re.match(r'^\.TF\b', rest):
        out.append(f'; {rest.strip()}\n')
        continue

    # .IN "filename"
    m = re.match(r'^\.IN\s+"([^"]+)"', rest)
    if m:
        fname = in_filename(m.group(1))
        out.append(f'    include "{fname}"\n')
        continue

    # .EQ VALUE [trailing]
    m = re.match(r'^\.EQ\s+(\S+)(.*)', rest)
    if m:
        value = fix_dots(m.group(1))
        trailing = m.group(2).strip()
        lbl = label_dasm if label_dasm else ''
        comment = f'  ; {trailing}' if trailing else ''
        out.append(f'{lbl:<20} EQU {value}{comment}\n')
        continue

    # .OR ADDR
    m = re.match(r'^\.OR\s+(\S+)', rest)
    if m:
        addr = fix_dots(m.group(1))
        if label_dasm:
            out.append(f'{label_dasm}:\n')
        out.append(f'    ORG {addr}\n')
        continue

    # .BS OPERAND [trailing]
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

    # .DA #%BITS  (immediate binary byte)
    m = re.match(r'^\.DA\s+#%([01]{8})', rest)
    if m:
        if label_dasm:
            out.append(f'{label_dasm}:\n')
        out.append(f'    dc.b %{m.group(1)}\n')
        continue

    # .DA #$HH  (immediate hex byte)
    m = re.match(r'^\.DA\s+#\$([0-9A-Fa-f]+)', rest)
    if m:
        if label_dasm:
            out.append(f'{label_dasm}:\n')
        out.append(f'    dc.b ${m.group(1)}\n')
        continue

    # .DA LABEL  (16-bit address word)
    m = re.match(r'^\.DA\s+(\S+)', rest)
    if m:
        addr = fix_dots(m.group(1))
        if label_dasm:
            out.append(f'{label_dasm}:\n')
        out.append(f'    dc.w {addr}\n')
        continue

    # .HS HEXSTRING  (raw hex bytes)
    m = re.match(r'^\.HS\s+([0-9A-Fa-f]+)', rest)
    if m:
        hexstr = m.group(1)
        bytes_list = [f'${hexstr[i:i+2]}' for i in range(0, len(hexstr), 2)]
        if label_dasm:
            out.append(f'{label_dasm}:\n')
        out.append(f'    dc.b {",".join(bytes_list)}\n')
        continue

    # Fallback
    line_out = fix_dots(rest)
    if label_dasm:
        out.append(f'{label_dasm}: {line_out}\n')
    else:
        out.append(f'    {line_out}\n')

with open(dst, 'w') as f:
    f.writelines(out)

print(f"fort.dasm written: {len(out)} output lines")