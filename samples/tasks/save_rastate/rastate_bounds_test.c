/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rastate_bounds_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Regression for commit 1f59d9d31d (save state + replay format bounds), the
 * rastate block-size trust boundary in
 * tasks/task_save.c::content_load_rastate1().
 *
 * A save state shared online (ROM-hack save, speedrun replay archive) can
 * declare a per-block size that overflows the 8-byte alignment add or reads
 * past the end of the buffer; the inner MEM/ACHV/RPLY dispatchers then read
 * the attacker-chosen length. The guard rounds the size up to 8 and rejects
 * an overflow or a block that runs off the end. The decode also widens each
 * byte to unsigned long before shifting, avoiding signed-promotion UB on the
 * high byte.
 *
 * Like samples/tasks/cloudsync (and unlike the archive/http copy-oracle
 * tests), this test #includes the REAL predicate source
 * (tasks/task_save_rastate.c), extracted into its own dependency-free
 * translation unit so the shipped parser and this test exercise the SAME
 * functions. A re-sync or refactor that reverts the bounds arithmetic fails
 * this test rather than drifting from a stale copy.
 *
 * Build standalone:
 *   cc -Wall -pedantic -std=gnu99 -g -O0 -o rastate_bounds_test \
 *      rastate_bounds_test.c
 *   ./rastate_bounds_test
 */

#include <stdio.h>
#include <stddef.h>

/* The real shipped predicates -- not a copy. Dependency-free, so they
 * compile here with no libretro-common include path. */
#include "../../../tasks/task_save_rastate.c"

static int failures = 0;

static void expect_le32(const char *label,
      unsigned char b0, unsigned char b1,
      unsigned char b2, unsigned char b3,
      unsigned long want)
{
   unsigned char  buf[4];
   unsigned long  got;

   buf[0] = b0; buf[1] = b1; buf[2] = b2; buf[3] = b3;
   got    = rastate_read_le32(buf);

   if (got != want)
   {
      printf("[FAILED]  le32 %-18s  expected %lu, got %lu\n",
            label, want, got);
      failures++;
      return;
   }
   printf("[SUCCESS] le32 %-18s  = %lu\n", label, got);
}

/* want_ok: the block is expected to be accepted, and its aligned advance
 * distance is expected to equal want_aligned.
 * !want_ok: the block must be rejected (aligned untouched). */
static void expect_block(const char *label, size_t block_size,
      size_t remaining, int want_ok, size_t want_aligned)
{
   size_t aligned = (size_t)-1;   /* sentinel: must stay untouched on reject */
   int    ok      = rastate_block_advance_ok(block_size, remaining, &aligned);

   if (ok != want_ok)
   {
      printf("[FAILED]  block %-28s  expected %s, got %s\n",
            label,
            want_ok ? "accept" : "REJECT",
            ok      ? "accept" : "REJECT");
      failures++;
      return;
   }
   if (ok && aligned != want_aligned)
   {
      printf("[FAILED]  block %-28s  aligned expected %lu, got %lu\n",
            label, (unsigned long)want_aligned, (unsigned long)aligned);
      failures++;
      return;
   }
   printf("[SUCCESS] block %-28s  %s\n",
         label, want_ok ? "accepted" : "correctly rejected");
}

int main(void)
{
   /* --- little-endian decode --- */
   expect_le32("zero",       0x00, 0x00, 0x00, 0x00, 0UL);
   expect_le32("one",        0x01, 0x00, 0x00, 0x00, 1UL);
   expect_le32("byte1",      0x00, 0x01, 0x00, 0x00, 256UL);
   expect_le32("mixed",      0x78, 0x56, 0x34, 0x12, 0x12345678UL);
   /* top bit of the high byte set: the case that is UB when the bytes are
    * promoted to signed int and shifted. Must decode cleanly. */
   expect_le32("high-bit",   0x00, 0x00, 0x00, 0x80, 0x80000000UL);
   expect_le32("all-ones",   0xFF, 0xFF, 0xFF, 0xFF, 0xFFFFFFFFUL);

   /* --- block advance: accepted, aligned up to 8 --- */
   expect_block("zero at start",       0,   100, 1, 0);
   expect_block("1 -> 8",              1,   100, 1, 8);
   expect_block("8 -> 8",              8,   100, 1, 8);
   expect_block("9 -> 16",             9,   100, 1, 16);
   expect_block("96 fits in 100",      96,  100, 1, 96);
   expect_block("100 -> 104 fits 104", 100, 104, 1, 104);
   expect_block("101 -> 104 fits 104", 101, 104, 1, 104);
   expect_block("exact end zero",      0,   0,   1, 0);

   /* --- block advance: rejected --- */
   /* 100 rounds up to 104 which runs one alignment step past a 100-byte
    * buffer -- the off-by-alignment case a naive `block_size <= remaining`
    * check would wrongly accept. */
   expect_block("100 -> 104 past 100", 100, 100, 0, 0);
   expect_block("105 -> 112 past 104", 105, 104, 0, 0);
   expect_block("1 past empty buffer", 1,   0,   0, 0);
   expect_block("max uint32 past buf", 0xFFFFFFFFUL, 64, 0, 0);
   /* Alignment-add overflow branch: only reachable when block_size is near
    * the size_t maximum (which a 32-bit-size_t platform can hit from a
    * large uint32 decode). SIZE_MAX + 7 wraps, so the aligned value comes
    * out smaller than block_size and the overflow guard must reject. */
   expect_block("size_t max overflow", (size_t)-1, (size_t)-1, 0, 0);

   if (failures)
   {
      printf("\n%d test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll rastate block-bounds regression tests passed.\n");
   return 0;
}
