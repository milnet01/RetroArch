/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TASK_SAVE_RASTATE_H
#define __TASK_SAVE_RASTATE_H

#include <stddef.h>

/* Kept dependency-free (only <stddef.h>) so the regression test
 * samples/tasks/save_rastate/rastate_bounds_test.c can compile the real
 * block-bounds predicate standalone -- no libretro-common include path
 * required. These helpers back content_load_rastate1() in task_save.c;
 * the shipped parser calls them so a re-sync that reverts the bounds
 * arithmetic is caught by the test rather than silently drifting from a
 * copy. */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * rastate_read_le32:
 * @p : pointer to the 4-byte little-endian block-size field of a rastate
 *      block header. @p must have 4 readable bytes.
 *
 * Decodes the field, widening each byte to unsigned long before shifting
 * so a set high bit (input[3] << 24) does not trip signed-int-promotion
 * undefined behaviour. Returns the decoded value (0 .. 0xFFFFFFFF).
 */
unsigned long rastate_read_le32(const unsigned char *p);

/**
 * rastate_block_advance_ok:
 * @block_size  : the block's declared payload size, as decoded from the
 *                header (attacker-controlled for a downloaded save state).
 * @remaining   : bytes left in the buffer AFTER the 8-byte block header
 *                has been consumed.
 * @out_aligned : written with the 8-byte-aligned advance distance on
 *                success; untouched on rejection. May be NULL.
 *
 * Rounds @block_size up to the 8-byte alignment the rastate format uses,
 * then rejects when that rounding overflows (a block_size near the size_t
 * maximum wraps the alignment add) or when the aligned block would read
 * past the end of the buffer. Returns 1 when the block is safe to consume,
 * 0 when it must be rejected.
 *
 * The inner block dispatchers (core_unserialize, replay_set_serialized_data,
 * rcheevos_set_serialized_data) all read the declared length without their
 * own bounds check, so this up-front reject is the trust boundary against a
 * hostile rastate buffer.
 */
int rastate_block_advance_ok(size_t block_size, size_t remaining,
      size_t *out_aligned);

#ifdef __cplusplus
}
#endif

#endif
