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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <formats/rjson.h>
#include <retro_miscellaneous.h>

#include "self_update.h"
#include "network/tls_config.h"

#define SELF_UPDATE_OLD_SUFFIX ".retroarch-old"

const struct self_update_endpoints self_update_endpoints_default =
{
   "https://api.github.com/",
   "https://buildbot.libretro.com/",
   true
};

/* ---- install kind ------------------------------------------------ */

static int su_lower(int c)
{
   return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
}

static bool su_is_sep(char c)
{
   return c == '/' || c == '\\';
}

/* Windows path equality: case-insensitive, either separator, trailing
 * separators ignored. */
static bool su_win_path_eq(const char *a, size_t a_len,
      const char *b, size_t b_len)
{
   size_t i;
   while (a_len && su_is_sep(a[a_len - 1]))
      a_len--;
   while (b_len && su_is_sep(b[b_len - 1]))
      b_len--;
   if (!a_len || a_len != b_len)
      return false;
   for (i = 0; i < a_len; i++)
   {
      if (su_is_sep(a[i]) && su_is_sep(b[i]))
         continue;
      if (su_lower((unsigned char)a[i]) != su_lower((unsigned char)b[i]))
         return false;
   }
   return true;
}

/* Does @uninstall (optionally quoted) name <dir>\uninstall.exe? */
static bool su_uninstall_names(const char *uninstall, const char *dir)
{
   static const char name[] = "uninstall.exe";
   size_t name_len          = sizeof(name) - 1;
   size_t len               = strlen(uninstall);
   size_t i;

   if (len >= 2 && uninstall[0] == '"' && uninstall[len - 1] == '"')
   {
      uninstall++;
      len -= 2;
   }
   if (len <= name_len || !su_is_sep(uninstall[len - name_len - 1]))
      return false;
   for (i = 0; i < name_len; i++)
      if (su_lower((unsigned char)uninstall[len - name_len + i]) != name[i])
         return false;
   return su_win_path_eq(uninstall, len - name_len - 1, dir, strlen(dir));
}

enum self_update_kind self_update_kind_decide(const struct self_update_env *env)
{
   if (!env)
      return SELF_UPDATE_KIND_NONE;

   if (env->windows)
   {
      if (!env->exe_dir || !*env->exe_dir)
         return SELF_UPDATE_KIND_NONE;
      /* uninstall.exe alone is not enough: a copied installer folder
       * carries it too.  The key must name this folder's copy. */
      if (     !env->has_uninstaller
            || !env->uninstall_string
            || !su_uninstall_names(env->uninstall_string, env->exe_dir))
         return SELF_UPDATE_KIND_WIN_PORTABLE;
      if (env->x86_64 && su_win_path_eq(env->exe_dir, strlen(env->exe_dir),
               SELF_UPDATE_WIN64_SILENT_DIR,
               sizeof(SELF_UPDATE_WIN64_SILENT_DIR) - 1))
         return SELF_UPDATE_KIND_WIN_INSTALLER;
      return SELF_UPDATE_KIND_WIN_INSTALLER_VISIBLE;
   }

   /* Positive detection: only the AppImage runtime sets APPIMAGE.  A
    * Flatpak or Snap that inherited it from its launcher is not one. */
   if (     !env->appimage
         || env->appimage[0] != '/'
         || !env->appimage_is_file
         || env->flatpak
         || env->snap)
      return SELF_UPDATE_KIND_NONE;
   return SELF_UPDATE_KIND_APPIMAGE;
}

/* ---- versions ---------------------------------------------------- */

int self_update_version_parse(const char *s, unsigned out[4])
{
   unsigned parts = 0;

   out[0] = out[1] = out[2] = out[3] = 0;
   if (!s)
      return -1;
   if (*s == 'v')
      s++;
   for (;;)
   {
      unsigned long v   = 0;
      const char *start = s;
      while (*s >= '0' && *s <= '9')
      {
         v = v * 10 + (unsigned long)(*s - '0');
         if (v > 65535)
            return -1;
         s++;
      }
      if (s == start || parts == 4)
         return -1;
      out[parts++] = (unsigned)v;
      if (!*s)
         break;
      if (*s != '.')
         return -1;
      s++;
   }
   if (parts < 2)
   {
      out[0] = 0;
      return -1;
   }
   return 0;
}

int self_update_version_cmp(const unsigned a[4], const unsigned b[4])
{
   int i;
   for (i = 0; i < 4; i++)
   {
      if (a[i] < b[i])
         return -1;
      if (a[i] > b[i])
         return 1;
   }
   return 0;
}

/* ---- release ----------------------------------------------------- */

enum su_key
{
   SU_KEY_NONE = 0,
   SU_KEY_TAG,
   SU_KEY_DRAFT,
   SU_KEY_PRERELEASE
};

int self_update_release_parse(const char *json, size_t len,
      struct self_update_release *out)
{
   rjson_t *j;
   enum su_key key = SU_KEY_NONE;
   int ret         = -1;

   memset(out, 0, sizeof(*out));
   if (!json || !len || !(j = rjson_open_buffer(json, len)))
      return -1;
   if (rjson_next(j) != RJSON_OBJECT)
      goto end;

   for (;;)
   {
      enum rjson_type t = rjson_next(j);
      if (t == RJSON_ERROR || t == RJSON_DONE)
         goto end;
      if (t == RJSON_OBJECT_END && rjson_get_context_depth(j) == 0)
         break;

      if (key != SU_KEY_NONE)
      {
         /* The value of a top-level key we want.  A value of any other
          * type leaves the field unset, which the offer rule reads as
          * absent. */
         if (key == SU_KEY_TAG && t == RJSON_STRING)
         {
            size_t n;
            const char *s = rjson_get_string(j, &n);
            out->has_tag  = true;
            if (n < sizeof(out->tag) && strlen(s) == n)
            {
               memcpy(out->tag, s, n);
               out->tag[n]    = '\0';
               out->tag_valid = self_update_version_parse(out->tag,
                     out->version) == 0;
            }
         }
         else if (key == SU_KEY_DRAFT
               && (t == RJSON_TRUE || t == RJSON_FALSE))
         {
            out->has_draft = true;
            out->draft     = t == RJSON_TRUE;
         }
         else if (key == SU_KEY_PRERELEASE
               && (t == RJSON_TRUE || t == RJSON_FALSE))
         {
            out->has_prerelease = true;
            out->prerelease     = t == RJSON_TRUE;
         }
         key = SU_KEY_NONE;
         continue;
      }

      /* A member name is a string at depth 1 with an odd event count.
       * Names inside nested objects (author, assets) are never read. */
      if (     t == RJSON_STRING
            && rjson_get_context_depth(j) == 1
            && (rjson_get_context_count(j) & 1))
      {
         const char *name = rjson_get_string(j, NULL);
         if (!strcmp(name, "tag_name"))
            key = SU_KEY_TAG;
         else if (!strcmp(name, "draft"))
            key = SU_KEY_DRAFT;
         else if (!strcmp(name, "prerelease"))
            key = SU_KEY_PRERELEASE;
      }
   }

   if (rjson_next(j) == RJSON_DONE)
      ret = 0;
end:
   rjson_free(j);
   if (ret != 0)
      memset(out, 0, sizeof(*out));
   return ret;
}

bool self_update_release_offer(const struct self_update_release *rel,
      const unsigned current[4])
{
   return rel
      && rel->has_tag        && rel->tag_valid
      && rel->has_draft      && !rel->draft
      && rel->has_prerelease && !rel->prerelease
      && self_update_version_cmp(rel->version, current) > 0;
}

/* ---- TLS and URLs ------------------------------------------------ */

bool self_update_tls_allowed(const struct self_update_endpoints *ep,
      unsigned tls_verify_mode)
{
   if (!ep)
      return false;
   return !ep->tls_gate || tls_verify_mode == TLS_VERIFY_REQUIRED;
}

/* For an snprintf result: an empty string and 0 rather than a cut one. */
static size_t su_fit(char *s, size_t len, int n)
{
   if (n < 0 || (size_t)n >= len)
   {
      if (len)
         *s = '\0';
      return 0;
   }
   return (size_t)n;
}

size_t self_update_api_url(char *s, size_t len,
      const struct self_update_endpoints *ep)
{
   if (!s || !len || !ep)
      return 0;
   return su_fit(s, len, snprintf(s, len,
            "%srepos/libretro/RetroArch/releases/latest", ep->api_base));
}

size_t self_update_file_url(char *s, size_t len,
      const struct self_update_endpoints *ep, enum self_update_kind kind,
      const char *version, bool x86_64, bool preferred)
{
   char tail[128];
   unsigned v[4];
   const char *file = NULL;
   const char *arch = x86_64 ? "x86_64" : "x86";

   if (!s || !len)
      return 0;
   *s = '\0';
   /* The version came off the network: only digits and dots reach the
    * URL. */
   if (     !ep || !version || version[0] < '0' || version[0] > '9'
         || self_update_version_parse(version, v) != 0)
      return 0;

   switch (kind)
   {
      case SELF_UPDATE_KIND_APPIMAGE:
         arch = "x86_64";
         file = preferred ? "linux/%s/RetroArch-Linux-x86_64.AppImage"
                          : "linux/%s/RetroArch.7z";
         break;
      case SELF_UPDATE_KIND_WIN_PORTABLE:
         file = preferred ? "windows/%s/RetroArch_update.7z"
                          : "windows/%s/RetroArch.7z";
         break;
      case SELF_UPDATE_KIND_WIN_INSTALLER:
      case SELF_UPDATE_KIND_WIN_INSTALLER_VISIBLE:
         file = x86_64 ? "windows/%s/RetroArch-Win64-setup.exe"
                       : "windows/%s/RetroArch-Win32-setup.exe";
         break;
      case SELF_UPDATE_KIND_NONE:
      default:
         return 0;
   }

   if (!su_fit(tail, sizeof(tail), snprintf(tail, sizeof(tail), file, arch)))
      return 0;
   return su_fit(s, len, snprintf(s, len, "%sstable/%s/%s",
            ep->files_base, version, tail));
}

/* ---- Windows program set ----------------------------------------- */

static bool su_ends_with_ci(const char *s, size_t len, const char *suffix)
{
   size_t n = strlen(suffix);
   size_t i;
   if (len < n)
      return false;
   for (i = 0; i < n; i++)
      if (su_lower((unsigned char)s[len - n + i]) != suffix[i])
         return false;
   return true;
}

/* A relative '/'-separated path with no empty, "." or ".." part, no
 * backslash and no drive colon. */
static bool su_safe_rel(const char *rel)
{
   const char *p = rel;
   if (!*rel || strchr(rel, '\\') || strchr(rel, ':'))
      return false;
   for (;;)
   {
      size_t n = strcspn(p, "/");
      if (     n == 0
            || (n == 1 && p[0] == '.')
            || (n == 2 && p[0] == '.' && p[1] == '.'))
         return false;
      if (!p[n])
         return true;
      p += n + 1;
   }
}

const char *self_update_program_member(const char *member, const char *top)
{
   size_t top_len;
   const char *rel;

   if (!member || !top || !*top)
      return NULL;
   top_len = strlen(top);
   if (strncmp(member, top, top_len) || member[top_len] != '/')
      return NULL;
   rel = member + top_len + 1;
   if (!su_safe_rel(rel))
      return NULL;

   if (!strchr(rel, '/'))
   {
      size_t n = strlen(rel);
      if (su_ends_with_ci(rel, n, ".exe") || su_ends_with_ci(rel, n, ".dll"))
         return rel;
      return NULL;
   }
   if (!strncmp(rel, "filters/", 8) || !strncmp(rel, "platforms/", 10))
      return rel;
   return NULL;
}

/* ---- marker and restart ------------------------------------------ */

size_t self_update_marker_format(char *s, size_t len,
      const char *version, enum self_update_kind kind)
{
   unsigned v[4];
   if (!s || !len)
      return 0;
   *s = '\0';
   if (self_update_version_parse(version, v) != 0)
      return 0;
   return su_fit(s, len, snprintf(s, len, "%s\n%d\n", version, (int)kind));
}

const char *self_update_restart_target(enum self_update_kind kind,
      const char *appimage, const char *exe_path)
{
   /* Inside an AppImage the process image is the binary in the old
    * mounted image; the file that now holds the new build is
    * $APPIMAGE. */
   if (kind == SELF_UPDATE_KIND_APPIMAGE && appimage && *appimage)
      return appimage;
   return exe_path;
}

enum self_update_marker_result self_update_marker_consume(
      const struct self_update_fileops *ops, const char *marker,
      const unsigned current[4], char *target, size_t target_len)
{
   char buf[128];
   unsigned v[4];
   long n;
   size_t vlen;

   if (target && target_len)
      *target = '\0';
   if (!ops || !marker || ops->exists(ops->ud, marker) != 1)
      return SELF_UPDATE_MARKER_INVALID;

   n = ops->read(ops->ud, marker, buf, sizeof(buf) - 1);
   ops->remove(ops->ud, marker);
   if (n <= 0)
      return SELF_UPDATE_MARKER_INVALID;
   buf[n] = '\0';

   vlen = strcspn(buf, "\n");
   if (buf[vlen] != '\n')
      return SELF_UPDATE_MARKER_INVALID;
   buf[vlen] = '\0';
   if (self_update_version_parse(buf, v) != 0)
      return SELF_UPDATE_MARKER_INVALID;
   if (target && target_len)
      strlcpy(target, buf, target_len);
   return self_update_version_cmp(current, v) >= 0
      ? SELF_UPDATE_MARKER_UPDATED
      : SELF_UPDATE_MARKER_NOT_FINISHED;
}

/* ---- replace and rollback ---------------------------------------- */

static bool su_join(char *s, size_t len, const char *dir, const char *rel,
      const char *suffix)
{
   return su_fit(s, len, snprintf(s, len, "%s/%s%s", dir, rel,
            suffix ? suffix : "")) != 0;
}

/* Puts one file back: a placed new file returns to staging, then a
 * set-aside old file returns to its name. */
static void su_undo(const struct self_update_fileops *ops, const char *dst,
      const char *staging, const char *rel, bool placed, bool aside)
{
   char cur[PATH_MAX_LENGTH], old[PATH_MAX_LENGTH], src[PATH_MAX_LENGTH];
   if (     !su_join(cur, sizeof(cur), dst, rel, NULL)
         || !su_join(old, sizeof(old), dst, rel, SELF_UPDATE_OLD_SUFFIX)
         || !su_join(src, sizeof(src), staging, rel, NULL))
      return;
   if (placed)
      ops->rename(ops->ud, cur, src);
   if (aside)
      ops->rename(ops->ud, old, cur);
}

int self_update_replace(const struct self_update_fileops *ops,
      const char *dst, const char *staging,
      const char *const *rel, size_t count)
{
   char cur[PATH_MAX_LENGTH], old[PATH_MAX_LENGTH], src[PATH_MAX_LENGTH];
   bool *aside;
   size_t i;

   if (!ops || !dst || !staging || (count && !rel))
      return -1;
   if (!(aside = (bool*)calloc(count ? count : 1, sizeof(*aside))))
      return -1;

   for (i = 0; i < count; i++)
   {
      if (     !su_join(cur, sizeof(cur), dst, rel[i], NULL)
            || !su_join(old, sizeof(old), dst, rel[i], SELF_UPDATE_OLD_SUFFIX)
            || !su_join(src, sizeof(src), staging, rel[i], NULL))
         goto fail;

      if (ops->exists(ops->ud, cur) == 1)
      {
         /* Windows renames a running exe or a loaded DLL but will not
          * overwrite or delete it, so the old file is moved aside.  A
          * stale one from an earlier update goes first. */
         if (ops->exists(ops->ud, old) == 1)
            ops->remove(ops->ud, old);
         if (ops->rename(ops->ud, cur, old) != 0)
            goto fail;
         aside[i] = true;
      }
      else
      {
         /* New in this release: its folder may be new too. */
         char *slash = strrchr(cur, '/');
         *slash      = '\0';
         if (ops->mkdir_p(ops->ud, cur) != 0)
            goto fail;
         *slash      = '/';
      }

      if (ops->rename(ops->ud, src, cur) != 0)
         goto fail;
   }
   free(aside);
   return 0;

fail:
   /* File i was not placed, though it may have been set aside. */
   if (i < count)
      su_undo(ops, dst, staging, rel[i], false, aside[i]);
   while (i-- > 0)
      su_undo(ops, dst, staging, rel[i], true, aside[i]);
   free(aside);
   return -1;
}

/* ---- AppImage ---------------------------------------------------- */

int self_update_appimage_swap(const struct self_update_fileops *ops,
      const char *new_file, const char *appimage)
{
   unsigned mode;
   if (!ops || !new_file || !appimage)
      return -1;
   if (ops->get_mode(ops->ud, appimage, &mode) != 0)
      return -1;
   if (ops->set_mode(ops->ud, new_file, mode) != 0)
      return -1;
   /* Same directory, so one atomic rename.  The running process keeps
    * its already-mounted image. */
   return ops->rename(ops->ud, new_file, appimage);
}

/* ---- start-up cleanup -------------------------------------------- */

struct su_walk
{
   const struct self_update_fileops *ops;
   char path[PATH_MAX_LENGTH];
   unsigned depth;
   bool all;     /* remove everything, not only *.retroarch-old */
   bool recurse;
};

static bool su_walk_cb(void *ctx, const char *name, bool is_dir)
{
   struct su_walk *w = (struct su_walk*)ctx;
   size_t base       = strlen(w->path);
   size_t n          = strlen(name);

   if (base + 1 + n >= sizeof(w->path))
      return true;
   w->path[base] = '/';
   memcpy(w->path + base + 1, name, n + 1);

   if (is_dir)
   {
      /* @list reports a symbolic link as a file, so a link to a folder
       * is never followed; the depth cap is a second guard. */
      if (w->recurse && w->depth < 16)
      {
         w->depth++;
         w->ops->list(w->ops->ud, w->path, su_walk_cb, w);
         w->depth--;
         if (w->all)
            w->ops->remove(w->ops->ud, w->path);
      }
   }
   else if (w->all || su_ends_with_ci(name, n, SELF_UPDATE_OLD_SUFFIX))
      w->ops->remove(w->ops->ud, w->path);

   w->path[base] = '\0';
   return true;
}

static void su_walk(const struct self_update_fileops *ops, const char *dir,
      const char *sub, bool all, bool recurse)
{
   struct su_walk w;
   w.ops     = ops;
   w.depth   = 0;
   w.all     = all;
   w.recurse = recurse;
   if (sub)
   {
      if (!su_join(w.path, sizeof(w.path), dir, sub, NULL))
         return;
   }
   else if (strlcpy(w.path, dir, sizeof(w.path)) >= sizeof(w.path))
      return;
   if (ops->exists(ops->ud, w.path) == 1)
      ops->list(ops->ud, w.path, su_walk_cb, &w);
}

/* A staging folder is removed only under one of the two names the
 * updater gives them, so a wrong path cannot empty some other folder. */
static bool su_is_staging_name(const char *path)
{
   const char *name = path + strlen(path);
   while (name > path && !su_is_sep(name[-1]))
      name--;
   return !strcmp(name, ".retroarch-update")
       || !strcmp(name, "retroarch-update");
}

void self_update_cleanup(const struct self_update_fileops *ops,
      const char *exe_dir, const char *const *staging, size_t count)
{
   size_t i;
   if (!ops)
      return;
   if (exe_dir && *exe_dir)
   {
      su_walk(ops, exe_dir, NULL,        false, false);
      su_walk(ops, exe_dir, "filters",   false, true);
      su_walk(ops, exe_dir, "platforms", false, true);
   }
   for (i = 0; staging && i < count; i++)
   {
      if (!staging[i] || !su_is_staging_name(staging[i]))
         continue;
      su_walk(ops, staging[i], NULL, true, true);
      ops->remove(ops->ud, staging[i]);
   }
}
