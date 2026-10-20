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
 * and the check for a newer stable release.  Every decision is made
 * by self_update.c; this file only gathers facts, makes the requests
 * and reports. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <encodings/utf.h>
#endif

#include <file/file_path.h>
#include <net/net_http.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#include "../configuration.h"
#include "../msg_hash.h"
#include "../runloop.h"
#include "../self_update.h"
#include "../verbosity.h"
#include "../version.h"

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
   if (kind == SELF_UPDATE_KIND_NONE || self_update_check_running)
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
