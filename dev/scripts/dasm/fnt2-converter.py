import re, sys

src = "fnt2.s"
dst = "fnt2.dasm"

with open(src) as f:
    lines = f.readlines()

out = []
byte_count = 0

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
        rows.append(f"    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR ${c:02X}\n")
        byte_count += 8
    return rows

for raw in lines:
    raw = raw.rstrip('\n')

    m = re.match(r'^\d{5} (.*)', raw)
    if m:
        rest = m.group(1)
    elif re.match(r'^\d{5}$', raw.strip()):
        rest = ''
    else:
        rest = raw

    if not rest.strip():
        continue

    # Label: no leading space, doesn't start with *
    if rest and rest[0] not in (' ', '*'):
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

    # Fallback
    out.append(f"; {rest.strip()}\n")

assert byte_count == 1024, f"Expected 1024 bytes, got {byte_count}"

with open(dst, 'w') as f:
    f.writelines(out)

print(f"OK: fnt2.dasm written — {byte_count} bytes, {len(out)} output lines")