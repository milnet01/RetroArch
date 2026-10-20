/* Test for r7z_archive_folder_info().
 *
 * A caller about to extract a member needs to know what decoding its
 * folder will cost before it starts, since the whole folder is decoded
 * at once. The figures that decide it are the folder's unpacked size,
 * the sum of its packed streams and how many coders it chains, all of
 * which the header records.
 *
 * The fixture, r7z_folder_info_fixture.7z, holds two folders:
 *
 *   - folder 0: readme.txt, one LZMA2 coder, packed 7820 B, unpacked
 *     71439 B;
 *   - folder 1: prog.exe, BCJ2 over LZMA2 and two LZMA coders, four
 *     coders and four packed streams totalling 20600 B, unpacked
 *     40000 B.
 *
 * The BCJ2 folder catches a packed size taken from one stream rather
 * than the sum, and a coder count of one. Two folders of different
 * sizes catch one folder reporting another's figures.
 *
 * Made with 7-Zip 26 from a 40000 B stand-in prog.exe (x86 call and
 * jump opcodes, so BCJ2's side streams are not empty) and a text
 * readme.txt:
 *
 *   7zz a -mf=BCJ2 r7z_folder_info_fixture.7z prog.exe
 *   7zz a -mf=off  r7z_folder_info_fixture.7z readme.txt
 *
 * The expected figures are from `7zz l -slt` of the fixture: its
 * Size and Packed Size per member, Method for the coders, Block for
 * the index.
 *
 * Build: make -C libretro-common/samples/formats/r7z check
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <7z/r7z_archive.h>

#define FIXTURE "r7z_folder_info_fixture.7z"

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

static void check_folder(const r7z_archive_t *a, uint32_t folder,
      uint64_t want_unpack, uint64_t want_pack, uint32_t want_coders)
{
   uint64_t unpack = 0;
   uint64_t pack   = 0;
   uint32_t coders = 0;
   int      res    = r7z_archive_folder_info(a, folder,
         &unpack, &pack, &coders);
   char     what[64];
   char     detail[128];

   sprintf(what, "folder %u returns R7Z_OK", (unsigned)folder);
   sprintf(detail, "got %d", res);
   check(res == R7Z_OK, what, detail);

   sprintf(what, "folder %u unpacked size", (unsigned)folder);
   sprintf(detail, "got %llu, want %llu", (unsigned long long)unpack,
         (unsigned long long)want_unpack);
   check(unpack == want_unpack, what, detail);

   sprintf(what, "folder %u packed size", (unsigned)folder);
   sprintf(detail, "got %llu, want %llu", (unsigned long long)pack,
         (unsigned long long)want_pack);
   check(pack == want_pack, what, detail);

   sprintf(what, "folder %u coder count", (unsigned)folder);
   sprintf(detail, "got %u, want %u", (unsigned)coders,
         (unsigned)want_coders);
   check(coders == want_coders, what, detail);
}

int main(void)
{
   r7z_archive_t *a    = NULL;
   uint8_t       *data;
   size_t         len  = 0;
   uint64_t       unpack, pack;
   uint32_t       coders;
   char           detail[64];
   int            res;

   printf("r7z folder info test\n\n");

   if (!(data = read_file(FIXTURE, &len)))
   {
      printf("cannot read %s\n", FIXTURE);
      return 1;
   }
   if ((res = r7z_archive_open(&a, data, len)) != R7Z_OK)
   {
      printf("cannot open %s: %d\n", FIXTURE, res);
      free(data);
      return 1;
   }

   check_folder(a, 0, 71439, 7820,  1);
   check_folder(a, 1, 40000, 20600, 4);

   res = r7z_archive_folder_info(a, 2, &unpack, &pack, &coders);
   sprintf(detail, "got %d", res);
   check(res == R7Z_ERROR_PARAM, "folder past the last is R7Z_ERROR_PARAM",
         detail);

   res = r7z_archive_folder_info(NULL, 0, &unpack, &pack, &coders);
   sprintf(detail, "got %d", res);
   check(res == R7Z_ERROR_PARAM, "NULL archive is R7Z_ERROR_PARAM", detail);

   r7z_archive_close(a);
   free(data);

   printf("\n%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
}
