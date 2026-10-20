/* Network lanes for the self-updater's check task, against the
 * shipping tasks/task_self_update.c, self_update.c, tasks/task_http.c,
 * libretro-common/queues/task_queue.c and libretro-common/net/net_http.c.
 *
 * A loopback server answers the release query and the HEAD requests
 * that size the download, and counts each request by what it asked
 * for.  Each lane pushes one check through the test endpoints, pumps
 * the queue until it retires, and asserts which notices were raised
 * and which requests were made.
 *
 *   startup   the start-up check, offered a newer release: one notice
 *             naming it, and no request for the file at all
 *   sized     the menu check, offered a newer release: the small file
 *             is sized with HEAD and its size is reported
 *   fallback  the small file answers 404: the full archive is sized
 *   notyet    both files answer 404: "not available yet", nothing else
 *   uptodate  the release is not newer: "up to date", one request
 *   failed    the release query answers 403: the status is reported
 *   tlsgate   the shipping endpoints with certificate checking not
 *             required: refused before any request
 *   none      an install the updater cannot replace: no request
 *   realfs    the shipping file-operation table, on a scratch folder:
 *             the start-up cleanup empties a staging folder without
 *             following a link out of it, the AppImage swap keeps the
 *             mode bits, and a marker is read and removed */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <queues/task_queue.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#include <retro_timers.h>

#include "../../../configuration.h"
#include "../../../msg_hash.h"
#include "../../../network/tls_config.h"
#include "../../../runloop.h"
#include "../../../self_update.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../version.h"

static int checks   = 0;
static int failures = 0;

#define CHECK(cond, name) \
   do \
   { \
      checks++; \
      if (cond) \
         printf("  ok   %s\n", name); \
      else \
      { \
         failures++; \
         printf("  FAIL %s\n", name); \
      } \
   } while (0)

/* ---------------- stubs for the frontend singletons -------------- */

static settings_t stub_settings;

settings_t *config_get_ptr(void)
{
   return &stub_settings;
}

/* Notices are recorded, so a lane can say exactly what the user saw.
 * Formats carry a tag the lanes match on, and keep the real strings'
 * conversions so the task's snprintf calls are exercised as shipped. */
static char notices[8][256];
static int  notice_count = 0;

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   switch (msg)
   {
      case MSG_SELF_UPDATE_CHECKING:          return "checking";
      case MSG_SELF_UPDATE_UP_TO_DATE:        return "uptodate %s";
      case MSG_SELF_UPDATE_AVAILABLE:         return "available %s %s";
      case MSG_SELF_UPDATE_AVAILABLE_STARTUP: return "startup %s";
      case MSG_SELF_UPDATE_NOT_YET_AVAILABLE: return "notyet %s";
      case MSG_SELF_UPDATE_CHECK_FAILED:      return "failed %s";
      case MSG_SELF_UPDATE_TLS_REQUIRED:      return "tlsrequired";
      default:                                break;
   }
   return "other";
}

void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration,
      bool flush, char *title,
      enum message_queue_icon icon,
      enum message_queue_category category)
{
   (void)len; (void)prio; (void)duration;
   (void)flush; (void)title; (void)icon; (void)category;
   if (notice_count < 8)
      strlcpy(notices[notice_count], msg, sizeof(notices[0]));
   notice_count++;
}

void task_window_progress_cb(retro_task_t *task)
{
   (void)task;
}

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }

/* ---------------- loopback server -------------------------------- */

struct lane_cfg
{
   const char *release;  /* body of the release query */
   int release_status;
   int small_status;     /* HEAD of the bare AppImage */
   int full_status;      /* HEAD of RetroArch.7z */
};

static struct lane_cfg cfg;
static retro_atomic_int_t n_release;
static retro_atomic_int_t n_small;
static retro_atomic_int_t n_full;
static retro_atomic_int_t n_other;

#define SMALL_SIZE 1048576u /* reported as "1.0 MB" */

static retro_atomic_int_t srv_fd;
static sthread_t *srv_thread  = NULL;
static volatile int srv_port  = 0;

static void answer(int cfd, const char *rbuf)
{
   char head[256];
   const char *body = "";
   int status       = 404;
   unsigned len     = 0;
   bool is_head     = strncmp(rbuf, "HEAD ", 5) == 0;
   const char *eol  = strstr(rbuf, " HTTP/");
   size_t line      = eol ? (size_t)(eol - rbuf) : strlen(rbuf);
   char path[512];

   if (line >= sizeof(path))
      line = sizeof(path) - 1;
   memcpy(path, rbuf, line);
   path[line] = '\0';

   if (strstr(path, "/releases/latest") && !is_head)
   {
      retro_atomic_fetch_add_int(&n_release, 1);
      status = cfg.release_status;
      body   = cfg.release;
      len    = (unsigned)strlen(body);
   }
   else if (is_head && strstr(path, "/RetroArch-Linux-x86_64.AppImage"))
   {
      retro_atomic_fetch_add_int(&n_small, 1);
      status = cfg.small_status;
      len    = status == 200 ? SMALL_SIZE : 0;
   }
   else if (is_head && strstr(path, "/RetroArch.7z"))
   {
      retro_atomic_fetch_add_int(&n_full, 1);
      status = cfg.full_status;
      len    = status == 200 ? 2 * SMALL_SIZE : 0;
   }
   else
      retro_atomic_fetch_add_int(&n_other, 1);

   snprintf(head, sizeof(head),
         "HTTP/1.1 %d X\r\nContent-Length: %u\r\n"
         "Connection: close\r\n\r\n", status, len);
   send(cfd, head, strlen(head), 0);
   /* A HEAD reply's Content-Length describes the GET; it has no body. */
   if (!is_head && *body)
      send(cfd, body, strlen(body), 0);
}

static void server_thread(void *unused)
{
   char rbuf[4096];
   (void)unused;
   for (;;)
   {
      struct sockaddr_in cli;
      socklen_t clen = sizeof(cli);
      int fd         = retro_atomic_load_acquire_int(&srv_fd);
      int cfd        = fd >= 0
            ? accept(fd, (struct sockaddr*)&cli, &clen) : -1;
      size_t have    = 0;
      if (cfd < 0)
         return;
      /* Read the whole request head before answering; answering after
       * one partial read and closing races the client's next send. */
      while (have < sizeof(rbuf) - 1)
      {
         ssize_t n = recv(cfd, rbuf + have, sizeof(rbuf) - 1 - have, 0);
         if (n <= 0)
            break;
         have += (size_t)n;
         rbuf[have] = '\0';
         if (strstr(rbuf, "\r\n\r\n"))
            break;
      }
      rbuf[have] = '\0';
      answer(cfd, rbuf);
      socket_close(cfd);
   }
}

static bool server_start(void)
{
   struct sockaddr_in addr;
   socklen_t alen = sizeof(addr);
   int fd = socket(AF_INET, SOCK_STREAM, 0);
   if (fd < 0)
      return false;
   memset(&addr, 0, sizeof(addr));
   addr.sin_family      = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
      return false;
   if (listen(fd, 4) < 0)
      return false;
   getsockname(fd, (struct sockaddr*)&addr, &alen);
   srv_port = ntohs(addr.sin_port);
   retro_atomic_store_release_int(&srv_fd, fd);
   srv_thread = sthread_create(server_thread, NULL);
   return srv_thread != NULL;
}

static void server_stop(void)
{
   int fd = retro_atomic_load_acquire_int(&srv_fd);
   retro_atomic_store_release_int(&srv_fd, -1);
   if (fd >= 0)
   {
      /* Unblock the accept loop before closing. */
      shutdown(fd, SHUT_RDWR);
      socket_close(fd);
   }
   if (srv_thread)
   {
      sthread_join(srv_thread);
      srv_thread = NULL;
   }
}

/* ---------------- lanes ------------------------------------------ */

#define NEWER_RELEASE \
   "{\"tag_name\":\"v99.0.0\",\"draft\":false,\"prerelease\":false}"
#define OLDER_RELEASE \
   "{\"tag_name\":\"v1.0.0\",\"draft\":false,\"prerelease\":false}"

static char api_base[64];
static char files_base[64];
static struct self_update_endpoints test_ep;

static void reset(const char *release, int release_status,
      int small_status, int full_status)
{
   cfg.release        = release;
   cfg.release_status = release_status;
   cfg.small_status   = small_status;
   cfg.full_status    = full_status;
   retro_atomic_store_release_int(&n_release, 0);
   retro_atomic_store_release_int(&n_small, 0);
   retro_atomic_store_release_int(&n_full, 0);
   retro_atomic_store_release_int(&n_other, 0);
   notice_count = 0;
   memset(notices, 0, sizeof(notices));
   stub_settings.uints.tls_verify_mode = TLS_VERIFY_REQUIRED;
}

static bool any_task(retro_task_t *task, void *user_data)
{
   (void)task;
   (void)user_data;
   return true;
}

/* Pumps the queue on this, the main, thread, as the runloop does,
 * until nothing is left in it. */
static void pump(void)
{
   task_finder_data_t find_data;
   int i;

   find_data.func     = any_task;
   find_data.userdata = NULL;
   for (i = 0; i < 1000; i++)
   {
      task_queue_check();
      if (!task_queue_find(&find_data))
         break;
      retro_sleep(5);
   }
   CHECK(i < 1000, "queue drained");
   for (i = 0; i < 5; i++)
   {
      task_queue_check();
      retro_sleep(5);
   }
}

static bool last_notice_is(const char *want)
{
   return notice_count > 0 && notice_count <= 8
       && !strcmp(notices[notice_count - 1], want);
}

static void lane_startup(void)
{
   printf("[startup]\n");
   reset(NEWER_RELEASE, 200, 200, 200);
   CHECK(task_push_self_update_check_at(&test_ep,
            SELF_UPDATE_KIND_APPIMAGE, true), "check started");
   pump();
   CHECK(notice_count == 1, "exactly one notice");
   CHECK(last_notice_is("startup 99.0.0"), "the notice names the release");
   CHECK(retro_atomic_load_acquire_int(&n_release) == 1,
         "one release query");
   CHECK(   retro_atomic_load_acquire_int(&n_small) == 0
         && retro_atomic_load_acquire_int(&n_full)  == 0
         && retro_atomic_load_acquire_int(&n_other) == 0,
         "no request for the file");
}

static void lane_sized(void)
{
   printf("[sized]\n");
   reset(NEWER_RELEASE, 200, 200, 200);
   CHECK(task_push_self_update_check_at(&test_ep,
            SELF_UPDATE_KIND_APPIMAGE, false), "check started");
   pump();
   CHECK(notice_count == 2 && !strcmp(notices[0], "checking"),
         "a checking notice, then one result");
   CHECK(last_notice_is("available 99.0.0 1.0 MB"),
         "the small file's size is reported");
   CHECK(   retro_atomic_load_acquire_int(&n_small) == 1
         && retro_atomic_load_acquire_int(&n_full)  == 0,
         "only the small file was sized");
}

static void lane_fallback(void)
{
   printf("[fallback]\n");
   reset(NEWER_RELEASE, 200, 404, 200);
   CHECK(task_push_self_update_check_at(&test_ep,
            SELF_UPDATE_KIND_APPIMAGE, false), "check started");
   pump();
   CHECK(last_notice_is("available 99.0.0 2.0 MB"),
         "the full archive's size is reported");
   CHECK(   retro_atomic_load_acquire_int(&n_small) == 1
         && retro_atomic_load_acquire_int(&n_full)  == 1,
         "small file first, then the full archive");
}

static void lane_notyet(void)
{
   printf("[notyet]\n");
   reset(NEWER_RELEASE, 200, 404, 404);
   CHECK(task_push_self_update_check_at(&test_ep,
            SELF_UPDATE_KIND_APPIMAGE, false), "check started");
   pump();
   CHECK(last_notice_is("notyet 99.0.0"), "reported as not available yet");
   CHECK(retro_atomic_load_acquire_int(&n_other) == 0, "no other request");
}

static void lane_uptodate(void)
{
   printf("[uptodate]\n");
   reset(OLDER_RELEASE, 200, 200, 200);
   CHECK(task_push_self_update_check_at(&test_ep,
            SELF_UPDATE_KIND_APPIMAGE, false), "check started");
   pump();
   CHECK(last_notice_is("uptodate " PACKAGE_VERSION),
         "reported as up to date with the running version");
   CHECK(   retro_atomic_load_acquire_int(&n_release) == 1
         && retro_atomic_load_acquire_int(&n_small)   == 0
         && retro_atomic_load_acquire_int(&n_full)    == 0,
         "one request");
}

static void lane_failed(void)
{
   printf("[failed]\n");
   reset("{}", 403, 200, 200);
   CHECK(task_push_self_update_check_at(&test_ep,
            SELF_UPDATE_KIND_APPIMAGE, false), "check started");
   pump();
   CHECK(last_notice_is("failed HTTP 403"), "the status is reported");
   CHECK(retro_atomic_load_acquire_int(&n_small) == 0, "nothing sized");
}

static void lane_tlsgate(void)
{
   unsigned modes[2];
   int i;

   printf("[tlsgate]\n");
   modes[0] = TLS_VERIFY_OPTIONAL;
   modes[1] = TLS_VERIFY_DISABLED;
   for (i = 0; i < 2; i++)
   {
      reset(NEWER_RELEASE, 200, 200, 200);
      stub_settings.uints.tls_verify_mode = modes[i];
      /* The shipping endpoints: a request here would leave the
       * machine, so the refusal must come before any is made. */
      CHECK(!task_push_self_update_check_at(&self_update_endpoints_default,
               SELF_UPDATE_KIND_APPIMAGE, false), "refused");
      CHECK(last_notice_is("tlsrequired") && notice_count == 1,
            "the user is told why");
      pump();
   }
}

static void lane_none(void)
{
   printf("[none]\n");
   reset(NEWER_RELEASE, 200, 200, 200);
   CHECK(!task_push_self_update_check_at(&test_ep,
            SELF_UPDATE_KIND_NONE, false), "refused");
   pump();
   CHECK(notice_count == 0, "no notice");
   CHECK(retro_atomic_load_acquire_int(&n_release) == 0, "no request");
}

static void rf_write(const char *path, const char *content)
{
   FILE *f = fopen(path, "wb");
   if (f)
   {
      fputs(content, f);
      fclose(f);
   }
}

static void lane_realfs(void)
{
   const struct self_update_fileops *ops = task_self_update_fileops();
   char root[64], outside[128], kept[160], stage[128], sub[160];
   char link[160], app[160], fresh[160], marker[160], target[32];
   const char *staging[1];
   unsigned cur[4];
   struct stat st;

   printf("realfs\n");
   snprintf(root, sizeof(root), "su_realfs_%ld", (long)getpid());
   snprintf(outside, sizeof(outside), "%s/outside", root);
   snprintf(kept, sizeof(kept), "%s/keep.txt", outside);
   snprintf(stage, sizeof(stage), "%s/.retroarch-update", root);
   snprintf(sub, sizeof(sub), "%s/sub", stage);
   snprintf(link, sizeof(link), "%s/link", stage);

   CHECK(ops->mkdir_p(ops->ud, sub) == 0 && ops->mkdir_p(ops->ud, outside) == 0,
         "mkdir_p makes nested folders");
   rf_write(kept, "keep");
   snprintf(fresh, sizeof(fresh), "%s/new.AppImage", stage);
   rf_write(fresh, "new build");
   snprintf(fresh, sizeof(fresh), "%s/sub/part", stage);
   rf_write(fresh, "part");
   /* A link to a folder outside staging: the cleanup must remove the
    * link and leave what it points at. */
   CHECK(symlink("../outside", link) == 0, "plant a link out of staging");

   staging[0] = stage;
   self_update_cleanup(ops, NULL, staging, 1);
   CHECK(ops->exists(ops->ud, stage) == 0, "cleanup removes the staging folder");
   CHECK(lstat(link, &st) != 0, "cleanup removes the link itself");
   CHECK(ops->exists(ops->ud, kept) == 1, "cleanup leaves the link's target alone");

   snprintf(app, sizeof(app), "%s/RetroArch.AppImage", root);
   snprintf(fresh, sizeof(fresh), "%s/new.AppImage", root);
   rf_write(app, "old build");
   chmod(app, 0750);
   rf_write(fresh, "new build");
   chmod(fresh, 0600);
   CHECK(self_update_appimage_swap(ops, fresh, app) == 0
         && stat(app, &st) == 0 && (st.st_mode & 07777) == 0750
         && ops->exists(ops->ud, fresh) == 0,
         "AppImage swap renames over the old file and keeps mode 0750");

   snprintf(marker, sizeof(marker), "%s/self_update.pending", root);
   rf_write(marker, "1.0.0\n1\n");
   self_update_version_parse(PACKAGE_VERSION, cur);
   CHECK(self_update_marker_consume(ops, marker, cur, target, sizeof(target))
         == SELF_UPDATE_MARKER_UPDATED && !strcmp(target, "1.0.0")
         && ops->exists(ops->ud, marker) == 0,
         "a marker is read through the table and removed");

   remove(kept);
   remove(app);
   rmdir(outside);
   rmdir(root);
   CHECK(ops->exists(ops->ud, root) == 0, "scratch folder removed");
}

int main(int argc, char **argv)
{
   const char *only = argc > 1 ? argv[1] : NULL;

   network_init();
   task_queue_init(true, NULL);
   if (!server_start())
   {
      printf("could not start the loopback server\n");
      return 1;
   }
   snprintf(api_base, sizeof(api_base), "http://127.0.0.1:%d/", srv_port);
   snprintf(files_base, sizeof(files_base), "http://127.0.0.1:%d/", srv_port);
   test_ep.api_base   = api_base;
   test_ep.files_base = files_base;
   test_ep.tls_gate   = false;

#define LANE(name) if (!only || !strcmp(only, #name)) lane_##name()
   LANE(startup);
   LANE(sized);
   LANE(fallback);
   LANE(notyet);
   LANE(uptodate);
   LANE(failed);
   LANE(tlsgate);
   LANE(none);
   LANE(realfs);
#undef LANE

   server_stop();
   task_queue_deinit();
   printf("%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
}
