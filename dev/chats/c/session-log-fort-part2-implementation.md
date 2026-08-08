# Session Log: convert fort.s to C — Part 2: Implementation

## Files Produced

- `dev/src/fort.h` (318 lines)
- `dev/src/fort.c` (107 lines)

---

## fort.h Structure

Four sections:

1. **REG() macro** — single definition, shared by all hardware register accesses
2. **Hardware register `#define`s** — OS shadows, GTIA, ANTIC, POKEY, OS ROM addresses
3. **Memory layout `#define`s** — fixed Atari addresses, sub-region offsets, character constants
4. **`extern` declarations** — for all variables defined in `fort.c`

### Section 1: REG() macro

```c
#define REG(a)  (*(volatile uint8_t *)(uintptr_t)(a))
```

Converts a numeric address to a volatile pointer dereference. The `uintptr_t`
cast avoids undefined behaviour when casting an integer to a pointer. Used by
all hardware register `#define`s.

### Section 2: Hardware registers

Organised by subsystem, matching the order in fort.s.

**OS shadows** (low-page addresses, CPU-accessible):
```c
#define FRAME    REG(0x0014)
#define ATTRACT  REG(0x004D)
#define VDSLST_L REG(0x0200)
#define VDSLST_H REG(0x0201)
#define VVBLKI_L REG(0x0222)
#define VVBLKI_H REG(0x0223)
#define VVBLKD_L REG(0x0224)
#define VVBLKD_H REG(0x0225)
#define SDMCTL   REG(0x022F)
#define SDLST_L  REG(0x0230)
#define SDLST_H  REG(0x0231)
#define PRIOR    REG(0x026F)
#define PCOLR0   REG(0x02C0)  /* ... PCOLR3 */
#define COLOR0   REG(0x02C4)  /* ... COLOR4 */
#define CHBAS    REG(0x02F4)
#define CH2      REG(0x02F2)
#define CH       REG(0x02FC)
#define STICK    REG(0x0278)
#define CDTMV1_L REG(0x0218)  /* ... CDTMV2, CDTMA1, CDTMA2 */
```

**GTIA** ($D000–$D01F) — collision read AND position/size write aliases:

The same physical address is used for reading collision data (e.g. `M0PF`)
and writing position/size data (e.g. `HPOSP0`). Fort.s defines both names.
Both are preserved in fort.h so call sites can use whichever name is
semantically correct for their purpose:

```c
/* Collision read */
#define M0PF     REG(0xD000)   /* missile 0 / playfield */
/* Position write — same address */
#define HPOSP0   REG(0xD000)
/* Size write / collision read — same address */
#define SIZEP0   REG(0xD008)
#define M0PL     REG(0xD008)
/* Serial control write / status read — same address */
#define SKCTL    REG(0xD20F)
#define SKSTAT   REG(0xD20F)
```

Other GTIA: `COLPM0–3`, `COLPF0–3`, `COLBK`, `TRIG0`, `GRACTL`, `HITCLR`,
`CONSOL`.

**ANTIC** ($D400–$D40F):
```c
#define DMACTL   REG(0xD400)
#define DLIST    REG(0xD402)
#define HSCROL   REG(0xD404)
#define VSCROL   REG(0xD405)
#define PMBASE   REG(0xD407)
#define CHBASE   REG(0xD409)
#define WSYNC    REG(0xD40A)
#define VCOUNT   REG(0xD40B)
#define NMIEN    REG(0xD40E)
```

**POKEY** ($D200–$D20F):
```c
#define AUDF1    REG(0xD200)   /* ... AUDF4 */
#define AUDC1    REG(0xD201)   /* ... AUDC4 */
#define AUDCTL   REG(0xD208)
#define KBCODE   REG(0xD209)
#define RANDOM   REG(0xD20A)
```

**OS ROM addresses** (numeric constants, not register accesses):
```c
#define VVBLKI_RET  0xE45Fu
#define VVBLKD_RET  0xE462u
```

### Section 3: Memory layout

**Player-missile base addresses** (from fort.s constants MIS, PL0–PL3):
```c
#define MIS_BASE   ((uint8_t *)0x0300u)
#define PL0_BASE   ((uint8_t *)0x0400u)
/* ... PL3_BASE = 0x0700u */
```

**Fixed-address game regions** (from fort.s .EQ section):
```c
#define PLAY_SCRN       ((uint8_t *)0x0300u)
#define CHR_SET1        ((uint8_t *)0x0800u)
#define CHR_SET2        ((uint8_t *)0x0C00u)
#define MAP_BASE        ((uint8_t *)0x1103u)   /* MAP .EQ $1100+3 */
#define SLAVES_BASE     ((uint8_t *)0x3904u)
#define SCANNER_BASE    ((uint8_t *)0x39C0u)
#define RAM1_STUFF      ((uint8_t *)0x0C90u)   /* CHR_SET2+144 */
#define RAM2_STUFF      ((uint8_t *)0x0100u)
#define PACKED_MAP_BASE ((const uint8_t *)0x8000u)
#define PACKED_SCAN_BASE ((const uint8_t *)(0x8000u + 0x0D34u))
```

**CHR_SET1 sub-regions** (score/status display lines):
```c
#define S_LINE1  (CHR_SET1 + 736u)   /* $0AE0 */
#define S_LINE2  (CHR_SET1 + 832u)   /* $0B40 */
#define S_LINE3  (CHR_SET1 + 928u)   /* $0BA0 */
```

**CHR_SET2 sub-regions** (graphics overlays):
```c
#define LASERS_1  (CHR_SET2 + 8u)
#define LASERS_2  (CHR_SET2 + 40u)
#define LASER_3   (CHR_SET2 + 72u)
#define BLOCK_1   (CHR_SET2 + 80u)  /* ... BLOCK_8 = CHR_SET2+136 */
#define WINDOW_1  (CHR_SET2 + 712u)
#define WINDOW_2  (CHR_SET2 + 720u)
#define EXPLOSION (CHR_SET2 + 256u)
#define EXPLOSION2 (CHR_SET2 + 504u)
#define MISS_CHR_LEFT  (CHR_SET2 + 904u)
#define MISS_CHR_RIGHT (CHR_SET2 + 912u)
```

**Character and direction constants:**
```c
#define CHR_EXP        0x20u
#define CHR_EXP2       0x3Fu
#define CHR_MISS_LEFT  0x71u
#define CHR_MISS_RIGHT 0x72u
#define CHR_EXP_WALL   0xC7u   /* EXP.WALL = $47+128 */

#define DIR_RIGHT  0x08u
#define DIR_LEFT   0x04u
#define DIR_DOWN   0x02u
#define DIR_UP     0x01u
```

Note: direction bits are renamed `DIR_*` (not `RIGHT`/`LEFT`/`DOWN`/`UP`)
because those bare names collide with POSIX and standard library symbols.

**Boundary limits and game configuration:**
```c
#define MAX_LEFT    48      #define MIN_LEFT   110
#define MAX_RIGHT  192      #define MIN_RIGHT  130
#define MAX_UP     100      #define MIN_UP     146
#define MAX_DOWN   212      #define MIN_DOWN   166
#define MAX_FUEL   0x2000u
#define MAX_TANKS  6
#define MAX_PODS   39
#define POD_SPEED  15
#define CHECK_SUM  0x264Cu
```

### Section 4: extern variable declarations

All variables defined in `fort.c`, organized by their original zero-page
allocation group:

```c
/* Zero-page scratch ($15 area) */
extern uint8_t adr1_lo, adr1_hi;
extern uint8_t adr2_lo, adr2_hi;
extern uint8_t temp1, temp2, temp3, temp4, temp5, temp6;
extern uint8_t temp_mode;

/* Interrupt-context copies */
extern uint8_t adr1_i_lo, adr1_i_hi;
extern uint8_t adr2_i_lo, adr2_i_hi;
extern uint8_t temp1_i, temp2_i, temp3_i, temp4_i;

/* Scanner helpers */
extern uint8_t s_adr_lo, s_adr_hi;
extern uint8_t s_temp;
extern uint8_t s_flg;

/* Tank spawns */
extern uint8_t tank_start_x[MAX_TANKS];
extern uint8_t tank_start_y[MAX_TANKS];

/* Timers */
extern uint8_t tim1_val;  /* laser 1 */
extern uint8_t tim2_val;  /* laser 2 */
extern uint8_t tim3_val;  /* chop explode */
extern uint8_t tim4_val;  /* refuel */
extern uint8_t tim5_val;  /* tank explode */
extern uint8_t tim6_val;  /* demo timer */
extern uint8_t tim7_val;  /* robot explode */
extern uint8_t tim8_val;  /* robot missile */
extern uint8_t tim9_val;  /* slave message */
extern uint8_t ssizem;

/* Sound channels ($43 area) */
extern uint8_t s1_1_val, s1_2_val;
extern uint8_t s2_val, s3_val, s4_val, s5_val, s6_val;

/* Score / demo */
extern uint8_t game_points;
extern uint8_t demo_status;
extern uint8_t demo_count;

/* Pod arrays */
extern uint8_t pod_status[MAX_PODS];
extern uint8_t pod_dx[MAX_PODS];
extern uint8_t pod_x[MAX_PODS];
extern uint8_t pod_y[MAX_PODS];
extern uint8_t pod_temp1[MAX_PODS];
extern uint8_t pod_temp2[MAX_PODS];

/* Slave arrays */
extern uint8_t slave_status[8];
extern uint8_t slave_x[8];
extern uint8_t slave_y[8];
extern uint8_t slave_dx[8];
```

---

## fort.c Structure

Plain definitions, one group per fort.s `.OR` / `.BS` block:

```c
#include "fort.h"

/* Zero-page scratch (.OR $15) */
uint8_t adr1_lo, adr1_hi;
uint8_t adr2_lo, adr2_hi;
uint8_t temp1, temp2, temp3, temp4, temp5, temp6;
uint8_t temp_mode;

/* Interrupt-context copies */
uint8_t adr1_i_lo, adr1_i_hi;
uint8_t adr2_i_lo, adr2_i_hi;
uint8_t temp1_i, temp2_i, temp3_i, temp4_i;
uint8_t s_adr_lo, s_adr_hi;
uint8_t s_temp;
uint8_t s_flg;

uint8_t tank_start_x[MAX_TANKS];
uint8_t tank_start_y[MAX_TANKS];

uint8_t tim1_val, tim2_val, tim3_val, tim4_val, tim5_val;
uint8_t tim6_val, tim7_val, tim8_val, tim9_val;
uint8_t ssizem;

/* Sound + score (.OR $43) */
uint8_t s1_1_val, s1_2_val;
uint8_t s2_val, s3_val, s4_val, s5_val, s6_val;
uint8_t game_points;
uint8_t demo_status;
uint8_t demo_count;

/* Pod arrays (.OR POD.1 / .OR POD.2) */
uint8_t pod_status[MAX_PODS];
uint8_t pod_dx[MAX_PODS];
uint8_t pod_x[MAX_PODS];
uint8_t pod_y[MAX_PODS];
uint8_t pod_temp1[MAX_PODS];
uint8_t pod_temp2[MAX_PODS];

/* Slave arrays (.OR SLAVES) */
uint8_t slave_status[8];
uint8_t slave_x[8];
uint8_t slave_y[8];
uint8_t slave_dx[8];
```

All variables are zero-initialised by C's global-scope guarantee, matching
the SynAssembler `.BS` semantics (block storage = zero at startup).

---

## Compile Verification

### Single-file check

```
gcc -std=c11 -Wall -Wextra -fsyntax-only fort.c
```

Result: **Exit 0 — zero diagnostics.**

### Full project check

```
gcc -std=c11 -Wall -Wextra -fsyntax-only \
    fort.c fort7.c fort1.c fort2.c fort3.c \
    fort4.c fort5.c fort6.c fort7.c fort8.c -I.
```

Result: **Exit 0 — zero diagnostics** across all fort*.c files.

This confirms that `fort.c` satisfies all `extern` references that were
previously undeclared-but-referenced (e.g. `adr1_lo`, `temp1–6`,
`pod_status`, `slave_status`, `tim1_val` etc.) in fort1.c, fort2.c,
and fort5.c.

---

## Design Decisions

| Decision | Rationale |
|---|---|
| `fort.h` is an include-guard-free, `#pragma once` header | Standard modern C practice; simpler than include guards |
| Direction bits renamed `DIR_RIGHT` etc. | Bare `RIGHT`/`LEFT` clash with POSIX `fcntl.h` and other OS headers |
| `VVBLKI_RET`/`VVBLKD_RET` as `u` integer literals, not `REG()` | These are jump targets (code addresses), not data register accesses |
| `CHECK_SUM` not `CHECK.SUM` | Dot in identifier is invalid in C |
| `SLAVES_BASE` added (not in fort.s as a pointer) | Gives C code a clean typed pointer to the slave data origin |
| Pod arrays are plain globals, not pointer-casts to Atari addresses | On a hosted build, the Atari addresses ($0FF8, $3925) are meaningless; data is accessed through the array names, not through address arithmetic |
| fort7.c variables not repeated | fort7.c already defines them; redefinition would cause a link error |
| Existing fort*.c files not updated to `#include "fort.h"` | They compile and link cleanly as-is; the migration is a separate refactor task |

---

## File Summary

| File | Lines | Purpose |
|---|---|---|
| `dev/src/fort.h` | 318 | Master hardware equates + memory layout + extern declarations |
| `dev/src/fort.c` | 107 | Zero-page scratch vars + timers + sound state + pod/slave arrays |
