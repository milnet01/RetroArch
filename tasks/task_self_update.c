/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

/* Self-updater tasks: the install-kind facts this platform supplies,
 * the check for a newer stable release, the start-up cleanup and the
 * download-and-apply of the release the check found.  Every decision is made
 * by self_update.c; this file only gathers facts, makes the requests
 * and reports. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#include <7z/r7z_archive.h>
#include <encodings/utf.h>
#include <file/file_path.h>
#include <lrc_hash.h>
#include <net/net_http.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../configuration.h"
#include "../msg_hash.h"
#include "../runloop.h"
#include "../self_update.h"
#include "../verbosity.h"
#include "../version.h"

#include "task_file_transfer.h"
#include "tasks_internal.h"

#define SELF_UPDATE_USER_AGENT "RetroArch/" PACKAGE_VERSION

#if defined(_M_X64) || defined(__x86_64__)
#define SELF_UPDATE_X86_64 true
#else
#define SELF_UPDATE_X86_64 false
#endif

typedef struct self_update_check
{
   const struct self_update_endpoints *ep;
   char url[256];
   char version[32]; /* the release on offer, without the 'v' */
   enum self_update_kind kind;
   bool quiet;       /* the start-up check: report only a newer release */
   bool preferred;   /* url is the small update file, not the full archive */
} self_update_check_t;

/* One check at a time; a second request while one runs is dropped. */
static bool self_update_check_running = false;
/* One update at a time, and no check while one runs. */
static bool self_update_apply_running = false;

/* The release the last menu check found and sized: what Update Now
 * installs. */
static char self_update_offer_version[32];
static bool self_update_offer_preferred = false;

static void self_update_notify(const char *msg)
{
   RARCH_LOG("[Self Update] %s\n", msg);
   runloop_msg_queue_push(msg, strlen(msg), 1, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

static void self_update_check_done(self_update_check_t *state)
{
   free(state);
   self_update_check_running = false;
}

static void self_update_check_failed(self_update_check_t *state,
      const http_transfer_data_t *data, const char *err)
{
   char msg[256];
   char reason[64];

   if (data && data->status > 0)
      snprintf(reason, sizeof(reason), "HTTP %d", data->status);
   else
      strlcpy(reason, (err && *err) ? err : "no reply", sizeof(reason));
   snprintf(msg, sizeof(msg),
         msg_hash_to_str(MSG_SELF_UPDATE_CHECK_FAILED), reason);
   /* The start-up check stays quiet about a failure; nobody asked. */
   if (state->quiet)
      RARCH_WARN("[Self Update] %s\n", msg);
   else
      self_update_notify(msg);
   self_update_check_done(state);
}

static void self_update_report_available(const self_update_check_t *state,
      const http_transfer_data_t *data)
{
   char msg[256];
   char size[32];
   const char *len = data
      ? net_http_header_value(data->headers, "Content-Length") : NULL;

   if (len && *len)
      snprintf(size, sizeof(size), "%.1f MB",
            (double)strtoul(len, NULL, 10) / (1024.0 * 1024.0));
   else
      strlcpy(size, "size unknown", sizeof(size));
   snprintf(msg, sizeof(msg),
         msg_hash_to_str(MSG_SELF_UPDATE_AVAILABLE), state->version, size);
   self_update_notify(msg);
}

static void cb_self_update_head(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   char msg[256];
   self_update_check_t *state = (self_update_check_t*)user_data;
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;

   if (!state)
      return;

   if (data && data->status == 200)
   {
      strlcpy(self_update_offer_version, state->version,
            sizeof(self_update_offer_version));
      self_update_offer_preferred = state->preferred;
      self_update_report_available(state, data);
      self_update_check_done(state);
      return;
   }

   if (data && data->status == 404)
   {
      /* The small file is not published for every release; the full
       * archive is the fallback. */
      if (state->preferred)
      {
         state->preferred = false;
         if (    self_update_file_url(state->url, sizeof(state->url),
                     state->ep, state->kind,
                     state->version, SELF_UPDATE_X86_64, false)
               && task_push_http_transfer_with_user_agent(state->url, true,
                     "HEAD", SELF_UPDATE_USER_AGENT, cb_self_update_head,
                     state))
            return;
      }
      snprintf(msg, sizeof(msg),
            msg_hash_to_str(MSG_SELF_UPDATE_NOT_YET_AVAILABLE),
            state->version);
      self_update_notify(msg);
      self_update_check_done(state);
      return;
   }

   self_update_check_failed(state, data, err);
}

static void cb_self_update_release(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   char msg[256];
   unsigned current[4];
   struct self_update_release rel;
   self_update_check_t *state = (self_update_check_t*)user_data;
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;
   const char *tag;

   if (!state)
      return;

   if (!data || data->status != 200 || !data->data)
   {
      self_update_check_failed(state, data, err);
      return;
   }

   if (     self_update_release_parse(data->data, data->len, &rel) != 0
         || self_update_version_parse(PACKAGE_VERSION, current) != 0)
   {
      self_update_check_failed(state, NULL, "unreadable reply");
      return;
   }

   if (!self_update_release_offer(&rel, current))
   {
      snprintf(msg, sizeof(msg),
            msg_hash_to_str(MSG_SELF_UPDATE_UP_TO_DATE), PACKAGE_VERSION);
      if (state->quiet)
         RARCH_LOG("[Self Update] %s\n", msg);
      else
         self_update_notify(msg);
      self_update_check_done(state);
      return;
   }

   tag = rel.tag;
   if (*tag == 'v')
      tag++;
   strlcpy(state->version, tag, sizeof(state->version));

   /* The start-up check only points the user at the menu entry; it
    * makes no request for the file itself. */
   if (state->quiet)
   {
      snprintf(msg, sizeof(msg),
            msg_hash_to_str(MSG_SELF_UPDATE_AVAILABLE_STARTUP),
            state->version);
      self_update_notify(msg);
      self_update_check_done(state);
      return;
   }

   /* Size the download before anything is offered.  An installer
    * has no preferred file, so the first request is the only one. */
   state->preferred =    state->kind == SELF_UPDATE_KIND_APPIMAGE
                      || state->kind == SELF_UPDATE_KIND_WIN_PORTABLE;
   if (     !self_update_file_url(state->url, sizeof(state->url),
               state->ep, state->kind, state->version,
               SELF_UPDATE_X86_64, state->preferred)
         || !task_push_http_transfer_with_user_agent(state->url, true,
               "HEAD", SELF_UPDATE_USER_AGENT, cb_self_update_head, state))
      self_update_check_failed(state, NULL, "request not started");
}

/* ---- file operations --------------------------------------------- */

/* The real calls behind self_update.c's file-operation table.  A link
 * is listed as a file (POSIX lstat; a Windows reparse point), so the
 * start-up cleanup never follows one out of a staging folder. */

static int su_fs_exists(void *ud, const char *path)
{
   return path_is_valid(path) ? 1 : 0;
}

static int su_fs_mkdir_p(void *ud, const char *path)
{
   return (path_is_directory(path) || path_mkdir(path)) ? 0 : -1;
}

static long su_fs_read(void *ud, const char *path, char *buf, size_t len)
{
   void   *data = NULL;
   int64_t n    = 0;

   if (!filestream_read_file(path, &data, &n) || n < 0)
      return -1;
   if ((uint64_t)n > len)
      n = (int64_t)len;
   memcpy(buf, data, (size_t)n);
   free(data);
   return (long)n;
}

#ifdef _WIN32
static int su_fs_rename(void *ud, const char *from, const char *to)
{
   int      ret   = -1;
   wchar_t *wfrom = utf8_to_utf16_string_alloc(from);
   wchar_t *wto   = utf8_to_utf16_string_alloc(to);

   if (wfrom && wto && MoveFileW(wfrom, wto))
      ret = 0;
   free(wfrom);
   free(wto);
   return ret;
}

static int su_fs_remove(void *ud, const char *path)
{
   int      ret = -1;
   wchar_t *w   = utf8_to_utf16_string_alloc(path);

   if (w && (DeleteFileW(w) || RemoveDirectoryW(w)))
      ret = 0;
   free(w);
   return ret;
}

/* Only the AppImage route keeps permission bits. */
static int su_fs_get_mode(void *ud, const char *path, unsigned *mode)
{
   *mode = 0;
   return 0;
}

static int su_fs_set_mode(void *ud, const char *path, unsigned mode)
{
   return 0;
}

static int su_fs_list(void *ud, const char *dir, self_update_list_cb cb,
      void *ctx)
{
   char             pattern[PATH_MAX_LENGTH];
   wchar_t         *w;
   HANDLE           h;
   WIN32_FIND_DATAW fd;

   fill_pathname_join_special(pattern, dir, "*", sizeof(pattern));
   if (!(w = utf8_to_utf16_string_alloc(pattern)))
      return -1;
   h = FindFirstFileW(w, &fd);
   free(w);
   if (h == INVALID_HANDLE_VALUE)
      return -1;
   do
   {
      char *name;
      bool  is_dir;

      if (     !wcscmp(fd.cFileName, L".")
            || !wcscmp(fd.cFileName, L".."))
         continue;
      if (!(name = utf16_to_utf8_string_alloc(fd.cFileName)))
         continue;
      is_dir =    (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
              && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
      if (!cb(ctx, name, is_dir))
      {
         free(name);
         break;
      }
      free(name);
   } while (FindNextFileW(h, &fd));
   FindClose(h);
   return 0;
}
#else
static int su_fs_rename(void *ud, const char *from, const char *to)
{
   return rename(from, to);
}

static int su_fs_remove(void *ud, const char *path)
{
   return remove(path);
}

static int su_fs_get_mode(void *ud, const char *path, unsigned *mode)
{
   struct stat st;
   if (stat(path, &st) != 0)
      return -1;
   *mode = (unsigned)(st.st_mode & 07777);
   return 0;
}

static int su_fs_set_mode(void *ud, const char *path, unsigned mode)
{
   return chmod(path, (mode_t)mode);
}

static int su_fs_list(void *ud, const char *dir, self_update_list_cb cb,
      void *ctx)
{
   DIR           *d = opendir(dir);
   struct dirent *e;

   if (!d)
      return -1;
   while ((e = readdir(d)))
   {
      char        path[PATH_MAX_LENGTH];
      struct stat st;

      if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
         continue;
      fill_pathname_join_special(path, dir, e->d_name, sizeof(path));
      if (lstat(path, &st) != 0)
         continue;
      if (!cb(ctx, e->d_name, S_ISDIR(st.st_mode)))
         break;
   }
   closedir(d);
   return 0;
}
#endif

static const struct self_update_fileops su_fileops = {
   su_fs_exists, su_fs_rename, su_fs_remove, su_fs_mkdir_p,
   su_fs_get_mode, su_fs_set_mode, su_fs_list, su_fs_read, NULL };

const struct self_update_fileops *task_self_update_fileops(void)
{
   return &su_fileops;
}

/* ---- install kind ------------------------------------------------ */

static enum self_update_kind self_update_kind_detect(void)
{
   struct self_update_env env;
#ifdef _WIN32
   /* Static: gathered once, on the main thread, and kept while the
    * decision reads them. */
   static char exe_dir[PATH_MAX_LENGTH];
   static char uninstaller[PATH_MAX_LENGTH];
   char *uninstall_string = NULL;
   HKEY key;
#else
   const char *appimage   = getenv("APPIMAGE");
#endif
   enum self_update_kind kind;

   memset(&env, 0, sizeof(env));
   env.x86_64 = SELF_UPDATE_X86_64;
#ifdef _WIN32
   env.windows = true;
   if (fill_pathname_application_dir(exe_dir, sizeof(exe_dir)) && *exe_dir)
   {
      env.exe_dir = exe_dir;
      fill_pathname_join_special(uninstaller, exe_dir, "uninstall.exe",
            sizeof(uninstaller));
      env.has_uninstaller = path_is_valid(uninstaller);
   }
   /* The 32-bit NSIS installer writes its key through the 32-bit view,
    * also on 64-bit Windows. */
   if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\RetroArch",
            0, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &key) == ERROR_SUCCESS)
   {
      wchar_t value[PATH_MAX_LENGTH];
      DWORD type;
      DWORD size = sizeof(value) - sizeof(wchar_t);

      if (     RegQueryValueExW(key, L"UninstallString", NULL, &type,
                  (LPBYTE)value, &size) == ERROR_SUCCESS
            && (type == REG_SZ || type == REG_EXPAND_SZ))
      {
         /* A registry string need not carry its terminator. */
         value[size / sizeof(wchar_t)] = L'\0';
         uninstall_string     = utf16_to_utf8_string_alloc(value);
         env.uninstall_string = uninstall_string;
      }
      RegCloseKey(key);
   }
#else
   env.appimage         = appimage;
   env.appimage_is_file =    appimage
                          && path_is_valid(appimage)
                          && !path_is_directory(appimage);
   env.flatpak          = getenv("FLATPAK_ID") != NULL;
   env.snap             = getenv("SNAP")       != NULL;
#endif

   kind = self_update_kind_decide(&env);
#ifdef _WIN32
   free(uninstall_string);
#endif
   return kind;
}

enum self_update_kind task_self_update_kind(void)
{
   static bool decided              = false;
   static enum self_update_kind kind = SELF_UPDATE_KIND_NONE;

   if (!decided)
   {
      kind    = self_update_kind_detect();
      decided = true;
      RARCH_LOG("[Self Update] Install kind: %d.\n", (int)kind);
   }
   return kind;
}

bool task_push_self_update_check(bool quiet)
{
   return task_push_self_update_check_at(&self_update_endpoints_default,
         task_self_update_kind(), quiet);
}

bool task_push_self_update_check_at(const struct self_update_endpoints *ep,
      enum self_update_kind kind, bool quiet)
{
   char url[256];
   self_update_check_t *state;
   settings_t *settings = config_get_ptr();

   /* No request at all from an install the updater cannot replace. */
   if (     kind == SELF_UPDATE_KIND_NONE || self_update_check_running
         || self_update_apply_running)
      return false;

   if (!self_update_tls_allowed(ep, settings->uints.tls_verify_mode))
   {
      self_update_notify(msg_hash_to_str(MSG_SELF_UPDATE_TLS_REQUIRED));
      return false;
   }

   if (!self_update_api_url(url, sizeof(url), ep))
      return false;
   if (!(state = (self_update_check_t*)calloc(1, sizeof(*state))))
      return false;
   state->ep    = ep;
   state->kind  = kind;
   state->quiet = quiet;

   if (!task_push_http_transfer_with_user_agent(url, true, "GET",
            SELF_UPDATE_USER_AGENT, cb_self_update_release, state))
   {
      free(state);
      return false;
   }
   self_update_check_running = true;
   if (!quiet)
      self_update_notify(msg_hash_to_str(MSG_SELF_UPDATE_CHECKING));
   return true;
}

/* ---- staging and start-up ---------------------------------------- */

#define SELF_UPDATE_MARKER "self_update.pending"

/* The staging folder for @kind (spec §4.6): beside retroarch.exe or
 * $APPIMAGE, so the final renames stay on one volume; the installer
 * route's goes under %TEMP%.  Empty when it cannot be worked out. */
static void self_update_staging_dir(char *s, size_t len,
      enum self_update_kind kind)
{
   char dir[PATH_MAX_LENGTH];

   *s   = '\0';
   *dir = '\0';
   switch (kind)
   {
#ifdef _WIN32
      case SELF_UPDATE_KIND_WIN_PORTABLE:
         if (fill_pathname_application_dir(dir, sizeof(dir)) && *dir)
            fill_pathname_join_special(s, dir, ".retroarch-update", len);
         break;
      case SELF_UPDATE_KIND_WIN_INSTALLER:
      case SELF_UPDATE_KIND_WIN_INSTALLER_VISIBLE:
         {
            wchar_t wtmp[MAX_PATH + 1];
            DWORD   n = GetTempPathW(ARRAY_SIZE(wtmp), wtmp);
            char   *tmp;

            if (n == 0 || n > MAX_PATH)
               break;
            if ((tmp = utf16_to_utf8_string_alloc(wtmp)))
            {
               fill_pathname_join_special(s, tmp, "retroarch-update", len);
               free(tmp);
            }
         }
         break;
#else
      case SELF_UPDATE_KIND_APPIMAGE:
         {
            const char *appimage = getenv("APPIMAGE");
            if (appimage && *appimage)
            {
               fill_pathname_basedir(dir, appimage, sizeof(dir));
               fill_pathname_join_special(s, dir, ".retroarch-update", len);
            }
         }
         break;
#endif
      default:
         break;
   }
}

void task_self_update_startup(const char *config_dir)
{
   char marker[PATH_MAX_LENGTH];
   char target[32];
   char msg[256];
   unsigned current[4];
   const char *staging[2];
   size_t count = 0;
   enum self_update_kind kind = task_self_update_kind();
#ifdef _WIN32
   char exe_dir[PATH_MAX_LENGTH];
   char stage_exe[PATH_MAX_LENGTH];
   char stage_tmp[PATH_MAX_LENGTH];
#else
   char stage_app[PATH_MAX_LENGTH];
#endif

   if (kind == SELF_UPDATE_KIND_NONE)
      return;

   /* Leftovers of an earlier update, whichever route it took; anything
    * still in use is left for the next start. */
#ifdef _WIN32
   *exe_dir = '\0';
   fill_pathname_application_dir(exe_dir, sizeof(exe_dir));
   self_update_staging_dir(stage_exe, sizeof(stage_exe),
         SELF_UPDATE_KIND_WIN_PORTABLE);
   self_update_staging_dir(stage_tmp, sizeof(stage_tmp),
         SELF_UPDATE_KIND_WIN_INSTALLER);
   if (*stage_exe)
      staging[count++] = stage_exe;
   if (*stage_tmp)
      staging[count++] = stage_tmp;
   self_update_cleanup(&su_fileops, exe_dir, staging, count);
#else
   self_update_staging_dir(stage_app, sizeof(stage_app), kind);
   if (*stage_app)
      staging[count++] = stage_app;
   self_update_cleanup(&su_fileops, NULL, staging, count);
#endif

   if (     !config_dir || !*config_dir
         || self_update_version_parse(PACKAGE_VERSION, current) != 0)
      return;
   fill_pathname_join_special(marker, config_dir, SELF_UPDATE_MARKER,
         sizeof(marker));
   switch (self_update_marker_consume(&su_fileops, marker, current,
            target, sizeof(target)))
   {
      case SELF_UPDATE_MARKER_UPDATED:
         snprintf(msg, sizeof(msg),
               msg_hash_to_str(MSG_SELF_UPDATE_UPDATED), target);
         self_update_notify(msg);
         break;
      case SELF_UPDATE_MARKER_NOT_FINISHED:
         snprintf(msg, sizeof(msg),
               msg_hash_to_str(MSG_SELF_UPDATE_NOT_FINISHED), target,
               PACKAGE_VERSION);
         self_update_notify(msg);
         break;
      default:
         break;
   }
}

/* ---- apply ------------------------------------------------------- */

/* Download, verify, unpack and install the release the menu check
 * found (spec §4.5, §4.6).  The .sha256 fetch and the download are
 * HTTP tasks chained through their callbacks; everything after the
 * download is one task, a few megabytes or one decode slice per call. */

#define SELF_UPDATE_READ_CHUNK (4 * 1024 * 1024)

enum self_update_stage
{
   SELF_UPDATE_STAGE_READ = 0, /* hash the download; load a .7z whole */
   SELF_UPDATE_STAGE_OPEN,     /* open the .7z, pick the members */
   SELF_UPDATE_STAGE_EXTRACT,  /* one block at a time */
   SELF_UPDATE_STAGE_INSTALL
};

typedef struct self_update_apply
{
   struct sha256_state sha;
   uint64_t archive_size;
   uint64_t read_size;
   RFILE *file;
   uint8_t *archive;          /* the whole .7z, while it is unpacked */
   r7z_archive_t *ar;
   uint32_t *members;         /* entry indices, ordered by block */
   char **rel;                /* each member's path below the payload */
   size_t num_members;
   size_t next;               /* the next member to extract */
   enum self_update_kind kind;
   enum self_update_stage stage;
   uint32_t folder;           /* the block decoded last */
   uint8_t expected[32];
   char url[256];
   char version[32];
   char error[160];
   char staging[PATH_MAX_LENGTH];
   char download[PATH_MAX_LENGTH]; /* the downloaded file in staging */
   char payload[PATH_MAX_LENGTH];  /* where members are extracted */
   char config_dir[PATH_MAX_LENGTH];
   bool has_expected;
   bool has_folder;           /* a block has been decoded */
   bool from_archive;         /* a .7z, not the bare file */
} self_update_apply_t;

static void self_update_apply_free(self_update_apply_t *state)
{
   size_t i;

   if (state->file)
      filestream_close(state->file);
   r7z_archive_close(state->ar);
   free(state->archive);
   for (i = 0; state->rel && i < state->num_members; i++)
      free(state->rel[i]);
   free(state->rel);
   free(state->members);
   free(state);
}

/* Ends a failed update: the staging folder goes, nothing installed has
 * been touched, and the user is told why. */
static void self_update_apply_failed(self_update_apply_t *state,
      const char *reason)
{
   char msg[256];
   const char *staging[1];

   if (state->file)
   {
      filestream_close(state->file);
      state->file = NULL;
   }
   staging[0] = state->staging;
   self_update_cleanup(&su_fileops, NULL, staging, 1);
   snprintf(msg, sizeof(msg),
         msg_hash_to_str(MSG_SELF_UPDATE_FAILED), reason);
   self_update_notify(msg);
   self_update_apply_free(state);
   self_update_apply_running = false;
}

/* Available memory, or 0 where the platform does not say (spec §4.5). */
static uint64_t self_update_memory_available(void)
{
#if defined(_WIN32)
   MEMORYSTATUSEX ms;
   ms.dwLength = sizeof(ms);
   if (!GlobalMemoryStatusEx(&ms))
      return 0;
   /* A 32-bit process runs out of address space before memory. */
   return ms.ullAvailPhys < ms.ullAvailVirtual
      ? ms.ullAvailPhys : ms.ullAvailVirtual;
#elif defined(__linux__)
   char line[128];
   unsigned long long kb = 0;
   FILE *f = fopen("/proc/meminfo", "r");

   if (!f)
      return 0;
   while (fgets(line, sizeof(line), f))
      if (sscanf(line, "MemAvailable: %llu kB", &kb) == 1)
         break;
   fclose(f);
   return (uint64_t)kb * 1024;
#else
   return 0;
#endif
}

/* Picks the members to extract, each with its path below the payload.
 * AppImage: the one member at depth two ending .AppImage.  Windows
 * portable: the program set under the single top-level folder, which
 * must hold retroarch.exe. */
static const char *self_update_pick_members(self_update_apply_t *state)
{
   char name[PATH_MAX_LENGTH];
   char top[PATH_MAX_LENGTH];
   uint32_t i, n = r7z_archive_num_entries(state->ar);
   bool has_exe  = false;

   *top = '\0';
   if (     !(state->members = (uint32_t*)calloc(n ? n : 1, sizeof(uint32_t)))
         || !(state->rel     = (char**)calloc(n ? n : 1, sizeof(char*))))
      return "out of memory";

   for (i = 0; i < n; i++)
   {
      const r7z_entry_t *e = r7z_archive_entry(state->ar, i);
      const char *rel      = NULL;
      const char *slash;

      if (!e || !utf16_to_char_string(e->name, name, sizeof(name)))
         return "unreadable archive";
      slash = strchr(name, '/');

      if (state->kind == SELF_UPDATE_KIND_APPIMAGE)
      {
         if (     !e->is_dir && slash && !strchr(slash + 1, '/')
               && string_ends_with(name, ".AppImage"))
         {
            if (state->num_members)
               return "more than one AppImage in the archive";
            rel = "new.AppImage";
         }
      }
      else
      {
         size_t len = slash ? (size_t)(slash - name) : strlen(name);
         if (!*top)
         {
            if (len == 0 || len >= sizeof(top))
               return "unexpected archive layout";
            memcpy(top, name, len);
            top[len] = '\0';
         }
         else if (len != strlen(top) || strncmp(name, top, len))
            return "unexpected archive layout";
         if (!e->is_dir)
            rel = self_update_program_member(name, top);
         if (rel && string_is_equal_noncase(rel, "retroarch.exe"))
            has_exe = true;
      }

      if (rel)
      {
         if (!(state->rel[state->num_members] = strdup(rel)))
            return "out of memory";
         state->members[state->num_members++] = i;
      }
   }

   if (!state->num_members)
      return "nothing to install in the archive";
   if (state->kind == SELF_UPDATE_KIND_WIN_PORTABLE && !has_exe)
      return "no retroarch.exe in the archive";

   /* Block order, so each block is decoded once.  An empty file has no
    * block and sorts first. */
   for (i = 1; i < state->num_members; i++)
   {
      uint32_t m   = state->members[i];
      char    *r   = state->rel[i];
      const r7z_entry_t *e = r7z_archive_entry(state->ar, m);
      uint64_t key = e->size ? (uint64_t)e->folder + 1 : 0;
      uint32_t j   = i;

      for (; j > 0; j--)
      {
         const r7z_entry_t *p = r7z_archive_entry(state->ar,
               state->members[j - 1]);
         if ((p->size ? (uint64_t)p->folder + 1 : 0) <= key)
            break;
         state->members[j] = state->members[j - 1];
         state->rel[j]     = state->rel[j - 1];
      }
      state->members[j] = m;
      state->rel[j]     = r;
   }
   return NULL;
}

/* Before a block is decoded: reopen the archive if another block's
 * output is still held, then check memory against the block's real
 * figures.  NULL to go on, else the reason to stop. */
static const char *self_update_enter_block(self_update_apply_t *state,
      uint32_t folder, size_t first)
{
   static char reason[128];
   uint64_t unpack, pack, largest = 0, need = 0;
   uint32_t coders;
   size_t i;

   if (state->has_folder)
   {
      /* Closing frees the decoded block; the archive bytes stay. */
      r7z_archive_close(state->ar);
      state->ar = NULL;
      if (r7z_archive_open(&state->ar, state->archive,
               (size_t)state->archive_size) != R7Z_OK)
         return "unreadable archive";
   }

   if (r7z_archive_folder_info(state->ar, folder, &unpack, &pack,
            &coders) != R7Z_OK)
      return "unreadable archive";
   for (i = first; i < state->num_members; i++)
   {
      const r7z_entry_t *e = r7z_archive_entry(state->ar, state->members[i]);
      if (e->folder != folder)
         break;
      if (e->size > largest)
         largest = e->size;
   }

   if (!self_update_memory_ok(self_update_memory_available(),
            state->archive_size, pack, unpack, coders, largest, &need))
   {
      snprintf(reason, sizeof(reason),
            msg_hash_to_str(MSG_SELF_UPDATE_NO_MEMORY),
            (unsigned)((need + 1024 * 1024 - 1) / (1024 * 1024)));
      return reason;
   }

   state->folder     = folder;
   state->has_folder = true;
   return NULL;
}

static bool self_update_write_member(const char *path,
      const uint8_t *data, size_t len)
{
   char dir[PATH_MAX_LENGTH];

   fill_pathname_basedir(dir, path, sizeof(dir));
   if (su_fs_mkdir_p(NULL, dir) != 0)
      return false;
   return filestream_write_file(path, data, (int64_t)len);
}

/* NULL when this call's work is done, else the reason to stop. */
static const char *self_update_apply_step(self_update_apply_t *state,
      bool *finished)
{
   switch (state->stage)
   {
      case SELF_UPDATE_STAGE_READ:
         {
            int64_t want, got;

            if (!state->file)
            {
               int64_t size;
               if (!(state->file = filestream_open(state->download,
                           RETRO_VFS_FILE_ACCESS_READ,
                           RETRO_VFS_FILE_ACCESS_HINT_NONE)))
                  return "cannot read the download";
               if ((size = filestream_get_size(state->file)) <= 0)
                  return "empty download";
               state->archive_size = (uint64_t)size;
               sha256_stream_init(&state->sha, 0);
               if (state->from_archive)
               {
                  if (state->archive_size > (uint64_t)((size_t)-1))
                     return "download too large";
                  if (!(state->archive = (uint8_t*)malloc(
                              (size_t)state->archive_size)))
                     return "out of memory";
               }
            }

            want = (int64_t)(state->archive_size - state->read_size);
            if (want > SELF_UPDATE_READ_CHUNK)
               want = SELF_UPDATE_READ_CHUNK;
            if (state->from_archive)
            {
               uint8_t *dst = state->archive + state->read_size;
               if ((got = filestream_read(state->file, dst, want)) != want)
                  return "cannot read the download";
               sha256_stream_update(&state->sha, dst, (size_t)got);
            }
            else
            {
               static uint8_t buf[64 * 1024];
               if (want > (int64_t)sizeof(buf))
                  want = (int64_t)sizeof(buf);
               if ((got = filestream_read(state->file, buf, want)) != want)
                  return "cannot read the download";
               sha256_stream_update(&state->sha, buf, (size_t)got);
            }
            state->read_size += (uint64_t)got;

            if (state->read_size < state->archive_size)
               return NULL;

            filestream_close(state->file);
            state->file = NULL;
            if (state->has_expected)
            {
               uint8_t digest[32];
               sha256_stream_final(&state->sha, digest);
               if (memcmp(digest, state->expected, sizeof(digest)))
                  return "checksum mismatch";
            }
            state->stage = state->from_archive
               ? SELF_UPDATE_STAGE_OPEN : SELF_UPDATE_STAGE_INSTALL;
         }
         return NULL;

      case SELF_UPDATE_STAGE_OPEN:
         {
            const char *err;
            if (r7z_archive_open(&state->ar, state->archive,
                     (size_t)state->archive_size) != R7Z_OK)
               return "unreadable archive";
            if ((err = self_update_pick_members(state)))
               return err;
            state->stage = SELF_UPDATE_STAGE_EXTRACT;
         }
         return NULL;

      case SELF_UPDATE_STAGE_EXTRACT:
         {
            char path[PATH_MAX_LENGTH];
            uint8_t *out = NULL;
            size_t len   = 0;
            int res;
            uint32_t idx         = state->members[state->next];
            const r7z_entry_t *e = r7z_archive_entry(state->ar, idx);

            if (      e->size
                  && (!state->has_folder || e->folder != state->folder))
            {
               const char *err = self_update_enter_block(state, e->folder,
                     state->next);
               if (err)
                  return err;
            }

            res = r7z_archive_extract_slice(state->ar, idx, &out, &len);
            if (res == R7Z_PENDING)
               return NULL;
            if (res != R7Z_OK)
               return "unreadable archive";

            fill_pathname_join_special(path, state->payload,
                  state->rel[state->next], sizeof(path));
            res = self_update_write_member(path, out, len);
            free(out);
            if (!res)
               return "cannot write the update";

            if (++state->next < state->num_members)
               return NULL;

            /* Everything is on disk; the archive's memory goes before
             * anything installed is touched. */
            r7z_archive_close(state->ar);
            state->ar = NULL;
            free(state->archive);
            state->archive = NULL;
            state->stage   = SELF_UPDATE_STAGE_INSTALL;
         }
         return NULL;

      case SELF_UPDATE_STAGE_INSTALL:
         {
            char marker[PATH_MAX_LENGTH];
            char text[64];
            const char *staging[1];
#ifdef _WIN32
            char exe_dir[PATH_MAX_LENGTH];

            *exe_dir = '\0';
            if (     !fill_pathname_application_dir(exe_dir, sizeof(exe_dir))
                  || !*exe_dir)
               return "cannot find the RetroArch folder";
            if (self_update_replace(&su_fileops, exe_dir, state->payload,
                     (const char *const*)state->rel, state->num_members) != 0)
               return "a program file is in use; nothing was changed";
#else
            char fresh[PATH_MAX_LENGTH];
            const char *appimage = getenv("APPIMAGE");
            int fd;

            if (state->from_archive)
               fill_pathname_join_special(fresh, state->payload,
                     "new.AppImage", sizeof(fresh));
            else
               strlcpy(fresh, state->download, sizeof(fresh));
            /* On disk before it replaces the running AppImage. */
            if ((fd = open(fresh, O_RDONLY)) >= 0)
            {
               fsync(fd);
               close(fd);
            }
            if (     !appimage || !*appimage
                  || self_update_appimage_swap(&su_fileops, fresh,
                     appimage) != 0)
               return "cannot replace the AppImage";
#endif
            fill_pathname_join_special(marker, state->config_dir,
                  SELF_UPDATE_MARKER, sizeof(marker));
            if (     !self_update_marker_format(text, sizeof(text),
                        state->version, state->kind)
                  || !filestream_write_file(marker, text,
                        (int64_t)strlen(text)))
               RARCH_WARN("[Self Update] Could not write %s.\n", marker);

            staging[0] = state->staging;
            self_update_cleanup(&su_fileops, NULL, staging, 1);
            *finished = true;
         }
         return NULL;
   }
   return "internal error";
}

static void self_update_apply_handler(retro_task_t *task)
{
   bool finished = false;
   const char *err;
   self_update_apply_t *state = (self_update_apply_t*)task->state;

   if (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED)
      err = "cancelled";
   else
      err = self_update_apply_step(state, &finished);

   if (err)
   {
      strlcpy(state->error, err, sizeof(state->error));
      finished = true;
   }
   else if (state->stage == SELF_UPDATE_STAGE_READ && state->archive_size)
      task_set_progress(task,
            (int8_t)((state->read_size * 50) / state->archive_size));
   else if (state->stage == SELF_UPDATE_STAGE_EXTRACT && state->num_members)
      task_set_progress(task,
            (int8_t)(50 + (state->next * 50) / state->num_members));

   if (finished)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

/* Runs on the main thread once the task ends. */
static void cb_self_update_applied(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   char msg[256];
   self_update_apply_t *state = (self_update_apply_t*)task->state;

   task->state = NULL;
   if (!state)
      return;
   if (*state->error)
   {
      self_update_apply_failed(state, state->error);
      return;
   }
   snprintf(msg, sizeof(msg),
         msg_hash_to_str(MSG_SELF_UPDATE_INSTALLED), state->version);
   self_update_notify(msg);
   self_update_apply_free(state);
   self_update_apply_running = false;
}

static void cb_self_update_download(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   char reason[64];
   char title[64];
   retro_task_t *install;
   self_update_apply_t *state = (self_update_apply_t*)user_data;
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;

   if (!state)
      return;
   if (!data || data->status < 200 || data->status > 299)
   {
      if (data && data->status > 0)
         snprintf(reason, sizeof(reason), "download: HTTP %d", data->status);
      else
         strlcpy(reason, "download did not finish", sizeof(reason));
      self_update_apply_failed(state, reason);
      return;
   }

   if (!(install = task_init()))
   {
      self_update_apply_failed(state, "out of memory");
      return;
   }
   snprintf(title, sizeof(title),
         msg_hash_to_str(MSG_SELF_UPDATE_INSTALLING), state->version);
   install->handler  = self_update_apply_handler;
   install->callback = cb_self_update_applied;
   install->state    = state;
   install->title    = strdup(title);
   task_queue_push(install);
}

/* Reads the first 64 hex digits of a .sha256 reply. */
static bool self_update_parse_sha256(const char *s, size_t len,
      uint8_t out[32])
{
   size_t i;

   if (!s || len < 64)
      return false;
   for (i = 0; i < 64; i++)
   {
      int c = (unsigned char)s[i];
      int v = (c >= '0' && c <= '9') ? c - '0'
            : (c >= 'a' && c <= 'f') ? c - 'a' + 10
            : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
      if (v < 0)
         return false;
      if (i & 1)
         out[i / 2] |= (uint8_t)v;
      else
         out[i / 2]  = (uint8_t)(v << 4);
   }
   return true;
}

static void cb_self_update_sha256(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   char reason[64];
   char title[64];
   self_update_apply_t *state = (self_update_apply_t*)user_data;
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;

   if (!state)
      return;

   /* Only a 404 means "no checksum published" (D5); any other
    * failure stops the update rather than skipping the check. */
   if (data && data->status == 200)
   {
      if (!self_update_parse_sha256(data->data, data->len, state->expected))
      {
         self_update_apply_failed(state, "unreadable checksum");
         return;
      }
      state->has_expected = true;
   }
   else if (!data || data->status != 404)
   {
      if (data && data->status > 0)
         snprintf(reason, sizeof(reason), "checksum: HTTP %d", data->status);
      else
         strlcpy(reason, "checksum: no reply", sizeof(reason));
      self_update_apply_failed(state, reason);
      return;
   }

   snprintf(title, sizeof(title),
         msg_hash_to_str(MSG_SELF_UPDATE_DOWNLOADING), state->version);
   if (!task_push_http_download_file(state->url, state->download, false,
            title, cb_self_update_download, state))
      self_update_apply_failed(state, "request not started");
}

bool task_push_self_update_apply_at(const struct self_update_endpoints *ep,
      enum self_update_kind kind, const char *version, bool preferred,
      const char *config_dir)
{
   char msg[PATH_MAX_LENGTH + 128];
   char sha_url[sizeof(((self_update_apply_t*)0)->url) + 8];
   self_update_apply_t *state;
   settings_t *settings = config_get_ptr();

   /* The installer route is not built yet. */
   if (     (kind != SELF_UPDATE_KIND_APPIMAGE
            && kind != SELF_UPDATE_KIND_WIN_PORTABLE)
         || !version || !*version || !config_dir
         || self_update_apply_running || self_update_check_running)
      return false;

   if (!self_update_tls_allowed(ep, settings->uints.tls_verify_mode))
   {
      self_update_notify(msg_hash_to_str(MSG_SELF_UPDATE_TLS_REQUIRED));
      return false;
   }

   if (!(state = (self_update_apply_t*)calloc(1, sizeof(*state))))
      return false;
   state->kind         = kind;
   state->from_archive = !(preferred && kind == SELF_UPDATE_KIND_APPIMAGE);
   strlcpy(state->version, version, sizeof(state->version));
   strlcpy(state->config_dir, config_dir, sizeof(state->config_dir));

   self_update_staging_dir(state->staging, sizeof(state->staging), kind);
   if (     !*state->staging
         || !self_update_file_url(state->url, sizeof(state->url), ep, kind,
               version, SELF_UPDATE_X86_64, preferred))
   {
      self_update_apply_free(state);
      return false;
   }
   fill_pathname_join_special(state->download, state->staging,
         path_basename(state->url), sizeof(state->download));
   fill_pathname_join_special(state->payload, state->staging,
         "new", sizeof(state->payload));

   /* A read-only folder stops the update before anything is fetched. */
   if (su_fs_mkdir_p(NULL, state->staging) != 0)
   {
      char dir[PATH_MAX_LENGTH];
      fill_pathname_basedir(dir, state->staging, sizeof(dir));
      snprintf(msg, sizeof(msg),
            msg_hash_to_str(MSG_SELF_UPDATE_NOT_WRITABLE), dir);
      self_update_notify(msg);
      self_update_apply_free(state);
      return false;
   }

   snprintf(sha_url, sizeof(sha_url), "%s.sha256", state->url);
   if (!task_push_http_transfer_with_user_agent(sha_url, true, "GET",
            SELF_UPDATE_USER_AGENT, cb_self_update_sha256, state))
   {
      self_update_apply_failed(state, "request not started");
      return false;
   }
   self_update_apply_running = true;
   return true;
}

bool task_push_self_update_apply(const char *config_dir)
{
   if (!*self_update_offer_version)
      return false;
   return task_push_self_update_apply_at(&self_update_endpoints_default,
         task_self_update_kind(), self_update_offer_version,
         self_update_offer_preferred, config_dir);
}
