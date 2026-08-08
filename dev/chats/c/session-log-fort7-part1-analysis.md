# Session Log: convert fort7.s — Part 1: Analysis

## Task
Convert `fort7.s` (VARIABLES) to C.
Output files: `dev/src/fort7.h` and `dev/src/fort7.c`.

---

## fort7.s — Source File Overview

`fort7.s` is 140 lines and is a pure data/variable file — no code whatsoever. It has two kinds of content:

1. **`.EQ` equates** — named integer constants
2. **`.BS N` blocks** — zero-initialised BSS storage allocations

---

## .EQ Constants — Already Covered

### Status constants (lines 5–16)
```
OFF          .EQ 1
ON           .EQ 2
FLY          .EQ 3
CRASH        .EQ 4
EXPLODE      .EQ 5
LAND         .EQ 6
BEGIN        .EQ 7
FULL         .EQ 8
EMPTY        .EQ 9
REFUEL       .EQ 10
PICKUP       .EQ 11
```

### Mode constants (lines 91–100)
```
TITLE.MODE      .EQ 1
GO.MODE         .EQ 2
START.MODE      .EQ 3
NEW.LEVEL.MODE  .EQ 4
NEW.PLAYER.MODE .EQ 5
GAME.OVER.MODE  .EQ 6
STOP.MODE       .EQ 7
PAUSE.MODE      .EQ 8
OPTION.MODE     .EQ 9
HYPERSPACE.MODE .EQ 10
```

**Finding:** Both groups are already defined in `fort1.h` as `STATUS_OFF..STATUS_PICKUP` and `TITLE_MODE..HYPERSPACE_MODE`. No re-definition needed in `fort7.h`.

### MAX_TANKS
The comment at line 113 states `* MAX.TANKS*3  6*3=18`, confirming MAX_TANKS = 6.
`fort1.h` already has `#define MAX_TANKS 6` and `#define MAX_PODS 39`.

**Decision:** `fort7.h` includes `fort1.h` to inherit MAX_TANKS; `fort7.c` also includes `fort1.h`.

---

## .BS Variables — Complete Survey

Assembly name → C name — the `.` separator becomes `_`, acronym/abbreviation preserved.

### Scanner address pointers
```
SCAN.ADR1  .BS 2   →  scan_adr1_lo, scan_adr1_hi
SCAN.ADR2  .BS 2   →  scan_adr2_lo, scan_adr2_hi
```
`fort5.c` already references these as `extern uint8_t scan_adr1_lo, scan_adr1_hi` and
`extern uint8_t scan_adr2_lo, scan_adr2_hi`. The 2-byte allocation is split into two
named `uint8_t` variables following the lo/hi convention used for all 16-bit pointers
throughout the C port (e.g., `adr1_lo`/`adr1_hi`).

### Scrolling position
```
SX    .BS 1   →  sx
SX.F  .BS 1   →  sx_f
SY    .BS 1   →  sy
SY.F  .BS 1   →  sy_f
```
Referenced in fort1.c, fort2.c, fort3.c, fort4.c, fort5.c.

### Input flags
```
CONSOL.FLAG  .BS 1   →  consol_flag    (fort2.c)
TRIG.FLAG    .BS 1   →  trig_flag      (fort4.c)
```

### Game progress
```
LEVEL  .BS 1   →  level   (fort1.c, fort3.c, fort5.c)
MODE   .BS 1   →  mode    (fort1.c–fort5.c — most heavily used variable)
```
Note: `MODE` appears in the assembly at line 88, after BAK2.COLOR. Its C name `mode`
is used across all five existing .c files.

### Landing-site snapshot
```
LAND.X         .BS 1   →  land_x
LAND.Y         .BS 1   →  land_y
LAND.FX        .BS 1   →  land_fx
LAND.FY        .BS 1   →  land_fy
LAND.CHOP.X    .BS 1   →  land_chop_x
LAND.CHOP.Y    .BS 1   →  land_chop_y
LAND.CHOP.ANGLE .BS 1  →  land_chop_angle
```
Referenced in fort1.c, fort3.c, fort5.c. Saved when the chopper lands.

### Chopper sprite state
```
CHOPPER.STATUS  .BS 1   →  chopper_status   (fort1.c–fort5.c)
CHOPPER.X       .BS 1   →  chopper_x
CHOPPER.Y       .BS 1   →  chopper_y
OCHOPPER.Y      .BS 1   →  ochopper_y       (old/previous sprite Y; fort3.c)
CHOPPER.ANGLE   .BS 1   →  chopper_angle
CHOPPER.COL     .BS 1   →  chopper_col      (collision byte; fort1.c, fort3.c)
CHOP.X          .BS 1   →  chop_x           (tile coords)
CHOP.Y          .BS 1   →  chop_y
CHOP.OX         .BS 1   →  chop_ox          (previous tile)
CHOP.OY         .BS 1   →  chop_oy
```
Note: `chop_ox` and `chop_oy` are not referenced in any existing `extern` declaration;
they are defined anyway since they exist in the source.

### Robot sprite/AI state
```
ROBOT.STATUS  .BS 1   →  robot_status   (fort2.c, fort3.c, fort4.c)
R.STATUS      .BS 1   →  r_status       (fort1.c, fort2.c, fort3.c) — AI phase
ROBOT.X       .BS 1   →  robot_x
ROBOT.Y       .BS 1   →  robot_y
OROBOT.Y      .BS 1   →  orobot_y       (previous sprite Y; fort3.c)
ROBOT.ANGLE   .BS 1   →  robot_angle
ROBOT.SPD     .BS 1   →  robot_spd      (frame-rate AND mask)
ROBOT.COL     .BS 1   →  robot_col
R.FX          .BS 1   →  r_fx           (sub-tile X fraction)
R.FY          .BS 1   →  r_fy           (sub-tile Y fraction)
R.X           .BS 1   →  r_x            (map tile X)
R.Y           .BS 1   →  r_y            (map tile Y)
```

### Rocket arrays (3 rockets)
```
ROCKET.STATUS  .BS 3   →  rocket_status[3]
ROCKET.X       .BS 3   →  rocket_x[3]
ROCKET.Y       .BS 3   →  rocket_y[3]
ROCKET.TEMP    .BS 3   →  rocket_temp[3]    (saved map tile at impact)
ROCKET.TEMPX   .BS 3   →  rocket_tempx[3]   (saved map X at impact)
ROCKET.TEMPY   .BS 3   →  rocket_tempy[3]   (saved map Y at impact)
ROCKET.TIM     .BS 3   →  rocket_tim[3]     (flight timer)
OROCKET.Y      .BS 3   →  orocket_y[3]      (previous sprite Y)
```

### Elevator
```
ELEVATOR.NUM  .BS 1   →  elevator_num
ELEVATOR.DX   .BS 1   →  elevator_dx
ELEVATOR.TIM  .BS 1   →  elevator_tim
ELEVATOR.SPD  .BS 1   →  elevator_spd
```
Referenced in fort1.c and fort4.c.

### Score / hi-score / bonus
```
SCORE1  .BS 1   →  score1    (3-digit BCD: hundreds, tens, units)
SCORE2  .BS 1   →  score2
SCORE3  .BS 1   →  score3
HI1     .BS 1   →  hi1
HI2     .BS 1   →  hi2
HI3     .BS 1   →  hi3
BONUS1  .BS 1   →  bonus1
BONUS2  .BS 1   →  bonus2
```

### Fuel
```
FUEL.STATUS  .BS 1   →  fuel_status
FUEL.TEMP    .BS 1   →  fuel_temp    (countdown to next fuel-base state; fort5.c)
FUEL1        .BS 1   →  fuel1        (BCD fuel counter, hi digit)
FUEL2        .BS 1   →  fuel2        (BCD fuel counter, lo digit)
```

### Display colours
```
BAK.COLOR   .BS 1   →  bak_color    (fort1.c, fort5.c)
BAK2.COLOR  .BS 1   →  bak2_color   (fort2.c, fort3.c, fort5.c)
```

### Cruise-missile arrays (MAX_TANKS = 6 entries)
```
CM.STATUS  .BS MAX.TANKS   →  cm_status[MAX_TANKS]
CM.X       .BS MAX.TANKS   →  cm_x[MAX_TANKS]
CM.Y       .BS MAX.TANKS   →  cm_y[MAX_TANKS]
CM.TIME    .BS MAX.TANKS   →  cm_time[MAX_TANKS]
CM.TEMP    .BS MAX.TANKS   →  cm_temp[MAX_TANKS]
```

### Tank arrays
```
TANK.STATUS  .BS MAX.TANKS   →  tank_status[MAX_TANKS]
TANK.X       .BS MAX.TANKS   →  tank_x[MAX_TANKS]
TANK.Y       .BS MAX.TANKS   →  tank_y[MAX_TANKS]
TANK.DX      .BS MAX.TANKS   →  tank_dx[MAX_TANKS]
TANK.TEMP    .BS 18          →  tank_temp[18]
```
`TANK.TEMP` is `.BS 18` (hardcoded, not `.BS MAX.TANKS`). The assembly comment
`* MAX.TANKS*3  6*3=18` explains the allocation: three scratch bytes per tank.

### Pod / slave counts
```
POD.NUM      .BS 1   →  pod_num
POD.COM      .BS 1   →  pod_com
SLAVE.NUM    .BS 1   →  slave_num
SLAVES.LEFT  .BS 1   →  slaves_left
SLAVES.SAVED .BS 1   →  slaves_saved
```

### Fort / laser / speed / skill
```
FORT.STATUS    .BS 1   →  fort_status
LASER.STATUS   .BS 1   →  laser_status
LASER.SPD      .BS 1   →  laser_spd
TANK.SPD       .BS 1   →  tank_spd       (current frame-rate mask)
TANK.SPEED     .BS 1   →  tank_speed     (per-level initial value)
MISSILE.SPD    .BS 1   →  missile_spd
MISSILE.SPEED  .BS 1   →  missile_speed
GRAV.SKILL     .BS 1   →  grav_skill     (option setting)
GRAV.SKL       .BS 1   →  grav_skl       (derived per-level value; fort3.c)
PILOT.SKILL    .BS 1   →  pilot_skill
PILOT.SKL      .BS 1   →  pilot_skl      (not yet referenced but defined)
CHOPS          .BS 1   →  chops          (total choppers in game)
CHOP.LEFT      .BS 1   →  chop_left      (choppers left for current player)
OPT.NUM        .BS 1   →  opt_num        (selected option row 0-2)
START.PODS     .BS 1   →  start_pods     (pods to place at level start)
```

---

## Variables NOT in fort7.s

Several variables referenced as `extern` in fort2.c–fort5.c are NOT in fort7.s.
These will be resolved by fort8.c:

- `temp1`, `temp2`, `temp3`, `temp4`, `temp5`, `temp6` — scratch vars
- `adr1_lo`, `adr1_hi`, `adr2_lo`, `adr2_hi` — indirect address pointers
- `adr1_i_lo`, `adr1_i_hi`, `adr2_i_lo`, `adr2_i_hi`
- `temp1_i`, `temp2_i`, `temp3_i`, `temp4_i`
- `s_adr_lo`, `s_adr_hi`, `s_flg`
- `frame_count`, `ssizem`
- `player_base[]` — 2 KB page-aligned P/M graphics buffer
- `slave_status[8]`, `slave_x[8]`, `slave_y[8]`, `slave_dx[8]`
- `pod_status[MAX_PODS]`, `pod_x[MAX_PODS]`, `pod_y[MAX_PODS]`, `pod_dx[MAX_PODS]`
- `pod_temp1[MAX_PODS]`, `pod_temp2[MAX_PODS]`
- `tank_start_x[MAX_TANKS]`, `tank_start_y[MAX_TANKS]`
- `tim1_val` … `tim9_val` — timer values
- `s1_1_val`, `s1_2_val`, `s2_val` … `s6_val` — sound channel values
- `demo_status`, `demo_count`, `temp_mode`, `game_points`

---

## Design Decisions

1. **No new constants** — STATUS, MODE, MAX_TANKS, MAX_PODS all already in `fort1.h`.
2. **fort7.h includes fort1.h** — needed so `[MAX_TANKS]` is valid in `extern` declarations.
3. **SCAN.ADR1/2 split as lo/hi** — consistent with the universal `adr1_lo`/`adr1_hi` pattern.
4. **All globals default to zero** — C guarantees zero-init for file-scope variables; no explicit initializers needed.
5. **No functions** — fort7.s contains no code, so fort7.c is definitions only.
6. **`tank_temp[18]` hardcoded** — the assembly hardcodes `.BS 18`, not `.BS MAX.TANKS`; matching this exactly.
7. **`chop_ox`, `chop_oy`, `pilot_skl` defined but not yet externs** — they are in the source; they will be used eventually.
