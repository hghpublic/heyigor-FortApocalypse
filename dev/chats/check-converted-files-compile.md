
----
cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src dev/src/fort2.c 2>&1 | head -60
cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src dev/src/fort3.c 2>&1
cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src dev/src/fort2.c dev/src/fort3.c 2>&1
cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src dev/src/fort2.c dev/src/fort3.c dev/src/fort4.c 2>&1
cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src dev/src/fort2.c dev/src/fort3.c dev/src/fort4.c dev/src/fort5.c 2>&1
cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src dev/src/fort1.c dev/src/fort2.c dev/src/fort3.c dev/src/fort4.c dev/src/fort5.c 2>&1
cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src dev/src/fort1.c dev/src/fort2.c dev/src/fort3.c dev/src/fort4.c dev/src/fort5.c dev/src/fort6.c 2>&1

cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src \
   dev/src/fort2.c dev/src/fort3.c dev/src/fort4.c \
   dev/src/fort5.c dev/src/fort6.c dev/src/fort7.c 2>&1

cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src \
   dev/src/fort2.c dev/src/fort3.c dev/src/fort4.c \
   dev/src/fort5.c dev/src/fort6.c dev/src/fort7.c dev/src/fort8.c 2>&1

gcc -std=c11 -Wall -Wextra -fsyntax-only fort.c 2>&1
echo "Exit: $?"


# Check that the variables fort1.c declares as extern are now satisfied
# by combining fort.c + fort7.c
gcc -std=c11 -Wall -Wextra -fsyntax-only \
    fort.c fort7.c fort1.c fort2.c fort3.c fort4.c fort5.c fort6.c fort7.c fort8.c \
    -I. 2>&1
echo "Exit: $?"

# Variables already defined in fort7.c (mode, level, score*, chopper_*, etc.) are intentionally absent — no duplication.


gcc -std=c11 -Wall -Wextra -fsyntax-only \
    main.c fort.c fort7.c fort1.c fort2.c fort3.c fort4.c fort5.c fort6.c fort8.c \
    fnt1.c fnt2.c -I. 2>&1
echo "Exit: $?"
----

----
cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src $(ls dev/src/fort*.c) 2>&1

cc -std=c11 -Wall -Wextra -fsyntax-only -I dev/src $(ls dev/src/*.c) 2>&1

cc -std=c23 -Wall -Wextra -fsyntax-only -I dev/src $(ls dev/src/*.c) 2>&1
cc -std=c23 -Wall -Wextra -fsyntax-only -I. $(ls *.c) 2>&1
cc -std=c23 -Wall -Wextra -I. $(ls *.c) 2>&1
----


----
cd xxx.worktrees/asm-to-c-conversion
python3 - <<'EOF'
# Count bytes in z1 initializer
z1 = [
    0x70,0x70,0x80,0x70,
    0x44,0x00,0x01,
    0x04,0x04,0x04,0x04,
    0x44,0x00,0x00,
    0xC4,0x00,0x03,
    0x50,0x20,0x80,
    # 8 pairs
    0x74,0x00,0x00, 0x74,0x00,0x00,
    0x74,0x00,0x00, 0x74,0x00,0x00,
    0x74,0x00,0x00, 0x74,0x00,0x00,
    0x74,0x00,0x00, 0x74,0x00,0x00,
    0x74,0x00,0x00, 0x74,0x00,0x00,
    0x74,0x00,0x00, 0x74,0x00,0x00,
    0x74,0x00,0x00, 0x74,0x00,0x00,
    0x74,0x00,0x00, 0x74,0x00,0x00,
    0xD4,0x00,0x00,
    0x41,0x90,0x0C,
]
print(f"z1 bytes: {len(z1)}, z1_len: {len(z1)-1}")

# Offsets in dsp_lst1 for patching
print(f"NAVA.PANEL addr at [12..13]: [{z1[12]:#x}, {z1[13]:#x}]")
print(f"JVB at offset 71 = {z1[71]:#x}, addr at [72..73]: [{z1[72]:#x}, {z1[73]:#x}]")

# Count z2 bytes
sections = [
    ("7 spaces", 7),
    ("SCORE hex", 10),
    ("2 spaces", 2),
    # SCORE.DIG at 19
    ("12 spaces", 12),
    ("9 spaces", 9),
    ("20 spaces", 20),
    ("20 spaces", 20),
    ("2 spaces", 2),
    ("FUEL hex", 8),
    ("2sp+colon", 3),
    ("fuel bar", 12),
    ("eq+2sp", 3),
    ("BONUS hex", 10),
    ("4 spaces", 4),
    # FUEL.DIG at 122
    ("8 spaces", 8),
    ("2sp+semi", 3),
    ("hiscore bar", 12),
    ("gt+3sp", 4),
    # BONUS.DIG at 149
    ("8 spaces", 8),
    ("3 spaces", 3),
    ("12sp+lt", 13),
    ("bonus bar", 12),
    ("qm+14sp", 15),
]
total = 0
checkpoints = {}
for name, n in sections:
    if total == 19: checkpoints['SCORE_DIG'] = total
    if total == 122: checkpoints['FUEL_DIG'] = total
    if total == 149: checkpoints['BONUS_DIG'] = total
    total += n
print(f"z2 bytes: {total}, z2_len: {total-1}")
print(f"Offsets: SCORE_DIG=19 ({'✓' if checkpoints.get('SCORE_DIG')==19 else 'WRONG'}), FUEL_DIG=122 ({'✓' if checkpoints.get('FUEL_DIG')==122 else 'WRONG'}), BONUS_DIG=149 ({'✓' if checkpoints.get('BONUS_DIG')==149 else 'WRONG'})")
EOF
----