#pragma once
#include <stdint.h>

/* -----------------------------------------------------------------------
 * fort8.h — Display list ROM templates and RAM working copy
 *            (converted from fort8.s)
 *
 * Z1: 74-byte ROM template for the main game display list.
 *     Copied to RAM1.STUFF ($0C90) at startup by unpack().
 *     DSP.MAP is at byte offset 20 within the working copy (dsp_lst1[20]).
 *
 * Z2: 200-byte ROM template for the panel display buffer.
 *     Copied to RAM2.STUFF ($0100) at startup by unpack().
 *     Panel digit field offsets: SCORE_DIG=19, FUEL_DIG=122, BONUS_DIG=149.
 * ----------------------------------------------------------------------- */

/* ROM template for game display list */
extern const uint8_t z1[74];
extern const uint8_t z1_len; /* Z1.LEN = 73 */

/* Working game display list in RAM (patched copy of z1) */
extern uint8_t dsp_lst1[74];

/* ROM template for panel display buffer */
extern const uint8_t z2[200];
extern const uint8_t z2_len; /* Z2.LEN = 199 */

/* Patch NAVA.PANEL and JVB self-reference addresses into dsp_lst1 */
void fort8_init(void);
