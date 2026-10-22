/* Menu settings display harness.
 *
 * Links the shipping RetroArch objects (see build.sh) with only main()
 * replaced, brings the menu up headless, and reads settings through
 * the display callback the menu itself uses.
 *
 * The password rows show "********" when a password is set.  The
 * RetroAchievements password is cleared once a login token is held,
 * so that row also shows as set while a token exists.  The cloud-sync
 * password and secret key share the same callback, and showed as set
 * whenever a RetroAchievements token existed, even when empty.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <string/stdstring.h>
#include <time/rtime.h>
#include <file/config_file.h>

#include "../../../msg_hash.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_setting.h"
#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../frontend/frontend_driver.h"

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

/* What the menu shows for the setting @idx; "(missing)" when this
 * build has no such setting. */
static const char *shown(enum msg_hash_enums idx, char *s, size_t len)
{
   rarch_setting_t *setting = menu_setting_find_enum(idx);
   if (!setting || !setting->actions || !setting->actions->repr)
      return "(missing)";
   s[0] = '\0';
   setting->actions->repr(setting, s, len);
   return s;
}

static void lane_passwords(void)
{
   settings_t *settings = config_get_ptr();
   char s[64];

   strlcpy(settings->arrays.cheevos_token, "token",
         sizeof(settings->arrays.cheevos_token));
   settings->arrays.cheevos_password[0] = '\0';
#ifdef HAVE_CLOUDSYNC
   settings->arrays.webdav_password[0]  = '\0';
   CHECK(string_is_equal(shown(MENU_ENUM_LABEL_CLOUD_SYNC_PASSWORD, s, sizeof(s)), ""),
         "empty cloud-sync password shown as '%s' with a RetroAchievements token", s);
#ifdef HAVE_S3
   settings->arrays.secret_access_key[0] = '\0';
   CHECK(string_is_equal(shown(MENU_ENUM_LABEL_CLOUD_SYNC_SECRET_ACCESS_KEY, s, sizeof(s)), ""),
         "empty S3 secret key shown as '%s' with a RetroAchievements token", s);
#endif

   strlcpy(settings->arrays.webdav_password, "pw",
         sizeof(settings->arrays.webdav_password));
   CHECK(string_is_equal(shown(MENU_ENUM_LABEL_CLOUD_SYNC_PASSWORD, s, sizeof(s)), "********"),
         "a set cloud-sync password shown as '%s'", s);
#else
   fprintf(stderr, "[skip] cloud-sync rows (no HAVE_CLOUDSYNC)\n");
#endif

#ifdef HAVE_CHEEVOS
   CHECK(string_is_equal(shown(MENU_ENUM_LABEL_CHEEVOS_PASSWORD, s, sizeof(s)), "********"),
         "RetroAchievements password with a login token shown as '%s'", s);
   settings->arrays.cheevos_token[0] = '\0';
   CHECK(string_is_equal(shown(MENU_ENUM_LABEL_CHEEVOS_PASSWORD, s, sizeof(s)), ""),
         "empty RetroAchievements password with no token shown as '%s'", s);
#else
   fprintf(stderr, "[skip] RetroAchievements row (no HAVE_CHEEVOS)\n");
#endif
}

int main(int argc, char *argv[])
{
   char cmd[700];
   static char fixture_dir[512];
   static char cfg_path[640];
   char *rarch_argv[8];
   int rarch_argc = 0;

   (void)argc;
   (void)argv;

   snprintf(fixture_dir, sizeof(fixture_dir),
         "/tmp/menu_settings_display_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", fixture_dir);
   if (system(cmd) != 0)
      return 1;

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--menu";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;

   /* The same prelude rarch_main() runs before main_init; see
    * samples/menu/playlist_nav for why none of it can be skipped. */
   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   {
      FILE *cfg;
      snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg", fixture_dir);
      if ((cfg = fopen(cfg_path, "wb")))
      {
         fprintf(cfg, "video_driver = \"null\"\n");
         fprintf(cfg, "audio_driver = \"null\"\n");
         fprintf(cfg, "input_driver = \"null\"\n");
         fprintf(cfg, "input_joypad_driver = \"null\"\n");
         fprintf(cfg, "menu_driver = \"rgui\"\n");
         fprintf(cfg, "video_threaded = \"false\"\n");
         fprintf(cfg, "history_list_enable = \"false\"\n");
         fprintf(cfg, "savefile_directory = \"%s\"\n", fixture_dir);
         fprintf(cfg, "savestate_directory = \"%s\"\n", fixture_dir);
         fclose(cfg);
      }
   }

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }

   lane_passwords();

   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { }

   if (failures)
   {
      fprintf(stderr, "FAIL menu_settings_display_test: %u failures\n",
            failures);
      return 1;
   }
   fprintf(stderr, "PASS menu_settings_display_test\n");
   return 0;
}
