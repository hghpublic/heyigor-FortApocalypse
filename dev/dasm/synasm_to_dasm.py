#!/usr/bin/env python3
"""Convert fnt1.s (SynAssembler) to DASM format."""
import re, sys

SRC  = sys.argv[1]
DEST = sys.argv[2]

LABEL_MAP = {
    'FNT1':      'FNT1',
    'POS.MASK1': 'POS_MASK1',
    'FORT.EX1':  'FORT_EX1',
    'FORT.EX2':  'FORT_EX2',
    'FORT.EX3':  'FORT_EX3',
    'FORT.EX4':  'FORT_EX4',
    'T.5':       'T_5',
    'EXP.SHAPE': 'EXP_SHAPE',
}

with open(SRC) as f:
    lines = f.readlines()

out = []
out += [
    '; fnt1.dasm  --  Fort Apocalypse character set 1',
    '; Converted from fnt1.s (SynAssembler format) to DASM',
    '; 128 characters x 8 bytes = 1024 bytes total',
    ';',
    '; Dot-labels renamed to underscore equivalents (DASM uses . as local-label prefix):',
    ';   POS.MASK1 -> POS_MASK1  (CHR $0B)',
    ';   FORT.EX1  -> FORT_EX1   (CHR $0C)',
    ';   FORT.EX2  -> FORT_EX2   (CHR $0D)',
    ';   FORT.EX3  -> FORT_EX3   (CHR $0E)',
    ';   FORT.EX4  -> FORT_EX4   (CHR $0F)',
    ';   T.5       -> T_5         (CHR $20)',
    ';   EXP.SHAPE -> EXP_SHAPE   (CHR $3C)',
    ';',
    '; CHR $00 and $5B-$7F are blank in the original but have no explicit .DA bytes.',
    '; DC.B $00 rows are added here so this file assembles to the correct 1024 bytes.',
    '; CHR $01: 7 commented-out zero rows in source are emitted as active DC.B $00 bytes.',
    '',
    '    PROCESSOR 6502',
    '',
]

# Track whether we are converting the CHR $01 commented zeros
in_chr01_comments = False

for raw in lines:
    line = raw.rstrip('\n')

    # Strip 5-digit line number + optional space: "00010 " -> ""
    m = re.match(r'^\d{5}\s?', line)
    body = line[m.end():] if m else line

    # ── completely blank lines ──────────────────────────────────────────────
    if not body.strip():
        out.append('')
        in_chr01_comments = False
        continue

    # ── SynAssembler full-comment lines:  * ... ────────────────────────────
    if body.startswith('*'):
        text = body[1:].strip()
        out.append(('; ' + text) if text else ';')

        # CHR $00 BLANK: inject 8 explicit zero bytes
        if re.search(r'CHR \$00\s+BLANK', body):
            out.append('    DC.B    $00,$00,$00,$00,$00,$00,$00,$00')
            out.append('; (8 zero bytes injected — not present in original source)')

        # CHR $01: next 7 commented-out .DA lines will be converted to active DC.B
        if re.match(r'\* CHR \$01\s*$', body):
            in_chr01_comments = True

        # CHR $5B-$7F BLANK: inject 37x8 zero bytes
        if re.search(r'CHR \$5B', body) and re.search(r'\$7F', body):
            out.append('; (37 x 8 = 296 zero bytes injected — not present in original source)')
            for i in range(37):
                out.append(f'    DC.B    $00,$00,$00,$00,$00,$00,$00,$00  ; CHR ${0x5B+i:02X}')
        continue

    # ── Commented-out data: ;.DA #%XXXXXXXX ────────────────────────────────
    if body.startswith(';.DA #%'):
        bits = body.split('#%')[1].split()[0]
        if in_chr01_comments:
            # Emit as active byte (correct binary has these as $00)
            out.append(f'    DC.B    %{bits}  ; was commented-out in source')
        else:
            out.append(f';   DC.B    %{bits}')
        continue

    # ── Active data: .DA #%XXXXXXXX  (leading space) ──────────────────────
    if '.DA #%' in body:
        in_chr01_comments = False          # first active byte ends the $01 block
        bits = body.split('#%')[1].split()[0]
        out.append(f'    DC.B    %{bits}')
        continue

    # ── .AT -/TEXT/ directive (T_5 label) ─────────────────────────────────
    if '.AT' in body:
        m2 = re.search(r'\.AT\s+-?/([^/]+)/', body)
        if m2:
            text = m2.group(1)
            bytes_hex = ','.join(f'${ord(c):02X}' for c in text)
            out.append(f'    DC.B    {bytes_hex}  ; .AT -/{text}/ (raw ASCII)')
        else:
            out.append('; [unparsed .AT] ' + body.strip())
        continue

    # ── Labels (no leading whitespace, not starting with . or ;) ──────────
    stripped = body.strip()
    if stripped and not body[0].isspace() and not body[0] in (';', '.', '*'):
        dasm_label = LABEL_MAP.get(stripped, stripped.replace('.', '_'))
        out.append(f'{dasm_label}:')
        continue

    # ── Anything else ──────────────────────────────────────────────────────
    out.append('; [?] ' + body)

with open(DEST, 'w') as f:
    f.write('\n'.join(out) + '\n')

print(f'Written {len(out)} lines.')
