#pragma once
#include <stdint.h>

/* -----------------------------------------------------------------------
 * fort5.h — Slave AI, fuel base, scanner, fort explosion, DLI, sounds
 *            (converted from fort5.s)
 * ----------------------------------------------------------------------- */

/* ROM integrity checks */
void do_checksum2(void);
void do_checksum3(void);

/* Slave entity management */
void move_slaves(void);
void print_slaves_left(void);
int pick_up_slave(void); /* 1 = slave captured, 0 = none in range */

/* Fuel base logic */
void check_fuel_base(void);

/* Scanner minimap update */
void set_scanner(void);
void pos_it(void); /* temp1=tile_x, temp2=tile_y → toggle scanner pixel */

/* Fort explosion sequence */
void check_fort(void);

/* Display-list interrupt handlers (chained: LINE1→2→3→4→LINE1) */
void line1(void);
void line2(void);
void line3(void);
void line4(void);

/* Sound subsystem (called from line4) */
void do_sounds(void);

/* Landing-pad tile characters — checked by fort3.c save_pos() */
extern const uint8_t land_chr[5];
