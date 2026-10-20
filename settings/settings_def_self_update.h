/* Single-source definitions: self-updater setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_SELF_UPDATER; the
 * string tables always carry this row via the strings pass. */
#if defined(HAVE_SELF_UPDATER) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(self_update_check_on_startup, SELF_UPDATE_CHECK_ON_STARTUP,
      "self_update_check_on_startup",
      DEFAULT_SELF_UPDATE_CHECK_ON_STARTUP, SD_FLAG_NONE, 0, 0,
      "Check for RetroArch Updates at Start-up",
      "Once per start, check whether a newer stable RetroArch is available and say so. Nothing is downloaded.")
#endif
