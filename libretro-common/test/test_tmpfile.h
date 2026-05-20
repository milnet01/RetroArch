/* Shared temp-file helper for the libretro-common unit tests.
 *
 * Replaces tmpnam(): mkstemp() on POSIX, GetTempFileName() on Windows.
 * tmpnam() carries a CWE-377 create-then-open TOCTOU race and trips a
 * glibc "dangerous, better use mkstemp" link-time warning. The tests need
 * both an open write stream *and* the on-disk path (so the system-under-test
 * can reopen the file by name), so the helper returns the FILE* and writes
 * the chosen path into the caller's buffer. Returns NULL on failure.
 */

#ifndef LIBRETRO_COMMON_TEST_TMPFILE_H
#define LIBRETRO_COMMON_TEST_TMPFILE_H

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>

static FILE *test_tmpfile_open(char *path, size_t path_len)
{
   char dir[MAX_PATH];
   DWORD n;

   /* GetTempFileNameA requires the output buffer to be at least MAX_PATH;
    * every caller passes a >= MAX_PATH buffer, so path_len is unused here. */
   (void)path_len;

   n = GetTempPathA((DWORD)sizeof(dir), dir);
   if (n == 0 || n > sizeof(dir))
      return NULL;
   if (GetTempFileNameA(dir, "rat", 0, path) == 0)
      return NULL;

   return fopen(path, "wb");
}
#else
#include <unistd.h>

static FILE *test_tmpfile_open(char *path, size_t path_len)
{
   int fd;
   FILE *stream;
   const char *dir = getenv("TMPDIR");

   if (!dir || !*dir)
      dir = "/tmp";

   snprintf(path, path_len, "%s/retroarch_test_XXXXXX", dir);

   if ((fd = mkstemp(path)) < 0)
      return NULL;

   if (!(stream = fdopen(fd, "wb")))
   {
      close(fd);
      remove(path);
      return NULL;
   }

   return stream;
}
#endif

#endif
