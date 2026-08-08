/* main.c — Fort Apocalypse entry point
 *
 * Compilation target: Atari 400/800 compatible hardware or emulator.
 * The game accesses Atari hardware registers via absolute addresses
 * (WSYNC=$D40A, AUDF1=$D200, etc.) that are only valid on Atari hardware
 * or inside an emulator.  On a hosted build, this file compiles cleanly
 * but the program can only run correctly under those conditions.
 *
 * Startup sequence:
 *   1. fort6_init() — patch runtime addresses into dsp_lst2 (game display
 *                     list) and dsp_lst3 (title display list).  These lists
 *                     contain self-referential JMP bytes that must reflect
 *                     the actual C process address of each array.
 *
 *   2. fort8_init() — patch nava_panel address and JVB self-reference into
 *                     dsp_lst1 (main game display list).  Same reason.
 *
 *   3. fort1_start() — ROM CART.START entry point.  Copies the Z1 display-
 *                      list template and Z2 panel template to their Atari
 *                      RAM destinations, initialises hardware registers,
 *                      installs DLI and VBlank interrupt handlers, loads
 *                      fonts into the character sets, and enters title().
 *                      Does not return.
 */

#include "fort1.h"
#include "fort6.h"
#include "fort8.h"

int main(void)
{
    fort6_init();
    fort8_init();
    fort1_start(); /* never returns */
    return 0;
}
