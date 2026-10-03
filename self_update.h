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

#ifndef __RARCH_SELF_UPDATE_H
#define __RARCH_SELF_UPDATE_H

/* Self-updater policy: everything that decides, with no network and no
 * file access of its own.  The file operations the apply, rollback and
 * start-up steps need go through a table the caller passes, so every
 * rule here runs on Linux under samples/tasks/self_update. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum self_update_kind
{
   SELF_UPDATE_KIND_NONE = 0,             /* hide the feature, make no request */
   SELF_UPDATE_KIND_APPIMAGE,
   SELF_UPDATE_KIND_WIN_PORTABLE,
   SELF_UPDATE_KIND_WIN_INSTALLER,        /* the silent installer targets this folder */
   SELF_UPDATE_KIND_WIN_INSTALLER_VISIBLE /* installer install elsewhere */
};

/* The folder the stable Win64 installer installs to when run with /S;
 * it ignores /D=. */
#define SELF_UPDATE_WIN64_SILENT_DIR "C:\\RetroArch-Win64"

/* Where requests go.  Production passes self_update_endpoints_default
 * and nothing else; only the test harness passes another. */
struct self_update_endpoints
{
   const char *api_base;   /* ends in '/' */
   const char *files_base; /* ends in '/' */
   bool tls_gate;          /* refuse unless certificate checking is required */
};

extern const struct self_update_endpoints self_update_endpoints_default;

/* Facts the kind decision needs, gathered by the platform code. */
struct self_update_env
{
   const char *appimage;         /* $APPIMAGE, or NULL */
   const char *exe_dir;          /* Windows: folder holding retroarch.exe */
   const char *uninstall_string; /* Windows: the uninstall key's UninstallString, or NULL */
   bool appimage_is_file;        /* $APPIMAGE names a regular file */
   bool flatpak;                 /* FLATPAK_ID is set */
   bool snap;                    /* SNAP is set */
   bool windows;
   bool x86_64;
   bool has_uninstaller;         /* uninstall.exe sits beside retroarch.exe */
};

enum self_update_kind self_update_kind_decide(const struct self_update_env *env);

/* 0 on success; parts beyond those present are 0.  Accepts one
 * optional leading 'v', then two to four dot-separated decimal
 * parts.  Rejects empty parts, signs, other non-digits, fewer than
 * two or more than four parts, any part > 65535. */
int self_update_version_parse(const char *s, unsigned out[4]);
/* -1, 0, 1. */
int self_update_version_cmp(const unsigned a[4], const unsigned b[4]);

struct self_update_release
{
   unsigned version[4];
   char tag[32];
   bool has_tag;
   bool tag_valid;   /* tag parses as a version */
   bool has_draft;
   bool has_prerelease;
   bool draft;
   bool prerelease;
};

/* Reads the top-level tag_name, draft and prerelease of a GitHub
 * release object.  0 on success, -1 when the JSON does not parse or
 * is not an object. */
int self_update_release_parse(const char *json, size_t len,
      struct self_update_release *out);

/* True only for a release that is not a draft, not a prerelease, has
 * a valid tag and is newer than @current. */
bool self_update_release_offer(const struct self_update_release *rel,
      const unsigned current[4]);

/* True when a request may be made under this certificate-checking
 * mode (enum tls_verify_mode). */
bool self_update_tls_allowed(const struct self_update_endpoints *ep,
      unsigned tls_verify_mode);

/* The GitHub API URL for the latest release. */
size_t self_update_api_url(char *s, size_t len,
      const struct self_update_endpoints *ep);

/* The download URL for @kind and @version ("1.22.2", no 'v').
 * @preferred picks the small file of spec §4.5 where one exists
 * (RetroArch_update.7z, the bare AppImage); for an installer it is
 * ignored.  Returns 0 for SELF_UPDATE_KIND_NONE. */
size_t self_update_file_url(char *s, size_t len,
      const struct self_update_endpoints *ep, enum self_update_kind kind,
      const char *version, bool x86_64, bool preferred);

/* Windows portable program set: for an archive member under @top,
 * returns the path below @top when it is a .exe or .dll directly in
 * @top or a file anywhere under filters/ or platforms/; NULL
 * otherwise, and NULL for directories and for any path that is
 * absolute, has a drive, a backslash or a ".." part. */
const char *self_update_program_member(const char *member, const char *top);

/* The pending marker: two lines, the target version and the kind. */
size_t self_update_marker_format(char *s, size_t len,
      const char *version, enum self_update_kind kind);

enum self_update_marker_result
{
   SELF_UPDATE_MARKER_INVALID = 0,
   SELF_UPDATE_MARKER_UPDATED,     /* running version reached the target */
   SELF_UPDATE_MARKER_NOT_FINISHED
};

/* The restart target: $APPIMAGE for an AppImage, else @exe_path, the
 * executable path captured before any file was replaced. */
const char *self_update_restart_target(enum self_update_kind kind,
      const char *appimage, const char *exe_path);

/* File operations.  Each returns 0 on success.  @remove removes a file
 * or an empty folder.  @list calls @cb for each entry of @dir other
 * than "." and ".."; @cb returns false to stop. */
typedef bool (*self_update_list_cb)(void *ctx, const char *name, bool is_dir);

struct self_update_fileops
{
   int  (*exists)(void *ud, const char *path);  /* 1 yes, 0 no */
   int  (*rename)(void *ud, const char *from, const char *to);
   int  (*remove)(void *ud, const char *path);
   int  (*mkdir_p)(void *ud, const char *path);
   int  (*get_mode)(void *ud, const char *path, unsigned *mode);
   int  (*set_mode)(void *ud, const char *path, unsigned mode);
   int  (*list)(void *ud, const char *dir, self_update_list_cb cb, void *ctx);
   long (*read)(void *ud, const char *path, char *buf, size_t len);
   void *ud;
};

/* Windows portable step 3 and 4: for each path in @rel (relative,
 * '/'-separated), moves <dst>/p aside to p.retroarch-old if it exists,
 * then moves <staging>/p to <dst>/p.  On any failure every move
 * already made is undone, in reverse, and -1 is returned. */
int self_update_replace(const struct self_update_fileops *ops,
      const char *dst, const char *staging,
      const char *const *rel, size_t count);

/* AppImage steps 2 and 3: gives @new_file the mode bits of @appimage,
 * then renames it over @appimage. */
int self_update_appimage_swap(const struct self_update_fileops *ops,
      const char *new_file, const char *appimage);

/* Start-up: removes *.retroarch-old files from @exe_dir, its filters/
 * tree and platforms/, and removes each staging folder given; any of
 * the paths may be NULL.  Something still in use is left for the next
 * start. */
void self_update_cleanup(const struct self_update_fileops *ops,
      const char *exe_dir, const char *const *staging, size_t count);

/* Start-up: when @marker exists, reads it, compares its target with
 * @current, writes the target version to @target and removes the
 * marker.  Returns SELF_UPDATE_MARKER_INVALID when there is no marker
 * or it does not parse (an unparsable marker is removed too). */
enum self_update_marker_result self_update_marker_consume(
      const struct self_update_fileops *ops, const char *marker,
      const unsigned current[4], char *target, size_t target_len);

RETRO_END_DECLS

#endif
