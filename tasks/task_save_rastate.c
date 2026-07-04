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

#include "task_save_rastate.h"

/* Mirrors CONTENT_ALIGN_SIZE in task_save.c: round up to 8 bytes. */
#define RASTATE_ALIGN_SIZE(size) ((((size) + 7) & ~((size_t)7)))

unsigned long rastate_read_le32(const unsigned char *p)
{
   return   ((unsigned long)p[0])
          | ((unsigned long)p[1] << 8)
          | ((unsigned long)p[2] << 16)
          | ((unsigned long)p[3] << 24);
}

int rastate_block_advance_ok(size_t block_size, size_t remaining,
      size_t *out_aligned)
{
   size_t aligned = RASTATE_ALIGN_SIZE(block_size);

   if (aligned < block_size)   /* alignment arithmetic overflow */
      return 0;
   if (aligned > remaining)    /* block extends past the buffer */
      return 0;

   if (out_aligned)
      *out_aligned = aligned;
   return 1;
}
