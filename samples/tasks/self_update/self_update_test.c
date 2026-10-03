/* Policy lanes for self_update.c (spec docs/private/specs/
 * 2026-10-03-self-updater.md, §5).  Each lane isolates one rule:
 *
 *   kind         INV-1, and which route INV-10 takes
 *   version      INV-3
 *   release      INV-4
 *   tls          INV-5's mode gate and URL prefixes
 *   program_set  INV-8, against a real stable member list
 *   replace      INV-9 and INV-13's rollback
 *   marker       INV-11
 *   appimage     INV-12
 *   cleanup      INV-13's start-up removal
 *
 *   ./self_update_test            every lane
 *   ./self_update_test <lane>     one lane
 *
 * The file lanes run in a scratch folder under the current directory,
 * through a file-operation table backed by the real calls that can be
 * told to fail one chosen rename. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../../../self_update.h"
#include "../../../network/tls_config.h"

static int failures;

static void check(int ok, const char *what)
{
   printf("[%s] %s\n", ok ? "pass" : "FAIL", what);
   if (!ok)
      failures++;
}

/* ---- kind -------------------------------------------------------- */

static enum self_update_kind linux_kind(const char *appimage, bool is_file,
      bool flatpak, bool snap)
{
   struct self_update_env e;
   memset(&e, 0, sizeof(e));
   e.appimage         = appimage;
   e.appimage_is_file = is_file;
   e.flatpak          = flatpak;
   e.snap             = snap;
   e.x86_64           = true;
   return self_update_kind_decide(&e);
}

static enum self_update_kind win_kind(const char *dir, bool uninstaller,
      const char *uninstall_string, bool x86_64)
{
   struct self_update_env e;
   memset(&e, 0, sizeof(e));
   e.windows          = true;
   e.exe_dir          = dir;
   e.has_uninstaller  = uninstaller;
   e.uninstall_string = uninstall_string;
   e.x86_64           = x86_64;
   return self_update_kind_decide(&e);
}

static void lane_kind(void)
{
   check(linux_kind(NULL, false, false, false) == SELF_UPDATE_KIND_NONE,
         "kind: APPIMAGE unset -> NONE");
   check(linux_kind("RetroArch.AppImage", true, false, false) == SELF_UPDATE_KIND_NONE,
         "kind: APPIMAGE relative -> NONE");
   check(linux_kind("/home/u/RetroArch.AppImage", false, false, false) == SELF_UPDATE_KIND_NONE,
         "kind: APPIMAGE names a directory -> NONE");
   check(linux_kind("/home/u/RetroArch.AppImage", true, true, false) == SELF_UPDATE_KIND_NONE,
         "kind: APPIMAGE with FLATPAK_ID -> NONE");
   check(linux_kind("/home/u/RetroArch.AppImage", true, false, true) == SELF_UPDATE_KIND_NONE,
         "kind: APPIMAGE with SNAP -> NONE");
   check(linux_kind("", true, false, false) == SELF_UPDATE_KIND_NONE,
         "kind: APPIMAGE empty -> NONE");
   check(linux_kind("/home/u/RetroArch.AppImage", true, false, false) == SELF_UPDATE_KIND_APPIMAGE,
         "kind: APPIMAGE valid -> APPIMAGE");

   check(win_kind("D:\\Games\\RetroArch", false, NULL, true) == SELF_UPDATE_KIND_WIN_PORTABLE,
         "kind: no uninstall.exe -> WIN_PORTABLE");
   check(win_kind("C:\\RetroArch-Win64", true, NULL, true) == SELF_UPDATE_KIND_WIN_PORTABLE,
         "kind: uninstall.exe with no key -> WIN_PORTABLE");
   check(win_kind("D:\\Copy", true, "C:\\RetroArch-Win64\\uninstall.exe", true)
            == SELF_UPDATE_KIND_WIN_PORTABLE,
         "kind: key naming another folder -> WIN_PORTABLE");
   check(win_kind("C:\\RetroArch-Win64", true, "C:\\RetroArch-Win64\\uninstall.exe", true)
            == SELF_UPDATE_KIND_WIN_INSTALLER,
         "kind: key matching in C:\\RetroArch-Win64 on x86_64 -> WIN_INSTALLER");
   check(win_kind("c:\\retroarch-win64\\", true, "\"C:/RetroArch-Win64/Uninstall.exe\"", true)
            == SELF_UPDATE_KIND_WIN_INSTALLER,
         "kind: matching ignores case, quotes, slash style and a trailing separator");
   check(win_kind("D:\\Emu\\RetroArch", true, "D:\\Emu\\RetroArch\\uninstall.exe", true)
            == SELF_UPDATE_KIND_WIN_INSTALLER_VISIBLE,
         "kind: key matching elsewhere -> WIN_INSTALLER_VISIBLE");
   check(win_kind("C:\\RetroArch-Win64", true, "C:\\RetroArch-Win64\\uninstall.exe", false)
            == SELF_UPDATE_KIND_WIN_INSTALLER_VISIBLE,
         "kind: key matching on x86 -> WIN_INSTALLER_VISIBLE");
   check(win_kind("C:\\RetroArch-Win64x", true, "C:\\RetroArch-Win64x\\uninstall.exe", true)
            == SELF_UPDATE_KIND_WIN_INSTALLER_VISIBLE,
         "kind: a folder that only starts with the silent one is elsewhere");
}

/* ---- version ----------------------------------------------------- */

static int vcmp(const char *a, const char *b)
{
   unsigned x[4], y[4];
   if (self_update_version_parse(a, x) || self_update_version_parse(b, y))
      return 99;
   return self_update_version_cmp(x, y);
}

static void lane_version(void)
{
   static const char *const good[] = { "v1.22.2", "1.22", "1.2.3.4", "65535.0" };
   static const char *const bad[]  = { "1", "vv1.2", "1.2.3.4.5", "1..2", "1.2a",
                                       "-1.2", "", "1.70000", "1.2.", ".1.2",
                                       "+1.2", "1. 2", "V1.2" };
   unsigned v[4];
   size_t i;

   check(vcmp("1.9.0", "1.22.2") == -1, "version: 1.9.0 < 1.22.2");
   check(vcmp("1.22.2", "1.22.2.0") == 0, "version: 1.22.2 = 1.22.2.0");
   check(vcmp("1.16.0", "1.16.0.3") == -1, "version: 1.16.0 < 1.16.0.3");
   check(vcmp("v1.22.2", "1.22.2") == 0, "version: a leading v is ignored");
   check(vcmp("2.0", "1.99.99.99") == 1, "version: 2.0 > 1.99.99.99");

   for (i = 0; i < sizeof(good) / sizeof(good[0]); i++)
   {
      char what[64];
      snprintf(what, sizeof(what), "version: \"%s\" parses", good[i]);
      check(self_update_version_parse(good[i], v) == 0, what);
   }
   check(self_update_version_parse("1.2.3.4", v) == 0
         && v[0] == 1 && v[1] == 2 && v[2] == 3 && v[3] == 4,
         "version: 1.2.3.4 gives its four parts");
   check(self_update_version_parse("1.22", v) == 0 && v[2] == 0 && v[3] == 0,
         "version: missing parts are 0");
   for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
   {
      char what[64];
      snprintf(what, sizeof(what), "version: \"%s\" is rejected", bad[i]);
      check(self_update_version_parse(bad[i], v) != 0, what);
   }
}

/* ---- release ----------------------------------------------------- */

/* Shaped like api.github.com/repos/libretro/RetroArch/releases/latest.
 * The nested objects carry the same key names, so a parser that reads
 * below the top level offers the wrong release. */
#define REL_FMT \
   "{\"url\":\"https://api.github.com/x\",\"author\":{\"login\":\"someone\"," \
   "\"tag_name\":\"v99.0.0\",\"draft\":false,\"prerelease\":false}," \
   "%s\"name\":\"RetroArch\",%s%s" \
   "\"assets\":[{\"name\":\"x\",\"draft\":true,\"uploader\":{\"prerelease\":true}}]," \
   "\"body\":\"notes\"}"

static int offer(const char *tag, const char *draft, const char *pre,
      const char *current, int *parsed)
{
   char json[1024];
   unsigned cur[4];
   struct self_update_release rel;
   snprintf(json, sizeof(json), REL_FMT, tag, draft, pre);
   self_update_version_parse(current, cur);
   *parsed = self_update_release_parse(json, strlen(json), &rel) == 0;
   return *parsed && self_update_release_offer(&rel, cur);
}

static void lane_release(void)
{
   static const char T[] = "\"tag_name\":\"v1.23.0\",";
   static const char D[] = "\"draft\":false,";
   static const char P[] = "\"prerelease\":false,";
   struct self_update_release rel;
   unsigned cur[4];
   int parsed;

   check(offer(T, D, P, "1.22.2", &parsed), "release: the passing release is offered");
   check(!offer(T, "\"draft\":true,", P, "1.22.2", &parsed) && parsed,
         "release: draft true is not offered");
   check(!offer(T, D, "\"prerelease\":true,", "1.22.2", &parsed) && parsed,
         "release: prerelease true is not offered");
   check(!offer("\"tag_name\":\"nightly\",", D, P, "1.22.2", &parsed) && parsed,
         "release: an invalid tag is not offered");
   check(!offer(T, D, P, "1.23.0", &parsed) && parsed,
         "release: an equal version is not offered");
   check(!offer(T, D, P, "1.23.0.1", &parsed) && parsed,
         "release: an older version is not offered");
   check(!offer("", D, P, "1.22.2", &parsed) && parsed,
         "release: no top-level tag_name is not offered");
   check(!offer(T, "", P, "1.22.2", &parsed) && parsed,
         "release: no top-level draft is not offered");
   check(!offer(T, D, "", "1.22.2", &parsed) && parsed,
         "release: no top-level prerelease is not offered");
   check(!offer(T, "\"draft\":\"false\",", P, "1.22.2", &parsed) && parsed,
         "release: a draft that is not a boolean is not offered");

   self_update_version_parse("1.22.2", cur);
   check(self_update_release_parse("{\"tag_name\":\"v1.23.0\",", 22, &rel) != 0,
         "release: truncated JSON does not parse");
   check(self_update_release_parse("[1,2]", 5, &rel) != 0,
         "release: a top-level array does not parse");
   check(self_update_release_parse("", 0, &rel) != 0,
         "release: empty input does not parse");
   {
      static const char min[] =
         "{\"tag_name\":\"v1.23.0\",\"draft\":false,\"prerelease\":false}";
      check(self_update_release_parse(min, sizeof(min) - 1, &rel) == 0
            && self_update_release_offer(&rel, cur),
            "release: the minimal object is offered");
      check(self_update_release_parse(min, sizeof(min) - 3, &rel) != 0,
            "release: the same object cut short does not parse");
   }
}

/* ---- tls --------------------------------------------------------- */

static int starts(const char *s, const char *prefix)
{
   return !strncmp(s, prefix, strlen(prefix));
}

static void lane_tls(void)
{
   static const enum self_update_kind kinds[] = {
      SELF_UPDATE_KIND_APPIMAGE, SELF_UPDATE_KIND_WIN_PORTABLE,
      SELF_UPDATE_KIND_WIN_INSTALLER, SELF_UPDATE_KIND_WIN_INSTALLER_VISIBLE };
   const struct self_update_endpoints *ep = &self_update_endpoints_default;
   char url[512];
   size_t i;
   int all_ok = 1, all_https = 1;

   check(self_update_tls_allowed(ep, TLS_VERIFY_REQUIRED),
         "tls: required -> requests allowed");
   check(!self_update_tls_allowed(ep, TLS_VERIFY_OPTIONAL),
         "tls: optional -> refused");
   check(!self_update_tls_allowed(ep, TLS_VERIFY_DISABLED),
         "tls: disabled -> refused");
   check(!self_update_tls_allowed(ep, 7), "tls: an unknown mode -> refused");

   check(self_update_api_url(url, sizeof(url), ep) > 0
         && !strcmp(url, "https://api.github.com/repos/libretro/RetroArch/releases/latest"),
         "tls: the API URL is the GitHub releases/latest endpoint");

   for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
   {
      int a, p;
      for (a = 0; a < 2; a++)
         for (p = 0; p < 2; p++)
         {
            if (!self_update_file_url(url, sizeof(url), ep, kinds[i],
                     "1.23.0", a != 0, p != 0))
               all_ok = 0;
            else if (!starts(url, "https://buildbot.libretro.com/stable/1.23.0/"))
               all_https = 0;
         }
   }
   check(all_ok, "tls: a URL is built for every kind, arch and preference");
   check(all_ok && all_https, "tls: every file URL starts with https://buildbot.libretro.com/stable/<v>/");
   check(self_update_file_url(url, sizeof(url), ep, SELF_UPDATE_KIND_NONE,
            "1.23.0", true, false) == 0,
         "tls: no URL for NONE");

   self_update_file_url(url, sizeof(url), ep, SELF_UPDATE_KIND_WIN_PORTABLE, "1.23.0", true, true);
   check(!strcmp(url, "https://buildbot.libretro.com/stable/1.23.0/windows/x86_64/RetroArch_update.7z"),
         "tls: portable preferred file");
   self_update_file_url(url, sizeof(url), ep, SELF_UPDATE_KIND_WIN_PORTABLE, "1.23.0", false, false);
   check(!strcmp(url, "https://buildbot.libretro.com/stable/1.23.0/windows/x86/RetroArch.7z"),
         "tls: portable fallback file, x86");
   self_update_file_url(url, sizeof(url), ep, SELF_UPDATE_KIND_WIN_INSTALLER, "1.23.0", true, true);
   check(!strcmp(url, "https://buildbot.libretro.com/stable/1.23.0/windows/x86_64/RetroArch-Win64-setup.exe"),
         "tls: Win64 installer");
   self_update_file_url(url, sizeof(url), ep, SELF_UPDATE_KIND_WIN_INSTALLER_VISIBLE, "1.23.0", false, false);
   check(!strcmp(url, "https://buildbot.libretro.com/stable/1.23.0/windows/x86/RetroArch-Win32-setup.exe"),
         "tls: Win32 installer");
   self_update_file_url(url, sizeof(url), ep, SELF_UPDATE_KIND_APPIMAGE, "1.23.0", true, true);
   check(!strcmp(url, "https://buildbot.libretro.com/stable/1.23.0/linux/x86_64/RetroArch-Linux-x86_64.AppImage"),
         "tls: AppImage preferred file");
   self_update_file_url(url, sizeof(url), ep, SELF_UPDATE_KIND_APPIMAGE, "1.23.0", true, false);
   check(!strcmp(url, "https://buildbot.libretro.com/stable/1.23.0/linux/x86_64/RetroArch.7z"),
         "tls: AppImage fallback file");
   check(self_update_file_url(url, sizeof(url), ep, SELF_UPDATE_KIND_APPIMAGE, "v1.23.0", true, false) == 0
         && self_update_file_url(url, sizeof(url), ep, SELF_UPDATE_KIND_APPIMAGE, "1.23/../x", true, false) == 0,
         "tls: only a bare version reaches the URL");
   check(self_update_file_url(url, 20, ep, SELF_UPDATE_KIND_APPIMAGE, "1.23.0", true, false) == 0,
         "tls: a buffer too small gives no URL rather than a cut one");
}

/* ---- program_set ------------------------------------------------- */

static void lane_program_set(void)
{
   static const char *const hostile[] = {
      "RetroArch-Win64/../evil.dll", "RetroArch-Win64/filters/../../evil.dll",
      "/RetroArch-Win64/retroarch.exe", "RetroArch-Win64\\retroarch.exe",
      "RetroArch-Win64/C:evil.dll", "RetroArch-Win64/filters/..",
      "RetroArch-Win64/filters/a/../../../x.dll", "Other/retroarch.exe",
      "RetroArch-Win64", "RetroArch-Win64/", "RetroArch-Win64-x/a.dll" };
   const char *top = "RetroArch-Win64";
   char line[1024];
   unsigned kept = 0, lines = 0, wrong = 0;
   size_t i;
   int hostile_ok = 1;
   FILE *f = fopen("win64_stable_members.txt", "r");

   check(f != NULL, "program_set: fixture opened");
   if (!f)
      return;
   while (fgets(line, sizeof(line), f))
   {
      const char *rel;
      line[strcspn(line, "\r\n")] = '\0';
      if (line[0] == '#' || !line[0])
         continue;
      lines++;
      rel = self_update_program_member(line, top);
      if (!rel)
         continue;
      kept++;
      if (strstr(line, "/assets/") || strstr(line, "/shaders/")
            || strstr(line, "/saves/") || strstr(line, "/overlays/")
            || strstr(line, ".cfg") || strstr(line, "qt.conf"))
         wrong++;
   }
   fclose(f);
   printf("  %u members, %u kept\n", lines, kept);
   check(kept == 185, "program_set: exactly the 185 program members are kept");
   check(wrong == 0, "program_set: nothing under assets, shaders, saves or overlays, and no config");
   check(self_update_program_member("RetroArch-Win64/retroarch.exe", top)
         && !strcmp(self_update_program_member("RetroArch-Win64/retroarch.exe", top), "retroarch.exe"),
         "program_set: retroarch.exe kept, relative to the top folder");
   check(self_update_program_member("RetroArch-Win64/platforms/qwindows.dll", top) != NULL,
         "program_set: platforms/qwindows.dll kept");
   check(self_update_program_member("RetroArch-Win64/FOO.DLL", top) != NULL,
         "program_set: the extension test ignores case");
   check(!self_update_program_member("RetroArch-Win64/retroarch.default.cfg", top)
         && !self_update_program_member("RetroArch-Win64/retroarch.cfg", top),
         "program_set: retroarch.default.cfg and retroarch.cfg are not kept");
   check(!self_update_program_member("RetroArch-Win64/filters/audio/", top),
         "program_set: a folder entry is not kept");
   check(!self_update_program_member("RetroArch-Win64/cores/x_libretro.dll", top),
         "program_set: a DLL below another folder is not kept");
   for (i = 0; i < sizeof(hostile) / sizeof(hostile[0]); i++)
      if (self_update_program_member(hostile[i], top))
      {
         printf("  kept hostile member %s\n", hostile[i]);
         hostile_ok = 0;
      }
   check(hostile_ok, "program_set: parent, absolute, drive and backslash paths are refused");
}

/* ---- file operations backed by the real calls -------------------- */

struct fs_ud
{
   int renames;     /* renames done so far */
   int fail_rename; /* 1-based rename to fail, 0 for none */
};

static int fs_exists(void *ud, const char *p)
{
   struct stat st;
   (void)ud;
   return lstat(p, &st) == 0;
}

static int fs_rename(void *ud, const char *a, const char *b)
{
   struct fs_ud *u = (struct fs_ud*)ud;
   if (++u->renames == u->fail_rename)
      return -1;
   return rename(a, b);
}

static int fs_remove(void *ud, const char *p) { (void)ud; return remove(p); }

static int fs_mkdir_p(void *ud, const char *p)
{
   char tmp[1024];
   char *s;
   (void)ud;
   snprintf(tmp, sizeof(tmp), "%s", p);
   for (s = tmp + 1; *s; s++)
      if (*s == '/')
      {
         *s = '\0';
         if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
            return -1;
         *s = '/';
      }
   return (mkdir(tmp, 0755) == 0 || errno == EEXIST) ? 0 : -1;
}

static int fs_get_mode(void *ud, const char *p, unsigned *mode)
{
   struct stat st;
   (void)ud;
   if (stat(p, &st) != 0)
      return -1;
   *mode = (unsigned)(st.st_mode & 07777);
   return 0;
}

static int fs_set_mode(void *ud, const char *p, unsigned mode)
{
   (void)ud;
   return chmod(p, (mode_t)mode);
}

static int fs_list(void *ud, const char *dir, self_update_list_cb cb, void *ctx)
{
   DIR *d = opendir(dir);
   struct dirent *e;
   (void)ud;
   if (!d)
      return -1;
   while ((e = readdir(d)))
   {
      char path[1024];
      struct stat st;
      if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
         continue;
      snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
      if (lstat(path, &st) != 0)
         continue;
      if (!cb(ctx, e->d_name, S_ISDIR(st.st_mode)))
         break;
   }
   closedir(d);
   return 0;
}

static long fs_read(void *ud, const char *p, char *buf, size_t len)
{
   FILE *f = fopen(p, "rb");
   size_t n;
   (void)ud;
   if (!f)
      return -1;
   n = fread(buf, 1, len, f);
   fclose(f);
   return (long)n;
}

static struct fs_ud ud;
static const struct self_update_fileops ops = {
   fs_exists, fs_rename, fs_remove, fs_mkdir_p, fs_get_mode, fs_set_mode,
   fs_list, fs_read, &ud };

static char scratch[64];

static void path_in(char *s, size_t len, const char *rel)
{
   snprintf(s, len, "%s/%s", scratch, rel);
}

static void write_file(const char *rel, const char *content)
{
   char p[1024];
   FILE *f;
   char *slash;
   path_in(p, sizeof(p), rel);
   slash = strrchr(p, '/');
   *slash = '\0';
   fs_mkdir_p(NULL, p);
   *slash = '/';
   f = fopen(p, "wb");
   fputs(content, f);
   fclose(f);
}

/* 1 when the file holds exactly @content, 0 when it differs, -1 when
 * it is missing. */
static int file_is(const char *rel, const char *content)
{
   char p[1024], buf[256];
   long n;
   path_in(p, sizeof(p), rel);
   n = fs_read(NULL, p, buf, sizeof(buf) - 1);
   if (n < 0)
      return -1;
   buf[n] = '\0';
   return !strcmp(buf, content);
}

static void rm_rf(const char *p)
{
   char cmd[1200];
   snprintf(cmd, sizeof(cmd), "rm -rf '%s'", p);
   if (system(cmd) != 0)
      fprintf(stderr, "rm -rf %s failed\n", p);
}

static bool count_old_cb(void *ctx, const char *name, bool is_dir)
{
   size_t n = strlen(name);
   (void)is_dir;
   if (n > 14 && !strcmp(name + n - 14, ".retroarch-old"))
      (*(int*)ctx)++;
   return true;
}

static int olds_in(const char *rel)
{
   char p[1024];
   int n = 0;
   path_in(p, sizeof(p), rel);
   fs_list(NULL, p, count_old_cb, &n);
   return n;
}

/* ---- replace ----------------------------------------------------- */

static const char *const set[] = {
   "retroarch.exe", "SDL2.dll", "filters/audio/Echo.dsp",
   "filters/new/Added.filt", "zlib1.dll" };
#define SET_N (sizeof(set) / sizeof(set[0]))

static void setup_replace(void)
{
   size_t i;
   rm_rf(scratch);
   mkdir(scratch, 0755);
   for (i = 0; i < SET_N; i++)
   {
      char rel[256];
      /* filters/new/ is new in the update: nothing old to set aside. */
      if (strncmp(set[i], "filters/new/", 12))
         write_file(set[i], "old");
      snprintf(rel, sizeof(rel), "stage/%s", set[i]);
      write_file(rel, "new");
   }
   write_file("retroarch.cfg", "mine");
}

static int all_old(void)
{
   size_t i;
   for (i = 0; i < SET_N; i++)
   {
      int want_missing = !strncmp(set[i], "filters/new/", 12);
      int r = file_is(set[i], "old");
      if (want_missing ? r != -1 : r != 1)
         return 0;
   }
   return olds_in(".") == 0 && olds_in("filters/audio") == 0
       && file_is("retroarch.cfg", "mine") == 1;
}

static void lane_replace(void)
{
   char dst[256], stage[256];
   size_t i;
   int all_new = 1;
   /* Renames per file: one to set the old one aside (when it exists),
    * one to move the new one in.  Fail on the first, a middle and the
    * last rename. */
   static const int fail_at[] = { 1, 4, 9 };

   path_in(dst, sizeof(dst), "");
   dst[strlen(dst) - 1] = '\0';
   path_in(stage, sizeof(stage), "stage");

   setup_replace();
   ud.renames = 0;
   ud.fail_rename = 0;
   check(self_update_replace(&ops, dst, stage, set, SET_N) == 0,
         "replace: every file replaced");
   for (i = 0; i < SET_N; i++)
      if (file_is(set[i], "new") != 1)
         all_new = 0;
   check(all_new, "replace: each path holds the new file");
   check(olds_in(".") == 3 && olds_in("filters/audio") == 1,
         "replace: each old file was set aside as .retroarch-old");
   check(file_is("retroarch.cfg", "mine") == 1, "replace: retroarch.cfg untouched");

   for (i = 0; i < sizeof(fail_at) / sizeof(fail_at[0]); i++)
   {
      char what[96];
      setup_replace();
      ud.renames = 0;
      ud.fail_rename = fail_at[i];
      snprintf(what, sizeof(what), "replace: rename %d fails -> error", fail_at[i]);
      check(self_update_replace(&ops, dst, stage, set, SET_N) != 0, what);
      snprintf(what, sizeof(what),
            "replace: rename %d fails -> every original back, no .retroarch-old", fail_at[i]);
      check(all_old(), what);
   }
   ud.fail_rename = 0;

   /* A stale .retroarch-old from an earlier update is replaced. */
   setup_replace();
   write_file("SDL2.dll.retroarch-old", "stale");
   check(self_update_replace(&ops, dst, stage, set, SET_N) == 0
         && file_is("SDL2.dll", "new") == 1 && file_is("SDL2.dll.retroarch-old", "old") == 1,
         "replace: a stale .retroarch-old is replaced by the current old file");
   rm_rf(scratch);
}

/* ---- marker ------------------------------------------------------ */

static void lane_marker(void)
{
   static const struct { const char *target; enum self_update_marker_result want; const char *what; }
   rows[] = {
      { "1.22.1", SELF_UPDATE_MARKER_UPDATED,      "marker: target below running -> updated" },
      { "1.22.2", SELF_UPDATE_MARKER_UPDATED,      "marker: target equal to running -> updated" },
      { "1.23.0", SELF_UPDATE_MARKER_NOT_FINISHED, "marker: target above running -> not finished" } };
   char marker[256], buf[128], target[32];
   unsigned cur[4];
   size_t i;

   rm_rf(scratch);
   mkdir(scratch, 0755);
   path_in(marker, sizeof(marker), "self_update.pending");
   self_update_version_parse("1.22.2", cur);

   for (i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
   {
      char what[128];
      self_update_marker_format(buf, sizeof(buf), rows[i].target, SELF_UPDATE_KIND_WIN_INSTALLER);
      write_file("self_update.pending", buf);
      target[0] = '\0';
      check(self_update_marker_consume(&ops, marker, cur, target, sizeof(target)) == rows[i].want
            && !strcmp(target, rows[i].target), rows[i].what);
      snprintf(what, sizeof(what), "%s, and the marker is gone", rows[i].what);
      check(!fs_exists(NULL, marker), what);
   }

   check(self_update_marker_format(buf, sizeof(buf), "1.23.0", SELF_UPDATE_KIND_APPIMAGE) > 0
         && !strncmp(buf, "1.23.0\n", 7), "marker: first line is the target version");
   check(self_update_marker_consume(&ops, marker, cur, target, sizeof(target))
         == SELF_UPDATE_MARKER_INVALID, "marker: no marker -> invalid");
   write_file("self_update.pending", "garbage");
   check(self_update_marker_consume(&ops, marker, cur, target, sizeof(target))
         == SELF_UPDATE_MARKER_INVALID && !fs_exists(NULL, marker),
         "marker: an unparsable marker is invalid and removed");
   rm_rf(scratch);
}

/* ---- appimage ---------------------------------------------------- */

static void lane_appimage(void)
{
   char app[256], fresh[256];
   unsigned mode = 0;

   rm_rf(scratch);
   mkdir(scratch, 0755);
   write_file("RetroArch.AppImage", "old build");
   write_file(".retroarch-update/new.AppImage", "new build");
   path_in(app, sizeof(app), "RetroArch.AppImage");
   path_in(fresh, sizeof(fresh), ".retroarch-update/new.AppImage");
   chmod(app, 0750);
   chmod(fresh, 0600);

   check(self_update_appimage_swap(&ops, fresh, app) == 0, "appimage: swap succeeds");
   check(file_is("RetroArch.AppImage", "new build") == 1, "appimage: the path holds the new bytes");
   check(fs_get_mode(NULL, app, &mode) == 0 && mode == 0750, "appimage: mode 0750 kept");
   check(!fs_exists(NULL, fresh), "appimage: the staged file was renamed, not copied");
   check(!strcmp(self_update_restart_target(SELF_UPDATE_KIND_APPIMAGE, app, "/tmp/.mount_x/usr/bin/retroarch"), app),
         "appimage: restart target is $APPIMAGE, not the mounted binary");
   check(!strcmp(self_update_restart_target(SELF_UPDATE_KIND_WIN_PORTABLE, NULL, "C:\\RA\\retroarch.exe"),
            "C:\\RA\\retroarch.exe"),
         "appimage: on Windows the restart target is the captured exe path");

   write_file("RetroArch.AppImage", "old build");
   check(self_update_appimage_swap(&ops, fresh, app) != 0
         && file_is("RetroArch.AppImage", "old build") == 1,
         "appimage: a missing staged file fails and leaves the AppImage alone");
   rm_rf(scratch);
}

/* ---- cleanup ----------------------------------------------------- */

static void lane_cleanup(void)
{
   char exe_dir[256], s1[256], s2[256], s3[256];
   const char *staging[4];

   rm_rf(scratch);
   mkdir(scratch, 0755);
   path_in(exe_dir, sizeof(exe_dir), "ra");
   path_in(s1, sizeof(s1), "ra/.retroarch-update");
   path_in(s2, sizeof(s2), "temp/retroarch-update");
   path_in(s3, sizeof(s3), "apps/.retroarch-update");
   staging[0] = s1;
   staging[1] = s2;
   staging[2] = NULL;
   staging[3] = s3;

   write_file("ra/retroarch.exe", "x");
   write_file("ra/retroarch.exe.retroarch-old", "x");
   write_file("ra/SDL2.dll.retroarch-old", "x");
   write_file("ra/filters/audio/Echo.dsp.retroarch-old", "x");
   write_file("ra/platforms/qwindows.dll.retroarch-old", "x");
   write_file("ra/saves/game.srm.retroarch-old", "user");
   write_file("ra/.retroarch-update/RetroArch.7z", "x");
   write_file("ra/.retroarch-update/RetroArch-Win64/filters/a/b.dll", "x");
   write_file("temp/retroarch-update/setup.exe", "x");
   write_file("temp/retroarch-update/run.cmd", "x");
   write_file("apps/.retroarch-update/new.AppImage", "x");
   write_file("apps/RetroArch.AppImage", "x");

   self_update_cleanup(&ops, exe_dir, staging, 4);

   check(olds_in("ra") == 0 && olds_in("ra/filters/audio") == 0
         && olds_in("ra/platforms") == 0,
         "cleanup: .retroarch-old files removed from the folder, filters and platforms");
   check(file_is("ra/saves/game.srm.retroarch-old", "user") == 1,
         "cleanup: nothing is touched outside the program folders");
   check(file_is("ra/retroarch.exe", "x") == 1 && file_is("apps/RetroArch.AppImage", "x") == 1,
         "cleanup: the program itself stays");
   check(!fs_exists(NULL, s1), "cleanup: the portable staging folder is gone");
   check(!fs_exists(NULL, s2), "cleanup: the installer staging folder is gone");
   check(!fs_exists(NULL, s3), "cleanup: the AppImage staging folder is gone");

   self_update_cleanup(&ops, exe_dir, staging, 4);
   check(1, "cleanup: a second run with nothing to do returns");
   rm_rf(scratch);
}

/* ---- main -------------------------------------------------------- */

static const struct { const char *name; void (*fn)(void); } lanes[] = {
   { "kind", lane_kind }, { "version", lane_version }, { "release", lane_release },
   { "tls", lane_tls }, { "program_set", lane_program_set }, { "replace", lane_replace },
   { "marker", lane_marker }, { "appimage", lane_appimage }, { "cleanup", lane_cleanup } };

int main(int argc, char **argv)
{
   size_t i;
   int ran = 0;

   snprintf(scratch, sizeof(scratch), "su_scratch_%ld", (long)getpid());
   for (i = 0; i < sizeof(lanes) / sizeof(lanes[0]); i++)
      if (argc < 2 || !strcmp(argv[1], lanes[i].name))
      {
         lanes[i].fn();
         ran++;
      }
   if (!ran)
   {
      fprintf(stderr, "unknown lane %s\n", argv[1]);
      return 2;
   }
   if (failures)
   {
      printf("%d check(s) failed\n", failures);
      return 1;
   }
   puts("self_update: all checks passed");
   return 0;
}
