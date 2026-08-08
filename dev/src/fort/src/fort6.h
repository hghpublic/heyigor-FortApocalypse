#pragma once
#include <stdint.h>

/* -----------------------------------------------------------------------
 * fort6.h — Sprite shapes, display lists, ROM init data
 *            (converted from fort6.s)
 * ----------------------------------------------------------------------- */

/* CHOPPER.SHAPES — 18 shape pointers, indexed by chopper_angle (0-17).
 * Each entry: 36 bytes — [0..17] = player 0 left half, [18..35] = right half.
 */
static constexpr uint8_t LEN_CHOPPER_SHAPES = 18;
extern const uint8_t* const chopper_shapes[LEN_CHOPPER_SHAPES];

/* LASER.SHAPES — 4 × 8 bytes = 32 bytes of laser animation patterns */
extern const uint8_t laser_shapes[32];

/* ANTIC display lists (JMP self-reference bytes patched by fort6_init) */
extern uint8_t dsp_lst2[39];
extern uint8_t dsp_lst3[32];

/* Patch self-referential JMP addresses in display lists at runtime */
void fort6_init(void);
