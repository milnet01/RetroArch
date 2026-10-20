/* Single-source definitions: self-updater action.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_SELF_UPDATER; the
 * string tables always carry this row via the strings pass. */
#if defined(HAVE_SELF_UPDATER) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_EX(SELF_UPDATE,
      "self_update", SD_FLAG_NONE, NULL, NULL, CMD_EVENT_SELF_UPDATE_CHECK,
      "Update RetroArch",
      "Check for a newer stable RetroArch. Nothing is downloaded until you choose to update.")
#endif
