/* Test for an LZMA2 dictionary reset in the middle of a folder.
 *
 * 7-Zip's LZMA2 encoder splits a large stream into blocks and starts
 * each block with a dictionary reset. A reset means later matches may
 * not reach back past it; it does not mean the output starts again.
 * r7z decodes a folder into one buffer the size of the folder, so the
 * write position must carry on across the reset. If it went back to
 * zero, the next block would overwrite the start of the folder and
 * the extract fails. The stable RetroArch.7z on the buildbot resets
 * every 256 MiB, which is how the self-updater found it.
 *
 * The fixture, r7z_dict_reset_fixture.7z, holds data.txt, 270890 B of
 * text in one LZMA2 folder with a 64 KiB dictionary and 128 KiB blocks,
 * so the stream resets its dictionary at 131072 and 262144. Made with
 * 7-Zip 26:
 *
 *   7zz a -m0=LZMA2:d=64k:c=128k r7z_dict_reset_fixture.7z data.txt
 *
 * data.txt is the text make_expected() builds. Both extract paths are
 * checked: r7z_archive_extract() decodes the folder in one call, and
 * r7z_archive_extract_slice() in bounded steps, as the self-updater
 * does.
 *
 * Build: make -C libretro-common/samples/formats/r7z check
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <7z/r7z_archive.h>

#define FIXTURE    "r7z_dict_reset_fixture.7z"
#define NUM_LINES  12000
#define WANT_SIZE  270890

static int failures = 0;
static int checks   = 0;

static void check(int ok, const char *what, const char *detail)
{
   checks++;
   printf("  %-5s %-44s %s\n", ok ? "ok" : "FAIL", what,
         detail ? detail : "");
   if (!ok)
      failures++;
}

static uint8_t *read_file(const char *path, size_t *len)
{
   FILE    *f   = fopen(path, "rb");
   uint8_t *buf = NULL;
   long     n;

   if (!f)
      return NULL;
   if (fseek(f, 0, SEEK_END) == 0 && (n = ftell(f)) > 0
         && fseek(f, 0, SEEK_SET) == 0
         && (buf = (uint8_t *)malloc((size_t)n)))
   {
      if (fread(buf, 1, (size_t)n, f) == (size_t)n)
         *len = (size_t)n;
      else
      {
         free(buf);
         buf = NULL;
      }
   }
   fclose(f);
   return buf;
}

/* The fixture's data.txt: "<word> <word> line <n>\n" for each line. */
static char *make_expected(size_t *len)
{
   static const char *words[] = { "alpha", "bravo", "charlie", "delta",
      "echo", "foxtrot", "golf", "hotel" };
   char  *buf = (char *)malloc(WANT_SIZE + 64);
   size_t pos = 0;
   int    i;

   if (!buf)
      return NULL;
   for (i = 0; i < NUM_LINES && pos < WANT_SIZE; i++)
      pos += (size_t)sprintf(buf + pos, "%s %s line %d\n",
            words[i % 8], words[(i * 5 + 3) % 8], i);
   *len = pos;
   return buf;
}

static void check_output(const char *path, int res, const uint8_t *out,
      size_t len, const char *want, size_t want_len)
{
   char what[64];
   char detail[64];

   sprintf(what, "%s returns R7Z_OK", path);
   sprintf(detail, "got %d", res);
   check(res == R7Z_OK, what, detail);

   sprintf(what, "%s size", path);
   sprintf(detail, "got %lu, want %lu", (unsigned long)len,
         (unsigned long)want_len);
   check(len == want_len, what, detail);

   sprintf(what, "%s bytes match data.txt", path);
   check(out && len == want_len && !memcmp(out, want, want_len), what,
         NULL);
}

int main(void)
{
   r7z_archive_t *a        = NULL;
   uint8_t       *data;
   uint8_t       *out      = NULL;
   char          *want;
   size_t         len      = 0;
   size_t         out_len  = 0;
   size_t         want_len = 0;
   int            res;

   printf("r7z dictionary reset test\n\n");

   if (!(want = make_expected(&want_len)) || want_len != WANT_SIZE)
   {
      printf("expected text is %lu B, want %d\n", (unsigned long)want_len,
            WANT_SIZE);
      free(want);
      return 1;
   }
   if (!(data = read_file(FIXTURE, &len)))
   {
      printf("cannot read %s\n", FIXTURE);
      free(want);
      return 1;
   }

   if ((res = r7z_archive_open(&a, data, len)) != R7Z_OK)
   {
      printf("cannot open %s: %d\n", FIXTURE, res);
      free(data);
      free(want);
      return 1;
   }
   res = r7z_archive_extract(a, 0, &out, &out_len);
   check_output("extract", res, out, out_len, want, want_len);
   free(out);
   out     = NULL;
   out_len = 0;
   r7z_archive_close(a);
   a = NULL;

   /* A fresh archive, so the slice path decodes the folder itself
    * rather than finding it already decoded. */
   if ((res = r7z_archive_open(&a, data, len)) != R7Z_OK)
   {
      printf("cannot reopen %s: %d\n", FIXTURE, res);
      free(data);
      free(want);
      return 1;
   }
   do
      res = r7z_archive_extract_slice(a, 0, &out, &out_len);
   while (res == R7Z_PENDING);
   check_output("extract_slice", res, out, out_len, want, want_len);
   free(out);
   r7z_archive_close(a);

   free(data);
   free(want);

   printf("\n%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
}
