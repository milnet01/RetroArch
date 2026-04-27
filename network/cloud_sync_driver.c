/*  RetroArch - A frontend for libretro.
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

#include <streams/file_stream.h>

#include "cloud_sync_driver.h"
#include "../configuration.h"
#include "../list_special.h"
#include "../retroarch.h"
#include "../verbosity.h"

static cloud_sync_driver_t cloud_sync_null = {
   NULL,  /* sync_begin */
   NULL,  /* sync_end */
   NULL,  /* read */
   NULL,  /* update */
   NULL,  /* free */
   "null" /* ident */
};

const cloud_sync_driver_t *cloud_sync_drivers[] = {
   &cloud_sync_webdav,
#ifdef HAVE_SSL
   &cloud_sync_google_drive,
#endif
#ifdef HAVE_S3
   &cloud_sync_s3,
#endif
#ifdef HAVE_ICLOUD
   &cloud_sync_icloud,
#endif
#ifdef HAVE_ICLOUD_DRIVE
   &cloud_sync_icloud_drive,
#endif
#ifdef HAVE_SMBCLIENT
   &cloud_sync_smb,
#endif
   &cloud_sync_null,
   NULL
};

static cloud_sync_driver_state_t cloud_sync_driver_st = {0};

cloud_sync_driver_state_t *cloud_sync_state_get_ptr(void)
{
   return &cloud_sync_driver_st;
}

/**
 * config_get_cloud_sync_driver_options:
 *
 * Get an enumerated list of all cloud sync driver names, separated by '|'.
 *
 * @return string listing of all cloud sync driver names, separated by '|'.
 **/
const char* config_get_cloud_sync_driver_options(void)
{
   return char_list_new_special(STRING_LIST_CLOUD_SYNC_DRIVERS, NULL);
}

void cloud_sync_find_driver(const char *cloud_sync_driver,
      const char *prefix, bool verbosity_enabled)
{
   cloud_sync_driver_state_t
      *cloud_sync_st              = &cloud_sync_driver_st;
   int i                        = (int)driver_find_index(
         "cloud_sync_driver", cloud_sync_driver);

   if (i >= 0)
      cloud_sync_st->driver       = (const cloud_sync_driver_t*)
         cloud_sync_drivers[i];
   else
   {
      if (verbosity_enabled && cloud_sync_driver[0])
      {
         unsigned d;
         RARCH_ERR("Couldn't find any %s named \"%s\"\n", prefix,
               cloud_sync_driver);

         RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
         for (d = 0; cloud_sync_drivers[d]; d++)
            RARCH_LOG_OUTPUT("\t%s\n", cloud_sync_drivers[d]->ident);

         RARCH_WARN("Going to default to null...\n");
      }

      i = (int)driver_find_index("cloud_sync_driver", "null");
      cloud_sync_st->driver = (const cloud_sync_driver_t*)cloud_sync_drivers[i];
   }
}

bool cloud_sync_begin(cloud_sync_complete_handler_t cb, void *user_data)
{
   const cloud_sync_driver_t *driver = cloud_sync_state_get_ptr()->driver;
   if (driver && driver->cloud_sync_begin)
      return driver->cloud_sync_begin(cb, user_data);
   return false;
}

bool cloud_sync_end(cloud_sync_complete_handler_t cb,
      void *user_data)
{
   const cloud_sync_driver_t *driver = cloud_sync_state_get_ptr()->driver;
   if (driver && driver->cloud_sync_end)
      return driver->cloud_sync_end(cb, user_data);
   return false;
}

bool cloud_sync_read(const char *path, const char *file,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   const cloud_sync_driver_t *driver = cloud_sync_state_get_ptr()->driver;
   if (driver && driver->cloud_sync_read)
      return driver->cloud_sync_read(path, file, cb, user_data);
   return false;
}

bool cloud_sync_update(const char *path, RFILE *file,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   const cloud_sync_driver_t *driver = cloud_sync_state_get_ptr()->driver;

   /* Reject files larger than the configured cap before any back-end
    * runs.  s3 / webdav / google_drive each malloc the full body for
    * SigV4 / Content-Length / OAuth multipart respectively; an
    * unbounded path is an instant OOM-kill on memory-constrained
    * targets and a runloop stall on desktop.  See
    * docs/private/specs/2026-04-27-cloud-sync-streaming-upload-design.md */
   if (file)
   {
      settings_t  *settings  = config_get_ptr();
      unsigned     cap_mb    = settings ? settings->uints.cloud_sync_max_upload_mb : 0;

      if (cap_mb)
      {
         int64_t file_size;
         filestream_seek(file, 0, SEEK_END);
         file_size = filestream_tell(file);
         filestream_seek(file, 0, SEEK_SET);

         if (file_size > 0 && (uint64_t)file_size > (uint64_t)cap_mb * 1024 * 1024)
         {
            RARCH_ERR("[Cloud Sync] Refusing upload of '%s' (%lld bytes > %u MiB cap).\n",
                  path, (long long)file_size, cap_mb);
            if (cb)
               cb(user_data, path, false, NULL);
            return false;
         }
      }
   }

   if (driver && driver->cloud_sync_update)
      return driver->cloud_sync_update(path, file, cb, user_data);
   return false;
}

bool cloud_sync_free(const char *path,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   const cloud_sync_driver_t *driver = cloud_sync_state_get_ptr()->driver;
   if (driver && driver->cloud_sync_free)
      return driver->cloud_sync_free(path, cb, user_data);
   return false;
}
